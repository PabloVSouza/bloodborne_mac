/* Host platform layer: Linux (glibc) and macOS (an x86-64 process run by Rosetta 2 on Apple
 * Silicon, or natively on Intel Macs). The game's code is x86-64 on both; what differs is the
 * host's address space, TLS, signals and the few Linux system calls the runtime uses.
 * Included from C (runtime) and C++ (GPU library). */
#ifndef BB_PLATFORM_H
#define BB_PLATFORM_H
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __APPLE__
#define BB_MACOS 1
/* Signal contexts. ucontext_t comes from <signal.h>; <ucontext.h> needs _XOPEN_SOURCE here. */
#define BB_UC_RIP(uc) ((uc)->uc_mcontext->__ss.__rip)
#define BB_UC_RSP(uc) ((uc)->uc_mcontext->__ss.__rsp)
#define BB_UC_RBP(uc) ((uc)->uc_mcontext->__ss.__rbp)
#define BB_UC_RAX(uc) ((uc)->uc_mcontext->__ss.__rax)
#define BB_UC_RBX(uc) ((uc)->uc_mcontext->__ss.__rbx)
#define BB_UC_RCX(uc) ((uc)->uc_mcontext->__ss.__rcx)
#define BB_UC_RDX(uc) ((uc)->uc_mcontext->__ss.__rdx)
#define BB_UC_RSI(uc) ((uc)->uc_mcontext->__ss.__rsi)
#define BB_UC_RDI(uc) ((uc)->uc_mcontext->__ss.__rdi)
#define BB_UC_R8(uc) ((uc)->uc_mcontext->__ss.__r8)
#define BB_UC_R9(uc) ((uc)->uc_mcontext->__ss.__r9)
#define BB_UC_R10(uc) ((uc)->uc_mcontext->__ss.__r10)
#define BB_UC_R11(uc) ((uc)->uc_mcontext->__ss.__r11)
#define BB_UC_R12(uc) ((uc)->uc_mcontext->__ss.__r12)
#define BB_UC_R13(uc) ((uc)->uc_mcontext->__ss.__r13)
#define BB_UC_R14(uc) ((uc)->uc_mcontext->__ss.__r14)
#define BB_UC_R15(uc) ((uc)->uc_mcontext->__ss.__r15)
#define BB_UC_EFLAGS(uc) ((uc)->uc_mcontext->__ss.__rflags)
/* Page fault error code: bit 1 set for a write. */
#define BB_UC_ERR(uc) ((uc)->uc_mcontext->__es.__err)
/* A write to a write-protected page raises SIGBUS on macOS, SIGSEGV on Linux. */
#define BB_IS_ACCESS_FAULT(sig) ((sig) == SIGSEGV || (sig) == SIGBUS)
#define BB_STAT_ATIM(st) ((st)->st_atimespec)
#define BB_STAT_MTIM(st) ((st)->st_mtimespec)
#define BB_STAT_CTIM(st) ((st)->st_ctimespec)
#ifndef CLOCK_REALTIME_COARSE
#define CLOCK_REALTIME_COARSE CLOCK_REALTIME
#endif
#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#endif
#else
#include <ucontext.h>
#define BB_UC_RIP(uc) ((uc)->uc_mcontext.gregs[REG_RIP])
#define BB_UC_RSP(uc) ((uc)->uc_mcontext.gregs[REG_RSP])
#define BB_UC_RBP(uc) ((uc)->uc_mcontext.gregs[REG_RBP])
#define BB_UC_RAX(uc) ((uc)->uc_mcontext.gregs[REG_RAX])
#define BB_UC_RBX(uc) ((uc)->uc_mcontext.gregs[REG_RBX])
#define BB_UC_RCX(uc) ((uc)->uc_mcontext.gregs[REG_RCX])
#define BB_UC_RDX(uc) ((uc)->uc_mcontext.gregs[REG_RDX])
#define BB_UC_RSI(uc) ((uc)->uc_mcontext.gregs[REG_RSI])
#define BB_UC_RDI(uc) ((uc)->uc_mcontext.gregs[REG_RDI])
#define BB_UC_R8(uc) ((uc)->uc_mcontext.gregs[REG_R8])
#define BB_UC_R9(uc) ((uc)->uc_mcontext.gregs[REG_R9])
#define BB_UC_R10(uc) ((uc)->uc_mcontext.gregs[REG_R10])
#define BB_UC_R11(uc) ((uc)->uc_mcontext.gregs[REG_R11])
#define BB_UC_R12(uc) ((uc)->uc_mcontext.gregs[REG_R12])
#define BB_UC_R13(uc) ((uc)->uc_mcontext.gregs[REG_R13])
#define BB_UC_R14(uc) ((uc)->uc_mcontext.gregs[REG_R14])
#define BB_UC_R15(uc) ((uc)->uc_mcontext.gregs[REG_R15])
#define BB_UC_EFLAGS(uc) ((uc)->uc_mcontext.gregs[REG_EFL])
#define BB_UC_ERR(uc) ((uc)->uc_mcontext.gregs[REG_ERR])
#define BB_IS_ACCESS_FAULT(sig) ((sig) == SIGSEGV)
#define BB_STAT_ATIM(st) ((st)->st_atim)
#define BB_STAT_MTIM(st) ((st)->st_mtim)
#define BB_STAT_CTIM(st) ((st)->st_ctim)
#endif

