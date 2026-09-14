#include "minecpp/core/thread_pool.h"

#include <stdatomic.h>
#include <stdlib.h>

#include "minecpp/core/platform.h"
#include "minecpp/core/sync.h"

// Pool work-stealing: N workers, cada um com 3 rings FIFO (P0/P1/P2).
// - Submit: round-robin atômico + signal direto no worker alvo.
// - Worker ocioso: próprio P0→P1→P2, depois rouba P2→P1→P0 dos outros
//   (trylock — nunca bloqueia em lock alheio). Espera com timeout 2ms.
// - Shutdown drena os rings antes de sair (semântica executor shutdown).
// - Zero malloc após init: rings de tamanho fixo.

#define MINECPP_LANES 3
#define MINECPP_RING 1024
#define MINECPP_IDLE_WAIT_MS 2

typedef struct {
  minecpp_job_fn fn;
  void *ctx;
} job_t;

typedef struct {
  minecpp_mutex_t mu;
  minecpp_cond_t cv;
  job_t ring[MINECPP_LANES][MINECPP_RING];
  unsigned head[MINECPP_LANES];
  unsigned count[MINECPP_LANES];
  minecpp_thread_t th;
  unsigned idx;
  _Atomic uint64_t done;
} worker_t;

static worker_t *g_w = NULL;
static unsigned g_n = 0;
static _Atomic unsigned g_rr = 0;
static _Atomic int g_down = 0;
static _Atomic uint64_t g_submitted = 0;
static _Atomic uint64_t g_completed = 0;
static _Atomic uint64_t g_dropped = 0;

// Pré-condição: mu do worker lockado.
static int pop_lane(worker_t *w, unsigned lane, job_t *out) {
  if (!w->count[lane]) return 0;
  *out = w->ring[lane][w->head[lane]];
  w->head[lane] = (w->head[lane] + 1) % MINECPP_RING;
  w->count[lane]--;
  return 1;
}

static void worker_main(void *arg) {
  worker_t *self = (worker_t *)arg;
  for (;;) {
    job_t j;
    int have = 0;

    minecpp_mutex_lock(&self->mu);
    for (unsigned l = 0; l < MINECPP_LANES && !have; l++)
      have = pop_lane(self, l, &j);
    if (!have && !atomic_load(&g_down))
      minecpp_cond_wait(&self->cv, &self->mu, MINECPP_IDLE_WAIT_MS);
    if (!have && !atomic_load(&g_down)) {
      for (unsigned l = 0; l < MINECPP_LANES && !have; l++)
        have = pop_lane(self, l, &j);
    }
    const int stop = atomic_load(&g_down);
    minecpp_mutex_unlock(&self->mu);

    if (!have && !stop) {
      // Roubo: vizinhos em ordem, lanes frias primeiro, trylock.
      for (unsigned k = 1; k < g_n && !have; k++) {
        worker_t *v = &g_w[(self->idx + k) % g_n];
        if (!minecpp_mutex_trylock(&v->mu)) continue;
        for (int l = MINECPP_LANES - 1; l >= 0 && !have; l--)
          have = pop_lane(v, (unsigned)l, &j);
        minecpp_mutex_unlock(&v->mu);
      }
    }

    if (have) {
      j.fn(j.ctx);
      atomic_fetch_add(&self->done, 1);
      atomic_fetch_add(&g_completed, 1);
      continue;
    }
    if (stop) {
      // Drena o próprio ring antes de sair.
      for (;;) {
        minecpp_mutex_lock(&self->mu);
        int more = 0;
        for (unsigned l = 0; l < MINECPP_LANES && !more; l++)
          more = pop_lane(self, l, &j);
        minecpp_mutex_unlock(&self->mu);
        if (!more) break;
        j.fn(j.ctx);
        atomic_fetch_add(&self->done, 1);
        atomic_fetch_add(&g_completed, 1);
      }
      break;
    }
  }
}

