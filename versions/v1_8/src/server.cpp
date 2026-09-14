#include "minecpp/v1_8/server.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <zlib.h>

#include "minecpp/core/buf.h"
#include "minecpp/core/crypto.h"
#include "minecpp/core/nbt.h"
#include "minecpp/core/region.h"
#include "minecpp/core/thread_pool.h"
#include "minecpp/v1_8/chunk.h"
#include "minecpp/v1_8/protocol.h"

namespace minecpp::v18 {
namespace {

using proto::Chat;
using proto::Disconnect;
using proto::Handshake;
using proto::JoinGame;
using proto::KeepAlive;
using proto::LoginStart;
using proto::LoginSuccess;
using proto::Ping;
using proto::PlayerPosLook;
using proto::SbLook;
using proto::SbPosition;
using proto::SbPosLook;
using proto::SetCompression;
using proto::SpawnPosition;
using proto::StatusResponse;
using proto::TimeUpdate;

template <typename T>
std::vector<uint8_t> Enc(const T &p) {
  minecpp_writer_t w{nullptr, 0, 0, 0};
  proto::Encode(p, &w);
  size_t n = 0;
  uint8_t *b = minecpp_wr_take(&w, &n);
  std::vector<uint8_t> v(b, b + n);
  free(b);
  return v;
}

bool ValidName(const std::string &n) {
  if (n.size() < 3 || n.size() > 16) return false;
  for (char c : n)
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '_'))
      return false;
  return true;
}

int32_t FloorDiv16(double v) {
  const double d = v / 16.0;
  const int32_t c = (int32_t)d;  // trunc em direção a zero
  return (d < 0 && (double)c != d) ? c - 1 : c;
}

int32_t FloorDiv32(int32_t v) { return v >= 0 ? v / 32 : (v - 31) / 32; }

std::string JsonEscape(const std::string &s) {
  std::string o;
  for (char c : s) {
    if (c == '\\' || c == '"') {
      o += '\\';
      o += c;
    } else if (c == '\n') {
      o += "\\n";
    } else if (c == '\r') {
      o += "\\r";
    } else if (c == '\t') {
      o += "\\t";
    } else {
      o += c;
    }
  }
  return o;
}

// Inflate region payload (version 2 = zlib, 1 = gzip).
uint8_t *InflateChunk(const uint8_t *in, size_t n, uint8_t ver, size_t *out_n) {
  const int wbits[2] = {MAX_WBITS, 16 + MAX_WBITS};
  const int first = (ver == 1) ? 1 : 0;
  for (int t = 0; t < 2; t++) {
    const int wb = wbits[(first + t) % 2];
    size_t cap = n * 16 + 1024;
    uint8_t *out = (uint8_t *)malloc(cap);
    z_stream s;
    memset(&s, 0, sizeof s);
    if (!out || inflateInit2(&s, wb) != Z_OK) {
      free(out);
      continue;
    }
    s.next_in = (Bytef *)in;
    s.avail_in = (uInt)n;
    int zr = Z_OK;
    while (zr == Z_OK) {
      if (s.total_out >= cap) {
        if (cap >= (8u << 20)) {
          zr = Z_MEM_ERROR;
          break;
        }
        cap *= 2;
        uint8_t *nb = (uint8_t *)realloc(out, cap);
        if (!nb) {
          zr = Z_MEM_ERROR;
          break;
        }
        out = nb;
      }
      s.next_out = out + s.total_out;
      s.avail_out = (uInt)(cap - s.total_out);
      zr = inflate(&s, Z_NO_FLUSH);
    }
    inflateEnd(&s);
    if (zr == Z_STREAM_END) {
      *out_n = s.total_out;
      return out;
    }
    free(out);
  }
  return nullptr;
}

}  // namespace

Server::Server(const ServerConfig &cfg, minecpp_mqueue_t *tick_q,
               minecpp_mqueue_t *net_q)
    : cfg_(cfg), tick_q_(tick_q), net_q_(net_q) {}

Player::~Player() {
  for (auto &s : inv) FreeSlot(s);
  FreeSlot(cursor);
}

Entity::~Entity() { FreeSlot(stack); }

uint32_t Server::NextRand() {
  uint64_t x = rng_;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  rng_ = x;
  return (uint32_t)(x * 0x2545F4914F6CDD1Dull >> 32);
}

// ---------------------------------------------------------------- World

World::~World() = default;

Chunk *World::Get(int32_t cx, int32_t cz) const {
  const auto it = chunks_.find(ChunkKey(cx, cz));
  return it == chunks_.end() ? nullptr : it->second.get();
}

bool World::Has(int32_t cx, int32_t cz) const {
  return chunks_.count(ChunkKey(cx, cz)) != 0;
}

void World::Adopt(Chunk *c) {
  if (!c) return;
  chunks_[ChunkKey(c->x, c->z)].reset(c);
}

size_t World::Evict(const std::unordered_set<int64_t> &keep) {
  size_t n = 0;
  for (auto it = chunks_.begin();
       it != chunks_.end() && chunks_.size() > kMaxChunks;) {
    if (keep.count(it->first)) {
      ++it;
      continue;
    }
    it = chunks_.erase(it);
    n++;
  }
  return n;
}

size_t World::Size() const { return chunks_.size(); }

int32_t World::GetBlock(int x, int y, int z, int32_t *meta) const {
  if (y < 0 || y > 255) return -1;
  const int32_t cx = FloorDiv16((double)x), cz = FloorDiv16((double)z);
  const Chunk *c = Get(cx, cz);
  if (!c) return -1;
  return BlockGet(c, x - cx * 16, y, z - cz * 16, meta);
}

bool World::SetBlock(int x, int y, int z, int32_t id, int32_t meta) {
  if (y < 0 || y > 255) return false;
  const int32_t cx = FloorDiv16((double)x), cz = FloorDiv16((double)z);
  Chunk *c = Get(cx, cz);
  if (!c) return false;
  return BlockSet(c, x - cx * 16, y, z - cz * 16, id, meta) == 0;
}

bool Server::Init(std::string *err) {
  const std::string path = cfg_.world_dir + "/level.dat";
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) {
    if (err) *err = "sem level.dat: " + path;
    return false;
  }
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> gz((size_t)n);
  const bool ok =
      n > 0 && fread(gz.data(), 1, (size_t)n, f) == (size_t)n;
  fclose(f);
  if (!ok) {
    if (err) *err = "level.dat ilegível";
    return false;
  }
  size_t nn = 0;
  uint8_t *raw = InflateChunk(gz.data(), gz.size(), 1, &nn);  // gzip
  if (!raw) {
    if (err) *err = "level.dat não é gzip válido";
    return false;
  }
  minecpp_nbt_tag_t *root = nullptr;
  const bool pr =
      minecpp_nbt_parse(raw, nn, 0, &root, nullptr) == MINECPP_NBT_OK;
  free(raw);
  if (!pr || !root) {
    minecpp_nbt_free(root);
    if (err) *err = "level.dat NBT inválido";
    return false;
  }
  const minecpp_nbt_tag_t *data =
      minecpp_nbt_get(root, "Data");
  const minecpp_nbt_tag_t *sx =
      data ? minecpp_nbt_get(data, "SpawnX") : nullptr;
  const minecpp_nbt_tag_t *sy =
      data ? minecpp_nbt_get(data, "SpawnY") : nullptr;
  const minecpp_nbt_tag_t *sz =
      data ? minecpp_nbt_get(data, "SpawnZ") : nullptr;
  const minecpp_nbt_tag_t *tm = data ? minecpp_nbt_get(data, "Time") : nullptr;
  bool good = data && sx && sy && sz && sx->type == MINECPP_NBT_INT &&
              sy->type == MINECPP_NBT_INT && sz->type == MINECPP_NBT_INT;
  if (good) {
    spawn_x_ = sx->v.i32;
    spawn_y_ = sy->v.i32;
    spawn_z_ = sz->v.i32;
    if (tm && tm->type == MINECPP_NBT_LONG) world_time_ = tm->v.i64;
  }
  minecpp_nbt_free(root);
  if (!good) {
    if (err) *err = "level.dat sem Spawn (mundo inválido?)";
    return false;
  }
  return true;
}

Player *Server::AddPlayer(uint32_t conn) {
  auto p = std::make_unique<Player>();
  p->conn = conn;
  p->x = spawn_x_ + 0.5;
  p->y = spawn_y_;
  p->z = spawn_z_ + 0.5;
  Player *r = p.get();
  players_[conn] = std::move(p);
  return r;
}

void Server::RemovePlayer(uint32_t conn) {
  auto it = players_.find(conn);
  if (it == players_.end()) return;
  Player &gone = *it->second;
  // Some para os outros + limpa known sets.
  proto::DestroyEntities d;
  d.eids.push_back(gone.eid);
  const std::vector<uint8_t> b = Enc(d);
  for (const auto &kv : players_) {
    Player &q = *kv.second;
    if (q.conn == conn || q.state != Player::State::Play) continue;
    if (q.known_players.erase(conn)) Send(q.conn, proto::kCbDestroy, b.data(), b.size());
    // Entidades que só ele via: reavalia visibilidade no próximo sweep.
    for (auto &ekv : entities_) ekv.second->seen_by.erase(conn);
  }
  fprintf(stderr, "[tick remove conn=%u]\n", conn);
  fflush(stderr);
  players_.erase(it);
}

Player *Server::Find(uint32_t conn) {
  const auto it = players_.find(conn);
  return it == players_.end() ? nullptr : it->second.get();
}

size_t Server::PlayerCount() const { return players_.size(); }

void Server::Send(uint32_t conn, int32_t id, const uint8_t *p, size_t n) {
  auto *m = new (std::nothrow) MsgOut();
  if (!m) return;
  m->kind = MsgOut::Kind::Send;
  m->conn = conn;
  m->id = id;
  m->payload.assign(p, p + n);
  minecpp_mqueue_push(net_q_, m);
}

void Server::Kick(Player &p, const std::string &reason) {
  fprintf(stderr, "[tick kick conn=%u why=%s]\n", p.conn, reason.c_str());
  fflush(stderr);
  Disconnect d;
  d.reason = "{\"text\":\"" + JsonEscape(reason) + "\"}";
  const std::vector<uint8_t> b = Enc(d);
  const int32_t id = p.state == Player::State::Play
                         ? proto::kCbPlayDisconnect
                         : proto::kCbLoginDisconnect;
  Send(p.conn, id, b.data(), b.size());
  auto *m = new (std::nothrow) MsgOut();
  if (m) {
    m->kind = MsgOut::Kind::Close;
    m->conn = p.conn;
    minecpp_mqueue_push(net_q_, m);
  }
  players_.erase(p.conn);
}

std::string Server::StatusJson() const {
  char buf[512];
  snprintf(buf, sizeof buf,
           "{\"description\":{\"text\":\"%s\"},\"players\":{\"max\":%d,"
           "\"online\":%d},\"version\":{\"name\":\"1.8\",\"protocol\":47}}",
           JsonEscape(cfg_.motd).c_str(), cfg_.max_players,
           (int)players_.size());
  return buf;
}

