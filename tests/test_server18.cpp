// Teste do servidor 1.8 sem sockets: state machine scriptada contra o mundo
// real de test-data (level.dat + r.-1.-1.mca com chunks (-1,-12),(-3,-11)).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "minecpp/core/buf.h"
#include "minecpp/core/mqueue.h"
#include "minecpp/core/thread_pool.h"
#include "minecpp/v1_8/chunk.h"
#include "minecpp/v1_8/protocol.h"
#include "minecpp/v1_8/server.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "test-data/vanilla-1.8-flat"
#endif

#include <time.h>

static int g_fail = 0;

#define ASSERT(cond)                                         \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      g_fail++;                                              \
    }                                                        \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

using namespace minecpp::v18;
using namespace minecpp::v18::proto;

static minecpp_mqueue_t *g_tick_q = nullptr;
static minecpp_mqueue_t *g_net_q = nullptr;
static Server *g_srv = nullptr;
static uint64_t g_tick = 1;  // relógio virtual (50ms por OnTick)

template <typename T>
static std::vector<uint8_t> Enc(const T &p) {
  minecpp_writer_t w{nullptr, 0, 0, 0};
  proto::Encode(p, &w);
  size_t n = 0;
  uint8_t *b = minecpp_wr_take(&w, &n);
  std::vector<uint8_t> v(b, b + n);
  free(b);
  return v;
}

static void Feed(uint32_t conn, int32_t id, const std::vector<uint8_t> &p) {
  g_srv->OnPacket(conn, id, p.data(), p.size());
}

static void Sleep5() {
  struct timespec ts = {0, 5000000L};
  nanosleep(&ts, NULL);
}

// Responde KeepAlive (evita kick durante waits longos). Só em Play: id 0x00
// também é StatusResponse/LoginDisconnect — roteamento é por estado.
static void AckIfKeepAlive(uint32_t conn, MsgOut *m) {
  if (m->kind != MsgOut::Kind::Send || m->conn != conn ||
      m->id != kCbKeepAlive)
    return;
  Player *p = g_srv->Find(conn);
  if (!p || p->state != Player::State::Play) return;
  minecpp_reader_t r{m->payload.data(), m->payload.size()};
  KeepAlive k;
  if (Decode(k, &r)) Feed(conn, kSbKeepAlive, Enc(k));
}

// Espera Send(conn,id). autoack responde keepalives no caminho.
struct Got {
  bool found = false;
  std::vector<uint8_t> payload;
};

// Guarda não-alvo entre WaitSends (ordem de chegada varia).
static std::vector<MsgOut *> g_stash;

static void DrainIntoStash() {
  for (;;) {
    auto *m = static_cast<MsgOut *>(minecpp_mqueue_pop(g_net_q));
    if (!m) break;
    g_stash.push_back(m);
  }
}

static Got WaitSend(uint32_t conn, int32_t id, unsigned timeout_ms,
                    bool autoack = true) {
  Got g;
  for (unsigned t = 0; t < timeout_ms && !g.found; t += 5) {
    g_srv->OnTick(g_tick++);
    DrainIntoStash();
    for (auto it = g_stash.begin(); it != g_stash.end();) {
      MsgOut *m = *it;
      if (autoack) AckIfKeepAlive(conn, m);
      if (m->kind == MsgOut::Kind::Send && m->conn == conn && m->id == id &&
          !g.found) {
        g.found = true;
        g.payload = m->payload;
        delete m;
        it = g_stash.erase(it);
      } else {
        ++it;
      }
    }
    if (!g.found) Sleep5();
  }
  return g;
}

static void DrainNet() {
  DrainIntoStash();
  for (auto *m : g_stash) delete m;
  g_stash.clear();
}

