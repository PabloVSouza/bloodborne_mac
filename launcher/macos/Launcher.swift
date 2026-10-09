// bloodborne_mac launcher: the app bundle's front end (packaging/macos.sh builds it into
// Bloodborne.app). It keeps the game folder and the main settings, and starts run.sh with the
// bash and Python bundled in the app, so the user needs neither Homebrew nor Python.
//
// Bundle layout:
//   Contents/MacOS/        Bloodborne (this launcher), bb-probe, bb-gpu-capabilities, bash
//   Contents/Frameworks/   libbbgpu, MoltenVK, the Vulkan loader, SDL3, FFmpeg
//   Contents/Resources/game/    run.sh, scripts/, patches/, share/vulkan/icd.d/
//   Contents/Resources/python/  standalone Python 3
// Writable data (generated files, saves, shader cache, bbport.ini, logs):
//   ~/Library/Application Support/bloodborne_mac
import AppKit
import SwiftUI

// MARK: - Settings file (bbport.ini, shared with the in-game menu)

/// Reads and writes key=value lines, keeping every line it does not change.
struct IniFile {
    let url: URL
    private(set) var lines: [String] = []

    init(url: URL) {
        self.url = url
        if let text = try? String(contentsOf: url, encoding: .utf8) {
            lines = text.components(separatedBy: "\n")
            if lines.last == "" { lines.removeLast() }
        }
    }

    func value(_ key: String) -> String? {
        for line in lines where !line.hasPrefix("#") {
            let parts = line.split(separator: "=", maxSplits: 1).map(String.init)
            if parts.count == 2, parts[0].trimmingCharacters(in: .whitespaces) == key {
                return parts[1].trimmingCharacters(in: .whitespaces)
            }
        }
        return nil
    }

    mutating func set(_ key: String, _ value: String) {
        for (i, line) in lines.enumerated() where !line.hasPrefix("#") {
            if line.split(separator: "=", maxSplits: 1).first.map({ $0.trimmingCharacters(in: .whitespaces) }) == key {
                lines[i] = "\(key)=\(value)"
                return
            }
        }
        lines.append("\(key)=\(value)")
    }

    func save() throws {
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(),
                                                withIntermediateDirectories: true)
        try (lines.joined(separator: "\n") + "\n").write(to: url, atomically: true, encoding: .utf8)
    }
}

// MARK: - Model

struct Effect: Identifiable {
    let key: String
    let label: String
    let defaultOn: Bool
    var id: String { key }
}

let effects = [
    Effect(key: "effect_chromatic_aberration", label: "Chromatic aberration", defaultOn: true),
    Effect(key: "effect_dof", label: "Depth of field", defaultOn: true),
    Effect(key: "effect_motion_blur", label: "Motion blur", defaultOn: true),
    Effect(key: "effect_ssao", label: "Ambient occlusion (SSAO)", defaultOn: true),
    Effect(key: "effect_game_aa", label: "The game's own anti-aliasing", defaultOn: true),
    Effect(key: "effect_dynamic_shadows", label: "Dynamic light shadows", defaultOn: true),
    Effect(key: "effect_ssr", label: "Screen-space reflections (not in the original)", defaultOn: false),
]

let presets = ["Native AA", "Quality", "Balanced", "Performance", "Ultra Performance"]
let presetScales = [1.0, 1.5, 1.7, 2.0, 3.0]

@MainActor
final class Launcher: ObservableObject {
    @Published var gameDir: String = UserDefaults.standard.string(forKey: "gameDir") ?? ""
    @Published var upscaler = "fsr3"
    @Published var preset = 2
    @Published var fps = UserDefaults.standard.string(forKey: "fps") ?? "uncap"
    @Published var showFps = true
    @Published var objectMotion = false
    @Published var effectOn: [String: Bool] = [:]
    @Published var log = ""
    @Published var running = false
    @Published var status = ""

    private var process: Process?
    private var logFile: FileHandle?

    let bundle = Bundle.main.bundleURL
    var macos: URL { bundle.appendingPathComponent("Contents/MacOS") }
    var resources: URL { bundle.appendingPathComponent("Contents/Resources") }
    let dataDir = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("bloodborne_mac")
    var iniURL: URL { dataDir.appendingPathComponent("bbport.ini") }