void Server::OnPacket(uint32_t conn, int32_t id, const uint8_t *payload,
                      size_t n) {
  Player *pp = Find(conn);
  if (cfg_.verbose) {
    fprintf(stderr, "[tick pkt conn=%u id=%02x state=%d]\n", conn,
            (unsigned)id, pp ? (int)pp->state : -1);
    fflush(stderr);
  }
  if (!pp) return;
  Player &p = *pp;
  switch (p.state) {
    case Player::State::Handshake:
      if (id == proto::kSbHandshake) HandleHandshake(p, payload, n);
      break;
    case Player::State::Status:
      HandleStatus(p, id, payload, n);
      break;
    case Player::State::Login:
      HandleLogin(p, id, payload, n);
      break;
    case Player::State::Play:
      HandlePlay(p, id, payload, n);
      break;
  }
}

void Server::HandleHandshake(Player &p, const uint8_t *d, size_t n) {
  Handshake h;
  minecpp_reader_t r{d, n};
  if (!Decode(h, &r) || r.left != 0) return;  // malformado: ignora (timeout fecha)
  if (h.next == 1) {
    p.state = Player::State::Status;
  } else if (h.next == 2) {
    p.state = Player::State::Login;
  } else {
    auto *m = new (std::nothrow) MsgOut();
    if (m) {
      m->kind = MsgOut::Kind::Close;
      m->conn = p.conn;
      minecpp_mqueue_push(net_q_, m);
    }
    players_.erase(p.conn);
  }
}

void Server::HandleStatus(Player &p, int32_t id, const uint8_t *d, size_t n) {
  minecpp_reader_t r{d, n};
  if (id == proto::kSbStatusRequest) {
    StatusResponse s;
    s.json = StatusJson();
    const std::vector<uint8_t> b = Enc(s);
    Send(p.conn, proto::kCbStatusResponse, b.data(), b.size());
  } else if (id == proto::kSbStatusPing) {
    Ping q;
    if (Decode(q, &r) && r.left == 0) {
      const std::vector<uint8_t> b = Enc(q);
      Send(p.conn, proto::kCbStatusPong, b.data(), b.size());
    }
  }
}

void Server::HandleLogin(Player &p, int32_t id, const uint8_t *d, size_t n) {
  if (id != proto::kSbLoginStart) return;
  minecpp_reader_t r{d, n};
  LoginStart s;
  if (!Decode(s, &r) || r.left != 0 || !ValidName(s.name)) {
    Kick(p, "Invalid username");
    return;
  }
  for (const auto &kv : players_) {
    const Player &q = *kv.second;
    if (q.state == Player::State::Play && q.name == s.name) {
      Kick(p, "Logged in from another location");
      return;
    }
  }
  p.name = s.name;
  uint8_t raw[16];
  char ustr[37];
  if (minecpp_offline_uuid(s.name.c_str(), raw, ustr) != 0) {
    Kick(p, "Invalid username");
    return;
  }
  p.uuid = ustr;
  memcpy(p.uuid_raw, raw, 16);
  if (cfg_.compression_threshold >= 0) {
    SetCompression sc;
    sc.threshold = cfg_.compression_threshold;
    const std::vector<uint8_t> b = Enc(sc);
    Send(p.conn, proto::kCbLoginCompression, b.data(), b.size());
    auto *m = new (std::nothrow) MsgOut();
    if (m) {
      m->kind = MsgOut::Kind::EnableCompression;
      m->conn = p.conn;
      m->threshold = cfg_.compression_threshold;
      minecpp_mqueue_push(net_q_, m);
    }
  }
  LoginSuccess ok;
  ok.uuid = p.uuid;
  ok.name = p.name;
  const std::vector<uint8_t> b = Enc(ok);
  Send(p.conn, proto::kCbLoginSuccess, b.data(), b.size());
  p.state = Player::State::Play;
  OnJoin(p);
}

void Server::OnJoin(Player &p) {
  p.eid = next_eid_++;
  p.keepalive_tick = tick_;
  // Ordem espelha o vanilla (ver goldens net/goldens/_order.txt).
  JoinGame j;
  j.eid = p.eid;
  j.mode = 0;
  j.dimension = 0;
  j.difficulty = 1;
  j.max_players = (uint8_t)(cfg_.max_players > 255 ? 255 : cfg_.max_players);
  j.level_type = "flat";
  const std::vector<uint8_t> jb = Enc(j);
  Send(p.conn, proto::kCbJoinGame, jb.data(), jb.size());
  {
    proto::PluginMessage brand;
    brand.channel = "MC|Brand";
    const char tag[] = "\x07minecpp";
    brand.data.assign(tag, tag + sizeof(tag) - 1);
    const std::vector<uint8_t> b = Enc(brand);
    Send(p.conn, proto::kCbPlugin, b.data(), b.size());
  }
  {
    proto::ServerDifficulty d;
    d.difficulty = 1;
    const std::vector<uint8_t> b = Enc(d);
    Send(p.conn, proto::kCbDifficulty, b.data(), b.size());
  }
  SpawnPosition s;
  s.x = spawn_x_;
  s.y = spawn_y_;
  s.z = spawn_z_;
  const std::vector<uint8_t> sb = Enc(s);
  Send(p.conn, proto::kCbSpawnPosition, sb.data(), sb.size());
  {
    proto::PlayerAbilities a;
    const std::vector<uint8_t> b = Enc(a);
    Send(p.conn, proto::kCbAbilities, b.data(), b.size());
  }
  {
    proto::HeldItemChange h;
    const std::vector<uint8_t> b = Enc(h);
    Send(p.conn, proto::kCbHeldItem, b.data(), b.size());
  }
  {
    proto::Statistics st;
    const std::vector<uint8_t> b = Enc(st);
    Send(p.conn, proto::kCbStatistics, b.data(), b.size());
  }
  {
    // Quirk do vanilla: manda o ADD_PLAYER 2 vezes (goldens cb_38_1/2).
    proto::PlayerListItem pl;
    pl.action = 0;
    proto::PlayerListEntry e;
    memcpy(e.uuid, p.uuid_raw, 16);
    e.name = p.name;
    e.gamemode = 0;
    e.ping = 0;
    pl.players.push_back(e);
    const std::vector<uint8_t> b = Enc(pl);
    Send(p.conn, proto::kCbPlayerList, b.data(), b.size());
    Send(p.conn, proto::kCbPlayerList, b.data(), b.size());
  }
  p.x = spawn_x_ + 0.5;
  p.y = spawn_y_;
  p.z = spawn_z_ + 0.5;
  p.bsx = p.x;
  p.bsy = p.y;
  p.bsz = p.z;
  PlayerPosLook pl;
  pl.x = p.x;
  pl.y = p.y;
  pl.z = p.z;
  const std::vector<uint8_t> pb = Enc(pl);
  Send(p.conn, proto::kCbPlayerPosLook, pb.data(), pb.size());
  {
    proto::WorldBorder b;  // defaults = vanilla (golden cb_44_1)
    const std::vector<uint8_t> wb = Enc(b);
    Send(p.conn, proto::kCbBorder, wb.data(), wb.size());
  }
  TimeUpdate t;
  t.age = (int64_t)tick_;
  t.time = (world_time_ + (int64_t)tick_) % 24000;
  const std::vector<uint8_t> tb = Enc(t);
  Send(p.conn, proto::kCbTimeUpdate, tb.data(), tb.size());
  {
    proto::WindowItems wi;
    wi.window = 0;
    wi.slots = std::vector<proto::Slot>(45);  // inventário vazio
    const std::vector<uint8_t> wb = Enc(wi);
    Send(p.conn, proto::kCbWindowItems, wb.data(), wb.size());
  }
  {
    proto::SetSlot ss;  // cursor vazio (golden cb_2f_1)
    ss.window = -1;
    ss.slot = -1;
    const std::vector<uint8_t> wb = Enc(ss);
    Send(p.conn, proto::kCbSetSlot, wb.data(), wb.size());
  }
  UpdateVisibility(p);  // vê quem já está + entidades; eles te veem abaixo
  for (const auto &kv : players_) {
    if (kv.second->conn != p.conn &&
        kv.second->state == Player::State::Play)
      UpdateVisibility(*kv.second);
  }
  RequestChunks(p);
}

void Server::RequestChunks(Player &p) {
  const int32_t pcx = FloorDiv16(p.x), pcz = FloorDiv16(p.z);
  const int r = cfg_.view_distance < 1   ? 1
                : cfg_.view_distance > 8 ? 8
                                         : cfg_.view_distance;
  for (int dz = -r; dz <= r; dz++) {
    for (int dx = -r; dx <= r; dx++) {
      if (submits_this_tick_ >= 32) return;
      const int64_t k = ChunkKey(pcx + dx, pcz + dz);
      if (p.loaded.count(k) || p.inflight.count(k)) continue;
      p.inflight.insert(k);
      SubmitChunkJob(p, pcx + dx, pcz + dz);
    }
  }
}

void Server::SubmitChunkJob(Player &p, int32_t cx, int32_t cz) {
  auto *job = new (std::nothrow) ChunkJob();
  if (!job) return;
  job->region_dir = cfg_.world_dir + "/region";
  job->conn = p.conn;
  job->cx = cx;
  job->cz = cz;
  job->reply = tick_q_;
  if (minecpp_job_submit(0, ChunkJobMain, job) != 0) {
    delete job;
    p.inflight.erase(ChunkKey(cx, cz));  // tenta de novo no próximo tick
    return;
  }
  submits_this_tick_++;
}

void Server::OnChunkReady(MsgIn *m) {
  Chunk *c = m->chunk;
  m->chunk = nullptr;
  Player *pp = Find(m->conn);
  if (!pp) {
    ChunkFree(c);
    return;
  }
  Player &p = *pp;
  const int64_t k = ChunkKey(m->cx, m->cz);
  p.inflight.erase(k);
  if (!c || p.loaded.count(k)) {
    ChunkFree(c);
    return;
  }
  world_.Adopt(c);
  // Evicta o que ninguém usa (sem save ainda — restart perde). Protege o
  // recém-chegado incluindo a chave no keep.
  std::unordered_set<int64_t> keep;
  keep.insert(k);
  for (const auto &kv : players_)
    keep.insert(kv.second->loaded.begin(), kv.second->loaded.end());
  world_.Evict(keep);
  if (!world_.Has(m->cx, m->cz)) return;  // evictado na hora (raro)
  proto::ChunkData pkt;
  if (!proto::ChunkDataBuild(*world_.Get(m->cx, m->cz), true, &pkt)) return;
  minecpp_writer_t w{nullptr, 0, 0, 0};
  if (!proto::Encode(pkt, &w)) {
    free(w.buf);
    return;
  }
  Send(p.conn, proto::kCbChunkData, w.buf, w.len);
  free(w.buf);
  p.loaded.insert(k);
}

