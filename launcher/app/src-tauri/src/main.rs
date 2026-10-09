// SPDX-License-Identifier: GPL-2.0-or-later
//! bloodborne_mac launcher: the Tauri backend behind the React interface (launcher/app/src).
//!
//! It keeps two files in the data directory (~/Library/Application Support/bloodborne_mac):
//! launcher.json (start-up options passed to run.sh as environment variables) and bbport.ini
//! (the port's settings, shared with the in-game menu; lines it does not change are kept).
//! Mods, third-party patches and the game check come from scripts/launcher_api.py (the same code
//! run.sh uses), controllers from bb-gpu-capabilities (--gamepads, --read-input). The game runs
//! as `bash run.sh` with the app's own bash and Python; its output is sent to the window.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::Serialize;
use serde_json::{json, Map, Value};
use std::collections::BTreeMap;
use std::fs;
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::{Arc, Mutex};
use tauri::{AppHandle, Emitter, Manager, State};

/// Where the launcher finds the game's programs and scripts.
#[derive(Clone, Serialize)]
struct Paths {
    /// run.sh, scripts/, patches/ (Contents/Resources/game, or a source checkout).
    game_root: PathBuf,
    bash: PathBuf,
    python: PathBuf,
    probe: PathBuf,
    capabilities: PathBuf,
    data: PathBuf,
    /// Packaged in an app bundle (else a development run from a checkout).
    bundled: bool,
}

impl Paths {
    fn find() -> Paths {
        let exe = std::env::current_exe().unwrap_or_default();
        let macos = exe.parent().map(Path::to_path_buf).unwrap_or_default();
        let data = dirs_data().join("bloodborne_mac");
        let resources = macos.parent().map(|c| c.join("Resources")).unwrap_or_default();
        if resources.join("game/run.sh").is_file() {
            return Paths {
                game_root: resources.join("game"),
                bash: macos.join("bash"),
                python: resources.join("python/bin/python3"),
                probe: macos.join("bb-probe"),
                capabilities: macos.join("bb-gpu-capabilities"),
                data,
                bundled: true,
            };
        }
        // Development (npm run tauri dev): the checkout's build, Homebrew's bash, any Python 3.
        let root = std::env::var_os("BB_REPO").map(PathBuf::from).unwrap_or_else(|| {
            let mut dir = macos.clone();
            while !dir.join("run.sh").is_file() {
                if !dir.pop() {
                    break;
                }
            }
            dir
        });
        let bash = ["/opt/homebrew/bin/bash", "/usr/local/bin/bash"]
            .iter()
            .map(PathBuf::from)
            .find(|p| p.is_file())
            .unwrap_or_else(|| PathBuf::from("/bin/bash"));
        Paths {
            probe: root.join("out/bb-probe"),
            capabilities: root.join("out/bb-gpu-capabilities"),
            game_root: root,
            bash,
            python: PathBuf::from("python3"),
            data,
            bundled: false,
        }
    }
}

fn dirs_data() -> PathBuf {
    std::env::var_os("HOME")
        .map(|h| PathBuf::from(h).join("Library/Application Support"))
        .unwrap_or_else(|| PathBuf::from("."))
}

/// Start-up options (launcher.json) and their defaults; unknown keys are kept.
fn default_settings() -> Map<String, Value> {
    let v = json!({
        "game_dir": "",
        "user_dir": "",
        "mods_dir": "",
        "mods_enabled": true,
        "patches_dir": "",
        "language": "1",
        "fullscreen": false,
        "hdr": false,
        "present_mode": "Fifo",
        "gamepad": "",
        "gamepad_name": "",
        "fps_mode": "uncap",
        "fps_limit": 0,
        "draw_pipe": "",
        "readbacks": "",
        "preupload": "",
        "frame_stats": true,
        "save_log": false,
        "crash_diag": false,
        "gpu_profile": false,
        "extra_env": "",
        "ui_language": ""
    });
    v.as_object().cloned().unwrap_or_default()
}

/// bbport.ini values the launcher shows before the file has them (macOS defaults).
fn default_ini() -> BTreeMap<String, String> {
    [
        ("upscaler", "fsr3"),
        ("preset", "2"),
        ("sharpen", "1"),
        ("sharpness", "0.50"),
        ("object_motion", "0"),
        ("show_fps", "1"),
        ("output_res", "1920x1080"),
        ("model_lod", "0"),
        ("live_resolution", "0"),
        ("effect_chromatic_aberration", "1"),
        ("effect_dof", "1"),
        ("effect_motion_blur", "1"),
        ("effect_ssao", "1"),
        ("effect_game_aa", "1"),
        ("effect_dynamic_shadows", "1"),
        ("effect_ssr", "0"),
        ("skip_intro", "0"),
        ("debug_camera", "0"),
        ("debug_menu", "0"),
    ]
    .into_iter()
    .map(|(k, v)| (k.to_string(), v.to_string()))
    .collect()
}

