// Feature-test antes de tudo (ver region.c/sync.c).
#if !defined(_WIN32) && !defined(_WIN64)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "minecpp/core/net.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "minecpp/core/buf.h"

#define MINECPP_MAX_FRAME (2u << 20)  // 2MB por frame/pacote
#define MINECPP_BUF_CAP (8u << 20)  // 8MB por direção (anti-OOM)
#define MINECPP_RECV_CHUNK 65536

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// ---------------------------------------------------------------- helpers fd

static int fd_set_nonblock(minecpp_fd_t fd) {
#if defined(_WIN32) || defined(_WIN64)
  u_long m = 1;
  return ioctlsocket(fd, FIONBIO, &m) == 0 ? 0 : -1;
#else
  int f = fcntl(fd, F_GETFL, 0);
  if (f < 0) return -1;
  return fcntl(fd, F_SETFL, f | O_NONBLOCK) == 0 ? 0 : -1;
#endif
}

static void fd_set_nodelay(minecpp_fd_t fd) {
  int one = 1;
#if defined(_WIN32) || defined(_WIN64)
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
#else
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, (socklen_t)sizeof one);
#endif
}

void minecpp_net_fd_close(minecpp_fd_t fd) {
  if (fd == MINECPP_FD_INVALID) return;
#if defined(_WIN32) || defined(_WIN64)
  closesocket(fd);
#else
  close(fd);
#endif
}

// ---------------------------------------------------------------- listener

struct minecpp_listener {
  minecpp_fd_t fd;
  uint16_t port;
};

minecpp_listener_t *minecpp_net_listen(const char *ip, uint16_t port,
                                       minecpp_net_err_t *err) {
  minecpp_net_err_t e = MINECPP_NET_OK;
  minecpp_listener_t *l = NULL;
  minecpp_fd_t fd = MINECPP_FD_INVALID;
  struct sockaddr_in a;
  int one = 1;

#define LFAIL(code) \
  do {              \
    e = (code);     \
    goto fail;      \
  } while (0)

  l = (minecpp_listener_t *)calloc(1, sizeof *l);
  if (!l) LFAIL(MINECPP_NET_NOMEM);
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == MINECPP_FD_INVALID) LFAIL(MINECPP_NET_IO);
#if defined(_WIN32) || defined(_WIN64)
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
#else
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, (socklen_t)sizeof one);
#endif
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  if (!ip || !ip[0] || strcmp(ip, "0.0.0.0") == 0) {
    a.sin_addr.s_addr = htonl(INADDR_ANY);
  } else if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) {
    LFAIL(MINECPP_NET_BAD_ARG);
  }
  if (bind(fd, (struct sockaddr *)&a, (socklen_t)sizeof a) != 0)
    LFAIL(MINECPP_NET_IO);
  if (listen(fd, 128) != 0) LFAIL(MINECPP_NET_IO);
  if (fd_set_nonblock(fd) != 0) LFAIL(MINECPP_NET_IO);
  if (port == 0) {  // porta efetiva (testes usam 0)
    socklen_t n = (socklen_t)sizeof a;
    if (getsockname(fd, (struct sockaddr *)&a, &n) != 0) LFAIL(MINECPP_NET_IO);
    port = ntohs(a.sin_port);
  }
  l->fd = fd;
  l->port = port;
  if (err) *err = MINECPP_NET_OK;
  return l;

fail:
  if (fd != MINECPP_FD_INVALID) minecpp_net_fd_close(fd);
  free(l);
  if (err) *err = e;
  return NULL;
#undef LFAIL
}

void minecpp_net_listener_free(minecpp_listener_t *l) {
  if (!l) return;
  minecpp_net_fd_close(l->fd);
  free(l);
}

minecpp_fd_t minecpp_net_listener_fd(const minecpp_listener_t *l) {
  return l ? l->fd : MINECPP_FD_INVALID;
}

uint16_t minecpp_net_listener_port(const minecpp_listener_t *l) {
  return l ? l->port : 0;
}

intptr_t minecpp_net_accept(minecpp_listener_t *l) {
  minecpp_fd_t fd;
  if (!l) return -2;
#if defined(_WIN32) || defined(_WIN64)
  fd = accept(l->fd, NULL, NULL);
  if (fd == INVALID_SOCKET) {
    const int e = WSAGetLastError();
    if (e == WSAEWOULDBLOCK || e == WSAECONNRESET || e == WSAEINTR) return -1;
    return -2;
  }
#else
  for (;;) {
    fd = accept(l->fd, NULL, NULL);
    if (fd >= 0) break;
    if (errno == EINTR) continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED)
      return -1;
    return -2;
  }