    init() {
        let ini = IniFile(url: iniURL)
        upscaler = ini.value("upscaler") == "off" ? "off" : "fsr3"
        preset = Int(ini.value("preset") ?? "") ?? 2
        showFps = (ini.value("show_fps") ?? "1") != "0"
        objectMotion = (ini.value("object_motion") ?? "0") == "1"
        for effect in effects {
            effectOn[effect.key] = (ini.value(effect.key) ?? (effect.defaultOn ? "1" : "0")) == "1"
        }
    }

    var gameProblem: String? {
        if gameDir.isEmpty { return "Choose the game folder (CUSA03173)." }
        if !FileManager.default.fileExists(atPath: (gameDir as NSString).appendingPathComponent("eboot.bin")) {
            return "No eboot.bin in this folder: choose the CUSA03173 folder of your dump."
        }
        return nil
    }

    func chooseGame() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.message = "Choose your Bloodborne dump (the CUSA03173 folder, version 1.09)"
        if panel.runModal() == .OK, let url = panel.url {
            gameDir = url.path
            UserDefaults.standard.set(gameDir, forKey: "gameDir")
        }
    }

    func saveSettings() {
        var ini = IniFile(url: iniURL)
        ini.set("upscaler", upscaler)
        ini.set("preset", String(preset))
        ini.set("show_fps", showFps ? "1" : "0")
        ini.set("object_motion", objectMotion ? "1" : "0")
        ini.set("output_res", ini.value("output_res") ?? "1920x1080")
        for effect in effects { ini.set(effect.key, effectOn[effect.key] == true ? "1" : "0") }
        do { try ini.save() } catch { append("Could not save the settings: \(error)\n") }
        UserDefaults.standard.set(fps, forKey: "fps")
    }

    func append(_ text: String) {
        log += text
        if log.count > 200_000 { log = String(log.suffix(150_000)) }
        logFile?.write(Data(text.utf8))
    }

    func play() {
        guard !running, gameProblem == nil else { return }
        saveSettings()
        log = ""
        let fm = FileManager.default
        let logs = dataDir.appendingPathComponent("logs")
        try? fm.createDirectory(at: logs, withIntermediateDirectories: true)
        let logURL = logs.appendingPathComponent("last.log")
        fm.createFile(atPath: logURL.path, contents: nil)
        logFile = try? FileHandle(forWritingTo: logURL)

        let p = Process()
        p.executableURL = macos.appendingPathComponent("bash")
        p.arguments = ["run.sh"]
        p.currentDirectoryURL = resources.appendingPathComponent("game")
        var env = ProcessInfo.processInfo.environment
        env["PATH"] = macos.path + ":/usr/bin:/bin:/usr/sbin:/sbin"
        env["BB_PREBUILT"] = "1"
        env["BB_PROBE"] = macos.appendingPathComponent("bb-probe").path
        env["BB_DATA_DIR"] = dataDir.path
        env["BB_GAME_DIR"] = gameDir
        env["BB_FPS"] = fps
        env["BB_FRAME_STATS"] = env["BB_FRAME_STATS"] ?? "1"
        env["PYTHON"] = resources.appendingPathComponent("python/bin/python3").path
        env["PYTHONDONTWRITEBYTECODE"] = "1" // the bundle is read-only
        p.environment = env
        let pipe = Pipe()
        p.standardOutput = pipe
        p.standardError = pipe
        pipe.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            guard !data.isEmpty else { return }
            let text = String(decoding: data, as: UTF8.self)
            Task { @MainActor in self.append(text) }
        }
        p.terminationHandler = { proc in
            Task { @MainActor in
                pipe.fileHandleForReading.readabilityHandler = nil
                self.running = false
                self.process = nil
                self.status = proc.terminationStatus == 0 || proc.terminationReason == .uncaughtSignal
                    ? "The game has exited." : "The game stopped with an error (code \(proc.terminationStatus)): see the log."
                try? self.logFile?.close()
                self.logFile = nil
            }
        }
        do {
            try p.run()
            process = p
            running = true
            status = "Running. The first start compiles the game's shaders and takes a few minutes."
        } catch {
            status = "Could not start: \(error.localizedDescription)"
        }
    }

    func stop() { process?.terminate() }

    func openDataFolder() {
        try? FileManager.default.createDirectory(at: dataDir, withIntermediateDirectories: true)
        NSWorkspace.shared.open(dataDir)
    }
}

