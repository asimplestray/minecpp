#include "minecpp/core/mqueue.h"

#include <stdlib.h>

#include "minecpp/core/sync.h"

typedef struct node {
  void *p;
  struct node *next;
} node_t;

struct minecpp_mqueue {
  minecpp_mutex_t mu;
  minecpp_cond_t cv;
  node_t *head, *tail;
  size_t size;
};

minecpp_mqueue_t *minecpp_mqueue_create(void) {
  minecpp_mqueue_t *q = (minecpp_mqueue_t *)calloc(1, sizeof *q);
  if (!q) return NULL;
  if (minecpp_mutex_init(&q->mu) != 0) {
    free(q);
    return NULL;
  }
  if (minecpp_cond_init(&q->cv) != 0) {
    minecpp_mutex_destroy(&q->mu);
    free(q);
    return NULL;
  }
  return q;
}

void minecpp_mqueue_destroy(minecpp_mqueue_t *q, void (*free_fn)(void *)) {
  node_t *n;
  if (!q) return;
  n = q->head;
  while (n) {
    node_t *nx = n->next;
    if (free_fn) free_fn(n->p);
    free(n);
    n = nx;
  }
  minecpp_cond_destroy(&q->cv);
  minecpp_mutex_destroy(&q->mu);
  free(q);
}

void minecpp_mqueue_push(minecpp_mqueue_t *q, void *p) {
  node_t *n = (node_t *)malloc(sizeof *n);
  if (!q || !p || !n) {
    free(n);
    return;  // push nunca falha visivelmente; OOM aqui = drop (stats fora)
  }
  n->p = p;
  n->next = NULL;
  minecpp_mutex_lock(&q->mu);
  if (q->tail)
    q->tail->next = n;
  else
    q->head = n;
  q->tail = n;
  q->size++;
  minecpp_mutex_unlock(&q->mu);
  minecpp_cond_signal(&q->cv);
}

static void *pop_locked(minecpp_mqueue_t *q) {
  node_t *n = q->head;
  void *p;
  if (!n) return NULL;
  q->head = n->next;
  if (!q->head) q->tail = NULL;
  q->size--;
  p = n->p;
  free(n);
  return p;
}

void *minecpp_mqueue_pop(minecpp_mqueue_t *q) {
  void *p;
  if (!q) return NULL;
  minecpp_mutex_lock(&q->mu);
  p = pop_locked(q);
  minecpp_mutex_unlock(&q->mu);
  return p;
}

void *minecpp_mqueue_wait(minecpp_mqueue_t *q, uint32_t timeout_ms) {
  void *p;
  if (!q) return NULL;
  minecpp_mutex_lock(&q->mu);
  p = pop_locked(q);
  if (!p) {
    minecpp_cond_wait(&q->cv, &q->mu, timeout_ms);
    p = pop_locked(q);
  }
  minecpp_mutex_unlock(&q->mu);
  return p;
}

size_t minecpp_mqueue_size(minecpp_mqueue_t *q) {
  size_t s;
  if (!q) return 0;
  minecpp_mutex_lock(&q->mu);
  s = q->size;
  minecpp_mutex_unlock(&q->mu);
  return s;
}
