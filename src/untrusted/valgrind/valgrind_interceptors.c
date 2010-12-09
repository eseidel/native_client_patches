/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* This file contains valgrind interceptors for NaCl's untrusted library
   functions such as malloc, free, etc.
   When running under valgrind, the function named foo() will be replaced
   with a function from this file named I_WRAP_SONAME_FNNAME_ZZ(NaCl, foo).
   The latter function may in turn call the original foo().
   This requires that valgrind assigns SONAME "NaCl" to the functions
   in untrusted code (i.e. an appropriate patch in valgrind).

   For details about valgrind interceptors (function wrapping) refer to
     http://valgrind.org/docs/manual/manual-core-adv.html

   In order to use this library, link with the following gcc flags:
     -lvalgrind -Wl,-u,have_nacl_valgrind_interceptors

   TODO(kcc): extend the interceptors to fully cover memcheck needs:
       calloc/realloc/operator new/operator delete/etc
   TODO(kcc): extend the interceptors to fully cover ThreadSanitizer needs:
       pthread_mutex_.../pthread_rwlock_.../sem_.../etc
 */

#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/third_party/valgrind/nacl_valgrind.h"
#include "native_client/src/third_party/valgrind/nacl_memcheck.h"

/* For DYNAMIC_ANNOTATIONS_NAME() */
#include "native_client/src/untrusted/valgrind/dynamic_annotations.h"

#define INLINE __attribute__((always_inline))

#include "native_client/src/third_party/valgrind/ts_valgrind_client_requests.h"

/* For sizeof(nc_thread_memory_block_t) */
#include "native_client/src/untrusted/pthread/pthread_types.h"

/* This variable needs to be referenced by a program (either in sources,
  or using the linker flag -u) to which this library is linked.
  When using gcc/g++ as a linker, use -Wl,-u,have_nacl_valgrind_interceptors.
*/
int have_nacl_valgrind_interceptors;

/* TSan interceptors. */

#define VG_NACL_FUNC(f) I_WRAP_SONAME_FNNAME_ZZ(NaCl, f)
#define VG_NACL_ANN(f) \
  I_WRAP_SONAME_FNNAME_ZZ(NaCl, DYNAMIC_ANNOTATIONS_NAME(f))


#define VG_CREQ_v_v(_req)                                               \
  do {                                                                  \
    uint64_t _res;                                                      \
    VALGRIND_DO_CLIENT_REQUEST(_res, 0, _req, 0, 0, 0, 0, 0);           \
  } while (0)

#define VG_CREQ_v_W(_req, _arg1)                                        \
  do {                                                                  \
    uint64_t _res;                                                      \
    VALGRIND_DO_CLIENT_REQUEST(_res, 0, _req, _arg1, 0, 0, 0, 0);   \
  } while (0)

#define VG_CREQ_v_WW(_req, _arg1, _arg2)                                \
  do {                                                                  \
    uint64_t _res;                                                      \
    VALGRIND_DO_CLIENT_REQUEST(_res, 0, _req, _arg1, _arg2, 0, 0, 0);   \
  } while (0)

#define VG_CREQ_v_WWW(_req, _arg1, _arg2, _arg3)                        \
  do {                                                                  \
    uint64_t _res;                                                      \
    VALGRIND_DO_CLIENT_REQUEST(_res, 0, _req, _arg1, _arg2, _arg3, 0, 0); \
  } while (0)

static inline void start_ignore_all_accesses(void) {
  VG_CREQ_v_W(TSREQ_IGNORE_ALL_ACCESSES_BEGIN, 0);
}

static inline void stop_ignore_all_accesses(void) {
  VG_CREQ_v_W(TSREQ_IGNORE_ALL_ACCESSES_END, 0);
}

static inline void start_ignore_all_sync(void) {
  VG_CREQ_v_W(TSREQ_IGNORE_ALL_SYNC_BEGIN, 0);
}

static inline void stop_ignore_all_sync(void) {
  VG_CREQ_v_W(TSREQ_IGNORE_ALL_SYNC_END, 0);
}

static inline void start_ignore_all_accesses_and_sync(void) {
  start_ignore_all_accesses();
  start_ignore_all_sync();
}

