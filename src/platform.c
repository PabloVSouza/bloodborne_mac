/* Host platform layer (platform.h): Linux and macOS implementations. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define BB_PLATFORM_IMPL
#include "platform.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>

#define LOW_PAGE UINT64_C(16384)
static uint64_t low_align(uint64_t n) { return (n+LOW_PAGE-1) & ~(LOW_PAGE-1); }
static pthread_mutex_t low_lock=PTHREAD_MUTEX_INITIALIZER;
uint64_t bb_image_base;
static uintptr_t low_next=BB_LOW_MIN;

#ifndef __APPLE__
#include <dirent.h>
#include <sys/random.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <asm/prctl.h>

uint64_t bb_gettid(void) { return (uint64_t)syscall(SYS_gettid); }
size_t bb_read_memory(void *dst, uintptr_t src, size_t n) {
    struct iovec local={dst,n}, remote={(void *)src,n};
    const ssize_t got=process_vm_readv(getpid(),&local,1,&remote,1,0);
    return got>0 ? (size_t)got : 0;
}
int bb_describe_mapping(uintptr_t address, char *out, size_t size) {
    FILE *maps=fopen("/proc/self/maps","r");
    int found=0;
    while (maps && !found && fgets(out,(int)size,maps)) {
        unsigned long from, to;
        if (sscanf(out,"%lx-%lx",&from,&to)==2 && address>=from && address<to) found=1;
    }
    if (maps) fclose(maps);
    return found;
}
int bb_shared_memory(const char *name, uint64_t size) {
    int fd=memfd_create(name,MFD_CLOEXEC);
    if (fd>=0 && ftruncate(fd,(off_t)size)) { close(fd); return -1; }
    return fd;
}
void bb_shared_memory_zero(int fd, unsigned char *view, uint64_t offset, uint64_t size) {
    (void)view;
    fallocate(fd,FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE,(off_t)offset,(off_t)size);
}
void *bb_low_map(size_t size, int prot) {
    size=low_align(size);
    pthread_mutex_lock(&low_lock);
    void *p=MAP_FAILED;
    while (low_next+size<=BB_LOW_MAX) {
        p=mmap((void *)low_next,size,prot,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        low_next+=size+LOW_PAGE; /* unmapped gap catches overruns */
        if (p!=MAP_FAILED) break;
    }
    pthread_mutex_unlock(&low_lock);
    return p==MAP_FAILED ? NULL : p;
}
int bb_unmap_guest(uintptr_t at, size_t size) { return munmap((void *)at,size); }
int bb_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *deadline) { return pthread_mutex_timedlock(mutex,deadline); }
int bb_rwlock_timedrdlock(pthread_rwlock_t *lock, const struct timespec *deadline) { return pthread_rwlock_timedrdlock(lock,deadline); }
int bb_rwlock_timedwrlock(pthread_rwlock_t *lock, const struct timespec *deadline) { return pthread_rwlock_timedwrlock(lock,deadline); }
int bb_cond_init_monotonic(pthread_cond_t *cond) {
    pthread_condattr_t attr;
    int e=pthread_condattr_init(&attr);
    if (!e) e=pthread_condattr_setclock(&attr,CLOCK_MONOTONIC);
    if (!e) e=pthread_cond_init(cond,&attr);
    pthread_condattr_destroy(&attr);
    return e;
}
int bb_cond_timedwait_monotonic(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *deadline) {
    return pthread_cond_timedwait(cond,mutex,deadline);
}
void bb_sleep_until(uint64_t deadline_ns) {
    struct timespec t={(time_t)(deadline_ns/1000000000u),(long)(deadline_ns%1000000000u)};
    while (clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&t,NULL)) {}
}
void bb_set_thread_name(const char *name) { pthread_setname_np(pthread_self(),name); }
int bb_random(void *buffer, size_t size) { return getrandom(buffer,size,0)<0 ? -1 : 0; }
void bb_latency_critical(void) {}
uintptr_t bb_thread_stack_top(void) {
    pthread_attr_t attr; void *base=NULL; size_t size=0;
    if (!pthread_getattr_np(pthread_self(),&attr)) { pthread_attr_getstack(&attr,&base,&size); pthread_attr_destroy(&attr); }
    return (uintptr_t)base+size;
}
void bb_thread_cpu_time(struct timeval *user, struct timeval *system) {
    struct rusage r;
    getrusage(RUSAGE_THREAD,&r);
    *user=r.ru_utime; *system=r.ru_stime;
}
void bb_close_from(int first) { syscall(SYS_close_range,(unsigned)first,~0u,0u); }
uint64_t bb_find_thread(const char *name) {
    pid_t tid=0;
    DIR *dir=opendir("/proc/self/task");
    struct dirent *e;
    while (!tid && dir && (e=readdir(dir))) {
        char path[300], comm[32]={0};
        snprintf(path,sizeof(path),"/proc/self/task/%s/comm",e->d_name);
        FILE *f=fopen(path,"r");
        if (!f) continue;
        if (fgets(comm,sizeof(comm),f) && !strncmp(comm,name,strlen(name))) tid=(pid_t)atoi(e->d_name);
        fclose(f);
    }
    if (dir) closedir(dir);
    return (uint64_t)tid;
}
int bb_signal_thread(uint64_t thread, int sig) { return (int)syscall(SYS_tgkill,getpid(),(pid_t)thread,sig); }
/* Async-signal-safe (the watchdog calls it from its signal handler). */
void bb_signal_other_threads(int sig, unsigned pause_us) {
    int dir=open("/proc/self/task",O_RDONLY|O_DIRECTORY);
    char buffer[4096];
    long n;
    pid_t self=(pid_t)syscall(SYS_gettid);
    while (dir>=0 && (n=syscall(SYS_getdents64,dir,buffer,sizeof(buffer)))>0)
        for (long at=0; at<n;) {
            struct { uint64_t ino; int64_t off; unsigned short reclen; unsigned char type; char name[]; } *d=(void *)(buffer+at);
            pid_t tid=(pid_t)strtol(d->name,NULL,10);
            if (tid>0 && tid!=self) { syscall(SYS_tgkill,getpid(),tid,sig); usleep(pause_us); }
            at+=d->reclen;
        }
    if (dir>=0) close(dir);
}
/* glibc owns FS; GS base is the guest TCB, read with gs:[0] (tcb_self). */
void bb_guest_tls_set(void *tcb) {
    if (syscall(SYS_arch_prctl,ARCH_SET_GS,(unsigned long)tcb)) { perror("STOP: arch_prctl(ARCH_SET_GS)"); exit(21); }
}
uint32_t bb_guest_tls_displacement(void) { return 0; }

