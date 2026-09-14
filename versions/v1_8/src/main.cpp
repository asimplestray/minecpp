// minecpp-1.8: servidor jogável (offline-mode).
// Uso: minecpp-1.8 [--port 25565] [--world ./world] [--view-dist 5]
//                  [--threshold 256] [--motd "..."] [--max 20]
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "minecpp/core/mqueue.h"
#include "minecpp/core/platform.h"
#include "minecpp/core/thread_pool.h"
#include "minecpp/v1_8/server.h"

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
static std::atomic<bool> *g_stop = nullptr;
static BOOL WINAPI CtrlHandler(DWORD) {
  if (g_stop) g_stop->store(true);
  return TRUE;
}
#else
#include <csignal>
static std::atomic<bool> *g_stop = nullptr;
static void OnSignal(int) {
  if (g_stop) g_stop->store(true);
}
#endif

namespace {

void SleepMs(unsigned ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

uint64_t NowMs() {
  return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void Usage(const char *argv0) {
  printf(
      "uso: %s [--port 25565] [--world ./world] [--view-dist 5] "
      "[--threshold 256] [--motd \"...\"] [--max 20] [--verbose]\n",
      argv0);
}

}  // namespace

int main(int argc, char **argv) {
  minecpp::v18::ServerConfig cfg;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    auto val = [&](const char *name) -> const char * {
      const size_t n = strlen(name);
      if (a == name && i + 1 < argc) return argv[++i];
      if (a.rfind(name, 0) == 0 && a.size() > n + 1 && a[n] == '=')  // --x=y
        return argv[i] + n + 1;
      return nullptr;
    };
    const char *v = nullptr;
    if ((v = val("--port"))) cfg.port = (uint16_t)atoi(v);
    else if ((v = val("--world"))) cfg.world_dir = v;
    else if ((v = val("--view-dist"))) cfg.view_distance = atoi(v);
    else if ((v = val("--threshold"))) cfg.compression_threshold = atoi(v);
    else if ((v = val("--motd"))) cfg.motd = v;
    else if ((v = val("--max"))) cfg.max_players = atoi(v);
    else if (a == "--verbose") cfg.verbose = true;
    else {
      Usage(argv[0]);
      return 2;
    }
  }

  if (minecpp_platform_init() != 0) {
    fprintf(stderr, "platform init falhou\n");
    return 1;
  }
  minecpp_thread_pool_init(0);

  minecpp_mqueue_t *tick_q = minecpp_mqueue_create();
  minecpp_mqueue_t *net_q = minecpp_mqueue_create();
  if (!tick_q || !net_q) {
    fprintf(stderr, "sem filas\n");
    return 1;
  }

  minecpp::v18::Server server(cfg, tick_q, net_q);
  std::string err;
  if (!server.Init(&err)) {
    fprintf(stderr, "mundo inválido: %s\n", err.c_str());
    return 1;
  }

  minecpp::v18::NetThread net(cfg.port, tick_q, net_q);
  net.SetVerbose(cfg.verbose);
  if (!net.Start(&err)) {
    fprintf(stderr, "net falhou: %s\n", err.c_str());
    return 1;
  }

  static std::atomic<bool> stop{false};
  g_stop = &stop;
#if defined(_WIN32) || defined(_WIN64)
  SetConsoleCtrlHandler(CtrlHandler, TRUE);
#else
  signal(SIGINT, OnSignal);
  signal(SIGTERM, OnSignal);
#endif

  printf("minecpp-1.8 ouvindo :%u mundo=%s view=%d threshold=%d\n", cfg.port,
         cfg.world_dir.c_str(), cfg.view_distance, cfg.compression_threshold);
  fflush(stdout);

  uint64_t tick = 0;
  uint64_t next = NowMs();
  while (!stop.load()) {
    server.OnTick(tick++);
    next += 50;
    const uint64_t now = NowMs();
    if (next > now) {
      SleepMs((unsigned)(next - now));
    } else if (now - next > 500) {
      next = now;  // espiral da morte: resync sem catch-up
    }
  }

  printf("\nparando...\n");
  net.Stop();  // join: nenhum push da net após aqui
  minecpp_thread_pool_shutdown();  // join: nenhum push de job após aqui
  // Drena leftovers (completions tardias) antes de destruir.
  for (;;) {
    auto *m = static_cast<minecpp::v18::MsgIn *>(
        minecpp_mqueue_pop(tick_q));
    if (!m) break;
    minecpp::v18::FreeMsgIn(m);
  }
  for (;;) {
    auto *m = static_cast<minecpp::v18::MsgOut *>(
        minecpp_mqueue_pop(net_q));
    if (!m) break;
    delete m;
  }
  minecpp_mqueue_destroy(net_q, nullptr);
  minecpp_mqueue_destroy(tick_q, nullptr);
  minecpp_platform_shutdown();
  return 0;
}
