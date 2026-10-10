# Recompilation

> [!NOTE]
> This work happens on the [`decomp`](https://github.com/PabloVSouza/bloodborne_mac/tree/decomp)
> branch: the tools named below are there, not in `main` yet. Progress is tracked on the wiki:
> [Recompilation status](https://github.com/PabloVSouza/bloodborne_mac/wiki/Recompilation-status).

Goal: the game's own code running as native arm64 code, compiled ahead of time instead of
translated while the game runs, and in the end the engine's graphics drawing with Metal directly
instead of building PS4 command buffers for shadPS4 and MoltenVK to translate.

The way there is a **static recompiler**: a tool that runs on the player's Mac, reads *their own*
`eboot.bin` and writes the game's functions out as C, which clang compiles into a native library
the game loads. This project publishes the recompiler, never the game's code, as *Unleashed
Recompiled* and N64Recomp do.

## Why a recompiler, not a published decompilation

Decompiled code is a translation of the game's copyrighted code: publishing it would publish a
derivative of Sony's code, and Sony acts against Bloodborne projects. Reverse engineering for
interoperability is lawful in many places, but that covers studying the code, not distributing
the result. The Mario 64 decompilation has been tolerated, never cleared. (Not legal advice.)

So the work is split:

| Public (this repository) | Never published |
|---|---|
| The recompiler, its runtime, the translator | The game's code, decompiled or generated |
| Record and replay, profiling and analysis tools | Hand-written native versions of game functions (`private/`) |
| Annotations: function names, signatures, struct layouts (facts about the binary) | |

The generated code is produced on the player's machine from their copy and stays there.
Hand-written native functions (below) remain as private references: what the generated code is
compared with, for correctness and for speed.

## Principles

1. **Only FromSoftware's code first.** Havok, FMOD, Scaleform, YEBIS, Lua and zlib are licensed
   libraries; they can be recompiled like the rest later, the game's and engine's code comes first.
2. **Always playable.** Recompiled functions replace the translated ones one by one; anything not
   recompiled runs in the translator, which is also the fallback inside a generated function.
3. **The game's own data layouts.** Guest memory is host memory and x86-64 and arm64 share the
   LP64 layout rules: generated code works on the game's live objects.
4. **Checked by behaviour.** Calls recorded during play are replayed against the generated code;
   results, memory writes and calls must match exactly.
5. **Behind switches.** Generated functions can be turned off one by one, so a wrong one is found
   by bisection.

## Milestones

| | Goal | Status |
|---|---|---|
| M0 | Function table, library labels, CPU profile, Ghidra, record and replay | Done |
| M1 | Hand-written pilot: native functions in the game, checked by replay | Done (3 functions), stopped for the recompiler |
| R1 | Recompiler prototype: generated C for the recorded functions, checked by replay | In progress |
| R2 | Coverage: every function generated; the game runs recompiled, the translator as fallback | |
| R3 | Speed: integer, flags and vector instructions in C; memory ordering relaxed where safe | |
| R4 | Annotations: names, signatures and structs make the generated C readable | |
| R5 | The engine's graphics on Metal, at the libGnm boundary | |

## The recompiler

`bbrecomp` (planned in `tools/recomp/`) reads `eboot.bin`, takes every function from the unwind
tables, and writes one C function per game function:

- Control flow becomes `goto`s between labels; calls and returns become C calls.
- Guest registers live in local variables, flags are computed lazily (clang drops the ones no
  instruction reads).
- Memory accesses keep x86's ordering between threads (acquire/release), as the translator's do.
- Any instruction the generator does not handle yet runs in the interpreter, one instruction at a
  time: the output is complete and correct from the start and gets faster as the generator learns.
- Calls of functions that are not recompiled, and indirect jumps to unknown places, go back to the
  translator.

The game loads the compiled library (`BB_RECOMP_LIB`) the way it loads hand-written native
functions (`BB_NATIVE_LIB`), and the replay checks it against the recordings.

## Tools

All write to `out/decomp/`.

| Tool | What it does |
|---|---|
| `tools/decomp/scan.c` | Every function from the unwind tables (exact start and size), decoded with Zydis: `functions.tsv`, `calls.tsv`, `slots.tsv`, `strings.tsv` |
| `tools/decomp/label.py` | The library of each function (`modules.tsv`), from source paths and messages, spread to neighbours |
| `tools/decomp/profile.py` | CPU time per function and library from a macOS `sample` (`profile.tsv`) |
| `tools/decomp/decompile.sh` | Ghidra's C for chosen functions (`c/<offset>.c`), without analysing the whole image |
| `BB_RECORD` (`src/cpu/record.c`) | Calls of chosen functions recorded in the game (`records/<offset>.rec`) |
| `tools/decomp/replay.sh` | Recorded calls replayed and checked |

```sh
D=deps/macos-arm64
clang -arch arm64 -O2 -I$D/include tools/decomp/scan.c $D/lib/libZydis.a $D/lib/libZycore.a \
    -o out/decomp/scan
out/decomp/scan out/eboot.elf out/decomp
python3 tools/decomp/label.py out/decomp

# A profile: 5 s of every thread, then the main thread's functions.
SAMPLE=1 tools/mac_bench.sh prof BB_JIT_MAP=$PWD/out/jit.map
python3 tools/decomp/profile.py out/bench_prof_*.sample.txt out/jit.map out/bench_prof_*.log Thread_<id>

brew install ghidra
tools/decomp/decompile.sh 0x21b8710

# Record calls (OFFSET:SIZE from functions.tsv) and replay them.
tools/mac_bench.sh rec BB_RECORD=0x22928a0:247,0x227e230:579 BB_RECORD_EVERY=7 \
    BB_RECORD_DIR=$PWD/out/decomp/records
tools/decomp/replay.sh out/decomp/records/0x22928a0.rec
```

## Record and replay

With `BB_RECORD`, the translator leaves the chosen functions' first instruction to the
interpreter, which runs each recorded call in a tracer (`src/cpu/record.c`): one instruction at a
time, reading the memory each one accesses before and after it runs. The record holds the
registers at entry, the memory the function read (the first time in each epoch), the memory it
wrote, and the calls it made with their registers. Calls run at full speed; after each one the
memory the function reads is recorded again, so what the callee changed is in the record.

`tools/decomp/replay.sh` replays each record in a child process: the game image is mapped where it
was, memory is rebuilt from the record, and the function runs in the interpreter. Its calls are not
run: each must match the recorded target and argument registers, and is answered with the
recorded registers and memory. The registers at the return and every byte written must match.

Replaying the original function checks the record; a native version will be checked the same way.
A record is flagged when another thread changed memory the function read during the call; such
calls (locks, job queues) cannot be replayed alone. The game's code must only touch guest memory
for a record to be replayable: the variables the game imports (the stack canary) live in guest
memory for that reason.

First results (2026-10-10): 8 functions, 50 calls each, recorded during the Hunter's Dream
cutscene with no visible slowdown. The 7 ordinary functions replay exactly on all 350 calls. The
job loop (`0x21b8710`) matches on 31 of 50; 18 of the others had other threads' writes.


## Hand-written native functions (private)

The M1 pilot: native versions written by hand from Ghidra's output. They are decompiled code, so
they stay in `private/native/` (ignored here; its own repository), as references for the generated
code. The interface (`src/native/bbnative.h`) and its build script (`tools/decomp/native.sh`) are
public. Each function has the game function's SysV
signature; the program calls it through the guest -> host bridge, and it calls game functions
back through the API (`BB_CALL`).

```sh
tools/decomp/native.sh
tools/decomp/replay.sh out/decomp/records/0x22928a0.rec --native out/decomp/libbbnative.dylib
tools/mac_bench.sh native BB_NATIVE_LIB=$PWD/out/decomp/libbbnative.dylib  # BB_NATIVE_OFF=OFFSET,...
```

The replay runs the native version natively against the same records: calls answered from the
record (only the arguments it passes are compared), what it wrote found by comparing memory with
a copy. A callee's writes into a buffer in the original's stack frame go to the buffer the native
function passed. Floating point is compiled as on x86 (no fused multiply-add).

A record only tests the paths its calls took: deliberate mistakes on rare paths (a sum exactly 0,
a mutex with a count) passed 50 records; mistakes on common paths failed them all. More calls and
more places in the game make the check stronger.

First functions (2026-10-10): a bit field (`0x1089910`), a box-frustum test (`0x22928a0`) and an
adaptive mutex release (`0x2036a40`) match all their 150 records and run natively in the game,
about 250,000 calls a second. A direct call of a native function is translated into a call of
it (registers synced around it, as for an interpreted instruction), not an exit to the
dispatcher: no measurable cost left (median frame 16.6 ms with them, 16.8 ms without).

## What the image contains

162,959 functions with unwind information (median 80 bytes), about 10 million instructions. No
C++ RTTI. About 500 source paths in assertion and log messages name the libraries:

| Library | Functions | Code | Known by |
|---|---:|---:|---|
| Game (`SPRJ\Source\SPRJ\Source\Game`, `Sys`) | ~31,000 | 6.5 MiB | source paths |
| Havok 2014.1 (physics, AI, cloth) | ~24,700 | 9.2 MiB | source paths, names |
| Dantelion2 (FromSoftware's core library) | ~17,400 | 4.7 MiB | source paths |
| Scaleform GFx (menus, HUD) | ~12,300 | 3.2 MiB | messages |
| FD4 (FromSoftware's engine) | ~9,600 | 2.9 MiB | source paths, names |
| FMOD Ex 4.44.50 | ~6,400 | 1.3 MiB | source paths |
| Lua (LuaPlus) | ~2,200 | 0.9 MiB | source paths |
| YEBIS (post-processing) | ~2,200 | 0.8 MiB | source paths |
| Not labelled yet | ~56,300 | 10.4 MiB | |

The labels are a first pass: a library's code is found by the paths in its own messages and the
functions between them, so the boundaries between game, FD4 and Dantelion2 are approximate.

## First profile (2026-10-10, cutscene in the Hunter's Dream)

The game's main thread is about 95% busy; 85% of its samples are in translated code. 287 of its
functions ran; the busiest 69 take half of the time. The busiest one (`0x21b8710`, 13.8%) runs
jobs and updates shared counters with atomic operations: the main thread waiting on, or helping,
worker threads.

## Gameplay profile (2026-10-10, 20 s in the Hunter's Dream)

The main thread's time is spread thin: 933 functions ran, and the busiest 145 take half of it. The
busiest one is the job loop (7.3%); no other function takes more than 1.5%. Writing functions by
hand would take hundreds of them to matter, which is one reason for generating them all instead.

A batch of 16 of the hottest small and medium functions (46 to 1,125 bytes; game, FD4 and
Dantelion2 code), 60 calls each, recorded across loading, the cutscene and play: 15 replay exactly
on every call; one (`0x1d37400`) on 8 of 60, not explained yet.
