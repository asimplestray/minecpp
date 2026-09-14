#pragma once

#include <stddef.h>
#include <stdint.h>

// Job system work-stealing. Tick orquestra, workers executam.
// Regra: workers escrevem em delta, tick faz commit serial. Ver architecture 4.5.

typedef void (*minecpp_job_fn)(void *ctx);

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa com worker_count threads. 0 = auto (cpu_count - 2, min 2).
int minecpp_thread_pool_init(unsigned worker_count);
void minecpp_thread_pool_shutdown(void);
unsigned minecpp_thread_pool_workers(void);

// Enfileira job. prio 0 = urgente (P0), 1 = prefetch (P1), 2 = fundo (P2).
// Sem malloc no caminho quente: rings limitados, retorna -1 se cheia (caller degrada).
int minecpp_job_submit(unsigned prio, minecpp_job_fn fn, void *ctx);

// Estatísticas (testes + futura telemetria de TPS).
typedef struct minecpp_pool_stats {
  uint64_t submitted;
  uint64_t completed;
  uint64_t dropped;  // submits com ring cheio
  unsigned workers;
} minecpp_pool_stats_t;

void minecpp_thread_pool_stats(minecpp_pool_stats_t *out);
// Jobs completados pelo worker i (i < workers). Prova distribuição/roubo.
uint64_t minecpp_thread_pool_worker_completed(unsigned i);

#ifdef __cplusplus
}
#endif