int minecpp_thread_pool_init(unsigned worker_count) {
  if (g_w) return -1;  // exige shutdown antes de re-init
  if (worker_count == 0) {
    const unsigned cpu = minecpp_cpu_count();
    worker_count = cpu > 2 ? cpu - 2 : 2;  // 1 tick + 1 net-io reservados
  }
  worker_t *w = (worker_t *)calloc(worker_count, sizeof *w);
  if (!w) return -1;
  unsigned i = 0;
  for (; i < worker_count; i++) {
    w[i].idx = i;
    atomic_init(&w[i].done, 0);
    if (minecpp_mutex_init(&w[i].mu) != 0) break;
    if (minecpp_cond_init(&w[i].cv) != 0) {
      minecpp_mutex_destroy(&w[i].mu);
      break;
    }
    if (minecpp_thread_create(&w[i].th, worker_main, &w[i]) != 0) {
      minecpp_cond_destroy(&w[i].cv);
      minecpp_mutex_destroy(&w[i].mu);
      break;
    }
  }
  if (i < worker_count) {  // falha no meio: desmonta
    atomic_store(&g_down, 1);
    for (unsigned k = 0; k < i; k++) minecpp_cond_broadcast(&w[k].cv);
    for (unsigned k = 0; k < i; k++) {
      minecpp_thread_join(&w[k].th);
      minecpp_cond_destroy(&w[k].cv);
      minecpp_mutex_destroy(&w[k].mu);
    }
    atomic_store(&g_down, 0);
    free(w);
    return -1;
  }
  atomic_store(&g_rr, 0);
  atomic_store(&g_down, 0);
  atomic_store(&g_submitted, 0);
  atomic_store(&g_completed, 0);
  atomic_store(&g_dropped, 0);
  g_w = w;
  g_n = worker_count;
  return 0;
}

void minecpp_thread_pool_shutdown(void) {
  if (!g_w) return;
  atomic_store(&g_down, 1);
  for (unsigned i = 0; i < g_n; i++) minecpp_cond_broadcast(&g_w[i].cv);
  for (unsigned i = 0; i < g_n; i++) {
    minecpp_thread_join(&g_w[i].th);
    minecpp_cond_destroy(&g_w[i].cv);
    minecpp_mutex_destroy(&g_w[i].mu);
  }
  free(g_w);
  g_w = NULL;
  g_n = 0;
  atomic_store(&g_down, 0);
}

unsigned minecpp_thread_pool_workers(void) { return g_n; }

int minecpp_job_submit(unsigned prio, minecpp_job_fn fn, void *ctx) {
  worker_t *w;
  if (!g_w || !fn || prio >= MINECPP_LANES) return -1;
  if (atomic_load(&g_down)) return -1;
  w = &g_w[atomic_fetch_add(&g_rr, 1) % g_n];
  minecpp_mutex_lock(&w->mu);
  if (w->count[prio] >= MINECPP_RING) {
    minecpp_mutex_unlock(&w->mu);
    atomic_fetch_add(&g_dropped, 1);
    return -1;
  }
  w->ring[prio][(w->head[prio] + w->count[prio]) % MINECPP_RING].fn = fn;
  w->ring[prio][(w->head[prio] + w->count[prio]) % MINECPP_RING].ctx = ctx;
  w->count[prio]++;
  minecpp_mutex_unlock(&w->mu);
  minecpp_cond_signal(&w->cv);
  atomic_fetch_add(&g_submitted, 1);
  return 0;
}

void minecpp_thread_pool_stats(minecpp_pool_stats_t *out) {
  if (!out) return;
  out->submitted = atomic_load(&g_submitted);
  out->completed = atomic_load(&g_completed);
  out->dropped = atomic_load(&g_dropped);
  out->workers = g_n;
}

uint64_t minecpp_thread_pool_worker_completed(unsigned i) {
  if (!g_w || i >= g_n) return 0;
  return atomic_load(&g_w[i].done);
}