static void t_login_flow() {
  g_srv->AddPlayer(7);
  Handshake h;
  h.proto = 47;
  h.host = "localhost";
  h.port = 25565;
  h.next = 2;
  Feed(7, kSbHandshake, Enc(h));
  ASSERT_EQ(g_srv->Find(7)->state, Player::State::Login);
  LoginStart ls;
  ls.name = "Minecpp";
  Feed(7, kSbLoginStart, Enc(ls));
  Player *p = g_srv->Find(7);
  ASSERT(p && p->state == Player::State::Play);
  ASSERT(p->uuid == "4ee4ebd9-458b-37d0-8c60-46e00dbe3399");
  // Ordem: SetCompression, LoginSuccess, JoinGame, ...
  Got sc = WaitSend(7, kCbLoginCompression, 500);
  ASSERT(sc.found);
  Got ok = WaitSend(7, kCbLoginSuccess, 500);
  ASSERT(ok.found);
  {
    minecpp_reader_t r{ok.payload.data(), ok.payload.size()};
    LoginSuccess d;
    ASSERT(Decode(d, &r));
    ASSERT(d.name == "Minecpp");
    ASSERT(d.uuid == p->uuid);
  }
  Got jg = WaitSend(7, kCbJoinGame, 500);
  ASSERT(jg.found);
  {
    minecpp_reader_t r{jg.payload.data(), jg.payload.size()};
    JoinGame j;
    ASSERT(Decode(j, &r));
    ASSERT(j.level_type == "flat");
  }
  Got sp = WaitSend(7, kCbSpawnPosition, 500);
  ASSERT(sp.found);
  Got pl = WaitSend(7, kCbPlayerPosLook, 500);
  ASSERT(pl.found);
  // Lote P0 na ordem vanilla (ver goldens net/goldens/_order.txt).
  Got br = WaitSend(7, kCbPlugin, 500);
  ASSERT(br.found);
  {
    minecpp_reader_t r{br.payload.data(), br.payload.size()};
    PluginMessage d;
    ASSERT(Decode(d, &r));
    ASSERT(d.channel == "MC|Brand");
  }
  Got di = WaitSend(7, kCbDifficulty, 500);
  ASSERT(di.found);
  Got ab = WaitSend(7, kCbAbilities, 500);
  ASSERT(ab.found);
  {
    minecpp_reader_t r{ab.payload.data(), ab.payload.size()};
    PlayerAbilities d;
    ASSERT(Decode(d, &r));
    ASSERT(d.fly_speed == 0.05f && d.walk_speed == 0.1f);
  }
  Got hi = WaitSend(7, kCbHeldItem, 500);
  ASSERT(hi.found);
  Got st = WaitSend(7, kCbStatistics, 500);
  ASSERT(st.found);
  Got pl1 = WaitSend(7, kCbPlayerList, 500);
  ASSERT(pl1.found);
  {
    minecpp_reader_t r{pl1.payload.data(), pl1.payload.size()};
    PlayerListItem d;
    ASSERT(Decode(d, &r));
    ASSERT_EQ(d.action, 0);
    ASSERT_EQ(d.players.size(), 1u);
    ASSERT(d.players[0].name == "Minecpp");
    ASSERT(d.players[0].uuid[0] == p->uuid_raw[0]);
  }
  Got bd = WaitSend(7, kCbBorder, 500);
  ASSERT(bd.found);
  {
    minecpp_reader_t r{bd.payload.data(), bd.payload.size()};
    WorldBorder d;
    ASSERT(Decode(d, &r));
    ASSERT(d.old_size == 6e7 && d.new_size == 6e7);
  }
  Got wi = WaitSend(7, kCbWindowItems, 500);
  ASSERT(wi.found);
  {
    minecpp_reader_t r{wi.payload.data(), wi.payload.size()};
    WindowItems d;
    ASSERT(Decode(d, &r));
    ASSERT_EQ(d.slots.size(), 45u);
  }
  Got ss = WaitSend(7, kCbSetSlot, 500);
  ASSERT(ss.found);
  DrainNet();
}

