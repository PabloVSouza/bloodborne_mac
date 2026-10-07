/* macOS: the runtime's allocations come from the heap below 1 TiB (platform.c), where Linux's
 * non-PIE brk heap puts them by itself. Only the runtime's C sources see these names; pointers
 * from the system allocator (strdup, realpath, ...) are still freed correctly. */
#ifndef BB_RUNTIME_HEAP_H
#define BB_RUNTIME_HEAP_H
#if defined(__APPLE__) && !defined(__cplusplus) && !defined(BB_PLATFORM_IMPL)
#include <stdlib.h>
#include "platform.h"
#define malloc(size) bb_low_malloc(size)
#define calloc(count, size) bb_low_calloc(count, size)
#define realloc(p, size) bb_low_realloc(p, size)
#define aligned_alloc(alignment, size) bb_low_aligned_alloc(alignment, size)
#define free(p) bb_low_free(p)
#endif
#endif
