#include "minecpp/v1_8/server.h"

#include <thread>

#include "minecpp/core/net.h"

namespace minecpp::v18 {

struct NetThread::Impl {
  minecpp_poller_t *poller = nullptr;
  minecpp_listener_t *listener = nullptr;
  struct Conn {
    minecpp_chan_t *chan = nullptr;
    bool closing = false;
  };
  std::unordered_map<uint32_t, Conn> conns;
  uint32_t next_id = 1;
};

static void DeleteOut(MsgOut *m) { delete m; }

NetThread::NetThread(uint16_t port, minecpp_mqueue_t *tick_q,
                     minecpp_mqueue_t *net_q)
    : port_(port), tick_q_(tick_q), net_q_(net_q) {}

NetThread::~NetThread() { Stop(); }

bool NetThread::Start(std::string *err) {
  if (started_) return true;
  impl_ = new (std::nothrow) Impl();
  if (!impl_) {
    if (err) *err = "sem memória";
    return false;
  }
  minecpp_net_err_t e = MINECPP_NET_OK;
  impl_->listener = minecpp_net_listen("0.0.0.0", port_, &e);
  if (!impl_->listener) {
    if (err) *err = "bind falhou (porta ocupada?)";
    delete impl_;
    impl_ = nullptr;
    return false;
  }
  impl_->poller = minecpp_poller_create();
  if (!impl_->poller) {
    if (err) *err = "sem poller";
    minecpp_net_listener_free(impl_->listener);
    delete impl_;
    impl_ = nullptr;
    return false;
  }
  if (minecpp_poller_add(impl_->poller,
                         minecpp_net_listener_fd(impl_->listener),
                         0, 1, 0) != 0) {
    if (err) *err = "poller_add falhou";
    minecpp_poller_destroy(impl_->poller);
    minecpp_net_listener_free(impl_->listener);
    delete impl_;
    impl_ = nullptr;
    return false;
  }
  stop_.store(false);
  worker_ = std::thread(&NetThread::Run, this);
  started_ = true;
  return true;
}

void NetThread::Stop() {
  if (!started_) return;
  stop_.store(true);
  if (worker_.joinable()) worker_.join();
  started_ = false;
}

void NetThread::CloseConn(uint32_t id, const char *why) {
  const auto it = impl_->conns.find(id);
  if (it == impl_->conns.end()) return;
  if (verbose_) {
    fprintf(stderr, "[net close conn=%u %s]\n", id, why ? why : "?");
    fflush(stderr);
  }
  minecpp_poller_del(impl_->poller, minecpp_chan_fd(it->second.chan));
  minecpp_chan_free(it->second.chan);
  impl_->conns.erase(it);
  auto *m = new (std::nothrow) MsgIn();
  if (m) {
    m->kind = MsgIn::Kind::Disconnected;
    m->conn = id;
    minecpp_mqueue_push(tick_q_, m);
  }
}

void NetThread::Run() {
  minecpp_net_event_t evts[64];
  for (;;) {
    if (stop_.load()) break;
    const int n =
        minecpp_net_poll(impl_->poller, evts, 64, 10);
    if (n < 0) continue;
    for (int i = 0; i < n; i++) {
      const uint64_t tag = evts[i].tag;
      if (tag == 0) {  // listener
        if (!evts[i].readable) continue;
        for (;;) {
          const intptr_t fd = minecpp_net_accept(impl_->listener);
          if (fd < 0) break;
          minecpp_chan_t *ch =
              minecpp_chan_wrap((minecpp_fd_t)(uintptr_t)fd);
          if (!ch) {
            minecpp_net_fd_close((minecpp_fd_t)(uintptr_t)fd);
            continue;
          }
          const uint32_t id = impl_->next_id++;
          if (minecpp_poller_add(impl_->poller, (minecpp_fd_t)(uintptr_t)fd,
                                 id, 1, 0) != 0) {
            minecpp_chan_free(ch);
            continue;
          }
          impl_->conns[id].chan = ch;
          auto *m = new (std::nothrow) MsgIn();
          if (m) {
            m->kind = MsgIn::Kind::Connected;
            m->conn = id;
            minecpp_mqueue_push(tick_q_, m);
          }
        }
        continue;
      }
      const uint32_t id = (uint32_t)tag;
      auto it = impl_->conns.find(id);
      if (it == impl_->conns.end()) continue;
      minecpp_chan_t *ch = it->second.chan;
      bool dead = false;
      const char *dead_why = "?";
      if (evts[i].readable || evts[i].hup) {
        const int r = minecpp_chan_recv(ch);
        if (r == 0 || r == -1) {
          // EOF/erro: drena frames no buffer antes de fechar.
          for (;;) {
            int32_t pid = 0;
            uint8_t *pay = nullptr;
            size_t nn = 0;
            const int g = minecpp_chan_next(ch, &pid, &pay, &nn);
            if (g != 1) {
              free(pay);
              break;
            }
            auto *m = new (std::nothrow) MsgIn();
            if (m) {
              m->kind = MsgIn::Kind::Packet;
              m->conn = id;
              m->id = pid;
              m->payload.assign(pay, pay + nn);
              minecpp_mqueue_push(tick_q_, m);
            }
            free(pay);
          }
          CloseConn(id, r == 0 ? "eof" : "recv-err");
          continue;
        }
        for (;;) {
          int32_t pid = 0;
          uint8_t *pay = nullptr;
          size_t nn = 0;
          const int g = minecpp_chan_next(ch, &pid, &pay, &nn);
          if (g == 0) {
            break;
          }
          if (g != 1) {
            dead = true;  // corrupt/nomem: mata a conexão
            dead_why = "corrupt-frame";
            break;
          }
          auto *m = new (std::nothrow) MsgIn();
          if (m) {
            m->kind = MsgIn::Kind::Packet;
            m->conn = id;
            m->id = pid;
            m->payload.assign(pay, pay + nn);
            minecpp_mqueue_push(tick_q_, m);
          }
          free(pay);
        }
      }
      if (!dead && evts[i].writable) {
        const int f = minecpp_chan_flush(ch);
        if (f < 0) {
          dead = true;
          dead_why = "flush-err";
        } else {
          const size_t pend = minecpp_chan_pending(ch);
          minecpp_poller_mod(impl_->poller, minecpp_chan_fd(ch), id, 1,
                             pend ? 1 : 0);
          if (f == 0 && it->second.closing) dead = true;  // flushou: fecha
        }
      }
      if (dead) CloseConn(id, dead_why);
    }
    // Fila tick->net.
    for (;;) {
      auto *m = static_cast<MsgOut *>(minecpp_mqueue_pop(net_q_));
      if (!m) break;
      const uint32_t conn_id = m->conn;
      const auto it = impl_->conns.find(conn_id);
      if (it == impl_->conns.end()) {
        DeleteOut(m);
        continue;
      }
      minecpp_chan_t *ch = it->second.chan;
      bool dead = false;
      const char *dead_why = "?";
      switch (m->kind) {
        case MsgOut::Kind::Send:
          if (minecpp_chan_send(ch, m->id, m->payload.data(),
                                m->payload.size()) != 0) {
            dead = true;
            dead_why = "send-full";
          } else {
            // Flush oportunista (baixa latência); resto sai no writable.
            const int f = minecpp_chan_flush(ch);
            if (f < 0) {
              dead = true;
              dead_why = "flush-err";
            } else if (f > 0) {
              minecpp_poller_mod(impl_->poller, minecpp_chan_fd(ch),
                                 m->conn, 1, 1);
            }
          }
          break;
        case MsgOut::Kind::EnableCompression:
          minecpp_chan_set_compression(ch, m->threshold);
          break;
        case MsgOut::Kind::Close:
          it->second.closing = true;
          dead_why = "close-msg";
          if (minecpp_chan_flush(ch) == 0 &&
              minecpp_chan_pending(ch) == 0) {
            dead = true;
          } else {
            minecpp_poller_mod(impl_->poller, minecpp_chan_fd(ch), m->conn,
                               1, 1);
          }
          break;
      }
      DeleteOut(m);
      if (dead) CloseConn(conn_id, dead_why);
    }
  }
  // Saída: fecha tudo.
  while (!impl_->conns.empty())
    CloseConn(impl_->conns.begin()->first, "shutdown");
  minecpp_poller_destroy(impl_->poller);
  minecpp_net_listener_free(impl_->listener);
  delete impl_;
  impl_ = nullptr;
}

}  // namespace minecpp::v18