static void t_chunks_and_move() {
  // Teleporta para a área dos chunks reais e espera ChunkData.
  SbPosLook mv;
  mv.x = -8;
  mv.y = 4;
  mv.z = -184;
  Feed(7, kSbPosLook, Enc(mv));
  Got cd;
  for (unsigned t = 0; t < 5000 && !cd.found; t += 5) {
    g_srv->OnTick(g_tick++);
    for (;;) {
      auto *m = static_cast<MsgOut *>(minecpp_mqueue_pop(g_net_q));
      if (!m) break;
      AckIfKeepAlive(7, m);
      if (m->kind == MsgOut::Kind::Send && m->conn == 7 &&
          m->id == kCbChunkData && !cd.found) {
        minecpp_reader_t r{m->payload.data(), m->payload.size()};
        ChunkData d;
        if (Decode(d, &r) && d.x == -1 && d.z == -12) {
          cd.found = true;
          cd.payload = m->payload;
        }
      }
      delete m;
    }
    if (!cd.found) Sleep5();
  }
  ASSERT(cd.found);
  if (cd.found) {
    minecpp_reader_t r{cd.payload.data(), cd.payload.size()};
    ChunkData d;
    ASSERT(Decode(d, &r));
    Chunk *c = ChunkCreate(0, 0);
    ASSERT(ChunkDataApply(d, c));
    ASSERT_EQ(BlockGet(c, 0, 0, 0, nullptr), 7);  // bedrock
    ASSERT_EQ(BlockGet(c, 0, 3, 0, nullptr), 2);  // grass
    ChunkFree(c);
  }
  // Chat ecoa para o próprio jogador.
  Chat ch;
  ch.msg = "hello";
  Feed(7, kSbChat, Enc(ch));
  Got echo = WaitSend(7, kCbChat, 1000);
  ASSERT(echo.found);
  ASSERT(echo.payload.size() > 0 &&
         std::string((char *)echo.payload.data(), echo.payload.size())
                 .find("hello") != std::string::npos);
  DrainNet();
}

static void t_invalid_name() {
  g_srv->AddPlayer(8);
  Handshake h;
  h.next = 2;
  Feed(8, kSbHandshake, Enc(h));
  LoginStart ls;
  ls.name = "!!";
  Feed(8, kSbLoginStart, Enc(ls));
  ASSERT(g_srv->Find(8) == nullptr);  // kickado
  // Deve ter Disconnect (login id 0x00) + Close.
  bool disc = false, close = false;
  for (;;) {
    auto *m = static_cast<MsgOut *>(minecpp_mqueue_pop(g_net_q));
    if (!m) break;
    if (m->kind == MsgOut::Kind::Send && m->conn == 8 &&
        m->id == kCbLoginDisconnect)
      disc = true;
    if (m->kind == MsgOut::Kind::Close && m->conn == 8) close = true;
    delete m;
  }
  ASSERT(disc && close);
}

static void t_status() {
  g_srv->AddPlayer(9);
  Handshake h;
  h.next = 1;
  Feed(9, kSbHandshake, Enc(h));
  ASSERT_EQ(g_srv->Find(9)->state, Player::State::Status);
  Feed(9, kSbStatusRequest, {});
  Got rs = WaitSend(9, kCbStatusResponse, 500);
  ASSERT(rs.found);
  {
    minecpp_reader_t r{rs.payload.data(), rs.payload.size()};
    StatusResponse s;
    ASSERT(Decode(s, &r));
    ASSERT(s.json.find("\"protocol\":47") != std::string::npos);
  }
  Ping q;
  q.payload = 99;
  Feed(9, kSbStatusPing, Enc(q));
  Got po = WaitSend(9, kCbStatusPong, 500);
  ASSERT(po.found);
  DrainNet();
  g_srv->RemovePlayer(9);
}