#else /* macOS */
#include <os/lock.h>
#include <dlfcn.h>
#include <objc/objc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

uint64_t bb_gettid(void) { uint64_t id=0; pthread_threadid_np(NULL,&id); return id; }
size_t bb_read_memory(void *dst, uintptr_t src, size_t n) {
    /* Page by page: a read stops at the first unreadable page, as process_vm_readv does. */
    size_t done=0;
    while (done<n) {
        const size_t room=4096-((src+done)&4095), chunk=n-done<room ? n-done : room;
        mach_vm_size_t got=0;
        if (mach_vm_read_overwrite(mach_task_self(),(mach_vm_address_t)(src+done),chunk,
                                   (mach_vm_address_t)((unsigned char *)dst+done),&got)!=KERN_SUCCESS || !got) break;
        done+=(size_t)got;
    }
    return done;
}
int bb_describe_mapping(uintptr_t address, char *out, size_t size) {
    mach_vm_address_t start=address;
    mach_vm_size_t length=0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object=MACH_PORT_NULL;
    if (mach_vm_region(mach_task_self(),&start,&length,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&object)!=KERN_SUCCESS ||
        start>address) return 0;
    snprintf(out,size,"%llx-%llx %c%c%c%s\n",(unsigned long long)start,(unsigned long long)(start+length),
             info.protection&VM_PROT_READ ? 'r' : '-',info.protection&VM_PROT_WRITE ? 'w' : '-',
             info.protection&VM_PROT_EXECUTE ? 'x' : '-',info.shared ? " shared" : "");
    return 1;
}
int bb_shared_memory(const char *name, uint64_t size) {
    char path[64];
    for (unsigned attempt=0; attempt<16; ++attempt) {
        snprintf(path,sizeof(path),"/%.20s-%d-%u",name,(int)getpid(),attempt);
        int fd=shm_open(path,O_RDWR|O_CREAT|O_EXCL,0600);
        if (fd<0) { if (errno==EEXIST) continue; return -1; }
        shm_unlink(path);
        fcntl(fd,F_SETFD,FD_CLOEXEC);
        if (ftruncate(fd,(off_t)size)) { close(fd); return -1; }
        return fd;
    }
    return -1;
}
/* No hole punching in shared memory objects: the pages are cleared through the host view. The
 * memory compressor keeps zero pages almost free. */
