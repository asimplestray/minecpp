#pragma once

// Primitivas de sincronização portáteis (pthreads no POSIX, Win32 no
// Windows). Camada fina — sem política, só mecanismo. O pool de jobs e o
// futuro I/O async usam isto; nada de pthread.h/windows.h fora daqui.

#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

typedef struct minecpp_mutex {
  CRITICAL_SECTION h;
} minecpp_mutex_t;

typedef struct minecpp_cond {
  CONDITION_VARIABLE h;
} minecpp_cond_t;

typedef struct minecpp_thread {
  HANDLE h;
} minecpp_thread_t;

#else
#include <pthread.h>

typedef struct minecpp_mutex {
  pthread_mutex_t h;
} minecpp_mutex_t;

typedef struct minecpp_cond {
  pthread_cond_t h;
} minecpp_cond_t;

typedef struct minecpp_thread {
  pthread_t h;
  int alive;
} minecpp_thread_t;

#endif

typedef void (*minecpp_thread_fn)(void *ctx);

#ifdef __cplusplus
extern "C" {
#endif

int minecpp_mutex_init(minecpp_mutex_t *m);
void minecpp_mutex_destroy(minecpp_mutex_t *m);
void minecpp_mutex_lock(minecpp_mutex_t *m);
void minecpp_mutex_unlock(minecpp_mutex_t *m);
// Retorna 1 se conseguiu, 0 se ocupado (steal usa try — nunca bloqueia).
int minecpp_mutex_trylock(minecpp_mutex_t *m);

int minecpp_cond_init(minecpp_cond_t *c);
void minecpp_cond_destroy(minecpp_cond_t *c);
// Espera (libera m, dorme, readquire). Retorna 0 acordado, 1 timeout.
int minecpp_cond_wait(minecpp_cond_t *c, minecpp_mutex_t *m,
                      uint32_t timeout_ms);
void minecpp_cond_signal(minecpp_cond_t *c);
void minecpp_cond_broadcast(minecpp_cond_t *c);

// Thread joinable. Retorna 0 ok.
int minecpp_thread_create(minecpp_thread_t *t, minecpp_thread_fn fn,
                          void *ctx);
void minecpp_thread_join(minecpp_thread_t *t);
unsigned minecpp_thread_id(void);

#ifdef __cplusplus
}
#endif