static void t_inventory() {
  // Player novo com burst drenado; itens encenados direto na struct.
  g_srv->AddPlayer(10);
  Handshake h;
  h.next = 2;
  Feed(10, kSbHandshake, Enc(h));
  LoginStart ls;
  ls.name = "Steve";
  Feed(10, kSbLoginStart, Enc(ls));
  Player *p = g_srv->Find(10);
  ASSERT(p && p->state == Player::State::Play);
  DrainNet();

  auto stage = [&](int slot, int16_t id, uint8_t n) {
    FreeSlot(p->inv[slot]);
    p->inv[slot].id = id;
    p->inv[slot].count = n;
    p->inv[slot].damage = 0;
  };
  auto clear_all = [&] {
    for (int i = 0; i < 45; i++) p->inv[i] = Slot();
    p->cursor = Slot();
  };
  struct Res {
    bool confirm = false;
    std::vector<std::pair<int, Slot>> slots;  // window 0
    bool got_cursor = false;
    Slot cursor;
    bool spawn_object = false;
  };
  auto click = [&](int16_t slot, uint8_t button, uint8_t mode,
                   int16_t action) {
    Res r;
    ClickWindow c;
    c.window = 0;
    c.slot = slot;
    c.button = button;
    c.action = action;
    c.mode = mode;
    Feed(10, kSbClickWindow, Enc(c));
    for (;;) {
      auto *m = static_cast<MsgOut *>(minecpp_mqueue_pop(g_net_q));
      if (!m) break;
      if (m->kind == MsgOut::Kind::Send && m->conn == 10) {
        minecpp_reader_t rd{m->payload.data(), m->payload.size()};
        if (m->id == kCbConfirmTxn) {
          ConfirmTransaction cf;
          if (Decode(cf, &rd) && cf.action == action) r.confirm = cf.accepted;
        } else if (m->id == kCbSetSlot) {
          SetSlot ss;
          if (Decode(ss, &rd)) {
            if (ss.window == 0)
              r.slots.emplace_back(ss.slot, ss.item);
            else if (ss.window == -1 && ss.slot == -1) {
              r.got_cursor = true;
              r.cursor = ss.item;
            }
          }
        } else if (m->id == kCbSpawnObject) {
          r.spawn_object = true;
        }
      }
      delete m;
    }
    return r;
  };
  auto total = [&] {
    int t = p->cursor.id == -1 ? 0 : p->cursor.count;
    for (int i = 0; i < 45; i++)
      if (p->inv[i].id != -1) t += p->inv[i].count;
    return t;
  };

  // pickup + place (modo 0).
  clear_all();
  stage(10, 1, 32);
  Res r = click(10, 0, 0, 1);
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[10].id, -1);
  ASSERT_EQ(p->cursor.id, 1);
  ASSERT_EQ(p->cursor.count, 32u);
  r = click(11, 0, 0, 2);
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[11].id, 1);
  ASSERT_EQ(p->inv[11].count, 32u);
  ASSERT_EQ(p->cursor.id, -1);
  // merge + right-click.
  clear_all();
  stage(12, 3, 60);
  p->cursor.id = 3;
  p->cursor.count = 10;
  r = click(12, 0, 0, 3);
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[12].count, 64u);
  ASSERT_EQ(p->cursor.count, 6u);
  clear_all();
  stage(14, 4, 10);
  r = click(14, 1, 0, 4);
  ASSERT(r.confirm);
  ASSERT_EQ(p->cursor.count, 5u);
  ASSERT_EQ(p->inv[14].count, 5u);
  r = click(15, 1, 0, 5);
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[15].count, 1u);
  ASSERT_EQ(p->cursor.count, 4u);
  // swap.
  clear_all();
  stage(13, 265, 1);
  p->cursor.id = 266;
  p->cursor.count = 1;
  r = click(13, 0, 0, 6);
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[13].id, 266);
  ASSERT_EQ(p->cursor.id, 265);
  // shift-click hotbar -> main.
  clear_all();
  stage(36, 12, 20);
  r = click(36, 0, 1, 7);
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[36].id, -1);
  ASSERT_EQ(p->inv[9].id, 12);
  ASSERT_EQ(p->inv[9].count, 20u);
  ASSERT_EQ(total(), 20);
  // tecla numérica 3 <-> slot.
  clear_all();
  stage(16, 5, 7);
  r = click(16, 3, 2, 8);
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[16].id, -1);
  ASSERT_EQ(p->inv[39].id, 5);
  // drop cria entidade de item (P1c).
  clear_all();
  stage(17, 1, 9);
  r = click(17, 1, 4, 9);  // right click = drop entire stack
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[17].count, 0u);
  ASSERT_EQ(p->inv[17].id, -1);
  ASSERT(r.spawn_object);  // entidade de item criada (SpawnObject 0x0E)
  // drag esquerdo: 10 em 3 slots.
  clear_all();
  p->cursor.id = 20;
  p->cursor.count = 10;
  ASSERT(click(20, 0, 5, 10).confirm);
  ASSERT(click(21, 4, 5, 11).confirm);
  ASSERT(click(22, 4, 5, 12).confirm);
  r = click(22, 8, 5, 13);
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[20].count, 3u);
  ASSERT_EQ(p->inv[21].count, 3u);
  ASSERT_EQ(p->inv[22].count, 3u);
  ASSERT_EQ(p->cursor.count, 1u);
  ASSERT_EQ(total(), 10);
  // drag direito: 1 por slot.
  clear_all();
  p->cursor.id = 45;
  p->cursor.count = 5;
  ASSERT(click(23, 1, 5, 14).confirm);
  ASSERT(click(24, 5, 5, 15).confirm);
  r = click(24, 9, 5, 16);
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[23].count, 1u);
  ASSERT_EQ(p->inv[24].count, 1u);
  ASSERT_EQ(p->cursor.count, 3u);
  // double-click coleta.
  clear_all();
  p->cursor.id = 263;
  p->cursor.count = 10;
  stage(25, 263, 30);
  stage(26, 263, 64);
  r = click(25, 0, 6, 17);
  ASSERT(r.confirm);
  ASSERT_EQ(p->cursor.count, 64u);
  ASSERT_EQ(p->inv[25].id, -1);
  ASSERT_EQ(p->inv[26].count, 40u);
  // inválidos rejeitados.
  {
    ClickWindow c;
    c.window = 1;
    c.slot = 10;
    Feed(10, kSbClickWindow, Enc(c));
    bool acc = true;
    for (;;) {
      auto *m = static_cast<MsgOut *>(minecpp_mqueue_pop(g_net_q));
      if (!m) break;
      if (m->kind == MsgOut::Kind::Send && m->conn == 10 &&
          m->id == kCbConfirmTxn) {
        minecpp_reader_t rd{m->payload.data(), m->payload.size()};
        ConfirmTransaction cf;
        if (Decode(cf, &rd)) acc = cf.accepted;
      }
      delete m;
    }
    ASSERT(!acc);
  }
  r = click(99, 0, 0, 19);
  ASSERT(!r.confirm);
  r = click(0, 0, 0, 20);  // resultado do craft travado
  ASSERT(!r.confirm);
  r = click(10, 0, 7, 21);  // modo inexistente
  ASSERT(!r.confirm);
  // craft grid armazena (sem receita).
  clear_all();
  stage(10, 5, 7);
  r = click(10, 0, 0, 22);
  ASSERT(r.confirm);
  r = click(1, 0, 0, 23);
  ASSERT(r.confirm);
  ASSERT_EQ(p->inv[1].id, 5);
  // held + swing.
  {
    SbHeldItem hb;
    hb.slot = 3;
    Feed(10, kSbHeldItemSB, Enc(hb));
    ASSERT_EQ(p->selected, 3);
    hb.slot = 99;
    Feed(10, kSbHeldItemSB, Enc(hb));
    ASSERT_EQ(p->selected, 3);
    DrainNet();
  }
  g_srv->RemovePlayer(10);
  DrainNet();
}

