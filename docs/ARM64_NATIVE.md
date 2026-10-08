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

## Status

- Stage 1: in progress.