namespace {

// Broadcast player position change to other players who know this player.
// Vanilla logic: if delta < 8 blocks (256 in 1/32 fixed point), use RelMove/RelMoveLook;
// otherwise use Teleport.
void BroadcastPlayerMove(Server &srv, const Player &p, double ox, double oy,
                         double oz, float yaw, float pitch, bool on_ground) {
  const double dx = p.x - ox, dy = p.y - oy, dz = p.z - oz;
  const int32_t fdx = (int32_t)std::round(dx * 32.0);
  const int32_t fdy = (int32_t)std::round(dy * 32.0);
  const int32_t fdz = (int32_t)std::round(dz * 32.0);
  const bool small = std::abs(fdx) < 256 && std::abs(fdy) < 256 &&
                     std::abs(fdz) < 256;
  if (small) {
    if (yaw == p.yaw && pitch == p.pitch) {
      proto::EntityRelMove m;
      m.eid = p.eid;
      m.dx = (int8_t)fdx;
      m.dy = (int8_t)fdy;
      m.dz = (int8_t)fdz;
      m.on_ground = on_ground;
      minecpp_writer_t w{nullptr, 0, 0, 0};
      proto::Encode(m, &w);
      size_t n = 0;
      uint8_t *b = minecpp_wr_take(&w, &n);
      for (uint32_t conn : p.known_players) {
        Player *q = srv.Find(conn);
        if (q && q->state == Player::State::Play)
          srv.Send(q->conn, proto::kCbRelMove, b, n);
      }
      free(b);
    } else {
      proto::EntityRelMoveLook m;
      m.eid = p.eid;
      m.dx = (int8_t)fdx;
      m.dy = (int8_t)fdy;
      m.dz = (int8_t)fdz;
      m.yaw = (uint8_t)(p.yaw * 256.0f / 360.0f);
      m.pitch = (uint8_t)(p.pitch * 256.0f / 360.0f);
      m.on_ground = on_ground;
      minecpp_writer_t w{nullptr, 0, 0, 0};
      proto::Encode(m, &w);
      size_t n = 0;
      uint8_t *b = minecpp_wr_take(&w, &n);
      for (uint32_t conn : p.known_players) {
        Player *q = srv.Find(conn);
        if (q && q->state == Player::State::Play)
          srv.Send(q->conn, proto::kCbRelMoveLook, b, n);
      }
      free(b);
    }
  } else {
    proto::EntityTeleport m;
    m.eid = p.eid;
    m.x = (int32_t)std::round(p.x * 32.0);
    m.y = (int32_t)std::round(p.y * 32.0);
    m.z = (int32_t)std::round(p.z * 32.0);
    m.yaw = (uint8_t)(p.yaw * 256.0f / 360.0f);
    m.pitch = (uint8_t)(p.pitch * 256.0f / 360.0f);
    m.on_ground = on_ground;
    minecpp_writer_t w{nullptr, 0, 0, 0};
    proto::Encode(m, &w);
    size_t n = 0;
    uint8_t *b = minecpp_wr_take(&w, &n);
    for (uint32_t conn : p.known_players) {
      Player *q = srv.Find(conn);
      if (q && q->state == Player::State::Play)
        srv.Send(q->conn, proto::kCbTeleport, b, n);
    }
    free(b);
  }
}

void BroadcastPlayerLook(Server &srv, const Player &p, float oyaw, float opitch,
                         bool on_ground) {
  if (oyaw == p.yaw && opitch == p.pitch) return;
  proto::EntityLook m;
  m.eid = p.eid;
  m.yaw = (uint8_t)(p.yaw * 256.0f / 360.0f);
  m.pitch = (uint8_t)(p.pitch * 256.0f / 360.0f);
  m.on_ground = on_ground;
  minecpp_writer_t w{nullptr, 0, 0, 0};
  proto::Encode(m, &w);
  size_t n = 0;
  uint8_t *b = minecpp_wr_take(&w, &n);
  for (uint32_t conn : p.known_players) {
    Player *q = srv.Find(conn);
    if (q && q->state == Player::State::Play)
      srv.Send(q->conn, proto::kCbLook, b, n);
  }
  free(b);
}

void BroadcastPlayerMoveLook(Server &srv, const Player &p, double ox, double oy,
                             double oz, float oyaw, float opitch, bool /*og*/) {
  const double dx = p.x - ox, dy = p.y - oy, dz = p.z - oz;
  const int32_t fdx = (int32_t)std::round(dx * 32.0);
  const int32_t fdy = (int32_t)std::round(dy * 32.0);
  const int32_t fdz = (int32_t)std::round(dz * 32.0);
  const bool small = std::abs(fdx) < 256 && std::abs(fdy) < 256 &&
                     std::abs(fdz) < 256;
  if (small) {
    if (p.yaw == oyaw && p.pitch == opitch) {
      proto::EntityRelMove m;
      m.eid = p.eid;
      m.dx = (int8_t)fdx;
      m.dy = (int8_t)fdy;
      m.dz = (int8_t)fdz;
      m.on_ground = p.on_ground;
      minecpp_writer_t w{nullptr, 0, 0, 0};
      proto::Encode(m, &w);
      size_t n = 0;
      uint8_t *b = minecpp_wr_take(&w, &n);
      for (uint32_t conn : p.known_players) {
        Player *q = srv.Find(conn);
        if (q && q->state == Player::State::Play)
          srv.Send(q->conn, proto::kCbRelMove, b, n);
      }
      free(b);
    } else {
      proto::EntityRelMoveLook m;
      m.eid = p.eid;
      m.dx = (int8_t)fdx;
      m.dy = (int8_t)fdy;
      m.dz = (int8_t)fdz;
      m.yaw = (uint8_t)(p.yaw * 256.0f / 360.0f);
      m.pitch = (uint8_t)(p.pitch * 256.0f / 360.0f);
      m.on_ground = p.on_ground;
      minecpp_writer_t w{nullptr, 0, 0, 0};
      proto::Encode(m, &w);
      size_t n = 0;
      uint8_t *b = minecpp_wr_take(&w, &n);
      for (uint32_t conn : p.known_players) {
        Player *q = srv.Find(conn);
        if (q && q->state == Player::State::Play)
          srv.Send(q->conn, proto::kCbRelMoveLook, b, n);
      }
      free(b);
    }
  } else {
    proto::EntityTeleport m;
    m.eid = p.eid;
    m.x = (int32_t)std::round(p.x * 32.0);
    m.y = (int32_t)std::round(p.y * 32.0);
    m.z = (int32_t)std::round(p.z * 32.0);
    m.yaw = (uint8_t)(p.yaw * 256.0f / 360.0f);
    m.pitch = (uint8_t)(p.pitch * 256.0f / 360.0f);
    m.on_ground = p.on_ground;
    minecpp_writer_t w{nullptr, 0, 0, 0};
    proto::Encode(m, &w);
    size_t n = 0;
    uint8_t *b = minecpp_wr_take(&w, &n);
    for (uint32_t conn : p.known_players) {
      Player *q = srv.Find(conn);
      if (q && q->state == Player::State::Play)
        srv.Send(q->conn, proto::kCbTeleport, b, n);
    }
    free(b);
  }
}

}  // namespace

void Server::HandlePlay(Player &p, int32_t id, const uint8_t *d, size_t n) {
  minecpp_reader_t r{d, n};
  switch (id) {
    case proto::kSbKeepAlive: {
      KeepAlive k;
      if (Decode(k, &r) && r.left == 0 && k.id == p.keepalive_id)
        p.keepalive_pending = false;
      break;
    }
    case proto::kSbChat: {
      Chat c;
      if (Decode(c, &r, 100) && r.left == 0 && !c.msg.empty())
        BroadcastChat(p.name.empty() ? "?" : p.name, c.msg);
      break;
    }
    case proto::kSbFlying: {
      proto::Flying f;
      if (Decode(f, &r) && r.left == 0) p.on_ground = f.on_ground;
      break;
    }
    case proto::kSbPosition: {
      SbPosition q;
      if (!Decode(q, &r) || r.left != 0) break;
      if (q.x != q.x || q.y != q.y || q.z != q.z) {
        Kick(p, "Invalid position");
        break;
      }
      if (q.x > 3e7 || q.x < -3e7 || q.z > 3e7 || q.z < -3e7 ||
          q.y > 1e4 || q.y < -1e4) {
        Kick(p, "Invalid position");
        break;
      }
      const int32_t ocx = FloorDiv16(p.x), ocz = FloorDiv16(p.z);
      const double ox = p.x, oy = p.y, oz = p.z;
      const bool og = p.on_ground;
      p.x = q.x;
      p.y = q.y;
      p.z = q.z;
      p.on_ground = q.on_ground;
      BroadcastPlayerMove(*this, p, ox, oy, oz, p.yaw, p.pitch, og);
      if (FloorDiv16(p.x) != ocx || FloorDiv16(p.z) != ocz) RequestChunks(p);
      break;
    }
    case proto::kSbLook: {
      SbLook q;
      if (Decode(q, &r) && r.left == 0) {
        const float oyaw = p.yaw, opitch = p.pitch;
        const bool og = p.on_ground;
        p.yaw = q.yaw;
        p.pitch = q.pitch;
        p.on_ground = q.on_ground;
        BroadcastPlayerLook(*this, p, oyaw, opitch, og);
      }
      break;
    }
    case proto::kSbPosLook: {
      SbPosLook q;
      if (!Decode(q, &r) || r.left != 0) break;
      if (q.x != q.x || q.y != q.y || q.z != q.z) {
        Kick(p, "Invalid position");
        break;
      }
      const int32_t ocx = FloorDiv16(p.x), ocz = FloorDiv16(p.z);
      const double ox = p.x, oy = p.y, oz = p.z;
      const float oyaw = p.yaw, opitch = p.pitch;
      const bool og = p.on_ground;
      p.x = q.x;
      p.y = q.y;
      p.z = q.z;
      p.yaw = q.yaw;
      p.pitch = q.pitch;
      p.on_ground = q.on_ground;
      BroadcastPlayerMoveLook(*this, p, ox, oy, oz, oyaw, opitch, og);
      if (FloorDiv16(p.x) != ocx || FloorDiv16(p.z) != ocz) RequestChunks(p);
      break;
    }
    case proto::kSbPluginMessage:
      break;  // ignora (ex: MC|Brand)
    case proto::kSbHeldItemSB: {
      proto::SbHeldItem q;
      if (Decode(q, &r) && r.left == 0 && q.slot >= 0 && q.slot <= 8)
        p.selected = q.slot;
      break;
    }
    case proto::kSbArmAnim: {
      proto::Animation a;
      a.eid = p.eid;
      a.action = 0;
      const std::vector<uint8_t> b = Enc(a);
      for (const auto &kv : players_) {
        const Player &q = *kv.second;
        if (q.state == Player::State::Play && q.conn != p.conn)
          Send(q.conn, proto::kCbAnimation, b.data(), b.size());
      }
      break;
    }
    case proto::kSbUseEntity: {
      proto::UseEntity q;
      if (Decode(q, &r) && r.left == 0) HandleUseEntity(p, q);
      break;
    }
    case proto::kSbCloseWindow:
      break;  // só existe a janela 0
    case proto::kSbClickWindow: {
      proto::ClickWindow q;
      if (Decode(q, &r) && r.left == 0) ProcessClick(p, q);
      break;
    }
    case proto::kSbDigging: {
      proto::BlockDig q;
      if (Decode(q, &r) && r.left == 0) HandleDig(p, q);
      break;
    }
    case proto::kSbBlockPlace: {
      proto::BlockPlace q;
      if (Decode(q, &r) && r.left == 0) HandlePlace(p, q);
      break;
    }
    case proto::kSbConfirmTxn:
      break;  // eco do cliente; ignoramos
    case proto::kSbCreativeAction:
      break;  // survival: ignora (modo criativo na Fase 6)
    default:
      break;  // pacote desconhecido: ignora (contará p/ telemetria futura)
  }
}

