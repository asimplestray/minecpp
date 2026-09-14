#pragma once

// Event bus com custo zero sem subscriber. Base dos hooks p/ futuro Paper.
// Ver docs/architecture.md secao 7.

typedef enum minecpp_ev {
  MINECPP_EV_TICK = 0,
  MINECPP_EV_CHUNK_LOAD,
  MINECPP_EV_CHUNK_UNLOAD,
  MINECPP_EV_PACKET_IN,
  MINECPP_EV_COUNT
} minecpp_ev_t;

typedef void (*minecpp_cb_t)(const void *payload, void *ctx);

#ifdef __cplusplus
extern "C" {
#endif

void minecpp_event_sub(minecpp_ev_t ev, minecpp_cb_t cb, void *ctx);
// Por enquanto sem unsub (plugin descarregavel vem depois da API nativa).
void minecpp_event_emit(minecpp_ev_t ev, const void *payload);
unsigned minecpp_event_count(minecpp_ev_t ev);

#ifdef __cplusplus
}
#endif