static inline void stop_ignore_all_accesses_and_sync(void) {
  stop_ignore_all_accesses();
  stop_ignore_all_sync();
}


/*----------------------------------------------------------------*/
/*--- memory allocation                                        ---*/
/*----------------------------------------------------------------*/

typedef struct {
  volatile int locked;
} spinlock_t;

static void spinlock_lock(spinlock_t* lock) {
  while (!__sync_bool_compare_and_swap(&lock->locked, 0, 1)) {
    while (lock->locked) {}
  }
}

static void spinlock_unlock(spinlock_t* lock) {
  __sync_lock_release(&lock->locked);
}

/* Create red zones of this size around malloc-ed memory.
 Must be >= 3*sizeof(size_t) */
static const int kRedZoneSize = 32;
/* Used for sanity checking. */
static const size_t kMallocMagic = 0x1234abcd;

/* We need to delay the reuse of free-ed memory so that memcheck can report
 the uses of free-ed memory with detailed stacks.
 When a pointer is passed to free(), we put it into this FIFO queue.
 Instead of free-ing this pointer instantly, we free a pointer
 in the back of the queue.
 */
enum {
  kDelayReuseQueueSize = 1024
};
static size_t delay_reuse_queue[kDelayReuseQueueSize];
static size_t drq_begin;
static spinlock_t drq_lock; /* Protects delay_reuse_queue. */

/* size: user-requested allocation size
   returns: real allocation size
*/
INLINE static size_t handle_malloc_before(size_t size) {
  start_ignore_all_accesses_and_sync();
  return size + 2 * kRedZoneSize;
}

/* ptr: address of allocated memory block (with the size received from
       handle_malloc_before())
   size: user-requested allocation size
   returns: address that should be returned to the caller
*/
INLINE static size_t handle_malloc_after(size_t ptr, size_t size) {
  uint64_t base;
  /* Mark all memory as defined, put our own data at the beginning. */
  base = VALGRIND_SANDBOX_PTR(ptr);
  VALGRIND_MAKE_MEM_DEFINED(base, kRedZoneSize);
  ((size_t*)ptr)[0] = kMallocMagic;
  ((size_t*)ptr)[1] = size;
  ((size_t*)ptr)[2] = kMallocMagic;
  /* Tell memcheck about malloc-ed memory and red-zones. */
  VALGRIND_MALLOCLIKE_BLOCK(base + kRedZoneSize, size, kRedZoneSize, 0);
  VALGRIND_MAKE_MEM_NOACCESS(base, kRedZoneSize);
  VALGRIND_MAKE_MEM_NOACCESS(base + kRedZoneSize + size, kRedZoneSize);
  stop_ignore_all_accesses_and_sync();
  /* Tell TSan about malloc-ed memory. */
  VG_CREQ_v_WW(TSREQ_MALLOC, base + kRedZoneSize, size);
  /* Done */
  return ptr + kRedZoneSize;
}

/* First part of the free handler.
   ptr: the pointer to be deallocated, coming from the client code
   returns the pointer that should be passed to the underlying free()
*/
INLINE static size_t handle_free_before(size_t ptr) {
  uint64_t base;
  size_t size, old_ptr;
  size_t orig_ptr = ptr;
  if (!ptr)
    return 0;
  start_ignore_all_accesses_and_sync();
  /* Get the size of allocated region, check sanity. */
  ptr -= kRedZoneSize;
  base = VALGRIND_SANDBOX_PTR(ptr);
  /* Tell TSan about deallocated memory. */
  VG_CREQ_v_W(TSREQ_FREE, base + kRedZoneSize);
  VALGRIND_MAKE_MEM_DEFINED(base, kRedZoneSize);
  if (((size_t*)ptr)[0] != kMallocMagic) {
    char buf[100];
    snprintf(buf, 100,
        "Bad free(%p). Did we miss a memory allocation? Please file a bug.\n",
        (void*)orig_ptr);
    VALGRIND_PRINTF_BACKTRACE(buf);
    assert(0 && "bad free");
  }
  size = ((size_t*)ptr)[1];
  assert(((size_t*)ptr)[2] == kMallocMagic);
  /* Tell memcheck that this memory is poisoned now.
     Don't poison first 8 bytes as they are used by malloc/free internally. */
  VALGRIND_MAKE_MEM_NOACCESS(base + 2 * sizeof(size_t),
      size - 2 * sizeof(size_t) + 2 * kRedZoneSize);
  VALGRIND_FREELIKE_BLOCK(base + kRedZoneSize, kRedZoneSize);

  /* Get a pointer free-ed some time ago from the reuse queue and put the
     current pointer in its place. */
  spinlock_lock(&drq_lock);
  old_ptr = delay_reuse_queue[drq_begin];
  delay_reuse_queue[drq_begin] = ptr;
  drq_begin = (drq_begin + 1) % kDelayReuseQueueSize;
  spinlock_unlock(&drq_lock);

  return old_ptr;
}