void Server::BroadcastChat(const std::string &from, const std::string &msg) {
  Chat c;
  c.msg = "{\"text\":\"<" + JsonEscape(from) + "> " + JsonEscape(msg) + "\"}";
  const std::vector<uint8_t> b = Enc(c);
  for (const auto &kv : players_) {
    const Player &q = *kv.second;
    if (q.state == Player::State::Play)
      Send(q.conn, proto::kCbChat, b.data(), b.size());
  }
}

// ---------------------------------------------------------------- inventário
// Máquina de clicks espelhada no ContainerPlayer vanilla (janela 0).
// Limitações documentadas (Fase 4/5): sem receitas (slot 0 travado), sem
// checagem de tipo em armor (5-8), drops (modo 4/-999) rejeitados — item
// seria deletado sem entidades de drop; maxStack 64 p/ tudo (registry vem).

namespace {

using proto::Slot;

bool SlotEmpty(const Slot &s) { return s.id == -1; }

bool SlotSame(const Slot &a, const Slot &b) { return SlotsSame(a, b); }

int MaxStack(const Slot &) { return 64; }  // registry de itens na Fase 4

void SlotClear(Slot &s) { FreeSlot(s); }

// Move ownership (sem clonar NBT). dst anterior é liberado.
void SlotMove(Slot &dst, Slot &src) {
  FreeSlot(dst);
  dst.id = src.id;
  dst.count = src.count;
  dst.damage = src.damage;
  dst.nbt = src.nbt;
  src.nbt = nullptr;
  src.id = -1;
  src.count = 0;
  src.damage = 0;
}

}  // namespace

void Server::SendSlot(uint32_t conn, int slot, const proto::Slot &item) {
  proto::SetSlot ss;
  ss.window = 0;
  ss.slot = (int16_t)slot;
  ss.item = item;
  const std::vector<uint8_t> b = Enc(ss);
  Send(conn, proto::kCbSetSlot, b.data(), b.size());
}

void Server::SendCursor(uint32_t conn, const proto::Slot &cursor) {
  proto::SetSlot ss;  // cursor = janela -1, slot -1 (igual ao vanilla)
  ss.window = -1;
  ss.slot = -1;
  ss.item = cursor;
  const std::vector<uint8_t> b = Enc(ss);
  Send(conn, proto::kCbSetSlot, b.data(), b.size());
}

void Server::SendConfirm(uint32_t conn, int16_t action, bool accepted) {
  proto::ConfirmTransaction c;
  c.window = 0;
  c.action = action;
  c.accepted = accepted;
  const std::vector<uint8_t> b = Enc(c);
  Send(conn, proto::kCbConfirmTxn, b.data(), b.size());
}

void Server::ProcessClick(Player &p, const proto::ClickWindow &c) {
  bool dirtyInv[Player::kInvSize] = {};
  bool cursorDirty = false;
  bool accepted = false;

  auto resync_all = [&] {
    for (int i = 0; i < Player::kInvSize; i++)
      if (dirtyInv[i]) SendSlot(p.conn, i, p.inv[i]);
    if (cursorDirty) SendCursor(p.conn, p.cursor);
  };

  if (c.window != 0) {  // só existe a janela 0 (player inventory)
    SendConfirm(p.conn, c.action, false);
    return;
  }
  if (c.mode > 6) {
    SendConfirm(p.conn, c.action, false);
    return;
  }
  const bool outside = c.slot == -999;
  if (!outside && (c.slot < 0 || c.slot >= Player::kInvSize)) {
    SendConfirm(p.conn, c.action, false);
    return;
  }
  // Novo click cancela drag em andamento (menos continuação modo 5).
  const bool drag_cont = c.mode == 5 && p.drag_active &&
                         ((c.button == 4 && p.drag_button == 0) ||
                          (c.button == 5 && p.drag_button == 1) ||
                          (c.button == 8 && p.drag_button == 0) ||
                          (c.button == 9 && p.drag_button == 1));
  if (p.drag_active && !drag_cont) {
    for (int s : p.drag_slots) dirtyInv[s] = true;
    cursorDirty = true;
    p.drag_active = false;
    p.drag_slots.clear();
  }

  switch (c.mode) {
    case 0: {  // click normal
      if (outside || c.slot == 0) {  // -999/-slot0: drop/resultado travados
        if (!outside) dirtyInv[c.slot] = true;
        cursorDirty = true;
        break;  // accepted=false abaixo
      }
      if (c.button > 1) break;
      proto::Slot &s = p.inv[c.slot];
      if (c.button == 0) {
        if (SlotEmpty(p.cursor)) {
          SlotMove(p.cursor, s);
        } else if (SlotEmpty(s)) {
          SlotMove(s, p.cursor);
        } else if (SlotSame(s, p.cursor)) {
          const int room = MaxStack(s) - s.count;
          const int k = room < p.cursor.count ? room : p.cursor.count;
          s.count = (uint8_t)(s.count + k);
          p.cursor.count = (uint8_t)(p.cursor.count - k);
          if (!p.cursor.count) SlotClear(p.cursor);
        } else {
          std::swap(s, p.cursor);
        }
      } else {
        if (SlotEmpty(p.cursor)) {
          const int total = s.count;
          const int n = (total + 1) / 2;
          if (!CopySlot(p.cursor, s)) {
            dirtyInv[c.slot] = true;
            cursorDirty = true;
            break;  // NOMEM: rejeita sem perda
          }
          p.cursor.count = (uint8_t)n;
          s.count = (uint8_t)(total - n);
          if (!s.count) SlotClear(s);
        } else if (SlotEmpty(s)) {
          if (!CopySlot(s, p.cursor)) {
            dirtyInv[c.slot] = true;
            cursorDirty = true;
            break;
          }
          s.count = 1;
          if (--p.cursor.count == 0) SlotClear(p.cursor);
        } else if (SlotSame(s, p.cursor)) {
          if (s.count < MaxStack(s)) {
            s.count++;
            if (--p.cursor.count == 0) SlotClear(p.cursor);
          }
        } else {
          std::swap(s, p.cursor);
        }
      }
      dirtyInv[c.slot] = true;
      cursorDirty = true;
      accepted = true;
      break;
    }
    case 1: {  // shift-click: hotbar <-> main
      if (outside || c.slot == 0) {
        if (!outside) dirtyInv[c.slot] = true;
        cursorDirty = true;
        break;
      }
      proto::Slot moving;
      moving.id = -1;
      if (!CopySlot(moving, p.inv[c.slot])) {
        dirtyInv[c.slot] = true;
        cursorDirty = true;
        break;  // NOMEM: rejeita sem perda
      }
      if (SlotEmpty(moving)) {
        accepted = true;  // nada a mover
        break;
      }
      const bool to_hotbar =
          c.slot < 36;  // main/craft/armor -> hotbar, senão main
      auto try_merge = [&](int i) {
        proto::Slot &d = p.inv[i];
        if (SlotEmpty(d) || !SlotSame(d, moving)) return;
        const int room = MaxStack(d) - d.count;
        const int k = room < moving.count ? room : moving.count;
        d.count = (uint8_t)(d.count + k);
        moving.count = (uint8_t)(moving.count - k);
        dirtyInv[i] = true;
      };
      auto try_fill = [&](int i) {
        proto::Slot &d = p.inv[i];
        if (!SlotEmpty(d) || SlotEmpty(moving)) return;
        SlotMove(d, moving);
        dirtyInv[i] = true;
      };
      if (to_hotbar) {
        for (int i = 36; i < 45 && !SlotEmpty(moving); i++) try_merge(i);
        for (int i = 36; i < 45 && !SlotEmpty(moving); i++) try_fill(i);
      } else {
        for (int i = 9; i < 36 && !SlotEmpty(moving); i++) try_merge(i);
        for (int i = 9; i < 36 && !SlotEmpty(moving); i++) try_fill(i);
      }
      SlotMove(p.inv[c.slot], moving);
      dirtyInv[c.slot] = true;
      accepted = true;
      break;
    }
    case 2: {  // tecla numérica: troca com hotbar[button]
      if (outside || c.slot == 0 || c.button > 8) {
        if (!outside && c.slot >= 0 && c.slot < Player::kInvSize)
          dirtyInv[c.slot] = true;
        break;
      }
      std::swap(p.inv[c.slot], p.inv[36 + c.button]);
      dirtyInv[c.slot] = true;
      dirtyInv[36 + c.button] = true;
      accepted = true;
      break;
    }
    case 3:  // middle-click criativo: rejeita em survival
      if (!outside) dirtyInv[c.slot] = true;
      cursorDirty = true;
      break;
    case 4: {  // drop: joga no mundo (slot ou cursor)
      if (outside) {
        if (SlotEmpty(p.cursor)) {
          accepted = true;
          break;
        }
        const double yaw = p.yaw * 3.14159265 / 180.0;
        if (c.button == 0) {
          proto::Slot one;
          if (!CopySlot(one, p.cursor)) {
            cursorDirty = true;
            break;
          }
          one.count = 1;
          Entity *e = SpawnItem(p.x, p.y + 1.5, p.z, one, -sin(yaw) * 0.4, 0.2,
                                cos(yaw) * 0.4);
          FreeSlot(one);
          if (e) SendEntityTo(*e, p);
          if (--p.cursor.count == 0) SlotClear(p.cursor);
        } else {
          const double yaw = p.yaw * 3.14159265 / 180.0;
          Entity *e = SpawnItem(p.x, p.y + 1.5, p.z, p.cursor, -sin(yaw) * 0.4, 0.2,
                                cos(yaw) * 0.4);
          if (e) SendEntityTo(*e, p);
          SlotClear(p.cursor);
        }
        cursorDirty = true;
        accepted = true;
        break;
      }
      proto::Slot &s = p.inv[c.slot];
      if (SlotEmpty(s)) {
        accepted = true;
        break;
      }
      const double yaw = p.yaw * 3.14159265 / 180.0;
      if (c.button == 0) {
        proto::Slot one;
        if (!CopySlot(one, s)) {
          dirtyInv[c.slot] = true;
          break;
        }
        one.count = 1;
        Entity *e = SpawnItem(p.x, p.y + 1.5, p.z, one, -sin(yaw) * 0.4, 0.2,
                              cos(yaw) * 0.4);
        FreeSlot(one);
        if (e) SendEntityTo(*e, p);
        if (--s.count == 0) SlotClear(s);
      } else {
        Entity *e = SpawnItem(p.x, p.y + 1.5, p.z, s, -sin(yaw) * 0.4, 0.2,
                              cos(yaw) * 0.4);
        if (e) SendEntityTo(*e, p);
        SlotClear(s);
      }
      dirtyInv[c.slot] = true;
      accepted = true;
      break;
    }
    case 5: {  // drag/paint: início E fim entram (gameplay vanilla)
      const bool left = c.button == 0 || c.button == 4 || c.button == 8;
      const bool right = c.button == 1 || c.button == 5 || c.button == 9;
      const bool start = c.button == 0 || c.button == 1;
      const bool add = c.button == 4 || c.button == 5;
      const bool end = c.button == 8 || c.button == 9;
      auto drag_add = [&](int slot) {
        if (slot < 0 || slot >= Player::kInvSize) return;
        for (int s : p.drag_slots)
          if (s == slot) return;  // dup: ignora (cliente reentra slots)
        if (p.drag_slots.size() < (size_t)Player::kInvSize)
          p.drag_slots.push_back(slot);
      };
      if ((!left && !right) || SlotEmpty(p.cursor)) {
        if (p.drag_active) {
          for (int s : p.drag_slots) dirtyInv[s] = true;
          p.drag_active = false;
          p.drag_slots.clear();
        }
        if (!outside) dirtyInv[c.slot] = true;
        cursorDirty = true;
        break;  // rejeitado
      }
      if (start) {
        if (outside) {
          cursorDirty = true;
          break;
        }
        p.drag_active = true;
        p.drag_button = left ? 0 : 1;
        p.drag_slots.clear();
        drag_add(c.slot);
        accepted = true;
        break;
      }
      if (!p.drag_active ||
          (left && p.drag_button != 0) || (right && p.drag_button != 1)) {
        if (!outside) dirtyInv[c.slot] = true;
        cursorDirty = true;
        break;
      }
      if (add) {
        if (!outside) drag_add(c.slot);  // fora: ignora, drag continua
        accepted = true;
        break;
      }
      if (end) {
        if (!outside) drag_add(c.slot);
        // Valida alvos contra o estado atual.
        bool ok = true;
        for (int s : p.drag_slots) {
          const proto::Slot &d = p.inv[s];
          if (!SlotEmpty(d) && !SlotSame(d, p.cursor)) ok = false;
        }
        if (ok && !p.drag_slots.empty()) {
          const size_t n = p.drag_slots.size();
          if (p.drag_button == 0) {
            const int per = p.cursor.count / (int)n;
            if (per > 0) {
              for (int s : p.drag_slots) {
                proto::Slot &d = p.inv[s];
                if (SlotEmpty(d)) {
                  if (!CopySlot(d, p.cursor)) {
                    ok = false;
                    break;
                  }
                  d.count = 0;
                }
                d.count = (uint8_t)(d.count + per);
                dirtyInv[s] = true;
              }
              p.cursor.count = (uint8_t)(p.cursor.count - per * (int)n);
              if (!p.cursor.count) SlotClear(p.cursor);
              cursorDirty = true;
            }
          } else {
            for (int s : p.drag_slots) {
              if (SlotEmpty(p.cursor)) break;
              proto::Slot &d = p.inv[s];
              if (SlotEmpty(d)) {
                if (!CopySlot(d, p.cursor)) {
                  ok = false;
                  break;
                }
                d.count = 0;
              }
              if (d.count >= MaxStack(d)) {
                ok = false;
                break;
              }
              d.count++;
              p.cursor.count--;
              dirtyInv[s] = true;
            }
            if (!p.cursor.count) SlotClear(p.cursor);
            cursorDirty = true;
          }
        }
        if (!ok) {
          for (int s : p.drag_slots) dirtyInv[s] = true;
          cursorDirty = true;
        } else {
          accepted = true;
        }
        p.drag_active = false;
        p.drag_slots.clear();
        break;
      }
      cursorDirty = true;
      break;
    }
    case 6: {  // double-click: coleta tipo do cursor (slots 1..44)
      if (SlotEmpty(p.cursor)) {
        accepted = true;
        break;
      }
      for (int i = 1; i < Player::kInvSize && p.cursor.count < MaxStack(p.cursor); i++) {
        proto::Slot &d = p.inv[i];
        if (SlotEmpty(d) || !SlotSame(d, p.cursor)) continue;
        const int room = MaxStack(p.cursor) - p.cursor.count;
        const int k = room < d.count ? room : d.count;
        p.cursor.count = (uint8_t)(p.cursor.count + k);
        d.count = (uint8_t)(d.count - k);
        if (!d.count) SlotClear(d);
        dirtyInv[i] = true;
      }
      cursorDirty = true;
      accepted = true;
      break;
    }
  }

  SendConfirm(p.conn, c.action, accepted);
  resync_all();  // aceito ou não, o cliente fica com a verdade do servidor
}

