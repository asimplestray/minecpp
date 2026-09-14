// Testes do job system: correção, ordem de prioridade, ring cheio, distribuição.
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "minecpp/core/thread_pool.h"

static int g_fail = 0;

#define ASSERT(cond)                                         \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      g_fail++;                                              \
    }                                                        \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static void sleep_ms(unsigned ms) {
  struct timespec ts = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000L};
  nanosleep(&ts, NULL);
}

static int wait_completed(uint64_t target, unsigned timeout_ms) {
  minecpp_pool_stats_t s;
  for (unsigned t = 0; t < timeout_ms; t += 5) {
    minecpp_thread_pool_stats(&s);
    if (s.completed >= target) return 0;
    sleep_ms(5);
  }
  return -1;
}

static _Atomic long g_sum = 0;

static void job_inc(void *ctx) {
  (void)ctx;
  atomic_fetch_add(&g_sum, 1);
}

// t1: 1000 jobs em 4 workers, soma exata.
static void t_basic(void) {
  ASSERT_EQ(minecpp_thread_pool_init(4), 0);
  ASSERT_EQ(minecpp_thread_pool_workers(), 4u);
  atomic_store(&g_sum, 0);
  for (int i = 0; i < 1000; i++) ASSERT_EQ(minecpp_job_submit((unsigned)(i % 3), job_inc, NULL), 0);
  ASSERT_EQ(wait_completed(1000, 5000), 0);
  ASSERT_EQ(atomic_load(&g_sum), 1000L);
  minecpp_pool_stats_t s;
  minecpp_thread_pool_stats(&s);
  ASSERT_EQ(s.submitted, 1000u);
  ASSERT_EQ(s.completed, 1000u);
  ASSERT_EQ(s.dropped, 0u);
  minecpp_thread_pool_shutdown();
}

// Ordem de prioridade com 1 worker bloqueado.
static _Atomic int g_release = 0;
static int g_order[8];
static _Atomic int g_norder = 0;

static void job_block(void *ctx) {
  (void)ctx;
  while (!atomic_load(&g_release)) {
    struct timespec ts = {0, 100000L};
    nanosleep(&ts, NULL);
  }
}

static void job_mark(void *ctx) {
  const int v = (int)(uintptr_t)ctx;
  g_order[atomic_fetch_add(&g_norder, 1)] = v;
}

static void t_prio_order(void) {
  ASSERT_EQ(minecpp_thread_pool_init(1), 0);
  atomic_store(&g_release, 0);
  atomic_store(&g_norder, 0);
  ASSERT_EQ(minecpp_job_submit(0, job_block, NULL), 0);
  sleep_ms(20);  // garante worker preso no blocker
  ASSERT_EQ(minecpp_job_submit(0, job_mark, (void *)(uintptr_t)10), 0);  // A
  ASSERT_EQ(minecpp_job_submit(1, job_mark, (void *)(uintptr_t)11), 0);  // B
  ASSERT_EQ(minecpp_job_submit(2, job_mark, (void *)(uintptr_t)12), 0);  // C
  atomic_store(&g_release, 1);
  ASSERT_EQ(wait_completed(4, 5000), 0);
  ASSERT_EQ(atomic_load(&g_norder), 3);
  ASSERT_EQ(g_order[0], 10);
  ASSERT_EQ(g_order[1], 11);
  ASSERT_EQ(g_order[2], 12);
  minecpp_thread_pool_shutdown();
}

// t3: ring cheio → -1 + dropped.
static void t_full(void) {
  ASSERT_EQ(minecpp_thread_pool_init(1), 0);
  atomic_store(&g_release, 0);
  atomic_store(&g_sum, 0);
  ASSERT_EQ(minecpp_job_submit(0, job_block, NULL), 0);
  sleep_ms(20);
  for (int i = 0; i < 1024; i++)
    ASSERT_EQ(minecpp_job_submit(0, job_inc, NULL), 0);
  ASSERT_EQ(minecpp_job_submit(0, job_inc, NULL), -1);
  minecpp_pool_stats_t s;
  minecpp_thread_pool_stats(&s);
  ASSERT_EQ(s.dropped, 1u);
  atomic_store(&g_release, 1);
  ASSERT_EQ(wait_completed(1025, 10000), 0);
  ASSERT_EQ(atomic_load(&g_sum), 1024L);
  minecpp_thread_pool_shutdown();
}

// t4: 20k jobs distribuídos nos 4 workers.
static void t_distribute(void) {
  ASSERT_EQ(minecpp_thread_pool_init(4), 0);
  atomic_store(&g_sum, 0);
  for (int i = 0; i < 20000; i++)
    ASSERT_EQ(minecpp_job_submit(2, job_inc, NULL), 0);
  ASSERT_EQ(wait_completed(20000, 15000), 0);
  ASSERT_EQ(atomic_load(&g_sum), 20000L);
  uint64_t total = 0;
  for (unsigned i = 0; i < 4; i++) {
    const uint64_t d = minecpp_thread_pool_worker_completed(i);
    ASSERT(d > 0);
    total += d;
  }
  ASSERT_EQ(total, 20000u);
  minecpp_thread_pool_shutdown();
}

// t5: re-init e double-init.
static void t_reinit(void) {
  ASSERT_EQ(minecpp_thread_pool_init(2), 0);
  ASSERT_EQ(minecpp_thread_pool_init(2), -1);  // sem shutdown → erro
  minecpp_thread_pool_shutdown();
  minecpp_thread_pool_shutdown();  // duplo shutdown é seguro
  ASSERT_EQ(minecpp_thread_pool_init(0), 0);  // auto
  ASSERT(minecpp_thread_pool_workers() >= 2);
  ASSERT_EQ(minecpp_job_submit(3, job_inc, NULL), -1);  // prio inválida
  ASSERT_EQ(minecpp_job_submit(0, NULL, NULL), -1);  // fn nula
  minecpp_thread_pool_shutdown();
}

int main(void) {
  t_basic();
  t_prio_order();
  t_full();
  t_distribute();
  t_reinit();
  if (g_fail == 0) printf("thread_pool: todas as verificações passaram\n");
  return g_fail ? 1 : 0;
}
