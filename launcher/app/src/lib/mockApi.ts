// Browser preview (npm run dev outside Tauri): the backend's answers, in memory, so the
// interface can be developed and screenshotted in any browser. Never used in the app.
import type { Config, GameCheck, Gamepad, GameState, Mod, Patch, Settings } from "@/lib/types";

const config: Config = {
  settings: {
    game_dir: "/Users/hunter/Games/CUSA03173",
    user_dir: "",
    mods_dir: "",
    mods_enabled: true,
    patches_dir: "",
    language: "1",
    fullscreen: false,
    hdr: false,
    present_mode: "Fifo",
    gamepad: "",
    gamepad_name: "",
    fps_mode: "uncap",
    fps_limit: 0,
    draw_pipe: "",
    readbacks: "",
    preupload: "",
    frame_stats: true,
    save_log: false,
    crash_diag: false,
    gpu_profile: false,
    extra_env: "",
    // ?lang=en sets the launcher language in the browser preview (screenshots).
    ui_language: new URLSearchParams(window.location.search).get("lang") ?? "",
  },
  ini: { upscaler: "fsr3", preset: "2", sharpen: "1", sharpness: "0.50", object_motion: "0", show_fps: "1", output_res: "1920x1080" },
  paths: {
    data: "~/Library/Application Support/bloodborne_mac",
    mods: "~/Library/Application Support/bloodborne_mac/mods",
    patches: "~/Library/Application Support/bloodborne_mac/patches",
    logs: "~/Library/Application Support/bloodborne_mac/logs",
    saves: "~/Library/Application Support/bloodborne_mac/user",
  },
  bundled: false,
  version: "0.3.0",
  running: false,
};

let mods: Mod[] = [
  { name: "HD Textures", enabled: true },
  { name: "Lady Maria Outfit", enabled: true },
  { name: "Reshade-like Colors", enabled: false },
];

let patches: Patch[] = [
  { key: "Bloodborne.xml/No Motion Blur", name: "No Motion Blur", file: "Bloodborne.xml", author: "Kyo", note: "", default: false, enabled: true },
  {
    key: "Bloodborne.xml/Disable Chromatic Aberration",
    name: "Disable Chromatic Aberration",
    file: "Bloodborne.xml",
    author: "Lance McDonald",
    note: "Removes the colour fringing at the edges of the screen.",
    default: true,
    enabled: true,
  },
];

const wait = <T,>(value: T) => new Promise<T>((resolve) => setTimeout(() => resolve(value), 120));

export const mockApi = {
  loadConfig: () => wait(structuredClone(config)),
  saveSettings: (settings: Partial<Settings>) => wait(void Object.assign(config.settings, settings)),
  saveIni: (values: Record<string, string | null>) =>
    wait(
      void Object.entries(values).forEach(([k, v]) => {
        if (v === null) delete config.ini[k];
        else config.ini[k] = v;
      }),
    ),
  checkGame: (dir: string): Promise<GameCheck> =>
    wait(
      dir
        ? { problem: null, game: { title_id: "CUSA03173", title: "Bloodborne™", version: "01.09", region: "europe", old_hunters: true } }
        : { problem: "Choose the game folder.", game: null },
    ),
  listMods: () => wait({ mods, dir: config.paths.mods }),
  saveMods: (order: string[], disabled: string[]) =>
    wait(void (mods = order.map((name) => ({ name, enabled: !disabled.includes(name) })))),
  listPatches: () => wait({ patches, dir: config.paths.patches }),
  savePatches: (_shown: string[], enabled: string[], disabled: string[]) =>
    wait(
      void (patches = patches.map((p) => ({
        ...p,
        enabled: enabled.includes(p.key) || (p.default && !disabled.includes(p.key)),
      }))),
    ),
  listGamepads: () => wait<Gamepad[]>([{ guid: "030000004c050000e60c000000016800", name: "DualSense Wireless Controller" }]),
  readInput: (kind: "key" | "pad") => wait(kind === "pad" ? "rightshoulder" : "G"),
  openPath: () => wait(undefined),
  launch: () => wait(undefined),
  stop: () => wait(undefined),
  onOutput: (_handler: (line: string) => void) => Promise.resolve(() => {}),
  onState: (_handler: (state: GameState) => void) => Promise.resolve(() => {}),
};