void FreeMsgIn(MsgIn *m) {
  if (!m) return;
  if (m->kind == MsgIn::Kind::ChunkReady) ChunkFree(m->chunk);
  delete m;
}

// ---------------------------------------------------------------- blocos
// P1b: quebrar/colocar com verdade server-side (World). Simplificações
// honestas (registry completo na Fase 4, entidades na 5):
// - timers de escavação confiados do cliente (anti-cheat pós-paridade);
// - drops vão direto pro inventário (sem entidade de drop ainda);
// - tabela de drops/s sons mínima abaixo; flow de fluidos/redstone na Fase 4.

namespace {

bool InBounds(int y) { return y >= 0 && y <= 255; }

bool Reachable(double px, double py, double pz, int x, int y, int z) {
  const double dx = (x + 0.5) - px, dy = (y + 0.5) - (py + 1.62),
               dz = (z + 0.5) - pz;
  return dx * dx + dy * dy + dz * dz <= 36.0;  // 6 blocos
}

bool Unbreakable(int32_t id) { return id == 7; }  // bedrock (+Fase 4)

bool Replaceable(int32_t id) {
  switch (id) {
    case 0:
    case 8:
    case 9:
    case 10:
    case 11:  // ar + fluidos
    case 6:
    case 31:
    case 37:
    case 38:
    case 39:
    case 40:
    case 59:  // plantas
      return true;
    default:
      return false;
  }
}

struct Drop {
  int32_t id, meta;
};

// Tabela mínima de drops 1.8 (determinística; chance/RNG na Fase 4/5).
Drop BlockDrop(int32_t id, int32_t meta) {
  switch (id) {
    case 0:
    case 8:
    case 9:
    case 10:
    case 11:
    case 18:  // leaves (muda→nada por enquanto)
    case 20:  // glass
      return {-1, 0};
    case 1:
      return {4, 0};  // stone -> cobble
    case 2:
      return {3, 0};  // grass -> dirt
    case 16:
      return {263, 0};  // coal ore
    case 21:
      return {351, 4};  // lapis
    case 56:
      return {264, 0};  // diamond
    case 73:
    case 74:
      return {331, 0};  // redstone
    case 129:
      return {388, 0};  // emerald
    case 153:
      return {406, 0};  // quartz
    case 103:
      return {360, 0};  // melon (fixo 4? ver abaixo)
    default:
      return {id, meta};
  }
}

int DropCount(int32_t id) { return id == 103 ? 4 : 1; }

const char *DigSound(int32_t id) {
  switch (id) {
    case 2:
    case 31:
    case 59:
    case 60:
    case 83:
    case 141:
    case 142:
      return "dig.grass";
    case 5:
    case 17:
    case 53:
    case 58:
    case 64:
    case 65:
    case 85:
    case 86:
    case 96:
    case 107:
    case 134:
    case 135:
    case 136:
    case 162:
    case 163:
    case 164:
    case 183:
    case 184:
    case 185:
    case 186:
    case 187:
      return "dig.wood";
    case 12:
    case 78:
    case 80:
      return "dig.sand";
    case 13:
    case 82:
      return "dig.gravel";
    case 20:
    case 102:
      return "dig.glass";
    case 35:
    case 171:
      return "dig.cloth";
    default:
      return "dig.stone";
  }
}

void FaceOffset(uint8_t face, int &dx, int &dy, int &dz) {
  dx = dy = dz = 0;
  switch (face) {
    case 0: dy = -1; break;
    case 1: dy = 1; break;
    case 2: dz = -1; break;
    case 3: dz = 1; break;
    case 4: dx = -1; break;
    case 5: dx = 1; break;
    default: break;
  }
}

}  // namespace

void Server::SendToLoaded(int32_t cx, int32_t cz, int32_t id, const uint8_t *p,
                          size_t n, uint32_t skip) {
  const int64_t k = ChunkKey(cx, cz);
  for (const auto &kv : players_) {
    const Player &q = *kv.second;
    if (q.state != Player::State::Play) continue;
    if (q.conn == skip && skip != 0) continue;
    if (!q.loaded.count(k)) continue;
    Send(q.conn, id, p, n);
  }
}

void Server::ResyncBlock(int x, int y, int z) {
  int32_t meta = 0;
  const int32_t id = world_.GetBlock(x, y, z, &meta);
  if (id < 0) return;
  proto::BlockChangePkt b;
  b.x = x;
  b.y = y;
  b.z = z;
  b.idmeta = (id << 4) | (meta & 15);
  const std::vector<uint8_t> bytes = Enc(b);
  SendToLoaded(FloorDiv16((double)x), FloorDiv16((double)z),
               proto::kCbBlockChange, bytes.data(), bytes.size());
}

int Server::InventoryAdd(Player &p, const proto::Slot &stack) {
  if (stack.id == -1 || stack.count == 0) return 0;
  int left = stack.count;
  // 1) merge em stacks do mesmo tipo (9..44).
  for (int i = 9; i < 45 && left > 0; i++) {
    proto::Slot &d = p.inv[i];
    if (d.id == -1 || !SlotsSame(d, stack)) continue;
    const int room = MaxStack(d) - d.count;
    const int k = room < left ? room : left;
    if (k <= 0) continue;
    d.count = (uint8_t)(d.count + k);
    left -= k;
    SendSlot(p.conn, i, d);
  }
  // 2) slots vazios.
  for (int i = 9; i < 45 && left > 0; i++) {
    proto::Slot &d = p.inv[i];
    if (d.id != -1) continue;
    if (!CopySlot(d, stack)) break;
    const int k = MaxStack(d) < left ? MaxStack(d) : left;
    d.count = (uint8_t)k;
    left -= k;
    SendSlot(p.conn, i, d);
  }
  return left;  // >0 = não coube (deletado; entidades na Fase 5)
}

