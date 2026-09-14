#include "minecpp/core/event.h"
#include <stddef.h>

#define MINECPP_MAX_SUBS 64

static minecpp_cb_t g_cb[MINECPP_EV_COUNT][MINECPP_MAX_SUBS];
static void *g_ctx[MINECPP_EV_COUNT][MINECPP_MAX_SUBS];
static unsigned g_n[MINECPP_EV_COUNT];

void minecpp_event_sub(minecpp_ev_t ev, minecpp_cb_t cb, void *ctx) {
  if ((unsigned)ev >= MINECPP_EV_COUNT || !cb) return;
  unsigned n = g_n[ev];
  if (n >= MINECPP_MAX_SUBS) return; // drop: plugin sistema trata como erro depois
  g_cb[ev][n] = cb;
  g_ctx[ev][n] = ctx;
  g_n[ev] = n + 1;
}

void minecpp_event_emit(minecpp_ev_t ev, const void *payload) {
  if ((unsigned)ev >= MINECPP_EV_COUNT) return;
  unsigned n = g_n[ev];
  if (n == 0) return; // custo zero sem subscriber
  for (unsigned i = 0; i < n; i++) g_cb[ev][i](payload, g_ctx[ev][i]);
}

unsigned minecpp_event_count(minecpp_ev_t ev) {
  if ((unsigned)ev >= MINECPP_EV_COUNT) return 0;
  return g_n[ev];
}
