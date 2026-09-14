// Testes mqueue: FIFO, wait/timeout, produtor paralelo.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "minecpp/core/mqueue.h"
#include "minecpp/core/sync.h"

static int g_fail = 0;

#define ASSERT(cond)                                         \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      g_fail++;                                              \
    }                                                        \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static void t_fifo(void) {
  minecpp_mqueue_t *q = minecpp_mqueue_create();
  ASSERT(q);
  ASSERT_EQ(minecpp_mqueue_size(q), 0u);
  ASSERT(minecpp_mqueue_pop(q) == NULL);
  for (int i = 1; i <= 5; i++)
    minecpp_mqueue_push(q, (void *)(uintptr_t)i);
  ASSERT_EQ(minecpp_mqueue_size(q), 5u);
  for (int i = 1; i <= 5; i++)
    ASSERT_EQ((int)(uintptr_t)minecpp_mqueue_pop(q), i);
  ASSERT(minecpp_mqueue_pop(q) == NULL);
  // wait com timeout em fila vazia → NULL rápido.
  ASSERT(minecpp_mqueue_wait(q, 20) == NULL);
  minecpp_mqueue_destroy(q, NULL);
}

static void producer(void *ctx) {
  minecpp_mqueue_t *q = (minecpp_mqueue_t *)ctx;
  struct timespec ts = {0, 50000000L};
  nanosleep(&ts, NULL);
  minecpp_mqueue_push(q, (void *)(uintptr_t)42);
}

static void t_wait(void) {
  minecpp_mqueue_t *q = minecpp_mqueue_create();
  minecpp_thread_t t;
  ASSERT_EQ(minecpp_thread_create(&t, producer, q), 0);
  ASSERT_EQ((int)(uintptr_t)minecpp_mqueue_wait(q, 5000), 42);
  minecpp_thread_join(&t);
  minecpp_mqueue_destroy(q, NULL);
}

static void t_destroy_leftovers(void) {
  minecpp_mqueue_t *q = minecpp_mqueue_create();
  minecpp_mqueue_push(q, malloc(8));
  minecpp_mqueue_push(q, malloc(8));
  minecpp_mqueue_destroy(q, free);  // sem leak (ASan confere)
}

int main(void) {
  t_fifo();
  t_wait();
  t_destroy_leftovers();
  if (g_fail == 0) printf("mqueue: todas as verificações passaram\n");
  return g_fail ? 1 : 0;
}