void Server::HandleDig(Player &p, const proto::BlockDig &d) {
  if (!InBounds(d.y) || d.face > 5) return;
  if (d.status == 3 || d.status == 4) {
    // Q (drop 1) / Ctrl+Q (drop stack): joga a mão no mundo.
    proto::Slot &held = p.inv[36 + p.selected];
    if (held.id == -1 || held.count == 0) return;
    const int n = d.status == 4 ? 1 : held.count;
    proto::Slot st;
    if (!CopySlot(st, held)) return;
    st.count = (uint8_t)(n < held.count ? n : held.count);
    held.count = (uint8_t)(held.count - st.count);
    if (!held.count) FreeSlot(held);
    SendSlot(p.conn, 36 + p.selected, held);
    const double yaw = p.yaw * 3.14159265 / 180.0;
    Entity *e = SpawnItem(p.x, p.y + 1.5, p.z, st, -sin(yaw) * 0.4, 0.2, cos(yaw) * 0.4);
    if (e) SendEntityTo(*e, p);
    FreeSlot(st);
    return;
  }
  if (d.status > 5) return;  // comer/atirar: Fase 5/6
  if (d.status == 1) {  // cancelou
    if (p.digging && p.dig_x == d.x && p.dig_y == d.y && p.dig_z == d.z) {
      proto::BreakAnim a;
      a.eid = p.eid;
      a.x = d.x;
      a.y = d.y;
      a.z = d.z;
      a.stage = -1;
      const std::vector<uint8_t> b = Enc(a);
      SendToLoaded(FloorDiv16((double)d.x), FloorDiv16((double)d.z),
                   proto::kCbBreakAnim, b.data(), b.size());
    }
    p.digging = false;
    return;
  }
  // status 0 (start) e 2 (finish).
  int32_t meta = 0;
  const int32_t id = world_.GetBlock(d.x, d.y, d.z, &meta);
  if (id < 0) return;  // chunk ausente: ignora
  if (id == 0 || Unbreakable(id) ||
      !Reachable(p.x, p.y, p.z, d.x, d.y, d.z)) {
    ResyncBlock(d.x, d.y, d.z);
    return;
  }
  if (d.status == 0) {
    p.digging = true;
    p.dig_x = d.x;
    p.dig_y = d.y;
    p.dig_z = d.z;
    proto::BreakAnim a;
    a.eid = p.eid;
    a.x = d.x;
    a.y = d.y;
    a.z = d.z;
    a.stage = 0;
    const std::vector<uint8_t> b = Enc(a);
    SendToLoaded(FloorDiv16((double)d.x), FloorDiv16((double)d.z),
                 proto::kCbBreakAnim, b.data(), b.size());
    return;
  }
  // finish: timers confiados do cliente (anti-cheat pós-paridade).
  p.digging = false;
  world_.SetBlock(d.x, d.y, d.z, 0, 0);
  const Drop drop = BlockDrop(id, meta);
  if (drop.id >= 0) {
    proto::Slot st;
    st.id = (int16_t)drop.id;
    st.count = (uint8_t)DropCount(drop.id);
    st.damage = (int16_t)drop.meta;
    const double jx = ((int64_t)NextRand() % 200 - 100) / 1000.0;
    const double jz = ((int64_t)NextRand() % 200 - 100) / 1000.0;
    Entity *e = SpawnItem(d.x + 0.5, d.y + 0.5, d.z + 0.5, st, jx, 0.25, jz);
    if (e) SendEntityTo(*e, p);
  }
  proto::BlockChangePkt bc;
  bc.x = d.x;
  bc.y = d.y;
  bc.z = d.z;
  bc.idmeta = 0;
  {
    const std::vector<uint8_t> b = Enc(bc);
    SendToLoaded(FloorDiv16((double)d.x), FloorDiv16((double)d.z),
                 proto::kCbBlockChange, b.data(), b.size());
  }
  {
    proto::BreakAnim a;
    a.eid = p.eid;
    a.x = d.x;
    a.y = d.y;
    a.z = d.z;
    a.stage = -1;
    const std::vector<uint8_t> b = Enc(a);
    SendToLoaded(FloorDiv16((double)d.x), FloorDiv16((double)d.z),
                 proto::kCbBreakAnim, b.data(), b.size());
  }
  {
    proto::Effect e;
    e.id = 2001;
    e.x = d.x;
    e.y = d.y;
    e.z = d.z;
    e.data = id;
    const std::vector<uint8_t> b = Enc(e);
    SendToLoaded(FloorDiv16((double)d.x), FloorDiv16((double)d.z),
                 proto::kCbEffect, b.data(), b.size());
  }
  {
    proto::SoundEffect s;
    s.name = DigSound(id);
    s.x = d.x * 8;
    s.y = d.y * 8;
    s.z = d.z * 8;
    s.volume = 1;
    s.pitch = 1;
    const std::vector<uint8_t> b = Enc(s);
    SendToLoaded(FloorDiv16((double)d.x), FloorDiv16((double)d.z),
                 proto::kCbSound, b.data(), b.size());
  }
}

void Server::HandlePlace(Player &p, const proto::BlockPlace &d) {
  if (d.dir == 255) return;  // usar item no ar (comer/arco: Fase 6)
  if (d.dir > 5 || !InBounds(d.y)) return;
  int32_t meta = 0;
  const int32_t target = world_.GetBlock(d.x, d.y, d.z, &meta);
  if (target < 0 || target == 0) return;  // sem alvo: ignora
  int dx = 0, dy = 0, dz = 0;
  FaceOffset(d.dir, dx, dy, dz);
  const int px = d.x + dx, py = d.y + dy, pz = d.z + dz;
  if (!InBounds(py)) return;
  int32_t pmeta = 0;
  const int32_t cur = world_.GetBlock(px, py, pz, &pmeta);
  if (cur < 0 || !Replaceable(cur)) {
    ResyncBlock(px, py, pz);
    return;
  }
  if (!Reachable(p.x, p.y, p.z, px, py, pz)) {
    ResyncBlock(px, py, pz);
    return;
  }
  // Item na mão tem que bater com o pacote (anti-cheat leve).
  proto::Slot &held = p.inv[36 + p.selected];
  if (held.id == -1 || held.id != d.held.id || held.damage != d.held.damage ||
      held.count == 0 || !SlotsSame(held, d.held)) {
    SendSlot(p.conn, 36 + p.selected, held);
    ResyncBlock(px, py, pz);
    return;
  }
  if (held.id < 0 || held.id > 255) {
    SendSlot(p.conn, 36 + p.selected, held);  // não-bloco: ignora
    return;
  }
  world_.SetBlock(px, py, pz, held.id, held.damage);
  if (--held.count == 0) FreeSlot(held);
  SendSlot(p.conn, 36 + p.selected, held);
  proto::BlockChangePkt bc;
  bc.x = px;
  bc.y = py;
  bc.z = pz;
  bc.idmeta = (held.id << 4) | (held.damage & 15);
  // Cuidado: held pode ter esvaziado; usa valores aplicados.
  {
    int32_t m2 = 0;
    const int32_t placed = world_.GetBlock(px, py, pz, &m2);
    bc.idmeta = (placed << 4) | (m2 & 15);
  }
  const std::vector<uint8_t> b = Enc(bc);
SendToLoaded(FloorDiv16((double)px), FloorDiv16((double)pz),
                 proto::kCbBlockChange, b.data(), b.size());
}

void Server::HandleUseEntity(Player &p, const proto::UseEntity &u) {
  // Find target entity.
  auto it = entities_.find(u.target);
  if (it == entities_.end()) return;
  Entity &e = *it->second;
  if (e.dead) return;
  // Check reach.
  const double dx = e.x - p.x, dy = (e.y + 0.5) - (p.y + 1.62), dz = e.z - p.z;
  if (dx * dx + dy * dy + dz * dz > 36.0) return;  // 6 blocks
  switch (u.action) {
    case 0: {  // Interact (right-click)
      // For now, just swing arm.
      proto::Animation a;
      a.eid = p.eid;
      a.action = 0;
      const std::vector<uint8_t> b = Enc(a);
      for (const auto &kv : players_) {
        const Player &q = *kv.second;
        if (q.state == Player::State::Play && q.conn != p.conn)
          Send(q.conn, proto::kCbAnimation, b.data(), b.size());
      }
      break;
    }
    case 1: {  // Attack (left-click)
      // Swing arm.
      proto::Animation a;
      a.eid = p.eid;
      a.action = 0;
      const std::vector<uint8_t> b = Enc(a);
      for (const auto &kv : players_) {
        const Player &q = *kv.second;
        if (q.state == Player::State::Play && q.conn != p.conn)
          Send(q.conn, proto::kCbAnimation, b.data(), b.size());
      }
      // Knockback velocity.
      if (e.kind == Entity::Kind::Mob) {
        const double kb = 0.4;
        const double adx = e.x - p.x, adz = e.z - p.z;
        const double dist = std::sqrt(adx * adx + adz * adz);
        if (dist > 0) {
          e.vx += (adx / dist) * kb;
          e.vz += (adz / dist) * kb;
          e.vy = 0.2;
          // Send velocity to viewers.
          proto::Velocity v;
          v.eid = e.eid;
          v.vx = (int16_t)std::round(e.vx * 8000.0);
          v.vy = (int16_t)std::round(e.vy * 8000.0);
          v.vz = (int16_t)std::round(e.vz * 8000.0);
          minecpp_writer_t w{nullptr, 0, 0, 0};
          proto::Encode(v, &w);
          size_t n = 0;
          uint8_t *buf = minecpp_wr_take(&w, &n);
          for (uint32_t conn : e.seen_by) Send(conn, proto::kCbVelocity, buf, n);
          free(buf);
        }
      }
      // Hurt animation (entity status 2 = hurt).
      proto::EntityStatus es;
      es.eid = e.eid;
      es.status = 2;
      minecpp_writer_t w2{nullptr, 0, 0, 0};
      proto::Encode(es, &w2);
      size_t n2 = 0;
      uint8_t *buf2 = minecpp_wr_take(&w2, &n2);
      for (uint32_t conn : e.seen_by) Send(conn, proto::kCbEntityStatus, buf2, n2);
      free(buf2);
      break;
    }
    case 2: {  // Interact at (right-click with coordinates)
      // For future use (e.g., villager trading, entity interaction).
      break;
    }
  }
}

// ---- Entity broadcast helpers (P1c) ----

void Server::SendEntityMetadata(const Entity &e) {
  proto::Metadata md = (e.kind == Entity::Kind::Mob) ? MobMetadata(e) : proto::Metadata();
  if (md.entries.empty()) return;
  minecpp_writer_t w{nullptr, 0, 0, 0};
  minecpp_wr_varint(&w, e.eid);
  proto::Encode(md, &w);
  size_t n = 0;
  uint8_t *buf = minecpp_wr_take(&w, &n);
  for (uint32_t conn : e.seen_by) Send(conn, proto::kCbMetadata, buf, n);
  free(buf);
}

void Server::SendEntityVelocity(const Entity &e) {
  proto::Velocity v;
  v.eid = e.eid;
  v.vx = (int16_t)std::round(e.vx * 8000.0);
  v.vy = (int16_t)std::round(e.vy * 8000.0);
  v.vz = (int16_t)std::round(e.vz * 8000.0);
  minecpp_writer_t w{nullptr, 0, 0, 0};
  proto::Encode(v, &w);
  size_t n = 0;
  uint8_t *buf = minecpp_wr_take(&w, &n);
  for (uint32_t conn : e.seen_by) Send(conn, proto::kCbVelocity, buf, n);
  free(buf);
}

