#pragma once

#include <stdint.h>

// Scheduler: tarefas sync no tick + offload p/ workers.
// Tick tem orcamento fixo de 50ms (20 TPS). Scheduler nunca bloqueia tick em I/O.

typedef void (*minecpp_task_fn)(void *ctx);

#ifdef __cplusplus
extern "C" {
#endif

// Roda cb depois de delay_ticks ticks. Chamado pela tick thread.
void minecpp_schedule_in_ticks(uint64_t delay_ticks, minecpp_task_fn fn, void *ctx);
// Roda cb em worker (fora da tick). Resultado volta via commit na tick.
void minecpp_schedule_async(minecpp_task_fn fn, void *ctx);

uint64_t minecpp_tick_now(void);

#ifdef __cplusplus
}
#endif
