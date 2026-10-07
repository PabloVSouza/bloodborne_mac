/* The guest thread pointer as the game reads it: link_modules.py rewrites `mov rax, fs:[0]` to
 * `mov rax, gs:[disp32]`, the loader writes bb_guest_tls_displacement() into disp32 (relocation
 * kind 3), and each thread's bb_guest_tls_set value is what the instruction loads. */
#define _GNU_SOURCE
#include "platform.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

typedef uint64_t (*ReadThreadPointer)(void);
static ReadThreadPointer read_thread_pointer;

static void *worker(void *tcb) {
    bb_guest_tls_set(tcb);
    assert(read_thread_pointer() == (uint64_t)(uintptr_t)tcb);
    return NULL;
}

int main(void) {
    /* mov rax, gs:[disp32]; ret */
    unsigned char code[] = {0x65, 0x48, 0x8b, 0x04, 0x25, 0, 0, 0, 0, 0xc3};
    const uint32_t displacement = bb_guest_tls_displacement();
    memcpy(code + 5, &displacement, 4);
    unsigned char *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(page != MAP_FAILED);
    memcpy(page, code, sizeof(code));
    assert(mprotect(page, 4096, PROT_READ | PROT_EXEC) == 0);
    read_thread_pointer = (ReadThreadPointer)(void *)page;

    /* tcb[0] points at the TCB itself (tcb_self), as runtime_thread.c builds it: Linux reads it
     * at GS base (displacement 0), macOS keeps the pointer in the TSD slot. */
    static uint64_t tcbs[4][4];
    for (int i = 0; i < 4; ++i) tcbs[i][0] = (uint64_t)(uintptr_t)tcbs[i];
    bb_guest_tls_set(tcbs[0]);
    assert(read_thread_pointer() == (uint64_t)(uintptr_t)tcbs[0]);
    pthread_t threads[3];
    for (int i = 0; i < 3; ++i) assert(pthread_create(&threads[i], NULL, worker, tcbs[i + 1]) == 0);
    for (int i = 0; i < 3; ++i) assert(pthread_join(threads[i], NULL) == 0);
    /* Other threads' values did not touch this one's. */
    assert(read_thread_pointer() == (uint64_t)(uintptr_t)tcbs[0]);
    printf("PASS: guest thread pointer (GS displacement %#x)\n", displacement);
    return 0;
}