void Server::SendEntityEquipment(const Entity &e, int32_t slot, const proto::Slot &item) {
  proto::Equipment eq;
  eq.eid = e.eid;
  eq.slot = slot;
  eq.item = item;
  minecpp_writer_t w{nullptr, 0, 0, 0};
  proto::Encode(eq, &w);
  size_t n = 0;
  uint8_t *buf = minecpp_wr_take(&w, &n);
  for (uint32_t conn : e.seen_by) Send(conn, proto::kCbEquipment, buf, n);
  free(buf);
}

void Server::SendEntityStatus(const Entity &e, uint8_t status) {
  proto::EntityStatus es;
  es.eid = e.eid;
  es.status = status;
  minecpp_writer_t w{nullptr, 0, 0, 0};
  proto::Encode(es, &w);
  size_t n = 0;
  uint8_t *buf = minecpp_wr_take(&w, &n);
  for (uint32_t conn : e.seen_by) Send(conn, proto::kCbEntityStatus, buf, n);
  free(buf);
}

void Server::SendPlayerHeadLook(const Player &p) {
  proto::HeadLook h;
  h.eid = p.eid;
  h.head = (uint8_t)(p.yaw * 256.0f / 360.0f);
  minecpp_writer_t w{nullptr, 0, 0, 0};
  proto::Encode(h, &w);
  size_t n = 0;
  uint8_t *buf = minecpp_wr_take(&w, &n);
  for (uint32_t conn : p.known_players) {
    Player *q = Find(conn);
    if (q && q->state == Player::State::Play) Send(q->conn, proto::kCbHeadLook, buf, n);
  }
  free(buf);
}

// ---------------------------------------------------------------- entidades
// P1c: rede + tracking + física de drops + spawn passivo. Sem IA (Fase 5):
// mobs parados; itens com física; players visíveis entre si.

namespace {

void MetaByte(proto::Metadata &m, uint8_t idx, int v) {
  proto::MetaEntry e;
  e.index = idx;
  e.type = 0;
  e.i = v;
  m.entries.push_back(e);
}

void MetaShort(proto::Metadata &m, uint8_t idx, int v) {
  proto::MetaEntry e;
  e.index = idx;
  e.type = 1;
  e.i = v;
  m.entries.push_back(e);
}

void MetaInt(proto::Metadata &m, uint8_t idx, int32_t v) {
  proto::MetaEntry e;
  e.index = idx;
  e.type = 2;
  e.i = v;
  m.entries.push_back(e);
}

void MetaFloat(proto::Metadata &m, uint8_t idx, double v) {
  proto::MetaEntry e;
  e.index = idx;
  e.type = 3;
  e.f = v;
  m.entries.push_back(e);
}

void MetaString(proto::Metadata &m, uint8_t idx, const std::string &v) {
  proto::MetaEntry e;
  e.index = idx;
  e.type = 4;
  e.s = v;
  m.entries.push_back(e);
}

double MobHealth(uint8_t type) {
  switch (type) {
    case 91:
      return 8;  // sheep
    case 93:
      return 4;  // chicken
    case 55:
      return 16;  // slime (size 4 goldens)
    default:
      return 10;  // pig, cow
  }
}

}  // namespace

proto::Metadata Server::MobMetadata(const Entity &e) const {
  proto::Metadata m;
  MetaByte(m, 0, 0);
  MetaShort(m, 1, 300);
  MetaString(m, 2, "");
  MetaByte(m, 3, 0);
  MetaByte(m, 4, 0);
  MetaFloat(m, 6, e.health);
  MetaInt(m, 7, 0);
  MetaByte(m, 8, 0);
  MetaByte(m, 9, 0);
  MetaByte(m, 15, 0);
  if (e.mob_type == 90 || e.mob_type == 91) MetaByte(m, 16, 0);  // saddle/cor
  if (e.mob_type == 55) MetaByte(m, 16, 4);  // slime size (golden)
  if (e.mob_type == 90 || e.mob_type == 91 || e.mob_type == 92 ||
      e.mob_type == 93)
    MetaByte(m, 12, 0);  // ageable adulto
  return m;
}

proto::Metadata Server::PlayerMetadata() const {
  proto::Metadata m;
  MetaByte(m, 0, 0);
  MetaShort(m, 1, 300);
  MetaString(m, 2, "");
  MetaByte(m, 3, 0);
  MetaByte(m, 4, 0);
  MetaFloat(m, 6, 20.0);
  MetaInt(m, 7, 0);
  MetaByte(m, 8, 0);
  MetaByte(m, 9, 0);
  return m;
}

Entity *Server::SpawnMob(uint8_t type, double x, double y, double z,
                         double health) {
  if (entities_.size() >= 256) return nullptr;
  auto e = std::make_unique<Entity>();
  e->eid = NextEid();
  e->kind = Entity::Kind::Mob;
  e->mob_type = type;
  e->health = health;
  e->x = x;
  e->y = y;
  e->z = z;
  e->sx = x;
  e->sy = y;
  e->sz = z;
  Entity *r = e.get();
  entities_[r->eid] = std::move(e);
  // Anuncia p/ quem tem a coluna.
  const int32_t cx = FloorDiv16(x), cz = FloorDiv16(z);
  const int64_t k = ChunkKey(cx, cz);
  for (const auto &kv : players_) {
    Player &q = *kv.second;
    if (q.state != Player::State::Play || !q.loaded.count(k)) continue;
    SendEntityTo(*r, q);
  }
  return r;
}

Entity *Server::SpawnItem(double x, double y, double z,
                          const proto::Slot &stack, double vx, double vy,
                          double vz) {
  if (stack.id == -1 || stack.count == 0) return nullptr;
  if (entities_.size() >= 512) return nullptr;
  auto e = std::make_unique<Entity>();
  e->eid = NextEid();
  e->kind = Entity::Kind::Item;
  if (!CopySlot(e->stack, stack)) return nullptr;
  e->x = x;
  e->y = y;
  e->z = z;
  e->vx = vx;
  e->vy = vy;
  e->vz = vz;
  e->sx = x;
  e->sy = y;
  e->sz = z;
  Entity *r = e.get();
  entities_[r->eid] = std::move(e);
  const int32_t cx = FloorDiv16(x), cz = FloorDiv16(z);
  const int64_t k = ChunkKey(cx, cz);
  for (const auto &kv : players_) {
    Player &q = *kv.second;
    if (q.state != Player::State::Play || !q.loaded.count(k)) continue;
    SendEntityTo(*r, q);
  }
  return r;
}

void Server::DestroyEntity(Entity &e) {
  proto::DestroyEntities d;
  d.eids.push_back(e.eid);
  const std::vector<uint8_t> b = Enc(d);
  for (uint32_t conn : e.seen_by) Send(conn, proto::kCbDestroy, b.data(), b.size());
  entities_.erase(e.eid);  // callers reiniciam iteração (ver EntityTick)
}

void Server::SendEntityTo(Entity &e, Player &viewer) {
  if (e.kind == Entity::Kind::Mob) {
    proto::SpawnMob s;
    s.eid = e.eid;
    s.type = e.mob_type;
    s.x = (int32_t)(e.x * 32.0);
    s.y = (int32_t)(e.y * 32.0);
    s.z = (int32_t)(e.z * 32.0);
    s.meta = MobMetadata(e);
    const std::vector<uint8_t> b = Enc(s);
    Send(viewer.conn, proto::kCbSpawnMob, b.data(), b.size());
    proto::Properties pr;
    pr.eid = e.eid;
    proto::Property hp;
    hp.key = "generic.maxHealth";
    hp.value = e.health;
    pr.props.push_back(hp);
    proto::Property sp;
    sp.key = "generic.movementSpeed";
    sp.value = 0.25;
    pr.props.push_back(sp);
    const std::vector<uint8_t> b2 = Enc(pr);
    Send(viewer.conn, proto::kCbProperties, b2.data(), b2.size());
  } else {
    proto::SpawnObject s;
    s.eid = e.eid;
    s.type = 2;
    s.x = (int32_t)(e.x * 32.0);
    s.y = (int32_t)(e.y * 32.0);
    s.z = (int32_t)(e.z * 32.0);
    s.data = 1;
    s.has_vel = true;
    s.vx = (int16_t)(e.vx * 8000.0);
    s.vy = (int16_t)(e.vy * 8000.0);
    s.vz = (int16_t)(e.vz * 8000.0);
    const std::vector<uint8_t> b = Enc(s);
    Send(viewer.conn, proto::kCbSpawnObject, b.data(), b.size());
    proto::Metadata md;
    proto::MetaEntry en;
    en.index = 10;
    en.type = 5;
    if (!CopySlot(en.item, e.stack)) return;
    md.entries.push_back(std::move(en));
    const std::vector<uint8_t> b2 = Enc(md);
    // Metadata avulsa usa o id do pacote 0x1C com eid prefixado:
    minecpp_writer_t w{nullptr, 0, 0, 0};
    minecpp_wr_varint(&w, e.eid);
    minecpp_wr_raw(&w, b2.data(), b2.size());
    size_t n = 0;
    uint8_t *buf = minecpp_wr_take(&w, &n);
    Send(viewer.conn, proto::kCbMetadata, buf, n);
    free(buf);
  }
  e.seen_by.insert(viewer.conn);
  e.sx = e.x;
  e.sy = e.y;
  e.sz = e.z;
}

int Server::CountMobsNear(double x, double z, double r) const {
  int n = 0;
  for (const auto &kv : entities_) {
    const Entity &e = *kv.second;
    if (e.kind != Entity::Kind::Mob || e.dead) continue;
    const double dx = e.x - x, dz = e.z - z;
    if (dx * dx + dz * dz < r * r) n++;
  }
  return n;
}

