/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
//
// Multi-threaded worker
//
// Original source:
//  https://chromium.googlesource.com/webm/libwebp

#ifndef AOM_THREAD_H_
#define AOM_THREAD_H_

#include "./aom_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Set maximum decode threads to be 8 due to the limit of frame buffers
// and not enough semaphores in the emulation layer on windows.
#define MAX_DECODE_THREADS 8

#if CONFIG_MULTITHREAD

#if defined(_WIN32) && !HAVE_PTHREAD_H
#include <errno.h>    // NOLINT
#include <process.h>  // NOLINT
#include <windows.h>  // NOLINT
typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_mutex_t;
typedef struct {
  HANDLE waiting_sem_;
  HANDLE received_sem_;
  HANDLE signal_event_;
} pthread_cond_t;

//------------------------------------------------------------------------------
// simplistic pthread emulation layer

// _beginthreadex requires __stdcall
#define THREADFN unsigned int __stdcall
#define THREAD_RETURN(val) (unsigned int)((DWORD_PTR)val)

static INLINE int pthread_create(pthread_t *const thread, const void *attr,
                                 unsigned int(__stdcall *start)(void *),
                                 void *arg) {
  (void)attr;
  *thread = (pthread_t)_beginthreadex(NULL,          /* void *security */
                                      0,             /* unsigned stack_size */
                                      start, arg, 0, /* unsigned initflag */
                                      NULL);         /* unsigned *thrdaddr */
  if (*thread == NULL) return 1;
  SetThreadPriority(*thread, THREAD_PRIORITY_ABOVE_NORMAL);
  return 0;
}

static INLINE int pthread_join(pthread_t thread, void **value_ptr) {
  (void)value_ptr;
  return (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0 ||
          CloseHandle(thread) == 0);
}

// Mutex
static INLINE int pthread_mutex_init(pthread_mutex_t *const mutex,
                                     void *mutexattr) {
  (void)mutexattr;
  InitializeCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_trylock(pthread_mutex_t *const mutex) {
  return TryEnterCriticalSection(mutex) ? 0 : EBUSY;
}

static INLINE int pthread_mutex_lock(pthread_mutex_t *const mutex) {
  EnterCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_unlock(pthread_mutex_t *const mutex) {
  LeaveCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_destroy(pthread_mutex_t *const mutex) {
  DeleteCriticalSection(mutex);
  return 0;
}

// Condition
static INLINE int pthread_cond_destroy(pthread_cond_t *const condition) {
  int ok = 1;
  ok &= (CloseHandle(condition->waiting_sem_) != 0);
  ok &= (CloseHandle(condition->received_sem_) != 0);
  ok &= (CloseHandle(condition->signal_event_) != 0);
  return !ok;
}

static INLINE int pthread_cond_init(pthread_cond_t *const condition,
                                    void *cond_attr) {
  (void)cond_attr;
  condition->waiting_sem_ = CreateSemaphore(NULL, 0, MAX_DECODE_THREADS, NULL);
  condition->received_sem_ = CreateSemaphore(NULL, 0, MAX_DECODE_THREADS, NULL);
  condition->signal_event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (condition->waiting_sem_ == NULL || condition->received_sem_ == NULL ||
      condition->signal_event_ == NULL) {
    pthread_cond_destroy(condition);
    return 1;
  }
  return 0;
}

static INLINE int pthread_cond_signal(pthread_cond_t *const condition) {
  int ok = 1;
  if (WaitForSingleObject(condition->waiting_sem_, 0) == WAIT_OBJECT_0) {
    // a thread is waiting in pthread_cond_wait: allow it to be notified
    ok = SetEvent(condition->signal_event_);
    // wait until the event is consumed so the signaler cannot consume
    // the event via its own pthread_cond_wait.
    ok &= (WaitForSingleObject(condition->received_sem_, INFINITE) !=
           WAIT_OBJECT_0);
  }
  return !ok;
}

static INLINE int pthread_cond_wait(pthread_cond_t *const condition,
                                    pthread_mutex_t *const mutex) {
  int ok;
  // note that there is a consumer available so the signal isn't dropped in
  // pthread_cond_signal
  if (!ReleaseSemaphore(condition->waiting_sem_, 1, NULL)) return 1;
  // now unlock the mutex so pthread_cond_signal may be issued
  pthread_mutex_unlock(mutex);
  ok = (WaitForSingleObject(condition->signal_event_, INFINITE) ==
        WAIT_OBJECT_0);
  ok &= ReleaseSemaphore(condition->received_sem_, 1, NULL);
  pthread_mutex_lock(mutex);
  return !ok;
}
#else                 // _WIN32
#include <pthread.h>  // NOLINT
#define THREADFN void *
#define THREAD_RETURN(val) val
#endif

#endif  // CONFIG_MULTITHREAD

// State of the worker thread object
typedef enum {
  NOT_OK = 0,  // object is unusable
  OK,          // ready to work
  WORK         // busy finishing the current task
} AVxWorkerStatus;

// Function to be called by the worker thread. Takes two opaque pointers as
// arguments (data1 and data2), and should return false in case of error.
typedef int (*AVxWorkerHook)(void *, void *);

// Platform-dependent implementation details for the worker.
typedef struct AVxWorkerImpl AVxWorkerImpl;

// Synchronization object used to launch job in the worker thread
typedef struct {
  AVxWorkerImpl *impl_;
  AVxWorkerStatus status_;
  AVxWorkerHook hook;  // hook to call
  void *data1;         // first argument passed to 'hook'
  void *data2;         // second argument passed to 'hook'
  int had_error;       // return value of the last call to 'hook'
} AVxWorker;

// The interface for all thread-worker related functions. All these functions
// must be implemented.
typedef struct {
  // Must be called first, before any other method.
  void (*init)(AVxWorker *const worker);
  // Must be called to initialize the object and spawn the thread. Re-entrant.
  // Will potentially launch the thread. Returns false in case of error.
  int (*reset)(AVxWorker *const worker);
  // Makes sure the previous work is finished. Returns true if worker->had_error
  // was not set and no error condition was triggered by the working thread.
  int (*sync)(AVxWorker *const worker);
  // Triggers the thread to call hook() with data1 and data2 arguments. These
  // hook/data1/data2 values can be changed at any time before calling this
  // function, but not be changed afterward until the next call to Sync().
  void (*launch)(AVxWorker *const worker);
  // This function is similar to launch() except that it calls the
  // hook directly instead of using a thread. Convenient to bypass the thread
  // mechanism while still using the AVxWorker structs. sync() must
  // still be called afterward (for error reporting).
  void (*execute)(AVxWorker *const worker);
  // Kill the thread and terminate the object. To use the object again, one
  // must call reset() again.
  void (*end)(AVxWorker *const worker);
} AVxWorkerInterface;

// Install a new set of threading functions, overriding the defaults. This
// should be done before any workers are started, i.e., before any encoding or
// decoding takes place. The contents of the interface struct are copied, it
// is safe to free the corresponding memory after this call. This function is
// not thread-safe. Return false in case of invalid pointer or methods.
int aom_set_worker_interface(const AVxWorkerInterface *const winterface);

// Retrieve the currently set thread worker interface.
const AVxWorkerInterface *aom_get_worker_interface(void);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_THREAD_H_