#endif
  if (fd_set_nonblock(fd) != 0) {
    minecpp_net_fd_close(fd);
    return -2;
  }
  fd_set_nodelay(fd);
  return (intptr_t)fd;
}

// ---------------------------------------------------------------- poller

struct minecpp_poller {
#if defined(_WIN32) || defined(_WIN64)
  struct {
    minecpp_fd_t fd;
    uint64_t tag;
    int rd, wr;
  } *items;
  size_t len, cap;
#else
  int ep;
#endif
};

minecpp_poller_t *minecpp_poller_create(void) {
  minecpp_poller_t *p = (minecpp_poller_t *)calloc(1, sizeof *p);
  if (!p) return NULL;
#if !defined(_WIN32) && !defined(_WIN64)
  p->ep = epoll_create1(EPOLL_CLOEXEC);
  if (p->ep < 0) {
    free(p);
    return NULL;
  }
#endif
  return p;
}

void minecpp_poller_destroy(minecpp_poller_t *p) {
  if (!p) return;
#if defined(_WIN32) || defined(_WIN64)
  free(p->items);
#else
  close(p->ep);
#endif
  free(p);
}

#if defined(_WIN32) || defined(_WIN64)

static int poll_grow(minecpp_poller_t *p) {
  size_t ncap = p->cap ? p->cap * 2 : 16;
  void *ni = realloc(p->items, ncap * sizeof *p->items);
  if (!ni) return -1;
  p->items = ni;
  p->cap = ncap;
  return 0;
}

static int poll_find(minecpp_poller_t *p, minecpp_fd_t fd) {
  for (size_t i = 0; i < p->len; i++)
    if (p->items[i].fd == fd) return (int)i;
  return -1;
}

int minecpp_poller_add(minecpp_poller_t *p, minecpp_fd_t fd, uint64_t tag,
                       int rd, int wr) {
  if (!p || fd == MINECPP_FD_INVALID || poll_find(p, fd) >= 0) return -1;
  if (p->len == p->cap && poll_grow(p) != 0) return -1;
  p->items[p->len].fd = fd;
  p->items[p->len].tag = tag;
  p->items[p->len].rd = rd;
  p->items[p->len].wr = wr;
  p->len++;
  return 0;
}

int minecpp_poller_mod(minecpp_poller_t *p, minecpp_fd_t fd, uint64_t tag,
                       int rd, int wr) {
  int i;
  if (!p) return -1;
  i = poll_find(p, fd);
  if (i < 0) return -1;
  p->items[i].tag = tag;
  p->items[i].rd = rd;
  p->items[i].wr = wr;
  return 0;
}

int minecpp_poller_del(minecpp_poller_t *p, minecpp_fd_t fd) {
  int i;
  if (!p) return -1;
  i = poll_find(p, fd);
  if (i < 0) return -1;
  p->items[i] = p->items[--p->len];
  return 0;
}