/* Second part of the free handler. */
INLINE static void handle_free_after() {
  stop_ignore_all_accesses_and_sync();
}

/* _malloc_r() */
size_t VG_NACL_FUNC(_malloc_r)(size_t reent, size_t size) {
  OrigFn fn;
  size_t ptr;
  VALGRIND_GET_ORIG_FN(fn);
  size_t allocSize = handle_malloc_before(size);
  CALL_FN_W_WW(ptr, fn, reent, allocSize);
  return handle_malloc_after(ptr, size);
}

/* _calloc_r() */
size_t VG_NACL_FUNC(_calloc_r)(size_t reent, size_t nmemb, size_t size) {
  size_t totalSize = nmemb * size;
  void* ptr = _malloc_r((void*)reent, totalSize);
  if (ptr)
    memset(ptr, 0, totalSize);
  return (size_t)ptr;
}

/* _realloc_r() */
size_t VG_NACL_FUNC(_realloc_r)(void* reent, size_t origPtr, size_t size) {
  if (!origPtr) {
    return (size_t)_malloc_r(reent, size);
  }
  if (!size) {
    free((void*)origPtr);
    return 0;
  }
  size_t newPtr = (size_t)_malloc_r(reent, size);
  if (!newPtr) {
    free((void*)origPtr);
    return 0;
  }

  VALGRIND_MAKE_MEM_DEFINED(VALGRIND_SANDBOX_PTR(origPtr - kRedZoneSize),
      kRedZoneSize);
  size_t origSize = ((size_t*)(origPtr - kRedZoneSize))[1];
  size_t copySize = size < origSize ? size : origSize;

  memcpy((void*)newPtr, (void*)origPtr, copySize);
  _free_r(reent, (void*)origPtr);
  return newPtr;
}

/* _free_r() */
void VG_NACL_FUNC(_free_r)(size_t reent, size_t ptr) {
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  size_t old_ptr = handle_free_before(ptr);
  if (old_ptr)
    CALL_FN_v_WW(fn, reent, old_ptr);
  handle_free_after();
}

/* Unoptimized string functions. Optimized versions often read several bytes
   beyond the end of the string. This makes Memcheck sad. */
/* strlen() */
size_t VG_NACL_FUNC(strlen)(char* ptr) {
  size_t i = 0;
  while (ptr[i])
    ++i;
  return i;
}

/* strchr() */
char* VG_NACL_FUNC(strchr)(char* s, int c) {
  size_t i;
  char *ret = 0;
  for (i = 0; ; i++) {
    if (s[i] == (char)c) {
      ret = s + i;
      break;
    }
    if (s[i] == 0) break;
  }
  return ret;
}

/* nc_allocate_memory_block_mu() - a cached malloc for thread stack & tls. */
size_t VG_NACL_FUNC(nc_allocate_memory_block_mu)(int type, size_t size) {
  OrigFn fn;
  size_t ret;
  VALGRIND_GET_ORIG_FN(fn);
  start_ignore_all_accesses_and_sync();
  CALL_FN_W_WW(ret, fn, type, size);
  stop_ignore_all_accesses_and_sync();
  if (ret) {
    VG_CREQ_v_WW(TSREQ_MALLOC, VALGRIND_SANDBOX_PTR(ret),
        size + sizeof(nc_thread_memory_block_t));
  }
  return ret;
}

/* Tell the tool that the stack has moved.
   This is the first untrusted function of any NaCl thread and the first
   place after the context switch that we can intercept. */
