#include "minecpp/core/scheduler.h"
#include <stdatomic.h>

#include "minecpp/core/thread_pool.h"

static _Atomic unsigned long long g_tick = 0;

void minecpp_schedule_in_ticks(uint64_t delay_ticks, minecpp_task_fn fn, void *ctx) {
  (void)delay_ticks;
  (void)fn;
  (void)ctx;
  // TODO MVP: fila MPSC de tarefas com deadline = tick_atual + delay.
}

void minecpp_schedule_async(minecpp_task_fn fn, void *ctx) {
  // Offload p/ workers (P2=fundo). Exige pool iniciado; drop conta em stats.
  if (fn) minecpp_job_submit(2, fn, ctx);
}

uint64_t minecpp_tick_now(void) { return atomic_load(&g_tick); }