struct Game {
    child: Option<Child>,
}

struct AppState {
    paths: Paths,
    game: Arc<Mutex<Game>>,
}

fn settings_file(p: &Paths) -> PathBuf {
    p.data.join("launcher.json")
}
fn ini_file(p: &Paths) -> PathBuf {
    p.data.join("bbport.ini")
}

fn load_settings(p: &Paths) -> Map<String, Value> {
    let mut settings = default_settings();
    if let Ok(text) = fs::read_to_string(settings_file(p)) {
        if let Ok(Value::Object(saved)) = serde_json::from_str::<Value>(&text) {
            settings.extend(saved);
        }
    }
    settings
}

fn read_ini_lines(p: &Paths) -> Vec<String> {
    fs::read_to_string(ini_file(p))
        .map(|t| t.lines().map(str::to_string).collect())
        .unwrap_or_default()
}

fn ini_key(line: &str) -> Option<&str> {
    if line.trim_start().starts_with('#') {
        return None;
    }
    line.split_once('=').map(|(k, _)| k.trim())
}

fn load_ini(p: &Paths) -> BTreeMap<String, String> {
    let mut values = default_ini();
    for line in read_ini_lines(p) {
        if let Some((k, v)) = line.split_once('=') {
            if !k.trim_start().starts_with('#') {
                values.insert(k.trim().to_string(), v.trim().to_string());
            }
        }
    }
    values
}

fn str_setting(s: &Map<String, Value>, key: &str) -> String {
    match s.get(key) {
        Some(Value::String(v)) => v.clone(),
        Some(Value::Number(n)) => n.to_string(),
        Some(Value::Bool(b)) => (if *b { "1" } else { "0" }).to_string(),
        _ => String::new(),
    }
}

fn bool_setting(s: &Map<String, Value>, key: &str) -> bool {
    matches!(s.get(key), Some(Value::Bool(true)))
}

fn expand(path: &str) -> PathBuf {
    if let Some(rest) = path.strip_prefix("~/") {
        if let Some(home) = std::env::var_os("HOME") {
            return PathBuf::from(home).join(rest);
        }
    }
    PathBuf::from(path)
}

fn mods_dir(p: &Paths, s: &Map<String, Value>) -> PathBuf {
    let dir = str_setting(s, "mods_dir");
    if dir.is_empty() { p.data.join("mods") } else { expand(&dir) }
}

fn patches_dir(p: &Paths, s: &Map<String, Value>) -> PathBuf {
    let dir = str_setting(s, "patches_dir");
    if dir.is_empty() { p.data.join("patches") } else { expand(&dir) }
}

/// scripts/launcher_api.py with the app's Python.
fn api(p: &Paths, args: &[&str]) -> Result<Value, String> {
    let out = Command::new(&p.python)
        .arg(p.game_root.join("scripts/launcher_api.py"))
        .args(args)
        .env("PYTHONDONTWRITEBYTECODE", "1")
        .output()
        .map_err(|e| format!("Python: {e}"))?;
    if !out.status.success() {
        return Err(String::from_utf8_lossy(&out.stderr).trim().to_string());
    }
    serde_json::from_slice(&out.stdout).map_err(|e| e.to_string())
}

// --- commands --------------------------------------------------------------------------------

#[tauri::command]
fn load_config(state: State<AppState>) -> Value {
    let p = &state.paths;
    let s = load_settings(p);
    json!({
        "settings": s,
        "ini": load_ini(p),
        "paths": {
            "data": p.data,
            "mods": mods_dir(p, &s),
            "patches": patches_dir(p, &s),
            "logs": p.data.join("logs"),
            "saves": if str_setting(&s, "user_dir").is_empty() { p.data.join("user") } else { expand(&str_setting(&s, "user_dir")) },
        },
        "bundled": p.bundled,
        "version": env!("CARGO_PKG_VERSION"),
        "running": state.game.lock().map(|g| g.child.is_some()).unwrap_or(false),
    })
}

