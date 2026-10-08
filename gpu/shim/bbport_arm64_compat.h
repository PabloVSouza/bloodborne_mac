// SPDX-License-Identifier: GPL-2.0-or-later
// bbport (native arm64 builds, force-included by gpu/CMakeLists.txt): the x86 intrinsics the GPU
// library uses for cheap cycle counts and spin waits. __rdtsc counts the arm64 virtual timer
// scaled to ~2.4 GHz, so thresholds written in x86 cycles keep their meaning.
#pragma once
#if defined(__aarch64__)
#include <stdint.h>

static inline uint64_t __rdtsc(void) {
    uint64_t ticks;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(ticks));
    return ticks * 100; /* 24 MHz on Apple Silicon */
}
static inline void _mm_pause(void) { __asm__ volatile("yield"); }
static inline void _mm_lfence(void) { __asm__ volatile("dsb ld" ::: "memory"); }
#define __builtin_ia32_pause() __asm__ volatile("yield")
/* Xbyak assembles the SRT walkers' x86-64 code, which bbcpu runs: its 64-bit mode. */
#define XBYAK64 1
#define XBYAK64_GCC 1
#endif
