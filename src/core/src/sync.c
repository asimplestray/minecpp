// Defesa em profundidade: o CMake já define _POSIX_C_SOURCE globalmente,
// mas compilação avulsa sem -D também precisa de clock_gettime & cia.
// (Tem que vir antes de QUALQUER include — nem o nosso sync.h pode vir
// antes, pois ele puxa pthread.h. Ver region.c.)
#if !defined(_WIN32) && !defined(_WIN64)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "minecpp/core/sync.h"

#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)

// ---------------- Windows: CRITICAL_SECTION + CONDITION_VARIABLE -----------

int minecpp_mutex_init(minecpp_mutex_t *m) {
  InitializeCriticalSection(&m->h);
  return 0;
}

void minecpp_mutex_destroy(minecpp_mutex_t *m) {
  DeleteCriticalSection(&m->h);
}

void minecpp_mutex_lock(minecpp_mutex_t *m) { EnterCriticalSection(&m->h); }

void minecpp_mutex_unlock(minecpp_mutex_t *m) { LeaveCriticalSection(&m->h); }

int minecpp_mutex_trylock(minecpp_mutex_t *m) {
  return TryEnterCriticalSection(&m->h) ? 1 : 0;
}

int minecpp_cond_init(minecpp_cond_t *c) {
  InitializeConditionVariable(&c->h);
  return 0;
}

void minecpp_cond_destroy(minecpp_cond_t *c) { (void)c; }

int minecpp_cond_wait(minecpp_cond_t *c, minecpp_mutex_t *m,
                      uint32_t timeout_ms) {
  const BOOL ok =
      SleepConditionVariableCS(&c->h, &m->h, timeout_ms ? timeout_ms : INFINITE);
  return ok ? 0 : 1;  // 0 acordado; 1 timeout (ou erro — tratado como wake)
}

void minecpp_cond_signal(minecpp_cond_t *c) { WakeConditionVariable(&c->h); }

void minecpp_cond_broadcast(minecpp_cond_t *c) {
  WakeAllConditionVariable(&c->h);
}

typedef struct {
  minecpp_thread_fn fn;
  void *ctx;
} win_start_t;

static DWORD WINAPI win_thread_main(LPVOID p) {
  win_start_t s = *(win_start_t *)p;
  free(p);
  s.fn(s.ctx);
  return 0;
}

int minecpp_thread_create(minecpp_thread_t *t, minecpp_thread_fn fn,
                          void *ctx) {
  win_start_t *s = (win_start_t *)malloc(sizeof *s);
  if (!s) return -1;
  s->fn = fn;
  s->ctx = ctx;
  t->h = CreateThread(NULL, 0, win_thread_main, s, 0, NULL);
  if (!t->h) {
    free(s);
    return -1;
  }
  return 0;
}

void minecpp_thread_join(minecpp_thread_t *t) {
  if (!t->h) return;
  WaitForSingleObject(t->h, INFINITE);
  CloseHandle(t->h);
  t->h = NULL;
}

unsigned minecpp_thread_id(void) { return (unsigned)GetCurrentThreadId(); }

#else

// ---------------- POSIX: pthreads ------------------------------------------

#include <errno.h>
#include <time.h>

int minecpp_mutex_init(minecpp_mutex_t *m) {
  return pthread_mutex_init(&m->h, NULL) == 0 ? 0 : -1;
}

void minecpp_mutex_destroy(minecpp_mutex_t *m) { pthread_mutex_destroy(&m->h); }

void minecpp_mutex_lock(minecpp_mutex_t *m) { pthread_mutex_lock(&m->h); }

void minecpp_mutex_unlock(minecpp_mutex_t *m) { pthread_mutex_unlock(&m->h); }

int minecpp_mutex_trylock(minecpp_mutex_t *m) {
  return pthread_mutex_trylock(&m->h) == 0 ? 1 : 0;
}

int minecpp_cond_init(minecpp_cond_t *c) {
  return pthread_cond_init(&c->h, NULL) == 0 ? 0 : -1;
}

void minecpp_cond_destroy(minecpp_cond_t *c) { pthread_cond_destroy(&c->h); }

int minecpp_cond_wait(minecpp_cond_t *c, minecpp_mutex_t *m,
                      uint32_t timeout_ms) {
  if (!timeout_ms) return pthread_cond_wait(&c->h, &m->h) == 0 ? 0 : 1;
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += timeout_ms / 1000;
  ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec++;
    ts.tv_nsec -= 1000000000L;
  }
  const int r = pthread_cond_timedwait(&c->h, &m->h, &ts);
  return r == 0 ? 0 : 1;  // timeout/erro viram wake (loop recheca)
}

void minecpp_cond_signal(minecpp_cond_t *c) { pthread_cond_signal(&c->h); }

void minecpp_cond_broadcast(minecpp_cond_t *c) {
  pthread_cond_broadcast(&c->h);
}

typedef struct {
  minecpp_thread_fn fn;
  void *ctx;
} posix_start_t;

static void *posix_thread_main(void *p) {
  posix_start_t s = *(posix_start_t *)p;
  free(p);
  s.fn(s.ctx);
  return NULL;
}

int minecpp_thread_create(minecpp_thread_t *t, minecpp_thread_fn fn,
                          void *ctx) {
  posix_start_t *s = (posix_start_t *)malloc(sizeof *s);
  if (!s) return -1;
  s->fn = fn;
  s->ctx = ctx;
  if (pthread_create(&t->h, NULL, posix_thread_main, s) != 0) {
    free(s);
    return -1;
  }
  t->alive = 1;
  return 0;
}

void minecpp_thread_join(minecpp_thread_t *t) {
  if (!t->alive) return;
  pthread_join(t->h, NULL);
  t->alive = 0;
}

unsigned minecpp_thread_id(void) {
  return (unsigned)(uintptr_t)pthread_self();
}

#endif