/* Guest address layout, all below 1 TiB (PS4 code and GPU descriptors pack 40-bit pointers).
 * [BB_LOW_MIN, BB_LOW_MAX): host memory the guest sees (image, stacks, trampolines, the runtime's
 * heap on macOS). [BB_USER_MIN, BB_USER_MAX): the game's own mappings. Under Rosetta 2 the range
 * [0xfc0000000, 0x7000000000) cannot be mapped, so the game's range starts at 448 GiB there. */
#define BB_LOW_MIN UINT64_C(0x0800000000)
#ifdef BB_MACOS
#define BB_LOW_MAX UINT64_C(0x0fc0000000)
#define BB_USER_MIN UINT64_C(0x7000000000)
#else
#define BB_LOW_MAX UINT64_C(0x1000000000)
#define BB_USER_MIN UINT64_C(0x1000000000)
#endif
#define BB_USER_MAX UINT64_C(0xfc00000000)

/* Host thread id (logs, statistics). */
uint64_t bb_gettid(void);
/* Copies up to n bytes from src without faulting on unmapped memory; returns the bytes read. */
size_t bb_read_memory(void *dst, uintptr_t src, size_t n);
/* One line describing the host mapping that contains `address` (crash reports); 0 when none. */
int bb_describe_mapping(uintptr_t address, char *out, size_t size);
/* Anonymous shared memory object of `size` bytes, as a file descriptor (-1 on failure). */
int bb_shared_memory(const char *name, uint64_t size);
/* Returns [offset, offset+size) of a bb_shared_memory object to zero; `view` maps all of it. */
void bb_shared_memory_zero(int fd, unsigned char *view, uint64_t offset, uint64_t size);
/* Host memory in [BB_LOW_MIN, BB_LOW_MAX), never reused; an unmapped page follows each block
 * to catch overruns. NULL when the range is full. */
void *bb_low_map(size_t size, int prot);
/* Releases guest mappings: munmap on Linux; on macOS the range goes back to the reservation
 * made at start, so host libraries are never given addresses in the guest ranges. */
int bb_unmap_guest(uintptr_t at, size_t size);
/* Timed locks. Deadlines are CLOCK_REALTIME, as POSIX's. */
int bb_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *deadline);
int bb_rwlock_timedrdlock(pthread_rwlock_t *lock, const struct timespec *deadline);
int bb_rwlock_timedwrlock(pthread_rwlock_t *lock, const struct timespec *deadline);
/* A condition variable whose bb_cond_timedwait_monotonic deadlines are CLOCK_MONOTONIC. */
int bb_cond_init_monotonic(pthread_cond_t *cond);
int bb_cond_timedwait_monotonic(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *deadline);
/* Sleeps until CLOCK_MONOTONIC reaches `deadline_ns`. */
void bb_sleep_until(uint64_t deadline_ns);
/* Names the calling thread (shown by debuggers and the samplers). */
void bb_set_thread_name(const char *name);
/* macOS: tells the system the process is latency-critical and user-initiated (no timer
 * coalescing or App Nap); nothing elsewhere. */
void bb_latency_critical(void);
/* Fills the buffer with random bytes; 0 on success. */
int bb_random(void *buffer, size_t size);
/* Highest address of the calling thread's stack. */
uintptr_t bb_thread_stack_top(void);
/* CPU time of the calling thread. */
void bb_thread_cpu_time(struct timeval *user, struct timeval *system);
/* Closes every descriptor from `first` up (before exec). */
void bb_close_from(int first);
/* Other threads of the process: the handle of the first one named `name` (0 when none). */
uint64_t bb_find_thread(const char *name);
/* Sends sig to a thread from bb_find_thread; 0 on success. */
int bb_signal_thread(uint64_t thread, int sig);
/* Sends sig to every other thread of the process, waiting `pause_us` after each. */
void bb_signal_other_threads(int sig, unsigned pause_us);
/* Guest thread pointer (the TCB the game reads with `mov rax, fs:[0]`, rewritten to GS by
 * link_modules.py): bb_guest_tls_set points the calling thread at it; the GS displacement the
 * rewritten loads use is bb_guest_tls_displacement (relocation kind 3). */
void bb_guest_tls_set(void *tcb);
uint32_t bb_guest_tls_displacement(void);

#ifdef BB_MACOS
/* The runtime's heap below 1 TiB (see runtime_heap.h). */
void *bb_low_malloc(size_t size);
void *bb_low_calloc(size_t count, size_t size);
void *bb_low_realloc(void *p, size_t size);
void *bb_low_aligned_alloc(size_t alignment, size_t size);
void bb_low_free(void *p);
#endif

#ifdef __cplusplus
}
#endif
#endif