static void t_blocks() {
  // Player novo no mundo real; teleporta e espera o chunk (-1,-12).
  g_srv->AddPlayer(11);
  Handshake h;
  h.next = 2;
  Feed(11, kSbHandshake, Enc(h));
  LoginStart ls;
  ls.name = "Digger";
  Feed(11, kSbLoginStart, Enc(ls));
  Player *p = g_srv->Find(11);
  ASSERT(p && p->state == Player::State::Play);
  DrainNet();
  SbPosLook mv;
  mv.x = -8;
  mv.y = 4;
  mv.z = -184;
  Feed(11, kSbPosLook, Enc(mv));
  bool have_chunk = false;
  for (unsigned t = 0; t < 5000 && !have_chunk; t += 5) {
    g_srv->OnTick(g_tick++);
    for (;;) {
      auto *m = static_cast<MsgOut *>(minecpp_mqueue_pop(g_net_q));
      if (!m) break;
      if (m->kind == MsgOut::Kind::Send && m->conn == 11 &&
          m->id == kCbChunkData) {
        minecpp_reader_t r{m->payload.data(), m->payload.size()};
        ChunkData d;
        if (Decode(d, &r) && d.x == -1 && d.z == -12) have_chunk = true;
      }
      delete m;
    }
    if (!have_chunk) Sleep5();
  }
  ASSERT(have_chunk);
  if (!have_chunk) return;

  auto collect = [&](std::vector<MsgOut *> *v) {
    for (;;) {
      auto *m = static_cast<MsgOut *>(minecpp_mqueue_pop(g_net_q));
      if (!m) break;
      if (m->kind == MsgOut::Kind::Send && m->conn == 11)
        v->push_back(m);
      else
        delete m;
    }
  };
  auto find_change = [&](const std::vector<MsgOut *> &v, int x, int y, int z,
                         int want_idmeta) {
    for (auto *m : v) {
      if (m->id != kCbChunkData && m->id == kCbBlockChange) {
        minecpp_reader_t r{m->payload.data(), m->payload.size()};
        BlockChangePkt b;
        if (Decode(b, &r) && b.x == x && b.y == y && b.z == z &&
            b.idmeta == want_idmeta)
          return true;
      }
    }
    return false;
  };
  auto dirt_count = [&] {
    int t = 0;
    for (int i = 0; i < 45; i++)
      if (p->inv[i].id == 3) t += p->inv[i].count;
    return t;
  };

  // Quebra grama (-8,3,-184): START + FINISH.
  {
    BlockDig d;
    d.status = 0;
    d.x = -8;
    d.y = 3;
    d.z = -184;
    d.face = 1;
    Feed(11, kSbDigging, Enc(d));
    d.status = 2;
    Feed(11, kSbDigging, Enc(d));
    std::vector<MsgOut *> v;
    collect(&v);
    ASSERT(find_change(v, -8, 3, -184, 0));
    bool effect = false, sound = false, anim = false, spawn_obj = false;
    for (auto *m : v) {
      if (m->id == kCbEffect) effect = true;
      if (m->id == kCbSound) sound = true;
      if (m->id == kCbBreakAnim) anim = true;
      if (m->id == kCbSpawnObject) spawn_obj = true;
    }
    ASSERT(effect && sound && anim);
    ASSERT(spawn_obj);  // entidade de item criada (P1c)
    for (auto *m : v) delete m;
    int32_t meta = -1;
    ASSERT_EQ(g_srv->GetWorld().GetBlock(-8, 3, -184, &meta), 0);
    // Aguarda pickup da entidade (10 ticks = 0.5s).
    for (int i = 0; i < 20 && dirt_count() == 0; i++) {
      g_srv->OnTick(g_tick++);
      Sleep5();
    }
    ASSERT(dirt_count() >= 1);  // drop grama->terra no inventário após pickup
  }
  // Bedrock rejeitada.
  {
    const int before = dirt_count();
    BlockDig d;
    d.status = 2;
    d.x = -8;
    d.y = 0;
    d.z = -184;
    Feed(11, kSbDigging, Enc(d));
    std::vector<MsgOut *> v;
    collect(&v);
    ASSERT(find_change(v, -8, 0, -184, 7 << 4));
    for (auto *m : v) delete m;
    int32_t meta = -1;
    ASSERT_EQ(g_srv->GetWorld().GetBlock(-8, 0, -184, &meta), 7);
    ASSERT_EQ(dirt_count(), before);
  }
  // Coloca terra em (-8,3,-184) mirando (-8,2,-184) face cima.
  {
    FreeSlot(p->inv[36]);
    p->inv[36].id = 3;
    p->inv[36].count = 5;
    p->selected = 0;
    BlockPlace q;
    q.x = -8;
    q.y = 2;
    q.z = -184;
    q.dir = 1;
    q.held.id = 3;
    q.held.count = 5;
    q.cx = q.cy = q.cz = 8;
    Feed(11, kSbBlockPlace, Enc(q));
    std::vector<MsgOut *> v;
    collect(&v);
    ASSERT(find_change(v, -8, 3, -184, 3 << 4));
    for (auto *m : v) delete m;
    int32_t meta = -1;
    ASSERT_EQ(g_srv->GetWorld().GetBlock(-8, 3, -184, &meta), 3);
    ASSERT_EQ(p->inv[36].count, 4u);  // consumiu 1
  }
  // Held divergente: rejeita sem mudar o mundo (destino y=4, ar).
  {
    BlockPlace q;
    q.x = -8;
    q.y = 3;
    q.z = -184;
    q.dir = 1;
    q.held.id = 1;  // pedra, mas segura terra
    q.held.count = 5;
    Feed(11, kSbBlockPlace, Enc(q));
    std::vector<MsgOut *> v;
    collect(&v);
    ASSERT(find_change(v, -8, 4, -184, 0));  // resync: continua ar
    for (auto *m : v) delete m;
    int32_t meta = -1;
    ASSERT_EQ(g_srv->GetWorld().GetBlock(-8, 4, -184, &meta), 0);
    ASSERT_EQ(p->inv[36].count, 4u);
  }
  // Drop da mão (status 3) rejeitado sem perda.
  {
    BlockDig d;
    d.status = 3;
    Feed(11, kSbDigging, Enc(d));
    std::vector<MsgOut *> v;
    collect(&v);
    bool spawn_obj = false;
    for (auto *m : v) {
      if (m->id == kCbSpawnObject) spawn_obj = true;
      delete m;
    }
    ASSERT(spawn_obj);  // entidade de item criada (P1c)
    ASSERT_EQ(p->inv[36].count, 0);  // stack inteira dropada (status 3 = Q)
    ASSERT_EQ(p->inv[36].id, -1);
  }
  // Chunk ausente: sem crash.
  {
    BlockDig d;
    d.status = 2;
    d.x = 30000;
    d.y = 3;
    d.z = 0;
    Feed(11, kSbDigging, Enc(d));
    DrainNet();
  }
  g_srv->RemovePlayer(11);
  DrainNet();
}