void bb_shared_memory_zero(int fd, unsigned char *view, uint64_t offset, uint64_t size) {
    (void)fd;
    if (view) memset(view+offset,0,size);
}
/* The guest ranges are reserved (PROT_NONE) before any library starts: macOS hands out
 * addresses from the bottom of the address space, so Metal, MoltenVK or the system allocator
 * would otherwise place their memory where the game later maps its own with MAP_FIXED. */
static int low_reserved, user_reserved;
static int reserve(uintptr_t start, uintptr_t end) {
#ifdef __aarch64__
    /* Native arm64 ignores mmap hints in this part of the address space: a fixed allocation,
     * which fails rather than overlap anything. */
    mach_vm_address_t at=start;
    if (mach_vm_allocate(mach_task_self(),&at,end-start,VM_FLAGS_FIXED)==KERN_SUCCESS) {
        mprotect((void *)start,end-start,PROT_NONE);
        return 1;
    }
    fprintf(stderr,"bbport: cannot reserve guest addresses %#lx-%#lx; mapped there:\n",(unsigned long)start,(unsigned long)end);
    for (mach_vm_address_t a=start; a<end;) {
        mach_vm_size_t size=0; vm_region_basic_info_data_64_t info; mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64; mach_port_t object;
        if (mach_vm_region(mach_task_self(),&a,&size,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&object) || a>=end) break;
        fprintf(stderr,"  %#llx-%#llx\n",(unsigned long long)a,(unsigned long long)(a+size));
        a+=size;
    }
    return 0;
#endif
    void *p=mmap((void *)start,end-start,PROT_NONE,MAP_PRIVATE|MAP_ANON|MAP_NORESERVE,-1,0);
    if (p==(void *)start) return 1;
    if (p!=MAP_FAILED) munmap(p,end-start);
    fprintf(stderr,"bbport: cannot reserve guest addresses %#lx-%#lx (got %p)\n",(unsigned long)start,(unsigned long)end,p);
    return 0;
}
__attribute__((constructor(101))) static void reserve_guest_ranges(void) {
    low_reserved=reserve(BB_LOW_MIN,BB_LOW_MAX);
    user_reserved=reserve(BB_USER_MIN,BB_USER_MAX);
}
static int in_range(uintptr_t at, size_t size, uintptr_t start, uintptr_t end) {
    return at>=start && at<=end && size<=end-at;
}
void *bb_low_map(size_t size, int prot) {
    size=low_align(size);
    pthread_mutex_lock(&low_lock);
    void *p=NULL;
    if (low_reserved) {
        /* The whole range is this process's: the bump pointer alone keeps blocks apart. */
        if (low_next+size<=BB_LOW_MAX) {
            p=mmap((void *)low_next,size,prot,MAP_PRIVATE|MAP_ANON|MAP_FIXED,-1,0);
            if (p==MAP_FAILED) p=NULL;
            else low_next+=size+LOW_PAGE;
        }
    } else {
        while (!p && low_next+size<=BB_LOW_MAX) {
            void *q=mmap((void *)low_next,size,prot,MAP_PRIVATE|MAP_ANON,-1,0);
            if (q==(void *)low_next) p=q;
            else if (q!=MAP_FAILED) munmap(q,size);
            low_next+=size+LOW_PAGE;
        }
    }
    pthread_mutex_unlock(&low_lock);
    return p;
}
int bb_unmap_guest(uintptr_t at, size_t size) {
    if ((user_reserved && in_range(at,size,BB_USER_MIN,BB_USER_MAX)) || (low_reserved && in_range(at,size,BB_LOW_MIN,BB_LOW_MAX)))
        return mmap((void *)at,size,PROT_NONE,MAP_PRIVATE|MAP_ANON|MAP_NORESERVE|MAP_FIXED,-1,0)==MAP_FAILED ? -1 : 0;
    return munmap((void *)at,size);
}
/* macOS has no timed mutex or rwlock locks: try, then sleep with a growing pause (50 us .. 1 ms). */
static int realtime_passed(const struct timespec *deadline) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME,&now);
    return now.tv_sec>deadline->tv_sec || (now.tv_sec==deadline->tv_sec && now.tv_nsec>=deadline->tv_nsec);
}
static int timed_try(int (*attempt)(void *), void *lock, const struct timespec *deadline) {
    if (deadline->tv_nsec<0 || deadline->tv_nsec>=1000000000) return EINVAL;
    for (long pause=50000;;) {
        int e=attempt(lock);
        if (e!=EBUSY) return e;
        if (realtime_passed(deadline)) return ETIMEDOUT;
        struct timespec t={0,pause};
        nanosleep(&t,NULL);
        if (pause<1000000) pause*=2;
    }
}
static int try_mutex(void *m) { return pthread_mutex_trylock(m); }
static int try_read(void *l) { return pthread_rwlock_tryrdlock(l); }
static int try_write(void *l) { return pthread_rwlock_trywrlock(l); }
int bb_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *deadline) { return timed_try(try_mutex,mutex,deadline); }
int bb_rwlock_timedrdlock(pthread_rwlock_t *lock, const struct timespec *deadline) { return timed_try(try_read,lock,deadline); }
int bb_rwlock_timedwrlock(pthread_rwlock_t *lock, const struct timespec *deadline) { return timed_try(try_write,lock,deadline); }
/* macOS condition variables wait on CLOCK_REALTIME deadlines; a relative wait is used instead. */
int bb_cond_init_monotonic(pthread_cond_t *cond) { return pthread_cond_init(cond,NULL); }
int bb_cond_timedwait_monotonic(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *deadline) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    int64_t left=(int64_t)(deadline->tv_sec-now.tv_sec)*1000000000+(deadline->tv_nsec-now.tv_nsec);
    if (left<=0) return ETIMEDOUT;
    struct timespec relative={(time_t)(left/1000000000),(long)(left%1000000000)};
    return pthread_cond_timedwait_relative_np(cond,mutex,&relative);
}
void bb_sleep_until(uint64_t deadline_ns) {
    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC,&now);
        const uint64_t at=(uint64_t)now.tv_sec*1000000000u+(uint64_t)now.tv_nsec;
        if (at>=deadline_ns) return;
        const uint64_t left=deadline_ns-at;
        struct timespec t={(time_t)(left/1000000000u),(long)(left%1000000000u)};
        nanosleep(&t,NULL);
    }
}
void bb_set_thread_name(const char *name) { pthread_setname_np(name); }
int bb_random(void *buffer, size_t size) { arc4random_buf(buffer,size); return 0; }
/* [[NSProcessInfo processInfo] beginActivityWithOptions:reason:] through the Objective-C runtime:
 * NSActivityUserInitiated | NSActivityLatencyCritical turns off timer coalescing and App Nap,
 * which delay the game's short sleeps and the GPU threads' wakeups. The activity is kept for the
 * whole process. Resolved at run time: programs without Foundation (the tests) skip it. */