// MARK: - Views

struct ContentView: View {
    @ObservedObject var model: Launcher
    @State private var showLog = false

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(alignment: .firstTextBaseline) {
                Text("Bloodborne").font(.largeTitle.weight(.semibold))
                Text("for Apple Silicon").foregroundStyle(.secondary)
            }
            GroupBox("Game") {
                VStack(alignment: .leading, spacing: 6) {
                    HStack {
                        Text(model.gameDir.isEmpty ? "No folder chosen" : model.gameDir)
                            .lineLimit(1).truncationMode(.middle)
                            .foregroundStyle(model.gameDir.isEmpty ? .secondary : .primary)
                        Spacer()
                        Button("Choose…") { model.chooseGame() }.disabled(model.running)
                    }
                    if let problem = model.gameProblem {
                        Text(problem).font(.callout).foregroundStyle(.orange)
                    }
                }.padding(4)
            }
            GroupBox("Graphics") {
                Form {
                    Picker("Upscaler", selection: $model.upscaler) {
                        Text("FSR 3.1").tag("fsr3")
                        Text("Off (native 1080p)").tag("off")
                    }
                    Picker("FSR preset", selection: $model.preset) {
                        ForEach(presets.indices, id: \.self) { i in
                            Text(i == 0 ? presets[i] : "\(presets[i]) (render \(Int((1920 / presetScales[i]).rounded()))×\(Int((1080 / presetScales[i]).rounded())))").tag(i)
                        }
                    }.disabled(model.upscaler == "off")
                    Picker("Frame rate", selection: $model.fps) {
                        Text("Unlocked").tag("uncap")
                        Text("60 FPS").tag("60")
                        Text("30 FPS (original)").tag("30")
                    }
                    Toggle("Character motion vectors (better FSR on moving characters, ~5 ms slower)",
                           isOn: $model.objectMotion).disabled(model.upscaler == "off")
                    Toggle("Show FPS", isOn: $model.showFps)
                }.padding(4)
            }
            DisclosureGroup("Effects") {
                VStack(alignment: .leading) {
                    ForEach(effects) { effect in
                        Toggle(effect.label, isOn: Binding(
                            get: { model.effectOn[effect.key] ?? effect.defaultOn },
                            set: { model.effectOn[effect.key] = $0 }))
                    }
                }.padding(.leading, 8)
            }.disabled(model.running)
            HStack {
                if model.running {
                    Button("Stop") { model.stop() }.keyboardShortcut(.cancelAction)
                } else {
                    Button("Play") { model.play() }
                        .keyboardShortcut(.defaultAction)
                        .disabled(model.gameProblem != nil)
                }
                Button("Open data folder") { model.openDataFolder() }
                Spacer()
                Toggle("Log", isOn: $showLog).toggleStyle(.switch)
            }
            if !model.status.isEmpty {
                Text(model.status).font(.callout).foregroundStyle(.secondary)
            }
            if showLog {
                ScrollViewReader { proxy in
                    ScrollView {
                        Text(model.log).font(.system(.caption, design: .monospaced))
                            .frame(maxWidth: .infinity, alignment: .leading).textSelection(.enabled)
                        Color.clear.frame(height: 1).id("end")
                    }
                    .frame(minHeight: 180)
                    .background(Color(nsColor: .textBackgroundColor))
                    .onChange(of: model.log) { _ in proxy.scrollTo("end") }
                }
            }
            Text("Settings are shared with the in-game menu (Insert or L3+R3). Preset changes apply at the next start.")
                .font(.caption).foregroundStyle(.secondary)
        }
        .padding(20)
        .frame(minWidth: 560)
        .onChange(of: model.upscaler) { _ in model.saveSettings() }
    }
}

@main
struct BloodborneApp: App {
    @StateObject private var model = Launcher()

    var body: some Scene {
        WindowGroup("Bloodborne") {
            ContentView(model: model)
                // `open Bloodborne.app --args --play`: start at once (shortcuts, scripted tests).
                .onAppear {
                    if CommandLine.arguments.contains("--play") { model.play() }
                }
                .onReceive(NotificationCenter.default.publisher(for: NSApplication.willTerminateNotification)) { _ in
                    model.stop()
                }
        }
        .windowResizability(.contentSize)
    }
}