#[tauri::command]
fn save_settings(state: State<AppState>, settings: Map<String, Value>) -> Result<(), String> {
    let p = &state.paths;
    fs::create_dir_all(&p.data).map_err(|e| e.to_string())?;
    let mut merged = load_settings(p);
    merged.extend(settings);
    let text = serde_json::to_string_pretty(&Value::Object(merged)).map_err(|e| e.to_string())?;
    fs::write(settings_file(p), text + "\n").map_err(|e| e.to_string())
}

/// Sets bbport.ini keys (null removes the line: the default applies), keeping the other lines.
#[tauri::command]
fn save_ini(state: State<AppState>, values: BTreeMap<String, Option<String>>) -> Result<(), String> {
    let p = &state.paths;
    fs::create_dir_all(&p.data).map_err(|e| e.to_string())?;
    let mut lines = read_ini_lines(p);
    for (key, value) in &values {
        let pos = lines.iter().position(|l| ini_key(l) == Some(key.as_str()));
        match (pos, value) {
            (Some(i), Some(v)) => lines[i] = format!("{key}={v}"),
            (Some(i), None) => {
                lines.remove(i);
            }
            (None, Some(v)) => lines.push(format!("{key}={v}")),
            (None, None) => {}
        }
    }
    fs::write(ini_file(p), lines.join("\n") + "\n").map_err(|e| e.to_string())
}

#[tauri::command]
async fn check_game(state: State<'_, AppState>, dir: String) -> Result<Option<String>, String> {
    if dir.is_empty() {
        return Ok(Some("Choose the game folder (CUSA03173).".into()));
    }
    let v = api(&state.paths, &["check", &expand(&dir).to_string_lossy()])?;
    Ok(v.get("problem").and_then(Value::as_str).map(str::to_string))
}

#[tauri::command]
async fn list_mods(state: State<'_, AppState>) -> Result<Value, String> {
    let p = &state.paths;
    let s = load_settings(p);
    let dir = mods_dir(p, &s);
    let _ = fs::create_dir_all(&dir);
    let mut v = api(p, &["mods", &dir.to_string_lossy(), &p.data.join("mods.json").to_string_lossy()])?;
    v["dir"] = json!(dir);
    Ok(v)
}

#[tauri::command]
fn save_mods(state: State<AppState>, order: Vec<String>, disabled: Vec<String>) -> Result<(), String> {
    let p = &state.paths;
    fs::create_dir_all(&p.data).map_err(|e| e.to_string())?;
    let text = serde_json::to_string_pretty(&json!({"order": order, "disabled": disabled}))
        .map_err(|e| e.to_string())?;
    fs::write(p.data.join("mods.json"), text + "\n").map_err(|e| e.to_string())
}

#[tauri::command]
async fn list_patches(state: State<'_, AppState>) -> Result<Value, String> {
    let p = &state.paths;
    let s = load_settings(p);
    let dir = patches_dir(p, &s);
    let _ = fs::create_dir_all(&dir);
    let mut v = api(p, &["patches", &dir.to_string_lossy(), &p.data.join("patches.json").to_string_lossy()])?;
    v["dir"] = json!(dir);
    Ok(v)
}

/// patches.json: the switches that differ from each file's default; choices for patches not
/// listed now (another folder) are kept.
#[tauri::command]
fn save_patches(state: State<AppState>, shown: Vec<String>, enabled: Vec<String>, disabled: Vec<String>) -> Result<(), String> {
    let p = &state.paths;
    let path = p.data.join("patches.json");
    let old: Value = fs::read_to_string(&path).ok().and_then(|t| serde_json::from_str(&t).ok()).unwrap_or(json!({}));
    let keep = |key: &str| -> Vec<String> {
        old.get(key)
            .and_then(Value::as_array)
            .map(|a| a.iter().filter_map(Value::as_str).filter(|k| !shown.iter().any(|s| s == k)).map(str::to_string).collect())
            .unwrap_or_default()
    };
    let mut en = keep("enabled");
    en.extend(enabled);
    en.sort();
    en.dedup();
    let mut dis = keep("disabled");
    dis.extend(disabled);
    dis.sort();
    dis.dedup();
    fs::create_dir_all(&p.data).map_err(|e| e.to_string())?;
    let text = serde_json::to_string_pretty(&json!({"enabled": en, "disabled": dis})).map_err(|e| e.to_string())?;
    fs::write(path, text + "\n").map_err(|e| e.to_string())
}

#[derive(Serialize)]
struct Gamepad {
    guid: String,
    name: String,
}

