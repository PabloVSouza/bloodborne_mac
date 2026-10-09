# Native arm64 (no Rosetta)

Goal: the macOS build runs natively on Apple Silicon. The game's code is x86-64 machine code with
no source, so it is translated in-process by `bbcpu` (src/cpu/); everything else (runtime, GPU
library, MoltenVK, SDL, FFmpeg) is built for arm64.

## Why our own translator

Box64 and FEX are Linux programs: their ELF loaders, syscall layers and signal handling are built
around Linux. This port needs much less: user-mode x86-64 (no syscalls: the game reaches the OS
only through imports the runtime already implements), no self-modifying code, one fixed image.

## Shape

- **Guest memory is identity-mapped.** Guest pointers are host pointers, as today; host code reads
  and writes guest memory directly.
- **Guest → host:** a call or jump to an address outside the guest image is a host function. The
  bridge passes rdi, rsi, rdx, rcx, r8, r9 and the stack slots after the return address as integer
  arguments (arm64: x0-x5, then x6, x7 from the stack), xmm0-7 as vector arguments (q0-q7), and
  returns rax/rdx/xmm0/xmm1. SysV and AAPCS classify integer and floating-point arguments
  independently, so this is exact for up to 8 of each; variadic functions and struct returns are
  listed exceptions.
- **Host → guest:** `bb_guest_call(fn, args...)` runs a guest function on the calling thread until
  it returns (entry, module init, atexit, once, TLS key destructors, thread entry, AvPlayer
  callbacks).
- **Threads:** each guest thread is a host thread with its own `BbCpu` (registers, flags, YMM,
  MXCSR, fs/gs bases, x87).
- **TLS:** the translator models fs/gs bases; the guest TCB is the fs base (the Rosetta build's
  fs→gs rewrite is not applied).

## Stages

1. Interpreter (portable C, Zydis decoder), run in the x86 build with `BB_CPU=interp`: validates
   instruction semantics and the bridge on the known-good host. Unimplemented instructions stop
   with the guest address and bytes.
2. Host ported to arm64 (runtime, GPU library: Xbyak SRT walker, rdtsc, SSE intrinsics; arm64
   dependencies), guest code interpreted.
3. arm64 JIT (block cache, guest registers in host registers, lazy flags, block chaining, x86-TSO
   memory ordering mode), checked against the interpreter.
4. Performance: translation cache across runs, hot paths.

## Status (2026-10-09)

- Stages 1–3 done: the native arm64 build is the default on Apple Silicon (`build.sh`, `run.sh`;
  `BB_ARCH=x86_64` builds the Rosetta 2 one). The game boots, loads saves and plays.
- The JIT (`src/cpu/jit_arm64.c`) translates the integer, SSE/AVX and atomic
  instructions the game uses; anything else runs in the interpreter, in-block where possible.
  - Guest registers in host registers; xmm0–15 low halves in v16–v31 (upper ymm halves in memory).
  - Flags: per-flag liveness, looking into the next block; flags are dead across call/ret
    (`BB_JIT_FLAGS_ABI=0` turns that off).
  - x86-TSO: loads and stores with acquire/release (ldapr/stlr); misaligned accesses take an
    out-of-line slow path. Locked operations use LSE atomics between full barriers.
  - `rdtsc` reads the arm64 virtual counter, scaled to the PS4 clock.
- Performance: the game is GPU-bound at 1080p; the main guest thread is ~85% busy. Memory
  ordering showed no measurable cost (`BB_JIT_UNORDERED`, measurement only).

## Verification

- `tools/cpu/fuzz_jit.sh FILE`: each instruction run translated and interpreted with random
  inputs, results compared. `FILE` comes from `tools/cpu/encodings.c` (all encodings in
  `out/eboot.elf`) or from a game run with `BB_JIT_DUMP=file`.
- `tests/test_a64.c`: the emitter's encodings against the system assembler.
- `tools/cpu/test_jit.sh`: translator unit tests.

## Debugging switches

`BB_JIT_DENY=tokens` (interpret chosen instruction kinds), `BB_JIT_MAP=file` (guest block → host
code map, for `tools/sample_guest.py` with macOS `sample`), `BB_JIT_DUMP=file` (instructions seen).

## Pitfalls found

- 16 KiB host pages: the GPU write tracker uses 16 KiB pages (`TRACKER_PAGE_BITS=14`).
- The native address space reserves 0x180000000–0x7000000000 (shared region): the guest lives
  above 0x8000000000.
- A heap panic during save load ("DLRegularHeap.cpp(710) improper or freed") appeared while
  vector code was much slower than integer code and vanished once both were translated: a race
  in the game exposed by uneven thread speeds, not a translation error (the fuzzer found none).