void bb_latency_critical(void) {
    typedef void *(*GetClass)(const char *);
    typedef SEL (*RegisterName)(const char *);
    typedef void *(*Send0)(void *, SEL);
    typedef void *(*SendString)(void *, SEL, const char *);
    typedef void *(*SendActivity)(void *, SEL, unsigned long long, void *);
    GetClass get_class=(GetClass)dlsym(RTLD_DEFAULT,"objc_getClass");
    RegisterName sel=(RegisterName)dlsym(RTLD_DEFAULT,"sel_registerName");
    void *send=dlsym(RTLD_DEFAULT,"objc_msgSend");
    if (!get_class || !sel || !send) return;
    void *process_class=get_class("NSProcessInfo"), *string_class=get_class("NSString");
    if (!process_class || !string_class) return;
    void *info=((Send0)send)(process_class,sel("processInfo"));
    void *reason=((SendString)send)(string_class,sel("stringWithUTF8String:"),"Bloodborne");
    const unsigned long long user_initiated=0x00FFFFFFULL, latency_critical=0xFF00000000ULL;
    void *activity=((SendActivity)send)(info,sel("beginActivityWithOptions:reason:"),
                                        user_initiated|latency_critical,reason);
    if (activity) ((Send0)send)(activity,sel("retain"));
}
uintptr_t bb_thread_stack_top(void) { return (uintptr_t)pthread_get_stackaddr_np(pthread_self()); }
void bb_thread_cpu_time(struct timeval *user, struct timeval *system) {
    thread_basic_info_data_t info;
    mach_msg_type_number_t count=THREAD_BASIC_INFO_COUNT;
    mach_port_t thread=mach_thread_self();
    memset(&info,0,sizeof(info));
    thread_info(thread,THREAD_BASIC_INFO,(thread_info_t)&info,&count);
    mach_port_deallocate(mach_task_self(),thread);
    *user=(struct timeval){info.user_time.seconds,info.user_time.microseconds};
    *system=(struct timeval){info.system_time.seconds,info.system_time.microseconds};
}
void bb_close_from(int first) {
    const int last=getdtablesize();
    for (int fd=first; fd<last; ++fd) close(fd);
}
/* Calls visit(thread) for each other thread with a pthread; stops when it returns nonzero. */
static uint64_t each_thread(int (*visit)(pthread_t, const void *), const void *context) {
    thread_act_array_t list; mach_msg_type_number_t count=0;
    if (task_threads(mach_task_self(),&list,&count)!=KERN_SUCCESS) return 0;
    const pthread_t self=pthread_self();
    uint64_t found=0;
    for (mach_msg_type_number_t i=0;i<count;++i) {
        pthread_t thread=pthread_from_mach_thread_np(list[i]);
        if (!found && thread && thread!=self && visit(thread,context)) found=(uint64_t)(uintptr_t)thread;
        mach_port_deallocate(mach_task_self(),list[i]);
    }
    vm_deallocate(mach_task_self(),(vm_address_t)list,count*sizeof(*list));
    return found;
}
static int named(pthread_t thread, const void *name) {
    char text[64]={0};
    pthread_getname_np(thread,text,sizeof(text));
    return !strncmp(text,name,strlen(name));
}
uint64_t bb_find_thread(const char *name) { return each_thread(named,name); }
int bb_signal_thread(uint64_t thread, int sig) { return pthread_kill((pthread_t)(uintptr_t)thread,sig); }
static int signal_one(pthread_t thread, const void *context) {
    const unsigned *args=context;
    pthread_kill(thread,(int)args[0]);
    usleep(args[1]);
    return 0;
}
void bb_signal_other_threads(int sig, unsigned pause_us) {
    const unsigned args[2]={(unsigned)sig,pause_us};
    each_thread(signal_one,args);
}
/* macOS keeps its own thread data at GS (the TSD array) and gives no way to move it. The guest
 * TCB pointer goes into a pthread key's slot of that array instead, and the rewritten loads read
 * gs:[key*8] - the same 9-byte instruction with a different displacement. */