static void t_keepalive_timeout() {
  // Quita pendência antiga e espera um KA fresco (sem responder).
  DrainNet();
  Got ka;
  for (unsigned t = 0; t < 3000 && !ka.found; t += 5) {
    g_srv->OnTick(g_tick++);
    for (;;) {
      auto *m = static_cast<MsgOut *>(minecpp_mqueue_pop(g_net_q));
      if (!m) break;
      if (m->kind == MsgOut::Kind::Send && m->conn == 7 &&
          m->id == kCbKeepAlive && !ka.found) {
        ka.found = true;
        ka.payload = m->payload;
      }
      delete m;  // sem ack de propósito
    }
    if (!ka.found) Sleep5();
  }
  ASSERT(ka.found);
  // 620 ticks sem resposta → kick com Disconnect play 0x40.
  bool disc = false;
  for (unsigned t = 0; t < 620; t++) {
    g_srv->OnTick(g_tick++);
    for (;;) {
      auto *m = static_cast<MsgOut *>(minecpp_mqueue_pop(g_net_q));
      if (!m) break;
      if (m->kind == MsgOut::Kind::Send && m->conn == 7 &&
          m->id == kCbPlayDisconnect)
        disc = true;
      delete m;  // sem ack de propósito
    }
  }
  ASSERT(disc);
  ASSERT(g_srv->Find(7) == nullptr);
}