void Server::EntityTick() {
  // Itens: física + pickup + despawn.
  for (auto it = entities_.begin(); it != entities_.end();) {
    Entity &e = *it->second;
    if (e.dead) {
      it = entities_.erase(it);
      continue;
    }
    if (e.kind == Entity::Kind::Item) {
      e.age++;
      if (e.age > 6000) {
        DestroyEntity(e);
        it = entities_.begin();  // mapa mudou: recomeça o scan
        break;
      }
      const int32_t cx = FloorDiv16(e.x), cz = FloorDiv16(e.z);
      Chunk *c = world_.Get(cx, cz);
      if (c) {
        e.vy -= 0.04;
        e.vx *= 0.98;
        e.vy *= 0.98;
        e.vz *= 0.98;
        e.x += e.vx;
        e.y += e.vy;
        e.z += e.vz;
        const int hm = c->heightmap[HeightIndex(
            std::max(0, std::min(15, (int)(e.x - cx * 16))),
            std::max(0, std::min(15, (int)(e.z - cz * 16))))];
        const double rest = hm - 0.75;
        if (e.y <= rest) {
          e.y = rest;
          e.vy = 0;
          e.vx *= 0.7;
          e.vz *= 0.7;
        }
        const double dx = e.x - e.sx, dy = e.y - e.sy, dz = e.z - e.sz;
        if (dx * dx + dy * dy + dz * dz > 0.25 && !e.seen_by.empty()) {
          proto::EntityTeleport t;
          t.eid = e.eid;
          t.x = (int32_t)(e.x * 32.0);
          t.y = (int32_t)(e.y * 32.0);
          t.z = (int32_t)(e.z * 32.0);
          const std::vector<uint8_t> b = Enc(t);
          for (uint32_t conn : e.seen_by)
            Send(conn, proto::kCbTeleport, b.data(), b.size());
          e.sx = e.x;
          e.sy = e.y;
          e.sz = e.z;
        }
      }
      // Pickup.
      if (e.age >= 10) {
        Player *best = nullptr;
        double bestd = 1.5 * 1.5;
        for (const auto &kv : players_) {
          Player &q = *kv.second;
          if (q.state != Player::State::Play) continue;
          const double dx = q.x - e.x, dy = (q.y + 0.5) - e.y, dz = q.z - e.z;
          const double d2 = dx * dx + dy * dy + dz * dz;
          if (d2 < bestd) {
            bestd = d2;
            best = &q;
          }
        }
        if (best) {
          proto::Slot rest;
          rest.id = -1;
          // Tenta tudo; sobra fica na entidade.
          int left = e.stack.count;
          // merge
          for (int i = 9; i < 45 && left > 0; i++) {
            proto::Slot &d = best->inv[i];
            if (d.id == -1 || !SlotsSame(d, e.stack)) continue;
            const int room = MaxStack(d) - d.count;
            const int k = room < left ? room : left;
            if (k <= 0) continue;
            d.count = (uint8_t)(d.count + k);
            left -= k;
            SendSlot(best->conn, i, d);
          }
          for (int i = 9; i < 45 && left > 0; i++) {
            proto::Slot &d = best->inv[i];
            if (d.id != -1) continue;
            if (!CopySlot(d, e.stack)) break;
            const int k = MaxStack(d) < left ? MaxStack(d) : left;
            d.count = (uint8_t)k;
            left -= k;
            SendSlot(best->conn, i, d);
          }
          if (left < e.stack.count) {
            proto::CollectItem ci;
            ci.collector = best->eid;
            ci.collected = e.eid;
            const std::vector<uint8_t> b = Enc(ci);
            const int32_t ecx = FloorDiv16(e.x), ecz = FloorDiv16(e.z);
            SendToLoaded(ecx, ecz, proto::kCbCollectItem, b.data(), b.size());
          }
          e.stack.count = (uint8_t)left;
          if (left == 0) {
            DestroyEntity(e);
            it = entities_.begin();
            break;
          }
        }
      }
    } else {
      // Mob: despawn longe de todos (>48 por 600 ticks).
      bool near = false;
      for (const auto &kv : players_) {
        const Player &q = *kv.second;
        if (q.state != Player::State::Play) continue;
        const double dx = q.x - e.x, dz = q.z - e.z;
        if (dx * dx + dz * dz < 48.0 * 48.0) {
          near = true;
          break;
        }
      }
      e.far_ticks = near ? 0 : e.far_ticks + 1;
      if (e.far_ticks > 600) {
        DestroyEntity(e);
        it = entities_.begin();
        break;
      }
    }
    ++it;
  }
  // Spawn passivo a cada 100 ticks.
  if (tick_ % 100 == 0) {
    for (const auto &kv : players_) {
      Player &q = *kv.second;
      if (q.state != Player::State::Play) continue;
      if (CountMobsNear(q.x, q.z, 48.0) >= 8) continue;
      if (entities_.size() >= 200) break;
      for (int t = 0; t < 4; t++) {
        const int dx = (int)(NextRand() % 9) - 4;
        const int dz = (int)(NextRand() % 9) - 4;
        if (!dx && !dz) continue;
        const int pcx = FloorDiv16(q.x), pcz = FloorDiv16(q.z);
        const int cx = pcx + dx, cz = pcz + dz;
        Chunk *c = world_.Get(cx, cz);
        if (!c) continue;
        const int lx = (int)(NextRand() % 16), lz = (int)(NextRand() % 16);
        const int hm = c->heightmap[HeightIndex(lx, lz)];
        if (hm < 1 || hm > 250) continue;
        int32_t meta = 0;
        if (world_.GetBlock(cx * 16 + lx, hm - 1, cz * 16 + lz, &meta) != 2)
          continue;  // nasce em grama
        if (world_.GetBlock(cx * 16 + lx, hm, cz * 16 + lz, &meta) != 0)
          continue;
        if (world_.GetBlock(cx * 16 + lx, hm + 1, cz * 16 + lz, &meta) != 0)
          continue;
        static const uint8_t types[4] = {90, 91, 92, 93};
        const uint8_t ty = types[NextRand() % 4];
        SpawnMob(ty, cx * 16 + lx + 0.5, hm, cz * 16 + lz + 0.5,
                 MobHealth(ty));
        break;
      }
    }
  }
}

void Server::UpdateVisibility(Player &p) {
  // Outros players.
  for (const auto &kv : players_) {
    Player &q = *kv.second;
    if (q.conn == p.conn || q.state != Player::State::Play) continue;
    const double dx = q.x - p.x, dy = q.y - p.y, dz = q.z - p.z;
    const bool near = dx * dx + dy * dy + dz * dz < 64.0 * 64.0;
    const bool known = p.known_players.count(q.conn) != 0;
    if (near && !known) {
      proto::SpawnPlayer s;
      s.eid = q.eid;
      memcpy(s.uuid, q.uuid_raw, 16);
      s.x = (int32_t)(q.x * 32.0);
      s.y = (int32_t)(q.y * 32.0);
      s.z = (int32_t)(q.z * 32.0);
      s.held = 0;
      s.meta = PlayerMetadata();
      const std::vector<uint8_t> b = Enc(s);
      Send(p.conn, proto::kCbSpawnPlayer, b.data(), b.size());
      p.known_players.insert(q.conn);
    } else if (!near && known) {
      proto::DestroyEntities d;
      d.eids.push_back(q.eid);
      const std::vector<uint8_t> b = Enc(d);
      Send(p.conn, proto::kCbDestroy, b.data(), b.size());
      p.known_players.erase(q.conn);
    }
  }
  // Entidades (só mostra; esconde longe).
  for (const auto &kv : entities_) {
    Entity &e = *kv.second;
    if (e.dead) continue;
    const double dx = e.x - p.x, dy = e.y - p.y, dz = e.z - p.z;
    const bool near = dx * dx + dy * dy + dz * dz < 64.0 * 64.0;
    const bool known = e.seen_by.count(p.conn) != 0;
    if (near && !known) {
      SendEntityTo(e, p);
    } else if (!near && known) {
      proto::DestroyEntities d;
      d.eids.push_back(e.eid);
      const std::vector<uint8_t> b = Enc(d);
      Send(p.conn, proto::kCbDestroy, b.data(), b.size());
      e.seen_by.erase(p.conn);
    }
  }
}

void Server::OnTick(uint64_t tick) {
  if (cfg_.verbose && (tick % 200) == 0) {
    fprintf(stderr, "[tick %llu players=%zu]\n", (unsigned long long)tick,
            players_.size());
    fflush(stderr);
  }
  tick_ = tick;
  submits_this_tick_ = 0;
  int dbg_n = 0;
  for (;;) {
    if (++dbg_n > 100) {
      fprintf(stderr, "[ontick-loop %d tick=%llu]\n", dbg_n, (unsigned long long)tick);
      fflush(stderr);
      abort();
    }
    auto *m = static_cast<MsgIn *>(minecpp_mqueue_pop(tick_q_));
    if (!m) break;
    switch (m->kind) {
      case MsgIn::Kind::Connected:
        AddPlayer(m->conn);
        break;
      case MsgIn::Kind::Packet:
        OnPacket(m->conn, m->id, m->payload.data(), m->payload.size());
        break;
      case MsgIn::Kind::Disconnected:
        RemovePlayer(m->conn);
        break;
      case MsgIn::Kind::ChunkReady:
        OnChunkReady(m);
        break;
    }
    FreeMsgIn(m);
  }
  // Entidades: física, pickup, despawn, spawn passivo.
  EntityTick();
  // Chunks: retry por tick (idempotente via loaded/inflight; budget corta).
  // Sem isso, requests barrados pelo budget nunca retentam até o próximo
  // movimento — stall visível no cliente.
  for (auto &kv : players_) {
    if (kv.second->state == Player::State::Play) RequestChunks(*kv.second);
  }
  // KeepAlive 10s; timeout 30s; stalados em handshake/login caem em 30s.
  for (auto it = players_.begin(); it != players_.end();) {
    Player &p = *it->second;
    ++it;  // Kick apaga; itera seguro
    if (p.state != Player::State::Play) continue;
    if (!p.keepalive_pending && tick - p.keepalive_tick >= 200) {
      KeepAlive k;
      k.id = (int32_t)tick;
      const std::vector<uint8_t> b = Enc(k);
      Send(p.conn, proto::kCbKeepAlive, b.data(), b.size());
      p.keepalive_id = k.id;
      p.keepalive_pending = true;
      p.keepalive_tick = tick;
    } else if (p.keepalive_pending && tick - p.keepalive_tick > 600) {
      Kick(p, "Timed out");
    }
  }
}

// ---------------------------------------------------------------- chunk jobs

void ChunkJobMain(void *arg) {
  std::unique_ptr<ChunkJob> job(static_cast<ChunkJob *>(arg));
  auto *m = new (std::nothrow) MsgIn();
  if (!m) return;
  m->kind = MsgIn::Kind::ChunkReady;
  m->conn = job->conn;
  m->cx = job->cx;
  m->cz = job->cz;
  m->chunk = nullptr;
  m->missing = true;
  const int32_t rx = FloorDiv32(job->cx), rz = FloorDiv32(job->cz);
  const int lx = job->cx - rx * 32, lz = job->cz - rz * 32;
  char path[1024];
  snprintf(path, sizeof path, "%s/r.%d.%d.mca", job->region_dir.c_str(), rx,
           rz);
  minecpp_region_t *rg = minecpp_region_open(path, 0, nullptr);
  if (rg) {
    uint8_t ver = 0, *pay = nullptr;
    size_t plen = 0;
    if (minecpp_region_read_raw(rg, lx, lz, &ver, &pay, &plen) ==
            MINECPP_REGION_OK &&
        (ver == 1 || ver == 2)) {
      size_t nn = 0;
      uint8_t *nbt = InflateChunk(pay, plen, ver, &nn);
      if (nbt) {
        minecpp_nbt_tag_t *root = nullptr;
        if (minecpp_nbt_parse(nbt, nn, 0, &root, nullptr) ==
            MINECPP_NBT_OK) {
          Chunk *c = ChunkCreate(job->cx, job->cz);
          if (c && ChunkFromRoot(c, root) == 0) {
            m->chunk = c;  // ownership p/ tick (World adota)
            m->missing = false;
          } else {
            ChunkFree(c);
          }
        }
        minecpp_nbt_free(root);
        free(nbt);
      }
    }
    free(pay);
    minecpp_region_close(rg);
  }
  minecpp_mqueue_push(job->reply, m);  // SEMPRE responde (destrava inflight)
}

}  // namespace minecpp::v18