int minecpp_net_poll(minecpp_poller_t *p, minecpp_net_event_t *evts, int cap,
                     uint32_t timeout_ms) {
  WSAPOLLFD *fds;
  int n, got = 0;
  if (!p || !evts || cap <= 0) return -1;
  if (!p->len) {
    Sleep(timeout_ms);
    return 0;
  }
  fds = (WSAPOLLFD *)malloc(p->len * sizeof *fds);
  if (!fds) return -1;
  for (size_t i = 0; i < p->len; i++) {
    fds[i].fd = p->items[i].fd;
    fds[i].events = 0;
    if (p->items[i].rd) fds[i].events |= POLLRDNORM | POLLRDBAND;
    if (p->items[i].wr) fds[i].events |= POLLWRNORM;
    fds[i].revents = 0;
  }
  n = WSAPoll(fds, (ULONG)p->len, (INT)timeout_ms);
  if (n <= 0) {
    free(fds);
    return n < 0 ? -1 : 0;
  }
  for (size_t i = 0; i < p->len && got < cap; i++) {
    if (!fds[i].revents) continue;
    evts[got].tag = p->items[i].tag;
    evts[got].readable =
        (fds[i].revents & (POLLRDNORM | POLLRDBAND | POLLHUP)) != 0;
    evts[got].writable = (fds[i].revents & POLLWRNORM) != 0;
    evts[got].hup =
        (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0;
    got++;
  }
  free(fds);
  return got;
}

#else  // epoll (include no topo POSIX)

static uint32_t poll_mask(int rd, int wr) {
  uint32_t m = EPOLLERR | EPOLLHUP | EPOLLRDHUP;
  if (rd) m |= EPOLLIN;
  if (wr) m |= EPOLLOUT;
  return m;
}

int minecpp_poller_add(minecpp_poller_t *p, minecpp_fd_t fd, uint64_t tag,
                       int rd, int wr) {
  struct epoll_event ev;
  if (!p || fd < 0) return -1;
  memset(&ev, 0, sizeof ev);
  ev.events = poll_mask(rd, wr);
  ev.data.u64 = tag;
  return epoll_ctl(p->ep, EPOLL_CTL_ADD, fd, &ev);
}

int minecpp_poller_mod(minecpp_poller_t *p, minecpp_fd_t fd, uint64_t tag,
                       int rd, int wr) {
  struct epoll_event ev;
  if (!p || fd < 0) return -1;
  memset(&ev, 0, sizeof ev);
  ev.events = poll_mask(rd, wr);
  ev.data.u64 = tag;
  return epoll_ctl(p->ep, EPOLL_CTL_MOD, fd, &ev);
}

int minecpp_poller_del(minecpp_poller_t *p, minecpp_fd_t fd) {
  if (!p || fd < 0) return -1;
  return epoll_ctl(p->ep, EPOLL_CTL_DEL, fd, NULL);
}

int minecpp_net_poll(minecpp_poller_t *p, minecpp_net_event_t *evts, int cap,
                     uint32_t timeout_ms) {
  struct epoll_event *raw;
  int n, got = 0;
  if (!p || !evts || cap <= 0) return -1;
  raw = (struct epoll_event *)malloc((size_t)cap * sizeof *raw);
  if (!raw) return -1;
  n = epoll_wait(p->ep, raw, cap, (int)timeout_ms);
  if (n < 0) {
    free(raw);
    return errno == EINTR ? 0 : -1;
  }
  for (int i = 0; i < n; i++) {
    evts[got].tag = raw[i].data.u64;
    evts[got].readable = (raw[i].events & EPOLLIN) != 0;
    evts[got].writable = (raw[i].events & EPOLLOUT) != 0;
    evts[got].hup =
        (raw[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0;
    got++;
  }
  free(raw);
  return got;
}

#endif

// ---------------------------------------------------------------- canal

typedef struct {
  uint8_t *b;
  size_t len;  // bytes válidos a partir de off
  size_t off;
  size_t cap;
} dynbuf_t;

struct minecpp_chan {
  minecpp_fd_t fd;
  dynbuf_t rx, tx;
  int32_t compression;  // -1 off
  minecpp_aes_t *enc, *dec;
};

static int dyn_reserve(dynbuf_t *d, size_t extra) {
  if (d->len + extra <= d->cap) return 0;
  if (d->off) {  // compacta antes de crescer
    memmove(d->b, d->b + d->off, d->len - d->off);
    d->len -= d->off;
    d->off = 0;
  }
  if (d->len + extra <= d->cap) return 0;
  if (d->len + extra > MINECPP_BUF_CAP) return -1;
  size_t ncap = d->cap ? d->cap : 4096;
  while (ncap < d->len + extra) ncap *= 2;
  if (ncap > MINECPP_BUF_CAP) ncap = MINECPP_BUF_CAP;
  uint8_t *nb = (uint8_t *)realloc(d->b, ncap);
  if (!nb) return -1;
  d->b = nb;
  d->cap = ncap;
  return 0;
}

minecpp_chan_t *minecpp_chan_wrap(minecpp_fd_t fd) {
  minecpp_chan_t *c;
  if (fd == MINECPP_FD_INVALID) return NULL;
  c = (minecpp_chan_t *)calloc(1, sizeof *c);
  if (!c) return NULL;
  c->fd = fd;
  c->compression = -1;
  return c;
}

void minecpp_chan_free(minecpp_chan_t *c) {
  if (!c) return;
  minecpp_net_fd_close(c->fd);
  free(c->rx.b);
  free(c->tx.b);
  minecpp_aes_free(c->enc);
  minecpp_aes_free(c->dec);
  free(c);
}

minecpp_fd_t minecpp_chan_fd(const minecpp_chan_t *c) {
  return c ? c->fd : MINECPP_FD_INVALID;
}

int minecpp_chan_recv(minecpp_chan_t *c) {
  uint8_t tmp[MINECPP_RECV_CHUNK];
  long n;
  size_t total = 0;
  if (!c) return -1;
  for (;;) {
#if defined(_WIN32) || defined(_WIN64)
    n = recv(c->fd, (char *)tmp, sizeof tmp, 0);
    if (n == SOCKET_ERROR) {
      const int e = WSAGetLastError();
      if (e == WSAEWOULDBLOCK) break;
      return total ? (int)total : -1;
    }
#else
    n = recv(c->fd, tmp, sizeof tmp, 0);
    if (n < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      return total ? (int)total : -1;
    }
#endif
    if (n == 0) return 0;  // EOF (pode restar frame no buffer)
    if (dyn_reserve(&c->rx, (size_t)n) != 0) return -1;
    if (c->dec) minecpp_aes_update(c->dec, tmp, c->rx.b + c->rx.len, (size_t)n);
    else memcpy(c->rx.b + c->rx.len, tmp, (size_t)n);
    c->rx.len += (size_t)n;
    total += (size_t)n;
    if (total >= MINECPP_RECV_CHUNK * 4) break;  // fatia justa por evento
    if ((size_t)n < sizeof tmp) break;
  }
  if (c->rx.len - c->rx.off > MINECPP_BUF_CAP) return -1;
  return total ? (int)(total > 0x7FFFFFFF ? 0x7FFFFFFF : total) : -2;
}

// VarInt direto no buffer (sem cursor): -1 incompleto, -2 inválido.
static int peek_varint(const uint8_t *p, size_t n, int32_t *v, size_t *used) {
  uint32_t out = 0;
  for (int shift = 0; shift < 35; shift += 7) {
    size_t i = (size_t)(shift / 7);
    uint8_t b;
    if (i >= n) return -1;
    b = p[i];
    out |= (uint32_t)(b & 0x7F) << shift;
    if (!(b & 0x80)) {
      *v = (int32_t)out;
      *used = i + 1;
      return 0;
    }
  }
  return -2;
}

static size_t varint_len(uint32_t v) {
  size_t n = 1;
  while (v & 0xFFFFFF80u) {
    n++;
    v >>= 7;
  }
  return n;
}

int minecpp_chan_next(minecpp_chan_t *c, int32_t *id, uint8_t **payload,
                      size_t *n) {
  const uint8_t *p;
  size_t avail, used = 0;
  int32_t len = 0;
  if (!c || !id || !payload || !n) return -2;
  p = c->rx.b + c->rx.off;
  avail = c->rx.len - c->rx.off;
  if (peek_varint(p, avail, &len, &used) != 0) return 0;  // need-more
  if (len <= 0 || (uint32_t)len > MINECPP_MAX_FRAME) return -1;
  if (avail - used < (size_t)len) return 0;  // need-more
  p += used;
  avail = (size_t)len;
  if (c->compression < 0) {
    // Sem compressão: id + payload crus.
    int32_t pid = 0;
    size_t u = 0;
    if (peek_varint(p, avail, &pid, &u) != 0) return -1;
    uint8_t *out = (uint8_t *)malloc(avail - u ? avail - u : 1);
    if (!out) return -2;
    memcpy(out, p + u, avail - u);
    *id = pid;
    *payload = out;
    *n = avail - u;
  } else {
    int32_t dl = 0;
    size_t u = 0;
    if (peek_varint(p, avail, &dl, &u) != 0 || dl < 0) return -1;
    if (dl == 0) {
      int32_t pid = 0;
      size_t u2 = 0;
      if (peek_varint(p + u, avail - u, &pid, &u2) != 0) return -1;
      uint8_t *out = (uint8_t *)malloc(avail - u - u2 ? avail - u - u2 : 1);
      if (!out) return -2;
      memcpy(out, p + u + u2, avail - u - u2);
      *id = pid;
      *payload = out;
      *n = avail - u - u2;
    } else {
      uLongf dstn;
      uint8_t *out;
      int zr;
      if ((uint32_t)dl > MINECPP_MAX_FRAME) return -1;
      out = (uint8_t *)malloc((size_t)dl ? (size_t)dl : 1);
      if (!out) return -2;
      dstn = (uLongf)dl;
      zr = uncompress(out, &dstn, p + u, (uLong)(avail - u));
      if (zr != Z_OK || dstn != (uLongf)dl) {
        free(out);
        return -1;
      }
      {
        int32_t pid = 0;
        size_t u2 = 0;
        if (peek_varint(out, (size_t)dl, &pid, &u2) != 0) {
          free(out);
          return -1;
        }
        // Reposiciona payload após o id (move dentro do malloc).
        memmove(out, out + u2, (size_t)dl - u2);
        *id = pid;
        *payload = out;
        *n = (size_t)dl - u2;
      }
    }
  }
  c->rx.off += used + (size_t)len;
  if (c->rx.off == c->rx.len) c->rx.off = c->rx.len = 0;
  return 1;
}

int minecpp_chan_send(minecpp_chan_t *c, int32_t id, const uint8_t *payload,
                      size_t n) {
  minecpp_writer_t body = {NULL, 0, 0, 0};
  minecpp_writer_t frame = {NULL, 0, 0, 0};
  if (!c || (!payload && n)) return -1;
  minecpp_wr_varint(&body, id);
  if (n) minecpp_wr_raw(&body, payload, n);
  if (!minecpp_wr_ok(&body)) {
    free(body.buf);
    return -1;
  }
  if (c->compression < 0) {
    minecpp_wr_varint(&frame, (int32_t)body.len);
    minecpp_wr_raw(&frame, body.buf, body.len);
  } else if (body.len < (size_t)c->compression) {
    minecpp_wr_varint(&frame, (int32_t)(body.len + varint_len(0)));
    minecpp_wr_varint(&frame, 0);
    minecpp_wr_raw(&frame, body.buf, body.len);
  } else {
    uLongf cn = compressBound((uLong)body.len);
    uint8_t *cb = (uint8_t *)malloc((size_t)cn);
    if (!cb) {
      free(body.buf);
      return -1;
    }
    if (compress2(cb, &cn, body.buf, (uLong)body.len, Z_DEFAULT_COMPRESSION) !=
        Z_OK) {
      free(cb);
      free(body.buf);
      return -1;
    }
    minecpp_wr_varint(&frame, (int32_t)((size_t)cn + varint_len((uint32_t)body.len)));
    minecpp_wr_varint(&frame, (int32_t)body.len);
    minecpp_wr_raw(&frame, cb, (size_t)cn);
    free(cb);
  }
  free(body.buf);
  if (!minecpp_wr_ok(&frame)) {
    free(frame.buf);
    return -1;
  }
  if (c->tx.len - c->tx.off + frame.len > MINECPP_BUF_CAP) {
    free(frame.buf);
    return -1;  // backpressure: caller fecha a conexão
  }
  if (dyn_reserve(&c->tx, frame.len) != 0) {
    free(frame.buf);
    return -1;
  }
  if (c->enc)
    minecpp_aes_update(c->enc, frame.buf, c->tx.b + c->tx.len, frame.len);
  else
    memcpy(c->tx.b + c->tx.len, frame.buf, frame.len);
  c->tx.len += frame.len;
  free(frame.buf);
  return 0;
}

int minecpp_chan_flush(minecpp_chan_t *c) {
  if (!c) return -1;
  while (c->tx.off < c->tx.len) {
#if defined(_WIN32) || defined(_WIN64)
    const int n = send(c->fd, (const char *)c->tx.b + c->tx.off,
                       (int)(c->tx.len - c->tx.off), 0);
    if (n == SOCKET_ERROR) {
      return WSAGetLastError() == WSAEWOULDBLOCK ? 1 : -1;
    }
#else
    const ssize_t n =
        send(c->fd, c->tx.b + c->tx.off, c->tx.len - c->tx.off, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
      return -1;
    }
#endif
    if (n == 0) return -1;
    c->tx.off += (size_t)n;
  }
  c->tx.off = c->tx.len = 0;
  return 0;
}

size_t minecpp_chan_pending(const minecpp_chan_t *c) {
  return c ? c->tx.len - c->tx.off : 0;
}

void minecpp_chan_set_compression(minecpp_chan_t *c, int32_t threshold) {
  if (c) c->compression = threshold;
}

int minecpp_chan_set_encryption(minecpp_chan_t *c, const uint8_t secret[16]) {
  if (!c || !secret) return -1;
  minecpp_aes_free(c->enc);
  minecpp_aes_free(c->dec);
  c->enc = c->dec = NULL;
  c->enc = minecpp_aes_encrypt_init(secret);
  c->dec = minecpp_aes_decrypt_init(secret);
  if (!c->enc || !c->dec) {
    minecpp_aes_free(c->enc);
    minecpp_aes_free(c->dec);
    c->enc = c->dec = NULL;
    return -1;
  }
  return 0;
}