#[tauri::command]
async fn list_gamepads(state: State<'_, AppState>) -> Result<Vec<Gamepad>, String> {
    let out = Command::new(&state.paths.capabilities)
        .arg("--gamepads")
        .output()
        .map_err(|e| e.to_string())?;
    Ok(String::from_utf8_lossy(&out.stdout)
        .lines()
        .filter_map(|l| l.split_once('\t'))
        .map(|(g, n)| Gamepad { guid: g.to_string(), name: n.to_string() })
        .collect())
}

/// One key or button for a control: bb-gpu-capabilities opens a small window and prints
/// "key <name>" or "pad <name>"; None when cancelled.
#[tauri::command]
async fn read_input(state: State<'_, AppState>, kind: String) -> Result<Option<String>, String> {
    let caps = state.paths.capabilities.clone();
    let out = tauri::async_runtime::spawn_blocking(move || Command::new(caps).args(["--read-input", &kind]).output())
        .await
        .map_err(|e| e.to_string())?
        .map_err(|e| e.to_string())?;
    let text = String::from_utf8_lossy(&out.stdout).trim().to_string();
    Ok(text.split_once(' ').map(|(_, name)| name.to_string()))
}

#[tauri::command]
fn open_path(path: String) -> Result<(), String> {
    let path = expand(&path);
    let _ = fs::create_dir_all(&path);
    Command::new("open").arg(path).spawn().map(|_| ()).map_err(|e| e.to_string())
}

fn game_environment(p: &Paths, s: &Map<String, Value>) -> Vec<(String, String)> {
    let mut env: Vec<(String, String)> = Vec::new();
    let mut set = |k: &str, v: String| env.push((k.to_string(), v));
    let path_dirs = [p.bash.parent().map(|d| d.to_string_lossy().to_string()).unwrap_or_default(),
                     "/usr/bin:/bin:/usr/sbin:/sbin".to_string()];
    set("PATH", path_dirs.join(":"));
    set("BB_PREBUILT", "1".into());
    set("BB_PROBE", p.probe.to_string_lossy().into());
    set("BB_DATA_DIR", p.data.to_string_lossy().into());
    set("PYTHON", p.python.to_string_lossy().into());
    set("PYTHONDONTWRITEBYTECODE", "1".into());
    set("BB_GAME_DIR", expand(&str_setting(s, "game_dir")).to_string_lossy().into());
    let user = str_setting(s, "user_dir");
    if !user.is_empty() {
        set("BB_USER_DIR", expand(&user).to_string_lossy().into());
    }
    set("BB_MODS_DIR", mods_dir(p, s).to_string_lossy().into());
    set("BB_MODS_CONFIG", p.data.join("mods.json").to_string_lossy().into());
    set("BB_MODS_ENABLED", (if bool_setting(s, "mods_enabled") { "1" } else { "0" }).into());
    set("BB_PATCHES_DIR", patches_dir(p, s).to_string_lossy().into());
    set("BB_PATCHES_CONFIG", p.data.join("patches.json").to_string_lossy().into());
    set("BB_LANGUAGE", str_setting(s, "language"));
    // The in-game menu has English and Russian.
    let ui = str_setting(s, "ui_language");
    let system_ru = std::env::var("LANG").map(|l| l.starts_with("ru")).unwrap_or(false);
    let ru = ui.starts_with("ru") || (ui.is_empty() && system_ru);
    set("BB_UI_LANGUAGE", (if ru { "ru" } else { "en" }).into());
    set("BB_FULLSCREEN", (if bool_setting(s, "fullscreen") { "1" } else { "0" }).into());
    set("BB_PRESENT_MODE", str_setting(s, "present_mode"));
    for (key, var) in [("gamepad", "BB_GAMEPAD"), ("draw_pipe", "BB_DRAW_PIPE"), ("readbacks", "BB_READBACKS"),
                       ("preupload", "BB_PREUPLOAD")] {
        let v = str_setting(s, key);
        if !v.is_empty() {
            set(var, v);
        }
    }
    if bool_setting(s, "hdr") {
        set("BB_HDR", "1".into());
    }
    set("BB_FPS", str_setting(s, "fps_mode"));
    let limit = s.get("fps_limit").and_then(Value::as_u64).unwrap_or(0);
    if limit > 0 {
        set("BB_FPS_LIMIT", limit.to_string());
    }
    if bool_setting(s, "frame_stats") {
        set("BB_FRAME_STATS", "1".into());
    }
    if bool_setting(s, "save_log") {
        set("BB_SAVE_LOG", "1".into());
    }
    if bool_setting(s, "crash_diag") {
        set("BB_FREE_CHECK", "1".into());
        set("BB_WRITE_LOG", "1".into());
    }
    if bool_setting(s, "gpu_profile") {
        set("BB_GPU_PROFILE", "1".into());
    }
    for item in str_setting(s, "extra_env").split_whitespace() {
        if let Some((k, v)) = item.split_once('=') {
            set(k, v.to_string());
        }
    }
    env
}