static pthread_key_t guest_tls_key;
static pthread_once_t guest_tls_once=PTHREAD_ONCE_INIT;
static void guest_tls_create(void) {
    if (pthread_key_create(&guest_tls_key,NULL) || guest_tls_key>=512) {
        fputs("STOP: no TSD slot for the guest thread pointer\n",stderr); exit(21);
    }
}
void bb_guest_tls_set(void *tcb) {
    pthread_once(&guest_tls_once,guest_tls_create);
    if (pthread_setspecific(guest_tls_key,tcb)) { fputs("STOP: pthread_setspecific (guest TCB)\n",stderr); exit(21); }
}
uint32_t bb_guest_tls_displacement(void) {
    pthread_once(&guest_tls_once,guest_tls_create);
    return (uint32_t)guest_tls_key*8;
}

/* ---- The runtime's heap below 1 TiB ----
 * Linux keeps the whole host heap low (the non-PIE brk heap); macOS's allocator places it at
 * 0x7f..., beyond the 40 bits the game keeps of the pointers it is given (thread handles, TLS
 * blocks, mutexes). The runtime's C code allocates here instead (runtime_heap.h). Blocks are
 * power-of-two classes with free lists, from 64 MiB chunks of the low range; nothing goes back
 * to the system. Pointers outside the low range belong to the system allocator (strdup, ...). */