void VG_NACL_FUNC(nc_thread_starter)(size_t func, size_t state) {
  OrigFn fn;
  int local_stack_var = 0;

  /* Let the tool guess where the stack starts. */
  VG_CREQ_v_W(TSREQ_THR_STACK_TOP, (size_t)&local_stack_var);

  VALGRIND_GET_ORIG_FN(fn);
  CALL_FN_v_WW(fn, func, state);
}


/* ------------------------------------------------------------------*/
/*                        PThread mutex.                             */

/* pthread_mutex_init */
int VG_NACL_FUNC(pthreadZumutexZuinit)(pthread_mutex_t *mutex,
    pthread_mutexattr_t* attr) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  CALL_FN_W_WW(ret, fn, (size_t)mutex, (size_t)attr);

  if (ret == 0 /*success*/) {
    VG_CREQ_v_WW(TSREQ_PTHREAD_RWLOCK_CREATE_POST,
        VALGRIND_SANDBOX_PTR((size_t)mutex), 0);
  }

  return ret;
}

/* pthread_mutex_destroy */
int VG_NACL_FUNC(pthreadZumutexZudestroy)(pthread_mutex_t *mutex) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  VG_CREQ_v_W(TSREQ_PTHREAD_RWLOCK_DESTROY_PRE,
      VALGRIND_SANDBOX_PTR((size_t)mutex));

  CALL_FN_W_W(ret, fn, (size_t)mutex);

  return ret;
}

/* pthread_mutex_lock */
int VG_NACL_FUNC(pthreadZumutexZulock)(pthread_mutex_t *mutex) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  start_ignore_all_accesses();
  CALL_FN_W_W(ret, fn, (size_t)mutex);
  stop_ignore_all_accesses();

  if (ret == 0 /*success*/) {
    VG_CREQ_v_WW(TSREQ_PTHREAD_RWLOCK_LOCK_POST,
        VALGRIND_SANDBOX_PTR((size_t)mutex), 1);
  }

  return ret;
}

/* pthread_mutex_trylock. */
int VG_NACL_FUNC(pthreadZumutexZutrylock)(pthread_mutex_t *mutex) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  start_ignore_all_accesses();
  CALL_FN_W_W(ret, fn, (size_t)mutex);
  stop_ignore_all_accesses();

  if (ret == 0 /*success*/) {
    VG_CREQ_v_WW(TSREQ_PTHREAD_RWLOCK_LOCK_POST,
        VALGRIND_SANDBOX_PTR((size_t)mutex), 1);
  }

  return ret;
}

/* pthread_mutex_timedlock. */
int VG_NACL_FUNC(pthreadZumutexZutimedlock)(pthread_mutex_t *mutex,
    void* timeout) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  start_ignore_all_accesses();
  CALL_FN_W_WW(ret, fn, (size_t)mutex, (size_t)timeout);
  stop_ignore_all_accesses();

  if (ret == 0 /*success*/) {
    VG_CREQ_v_WW(TSREQ_PTHREAD_RWLOCK_LOCK_POST,
        VALGRIND_SANDBOX_PTR((size_t)mutex), 1);
  }

  return ret;
}

/* pthread_mutex_unlock */
int VG_NACL_FUNC(pthreadZumutexZuunlock)(pthread_mutex_t *mutex) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  VG_CREQ_v_W(TSREQ_PTHREAD_RWLOCK_UNLOCK_PRE,
      VALGRIND_SANDBOX_PTR((size_t)mutex));

  start_ignore_all_accesses();
  CALL_FN_W_W(ret, fn, (size_t)mutex);
  stop_ignore_all_accesses();

  return ret;
}

/* ------------------------------------------------------------------*/
/*                     Conditional variables.                        */

/* pthread_cond_wait */
int VG_NACL_FUNC(pthreadZucondZuwait)(size_t cond, size_t mutex) {
  int ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  VG_CREQ_v_W(TSREQ_PTHREAD_RWLOCK_UNLOCK_PRE, VALGRIND_SANDBOX_PTR(mutex));

  start_ignore_all_accesses();
  CALL_FN_W_WW(ret, fn, cond, mutex);
  stop_ignore_all_accesses();

  VG_CREQ_v_W(TSREQ_WAIT, VALGRIND_SANDBOX_PTR(cond));
  VG_CREQ_v_WW(TSREQ_PTHREAD_RWLOCK_LOCK_POST, VALGRIND_SANDBOX_PTR(mutex), 1);

  return ret;
}

