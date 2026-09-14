#pragma once

// Rede do core: listener TCP, poller (epoll no Linux, WSAPoll no Windows) e
// canal com framing+compressão+criptografia do protocolo vanilla.
//
// O chan é agnóstico de versão: bytes no fio <-> pacotes (id+payload).
// Compressão (threshold, zlib) e AES/CFB8 vivem aqui; a thread dona (net)
// chama recv/next/send/flush. Roteamento por estado (STATUS/LOGIN/PLAY) é
// da camada de versão (tick thread).
//
// Limites: frame 2MB, buffers 8MB (anti-OOM). Tudo non-blocking.

#include <stddef.h>
#include <stdint.h>

#include "minecpp/core/crypto.h"

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
typedef SOCKET minecpp_fd_t;
#define MINECPP_FD_INVALID INVALID_SOCKET
#else
typedef int minecpp_fd_t;
#define MINECPP_FD_INVALID -1
#endif

typedef enum minecpp_net_err {
  MINECPP_NET_OK = 0,
  MINECPP_NET_IO,
  MINECPP_NET_NOMEM,
  MINECPP_NET_CORRUPT,
  MINECPP_NET_BAD_ARG,
} minecpp_net_err_t;

#ifdef __cplusplus
extern "C" {
#endif

// ---- listener ----
typedef struct minecpp_listener minecpp_listener_t;

// ip NULL ou "0.0.0.0" = any IPv4. backlog 128, reuseaddr, non-blocking.
minecpp_listener_t *minecpp_net_listen(const char *ip, uint16_t port,
                                       minecpp_net_err_t *err);
void minecpp_net_listener_free(minecpp_listener_t *l);
minecpp_fd_t minecpp_net_listener_fd(const minecpp_listener_t *l);
uint16_t minecpp_net_listener_port(const minecpp_listener_t *l);
// >=0 fd aceito (nonblock+nodelay); -1 nada pendente; -2 erro.
intptr_t minecpp_net_accept(minecpp_listener_t *l);

// ---- poller ----
typedef struct minecpp_poller minecpp_poller_t;

typedef struct minecpp_net_event {
  uint64_t tag;  // definida no add
  int readable;
  int writable;
  int hup;  // ERR/HUP/RDHUP — drenar e fechar
} minecpp_net_event_t;

minecpp_poller_t *minecpp_poller_create(void);
// Não fecha fds (dono é o chan/listener).
void minecpp_poller_destroy(minecpp_poller_t *p);
int minecpp_poller_add(minecpp_poller_t *p, minecpp_fd_t fd, uint64_t tag,
                       int rd, int wr);
// MOD precisa da tag (epoll não guarda; re-registra o evento completo).
int minecpp_poller_mod(minecpp_poller_t *p, minecpp_fd_t fd, uint64_t tag,
                       int rd, int wr);
int minecpp_poller_del(minecpp_poller_t *p, minecpp_fd_t fd);
// >=0 eventos preenchidos; -1 erro.
int minecpp_net_poll(minecpp_poller_t *p, minecpp_net_event_t *evts, int cap,
                     uint32_t timeout_ms);

void minecpp_net_fd_close(minecpp_fd_t fd);

// ---- canal ----
typedef struct minecpp_chan minecpp_chan_t;

// Assume ownership do fd.
minecpp_chan_t *minecpp_chan_wrap(minecpp_fd_t fd);
void minecpp_chan_free(minecpp_chan_t *c);  // fecha fd
minecpp_fd_t minecpp_chan_fd(const minecpp_chan_t *c);

// Lê do socket (descriptografa no ato). >0 bytes, 0 EOF, -1 erro, -2 bloqueio.
int minecpp_chan_recv(minecpp_chan_t *c);
// Extrai UM pacote: 1 ok (payload malloc, free nele), 0 need-more,
// -1 corrupt (mate a conexão), -2 nomem.
int minecpp_chan_next(minecpp_chan_t *c, int32_t *id, uint8_t **payload,
                      size_t *n);
// Enfileira pacote (comprime+framing+criptografa). 0 ok, -1 limite/nomem.
int minecpp_chan_send(minecpp_chan_t *c, int32_t id, const uint8_t *payload,
                      size_t n);
// Escreve pendente: 0 vazio, 1 parcial, -1 erro.
int minecpp_chan_flush(minecpp_chan_t *c);
size_t minecpp_chan_pending(const minecpp_chan_t *c);

void minecpp_chan_set_compression(minecpp_chan_t *c, int32_t threshold);
// 0 ok, -1 sem OpenSSL.
int minecpp_chan_set_encryption(minecpp_chan_t *c, const uint8_t secret[16]);

#ifdef __cplusplus
}
#endif
