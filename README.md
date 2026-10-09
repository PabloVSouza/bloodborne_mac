# bloodborne_mac — Bloodborne on macOS (Apple Silicon)

A macOS port of *Bloodborne* for PlayStation 4 (CUSA03173, game version 1.09) that runs
**natively on Apple Silicon**, without Rosetta 2.

This is a fork of [**deadinside28/bloodborne_pc**](https://github.com/deadinside28/bloodborne_pc)
(*bbport*, a native Linux port of the game), which in turn builds its renderer on
[**shadPS4**](https://github.com/shadps4-emu/shadPS4). Almost everything that makes the game run —
the loader, the PS4 runtime, the renderer and its extensions, the upscalers, the patches — comes
from those projects. This fork adds the macOS platform layer and an x86-64 → arm64 translator so
the game's code can run on Apple Silicon. The upstream README is kept in
[docs/upstream/README.md](docs/upstream/README.md) ([Русский](docs/upstream/README.ru.md)).

> This project is not affiliated with shadPS4, Sony Interactive Entertainment, FromSoftware or
> AMD. Please do not report problems with this fork to shadPS4 or to the upstream project.
>
> **No game files are included.** You need your own dump of Bloodborne (CUSA03173, v1.09).

**Status: experimental, playable.** The game boots, loads saves and plays with sound, gamepad and
saving. Only one machine has been tested: an Apple M3 Pro (18-core GPU, 18 GB) on macOS 27.

## How it works

- **CPU: `bbcpu`, an in-process x86-64 → arm64 translator** ([src/cpu/](src/cpu)). The game's
  code is x86-64 machine code with no source, so it is translated block by block into arm64 at
  run time (with an interpreter as fallback). Guest registers live in host registers, flags are
  computed only where they are read, and x86's strong memory ordering is kept with arm64
  acquire/release accesses. Each translation is checked against the interpreter by a fuzzer
  ([docs/ARM64_NATIVE.md](docs/ARM64_NATIVE.md)).
- **System libraries:** bbport's runtime (`src/runtime_*.c`) implements the PS4 OS functions the
  game calls: memory, threads, files, audio, pad, saves.
- **Graphics:** bbport's shadPS4-derived renderer translates the PS4 GPU's commands and shaders to
  Vulkan, which runs on Metal through [MoltenVK](https://github.com/KhronosGroup/MoltenVK).
  Mesa's KosmicKrisp driver can be used instead (`BB_VK_DRIVER=kosmickrisp`, macOS 26+), but it
  is much slower today.
- **Upscaling:** FSR 3.1 renders the game below the output resolution and reconstructs the output
  (in-game menu: *Insert* or *L3+R3*).

## Performance

Measured in the clinic area at the start of the game, Apple M3 Pro, 1080p output:

| Setting | FPS |
|---|---|
| Native 1080p | ~31 |
| FSR 3.1, preset 2 (renders 1130×636), character motion vectors off | ~44 |

The frame rate is uncapped by default (community frame-time patch). At 1080p the game is limited
by the GPU. What was measured and changed, and what could still be gained:
[docs/MACOS_PERFORMANCE.md](docs/MACOS_PERFORMANCE.md).

## Requirements

- A Mac with Apple Silicon. macOS 15 or newer is recommended (GPU memory residency sets).
- Xcode Command Line Tools and [Homebrew](https://brew.sh):
  `brew install bash pkgconf glslang nasm cmake ninja`.
- Your decrypted game dump: the `CUSA03173` folder at version 1.09 (copy a dumped update over the
  base game, replacing files).

## Download

Prebuilt packages for Apple Silicon are on the
[releases page](https://github.com/PabloVSouza/bloodborne_mac/releases) (built by GitHub Actions,
`.github/workflows/macos.yml`). Unpack and double-click `play.command`; it asks for the game
folder once. You still need Homebrew's bash and Python 3 (`brew install bash python`). The
package is not notarized: if macOS blocks it, run `xattr -dr com.apple.quarantine .` in the
unpacked folder.

## Build and run

```bash
git clone --recursive https://github.com/PabloVSouza/bloodborne_mac.git && cd bloodborne_mac
bash build.sh                                   # first run also builds the libraries (deps/)
BB_GAME_DIR=/path/to/CUSA03173 bash run.sh
```

`build.sh` builds the native arm64 program on Apple Silicon (`BB_ARCH=x86_64` builds the older
Rosetta 2 one). The first build compiles the dependencies (MoltenVK, SDL3, FFmpeg, ...) into
`deps/` and takes a while; the first launch, and the first launch after an update that changes
the shader cache format, compiles the game's shaders.

Saves and the shader cache go to `user/`, settings to `bbport.ini`. A gamepad is used through
SDL3; there is a keyboard fallback. The GTK4 launcher from upstream
(`bash launcher/bb-launcher.sh`) needs `brew install gtk4 libadwaita pygobject3`.

**Recommended settings** (in-game menu or `bbport.ini`): FSR 3.1 with preset 1–2 and *Character
motion vectors* off (`object_motion=0`, the macOS default). At 1080p output an upscaler preset
renders the whole game at the lower resolution from the start, so changing the preset needs a
restart. Character motion vectors improve FSR on moving characters but cost ~5 ms a frame on
Apple GPUs.

Useful variables: `BB_FRAME_STATS=1` (frame statistics in the log), `BB_GAME_DIR`,
`BB_RENDER_RES=WxH` with `BB_OUTPUT_RES=WxH` (render and output size), `BB_UPSCALER=fsr3|off`,
`BB_VK_DRIVER=kosmickrisp`. Developer tools for macOS (benchmark harness, Metal trace analysis):
`tools/mac_bench.sh`, `tools/mst_*.py`.

## Known issues

- An intermittent crash while a save loads (a corrupted GPU command buffer, roughly 1 load in 10);
  starting again works.
- Switching FSR on and off at run time with character motion vectors on can crash the GPU
  ("Invalid Resource"). It is not seen with them off.
- Two of the game's pipelines fail to compile in MoltenVK; their draws are skipped.
- FSR 4 / 4.1.1 and the experimental PC memory model of upstream are not available on macOS.

## Documentation

- [docs/ARM64_NATIVE.md](docs/ARM64_NATIVE.md): the arm64 translator.
- [docs/MACOS_PERFORMANCE.md](docs/MACOS_PERFORMANCE.md): macOS performance work and findings.
- [docs/upstream/README.md](docs/upstream/README.md) and the rest of [docs/](docs): upstream design
  notes (renderer, upscaler, motion vectors, mods and patches).

## Credits and licenses

Licensed under the **GNU GPL v2 or later** ([LICENSE](LICENSE)), like upstream.

- [deadinside28/bloodborne_pc](https://github.com/deadinside28/bloodborne_pc) (bbport): the port
  this fork is based on — loader, runtime, renderer extensions, upscalers, launcher.
- [shadPS4](https://github.com/shadps4-emu/shadPS4) video core and shader recompiler (GPL-2.0+),
  [sirit](https://github.com/shadps4-emu/sirit).
- [MoltenVK](https://github.com/KhronosGroup/MoltenVK) (Apache-2.0),
  [Zydis](https://github.com/zyantific/zydis) (MIT, the translator's decoder),
  [SDL3](https://www.libsdl.org/) (zlib), [FFmpeg](https://ffmpeg.org/) (LGPL),
  [Mesa](https://www.mesa3d.org/) KosmicKrisp (MIT, optional).
- [FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan) by FireBurn and the AMD FidelityFX SDK (MIT),
  [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9) (MIT),
  [Dear ImGui](https://github.com/ocornut/imgui) (MIT), [half](https://half.sourceforge.net/),
  DejaVu fonts.
- Game patches by Kyo, Lance McDonald, auser1337, illusion, emoose and other community members
  (`patches/Bloodborne.xml`).
