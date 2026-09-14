#pragma once

// Fila MPSC (múltiplos produtores, um consumidor) — o canal tick<->net e
// workers->tick. Mutex+cond: simples e correto; lock-free quando o perfil
// mandar. malloc por mensagem (fora do hot path de gameplay).

#include <stddef.h>
#include <stdint.h>

typedef struct minecpp_mqueue minecpp_mqueue_t;

#ifdef __cplusplus
extern "C" {
#endif

minecpp_mqueue_t *minecpp_mqueue_create(void);
// Destroi; leftovers liberados com free_fn (pode ser NULL → vaza proposital? não: exige free_fn ou fila vazia).
void minecpp_mqueue_destroy(minecpp_mqueue_t *q, void (*free_fn)(void *));
void minecpp_mqueue_push(minecpp_mqueue_t *q, void *p);  // p != NULL
void *minecpp_mqueue_pop(minecpp_mqueue_t *q);  // NULL se vazia
void *minecpp_mqueue_wait(minecpp_mqueue_t *q, uint32_t timeout_ms);
size_t minecpp_mqueue_size(minecpp_mqueue_t *q);  // aproximado

#ifdef __cplusplus
}
#endif