typedef struct { uint32_t cls, offset; uint64_t size; } HeapHeader; /* before each block */
_Static_assert(sizeof(HeapHeader)==16,"heap blocks stay 16-byte aligned");
#define HEAP_CLASSES 48
#define HEAP_ALIGNED UINT32_C(0xffffffff) /* header of an aligned_alloc block inside a larger one */
#define HEAP_CHUNK (UINT64_C(64) << 20)
static os_unfair_lock heap_lock=OS_UNFAIR_LOCK_INIT;
static void *heap_free[HEAP_CLASSES];
static unsigned char *chunk_at, *chunk_end;
static int heap_owns(const void *p) { return in_range((uintptr_t)p,1,BB_LOW_MIN,BB_LOW_MAX); }
static unsigned heap_class(size_t total) {
    unsigned c=4;
    while (((size_t)1<<c)<total) ++c;
    return c;
}
void *bb_low_malloc(size_t size) {
    if (size>(SIZE_MAX>>2)) { errno=ENOMEM; return NULL; }
    const unsigned c=heap_class(size+sizeof(HeapHeader));
    const size_t block=(size_t)1<<c;
    HeapHeader *h=NULL;
    os_unfair_lock_lock(&heap_lock);
    if (c<HEAP_CLASSES && heap_free[c]) {
        h=heap_free[c];
        memcpy(&heap_free[c],(unsigned char *)h+sizeof(HeapHeader),sizeof(void *));
    } else if (block>=HEAP_CHUNK) {
        h=bb_low_map(block,PROT_READ|PROT_WRITE);
    } else {
        if (!chunk_at || (size_t)(chunk_end-chunk_at)<block) {
            chunk_at=bb_low_map(HEAP_CHUNK,PROT_READ|PROT_WRITE);
            chunk_end=chunk_at ? chunk_at+HEAP_CHUNK : NULL;
        }
        if (chunk_at) { h=(HeapHeader *)chunk_at; chunk_at+=block; }
    }
    os_unfair_lock_unlock(&heap_lock);
    if (!h) { errno=ENOMEM; return NULL; }
    h->cls=c; h->offset=0; h->size=size;
    return h+1;
}
void bb_low_free(void *p) {
    if (!p) return;
    if (!heap_owns(p)) { free(p); return; }
    HeapHeader *h=(HeapHeader *)p-1;
    if (h->cls==HEAP_ALIGNED) { bb_low_free((unsigned char *)p-h->offset); return; }
    if (h->cls>=HEAP_CLASSES) { fputs("bbport: invalid free of a low heap block\n",stderr); abort(); }
    os_unfair_lock_lock(&heap_lock);
    memcpy(p,&heap_free[h->cls],sizeof(void *));
    heap_free[h->cls]=h;
    os_unfair_lock_unlock(&heap_lock);
}
static size_t usable(void *p) {
    HeapHeader *h=(HeapHeader *)p-1;
    if (h->cls==HEAP_ALIGNED) return h->size;
    return ((size_t)1<<h->cls)-sizeof(HeapHeader);
}
void *bb_low_calloc(size_t count, size_t size) {
    if (size && count>SIZE_MAX/size) { errno=ENOMEM; return NULL; }
    void *p=bb_low_malloc(count*size);
    if (p) memset(p,0,count*size); /* reused blocks are not clean */
    return p;
}
void *bb_low_realloc(void *p, size_t size) {
    if (!p) return bb_low_malloc(size);
    if (!heap_owns(p)) return realloc(p,size);
    const size_t room=usable(p);
    if (size<=room && ((HeapHeader *)p-1)->cls!=HEAP_ALIGNED) { ((HeapHeader *)p-1)->size=size; return p; }
    void *q=bb_low_malloc(size);
    if (!q) return NULL;
    memcpy(q,p,size<room ? size : room);
    bb_low_free(p);
    return q;
}
void *bb_low_aligned_alloc(size_t alignment, size_t size) {
    if (!alignment || (alignment&(alignment-1))) { errno=EINVAL; return NULL; }
    if (alignment<=16) return bb_low_malloc(size);
    if (size>SIZE_MAX-alignment) { errno=ENOMEM; return NULL; }
    unsigned char *raw=bb_low_malloc(size+alignment);
    if (!raw) return NULL;
    unsigned char *p=(unsigned char *)(((uintptr_t)raw+alignment-1)&~(uintptr_t)(alignment-1));
    if (p==raw) return p;
    HeapHeader *h=(HeapHeader *)p-1; /* p-raw >= 16: both are 16-byte aligned */
    h->cls=HEAP_ALIGNED; h->offset=(uint32_t)(p-raw); h->size=size;
    return p;
}
#endif