/* pthread_cond_signal */
int VG_NACL_FUNC(pthreadZucondZusignal)(size_t cond) {
  int ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  VG_CREQ_v_W(TSREQ_SIGNAL, VALGRIND_SANDBOX_PTR(cond));

  start_ignore_all_accesses();
  CALL_FN_W_W(ret, fn, cond);
  stop_ignore_all_accesses();

  return ret;
}

/* ------------------------------------------------------------------*/
/*                       POSIX semaphores.                           */

static int handle_sem_wait(void* sem) {
  OrigFn fn;
  int    ret;
  VALGRIND_GET_ORIG_FN(fn);

  start_ignore_all_accesses();
  CALL_FN_W_W(ret, fn, (size_t)sem);
  stop_ignore_all_accesses();

  if (!ret)
    VG_CREQ_v_W(TSREQ_WAIT, VALGRIND_SANDBOX_PTR((size_t)sem));

  return ret;
}

/* sem_wait */
int VG_NACL_FUNC(semZuwait)(void* sem) {
  return handle_sem_wait(sem);
}

/* sem_trywait */
int VG_NACL_FUNC(semZutrywait)(void* sem) {
  return handle_sem_wait(sem);
}

/* sem_timedwait */
int VG_NACL_FUNC(semZutimedwait)(void* sem, void* abs_timeout) {
  OrigFn fn;
  int    ret;
  VALGRIND_GET_ORIG_FN(fn);

  start_ignore_all_accesses();
  CALL_FN_W_WW(ret, fn, (size_t)sem, (size_t)abs_timeout);
  stop_ignore_all_accesses();

  if (!ret)
    VG_CREQ_v_W(TSREQ_WAIT, VALGRIND_SANDBOX_PTR((size_t)sem));

  return ret;
}

/* sem_post */
int VG_NACL_FUNC(semZupost)(void* sem) {
  OrigFn fn;
  int    ret;
  VALGRIND_GET_ORIG_FN(fn);

  VG_CREQ_v_W(TSREQ_SIGNAL, VALGRIND_SANDBOX_PTR((size_t)sem));

  start_ignore_all_accesses();
  CALL_FN_W_W(ret, fn, (size_t)sem);
  stop_ignore_all_accesses();

  return ret;
}

/* ------------------------------------------------------------------*/
/*             Limited support for dynamic annotations.              */

void VG_NACL_ANN(AnnotateTraceMemory)(char *file, int line, void *mem) {
  VG_CREQ_v_W(TSREQ_TRACE_MEM, VALGRIND_SANDBOX_PTR((size_t)mem));
}

void VG_NACL_ANN(AnnotateMutexIsNotPHB)(char *file, int line, void *mu) {
  VG_CREQ_v_W(TSREQ_MUTEX_IS_NOT_PHB, VALGRIND_SANDBOX_PTR((size_t)mu));
}

void VG_NACL_ANN(AnnotateExpectRace)(char *file, int line, void *addr,
    void* desc) {
  VG_CREQ_v_WWW(TSREQ_EXPECT_RACE, VALGRIND_SANDBOX_PTR((size_t)addr), 1,
      VALGRIND_SANDBOX_PTR((size_t)desc));
}

void VG_NACL_ANN(AnnotateCondVarSignal)(char *file, int line, void *cv) {
  VG_CREQ_v_W(TSREQ_SIGNAL, VALGRIND_SANDBOX_PTR((size_t)cv));
}

void VG_NACL_ANN(AnnotateCondVarWait)(char *file, int line, void *cv,
    void *unused_mu) {
  VG_CREQ_v_W(TSREQ_WAIT, VALGRIND_SANDBOX_PTR((size_t)cv));
}

void VG_NACL_ANN(AnnotateIgnoreWritesBegin)(char *file, int line) {
  VG_CREQ_v_v(TSREQ_IGNORE_WRITES_BEGIN);
}

void VG_NACL_ANN(AnnotateIgnoreWritesEnd)(char *file, int line) {
  VG_CREQ_v_v(TSREQ_IGNORE_WRITES_END);
}