int main() {
  minecpp_thread_pool_init(2);
  g_tick_q = minecpp_mqueue_create();
  g_net_q = minecpp_mqueue_create();
  ServerConfig cfg;
  cfg.world_dir = TEST_DATA_DIR;
  cfg.view_distance = 2;  // 25 colunas (mundo de teste é pequeno)
  Server srv(cfg, g_tick_q, g_net_q);
  std::string err;
  ASSERT(srv.Init(&err));
  g_srv = &srv;
  t_login_flow();
  t_chunks_and_move();
  t_invalid_name();
  t_status();
  t_inventory();
  t_blocks();
  t_keepalive_timeout();
  DrainNet();
  // Ordem: workers primeiro (drena jobs; nenhum push após o join), depois
  // drena leftovers das filas (jobs que completaram no fim) e destrói.
  minecpp_thread_pool_shutdown();
  for (;;) {
    auto *m = static_cast<MsgIn *>(minecpp_mqueue_pop(g_tick_q));
    if (!m) break;
    FreeMsgIn(m);
  }
  minecpp_mqueue_destroy(g_net_q, nullptr);
  minecpp_mqueue_destroy(g_tick_q, nullptr);
  if (g_fail == 0) printf("server18: todas as verificações passaram\n");
  return g_fail ? 1 : 0;
}