#[tauri::command]
fn launch(app: AppHandle, state: State<AppState>) -> Result<(), String> {
    start_game(&app, &state)
}

fn start_game(app: &AppHandle, state: &AppState) -> Result<(), String> {
    let app = app.clone();
    let p = state.paths.clone();
    let mut game = state.game.lock().map_err(|e| e.to_string())?;
    if game.child.is_some() {
        return Err("The game is already running.".into());
    }
    let s = load_settings(&p);
    fs::create_dir_all(p.data.join("logs")).map_err(|e| e.to_string())?;
    let log_path = p.data.join("logs/last.log");
    let mut cmd = Command::new(&p.bash);
    cmd.arg("run.sh")
        .current_dir(&p.game_root)
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    for (k, v) in game_environment(&p, &s) {
        cmd.env(k, v);
    }
    let mut child = cmd.spawn().map_err(|e| format!("Could not start the game: {e}"))?;
    let log = Arc::new(Mutex::new(fs::File::create(&log_path).ok()));
    for stream in [child.stdout.take().map(|s| Box::new(s) as Box<dyn std::io::Read + Send>),
                   child.stderr.take().map(|s| Box::new(s) as Box<dyn std::io::Read + Send>)]
        .into_iter()
        .flatten()
    {
        let app = app.clone();
        let log = log.clone();
        std::thread::spawn(move || {
            for line in BufReader::new(stream).lines().map_while(Result::ok) {
                if let Ok(mut f) = log.lock() {
                    if let Some(f) = f.as_mut() {
                        use std::io::Write;
                        let _ = writeln!(f, "{line}");
                    }
                }
                let _ = app.emit("game-output", line);
            }
        });
    }
    game.child = Some(child);
    let _ = app.emit("game-state", json!({"running": true}));
    // Waits for the game to end without holding the lock.
    let shared = state.game.clone();
    std::thread::spawn(move || loop {
        std::thread::sleep(std::time::Duration::from_millis(300));
        let mut g = match shared.lock() {
            Ok(g) => g,
            Err(_) => return,
        };
        let status = match g.child.as_mut().map(Child::try_wait) {
            Some(Ok(Some(status))) => status,
            Some(Ok(None)) => continue,
            _ => {
                g.child = None;
                return;
            }
        };
        g.child = None;
        let _ = app.emit("game-state", json!({"running": false, "code": status.code()}));
        return;
    });
    Ok(())
}

#[tauri::command]
fn stop(state: State<AppState>) -> Result<(), String> {
    let game = state.game.lock().map_err(|e| e.to_string())?;
    if let Some(child) = game.child.as_ref() {
        // SIGTERM: run.sh execs the game, which quits cleanly on it.
        let _ = Command::new("kill").arg(child.id().to_string()).status();
    }
    Ok(())
}

fn main() {
    let paths = Paths::find();
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(AppState { paths, game: Arc::new(Mutex::new(Game { child: None })) })
        // `open Bloodborne.app --args --play`: start the game at once (shortcuts, scripted tests).
        .setup(|app| {
            if std::env::args().any(|a| a == "--play") {
                let handle = app.handle().clone();
                let state = app.state::<AppState>();
                if let Err(e) = start_game(&handle, &state) {
                    eprintln!("--play: {e}");
                }
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            load_config, save_settings, save_ini, check_game, list_mods, save_mods, list_patches,
            save_patches, list_gamepads, read_input, open_path, launch, stop
        ])
        .on_window_event(|window, event| {
            if let tauri::WindowEvent::Destroyed = event {
                if let Some(state) = window.app_handle().try_state::<AppState>() {
                    if let Ok(game) = state.game.lock() {
                        if let Some(child) = game.child.as_ref() {
                            let _ = Command::new("kill").arg(child.id().to_string()).status();
                        }
                    }
                }
            }
        })
        .run(tauri::generate_context!())
        .expect("error while running the launcher");
}
