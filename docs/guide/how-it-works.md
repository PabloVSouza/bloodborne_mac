# How it works

[← Documentation](../README.md)

Bloodborne is a PS4 game: its code is x86-64 machine code written for the PS4's operating system
and GPU. Running it on a Mac takes three things: running x86-64 code on an arm64 CPU, providing
the PS4 system functions the game calls, and translating its graphics to the Mac's GPU.

## CPU: `bbcpu`, an x86-64 → arm64 translator

The game's code has no source, so it is translated block by block into arm64 code while the game
runs ([`src/cpu/`](../../src/cpu)), with an interpreter as a fallback.

- Guest registers live in host registers.
- CPU flags are only computed where something reads them.
- x86's strong memory ordering is kept with arm64 acquire/release accesses.
- A fuzzer checks each translation against the interpreter.

This is what lets the game run natively, without Rosetta 2. Details:
[Native arm64](../ARM64_NATIVE.md).

## System: the PS4 runtime

The runtime ([`src/runtime_*.c`](../../src)) implements the PS4 operating system functions the
game uses: memory, threads, files, audio, controllers and saves.

## Graphics: a shadPS4-derived renderer on Metal

The renderer, derived from [shadPS4](https://github.com/shadps4-emu/shadPS4), translates the PS4
GPU's commands and shaders to Vulkan. [MoltenVK](https://github.com/KhronosGroup/MoltenVK) runs
that Vulkan on Metal.

On top of the game's own rendering, the port adds:

- **Upscaling:** AMD FSR 3.1 renders the game below the output resolution and reconstructs a
  sharp image at the output resolution.
- **An in-game settings menu** (Dear ImGui), drawn over the game.

## The app

`Bloodborne.app` contains the game program, its libraries, a bundled Python and bash for the
start-up scripts, and the launcher ([`launcher/app`](../../launcher/app), Tauri with React). The
launcher writes the settings and starts `run.sh`. `run.sh` checks the game folder, prepares
patches and mods, and starts the game.

## Where it is going: native code

Translating the game's code works, but costs CPU time, and every frame still goes through the PS4
GPU's command format, shadPS4 and MoltenVK. The [decompilation](../DECOMPILATION.md) replaces the
game's own functions with native code, one at a time, while the game keeps running:

1. A function's calls are recorded while the game runs: the memory it reads and writes, and the
   calls it makes.
2. A native version is written from the decompiled code (Ghidra) and checked against those
   recordings.
3. It replaces the original in the game, behind a switch.

The goal is a native port: the engine's graphics drawing with Metal directly. Progress:
[Decompilation status](https://github.com/PabloVSouza/bloodborne_mac/wiki/Decompilation-status).
Decompiled code is not published in this repository.

## Where it comes from

Almost everything that makes the game run (the loader, the PS4 runtime, the renderer and its
extensions, the upscalers and the patches) comes from
[bbport](https://github.com/deadinside28/bloodborne_pc) and shadPS4. This fork adds the macOS
platform layer, the arm64 translator and the macOS app. Upstream's README is kept in
[upstream/README.md](../upstream/README.md).
