#include "minecpp/v1_8/protocol.h"

#include <cstdlib>
#include <cstring>
#include <new>

#include "minecpp/v1_8/chunk.h"

namespace minecpp::v18::proto {
namespace {

bool RdString(minecpp_reader_t *r, int max_chars, std::string *out) {
  char *s = nullptr;
  int n = 0;
  if (minecpp_rd_string(r, max_chars, &s, &n) != MINECPP_BUF_OK) return false;
  out->assign(s, (size_t)n);
  free(s);
  return true;
}

}  // namespace

bool Encode(const Handshake &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.proto);
  minecpp_wr_strn(w, p.host.data(), p.host.size());
  minecpp_wr_u16(w, p.port);
  minecpp_wr_varint(w, p.next);
  return minecpp_wr_ok(w);
}

bool Decode(Handshake &p, minecpp_reader_t *r) {
  uint16_t port = 0;
  return minecpp_rd_varint(r, &p.proto) == MINECPP_BUF_OK &&
         RdString(r, 255, &p.host) &&
         minecpp_rd_u16(r, &port) == MINECPP_BUF_OK &&
         minecpp_rd_varint(r, &p.next) == MINECPP_BUF_OK &&
         (p.port = port, true);
}

bool Encode(const StatusResponse &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.json.data(), p.json.size());
  return minecpp_wr_ok(w);
}

bool Decode(StatusResponse &p, minecpp_reader_t *r) {
  return RdString(r, 32767, &p.json);
}

bool Encode(const Ping &p, minecpp_writer_t *w) {
  minecpp_wr_i64(w, p.payload);
  return minecpp_wr_ok(w);
}

bool Decode(Ping &p, minecpp_reader_t *r) {
  return minecpp_rd_i64(r, &p.payload) == MINECPP_BUF_OK;
}

bool Encode(const LoginStart &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.name.data(), p.name.size());
  return minecpp_wr_ok(w);
}

bool Decode(LoginStart &p, minecpp_reader_t *r) {
  return RdString(r, 16, &p.name);
}

bool Encode(const LoginSuccess &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.uuid.data(), p.uuid.size());
  minecpp_wr_strn(w, p.name.data(), p.name.size());
  return minecpp_wr_ok(w);
}

bool Decode(LoginSuccess &p, minecpp_reader_t *r) {
  return RdString(r, 36, &p.uuid) && RdString(r, 16, &p.name);
}

bool Encode(const Disconnect &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.reason.data(), p.reason.size());
  return minecpp_wr_ok(w);
}

bool Decode(Disconnect &p, minecpp_reader_t *r) {
  return RdString(r, 32767, &p.reason);
}

bool Encode(const SetCompression &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.threshold);
  return minecpp_wr_ok(w);
}

bool Decode(SetCompression &p, minecpp_reader_t *r) {
  return minecpp_rd_varint(r, &p.threshold) == MINECPP_BUF_OK;
}

bool Encode(const KeepAlive &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.id);
  return minecpp_wr_ok(w);
}

bool Decode(KeepAlive &p, minecpp_reader_t *r) {
  return minecpp_rd_varint(r, &p.id) == MINECPP_BUF_OK;
}

bool Encode(const JoinGame &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.eid);
  minecpp_wr_u8(w, p.mode);
  minecpp_wr_u8(w, (uint8_t)p.dimension);
  minecpp_wr_u8(w, p.difficulty);
  minecpp_wr_u8(w, p.max_players);
  minecpp_wr_strn(w, p.level_type.data(), p.level_type.size());
  minecpp_wr_bool(w, p.reduced_debug);
  return minecpp_wr_ok(w);
}

bool Decode(JoinGame &p, minecpp_reader_t *r) {
  uint8_t mode = 0, dim = 0, diff = 0, max = 0;
  int red = 0;
  if (minecpp_rd_i32(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &mode) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &dim) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &diff) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &max) != MINECPP_BUF_OK) return false;
  if (!RdString(r, 16, &p.level_type)) return false;
  if (minecpp_rd_bool(r, &red) != MINECPP_BUF_OK) return false;
  p.mode = mode;
  p.dimension = (int8_t)dim;
  p.difficulty = diff;
  p.max_players = max;
  p.reduced_debug = red != 0;
  return true;
}

bool Encode(const SpawnPosition &p, minecpp_writer_t *w) {
  minecpp_wr_pos(w, p.x, p.y, p.z);
  return minecpp_wr_ok(w);
}

bool Decode(SpawnPosition &p, minecpp_reader_t *r) {
  return minecpp_rd_pos(r, &p.x, &p.y, &p.z) == MINECPP_BUF_OK;
}

bool Encode(const PlayerPosLook &p, minecpp_writer_t *w) {
  minecpp_wr_f64(w, p.x);
  minecpp_wr_f64(w, p.y);
  minecpp_wr_f64(w, p.z);
  minecpp_wr_f32(w, p.yaw);
  minecpp_wr_f32(w, p.pitch);
  minecpp_wr_u8(w, p.flags);
  return minecpp_wr_ok(w);
}

bool Decode(PlayerPosLook &p, minecpp_reader_t *r) {
  uint8_t f = 0;
  if (minecpp_rd_f64(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f64(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f64(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.yaw) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.pitch) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &f) != MINECPP_BUF_OK) return false;
  p.flags = f;
  return true;
}

bool Encode(const ChunkData &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.z);
  minecpp_wr_bool(w, p.continuous);
  minecpp_wr_u16(w, p.mask);
  if (p.data.size() > (size_t)0x7FFFFFFF) return false;
  minecpp_wr_varint(w, (int32_t)p.data.size());
  if (!p.data.empty()) minecpp_wr_raw(w, p.data.data(), p.data.size());
  return minecpp_wr_ok(w);
}

bool Decode(ChunkData &p, minecpp_reader_t *r) {
  int32_t n = 0;
  int cont = 0;
  uint16_t mask = 0;
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_bool(r, &cont) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &mask) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &n) != MINECPP_BUF_OK || n < 0) return false;
  if ((size_t)n > r->left) return false;
  p.continuous = cont != 0;
  p.mask = mask;
  p.data.assign(r->p, r->p + (size_t)n);
  r->p += (size_t)n;
  r->left -= (size_t)n;
  return true;
}

bool Encode(const Chat &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.msg.data(), p.msg.size());
  return minecpp_wr_ok(w);
}

bool Decode(Chat &p, minecpp_reader_t *r, int max_chars) {
  return RdString(r, max_chars, &p.msg);
}

bool Encode(const TimeUpdate &p, minecpp_writer_t *w) {
  minecpp_wr_i64(w, p.age);
  minecpp_wr_i64(w, p.time);
  return minecpp_wr_ok(w);
}

bool Decode(TimeUpdate &p, minecpp_reader_t *r) {
  return minecpp_rd_i64(r, &p.age) == MINECPP_BUF_OK &&
         minecpp_rd_i64(r, &p.time) == MINECPP_BUF_OK;
}

bool Encode(const Flying &p, minecpp_writer_t *w) {
  minecpp_wr_bool(w, p.on_ground);
  return minecpp_wr_ok(w);
}

bool Decode(Flying &p, minecpp_reader_t *r) {
  int g = 0;
  if (minecpp_rd_bool(r, &g) != MINECPP_BUF_OK) return false;
  p.on_ground = g != 0;
  return true;
}

bool Encode(const SbPosition &p, minecpp_writer_t *w) {
  minecpp_wr_f64(w, p.x);
  minecpp_wr_f64(w, p.y);
  minecpp_wr_f64(w, p.z);
  minecpp_wr_bool(w, p.on_ground);
  return minecpp_wr_ok(w);
}

bool Decode(SbPosition &p, minecpp_reader_t *r) {
  int g = 0;
  if (minecpp_rd_f64(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f64(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f64(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_bool(r, &g) != MINECPP_BUF_OK) return false;
  p.on_ground = g != 0;
  return true;
}

bool Encode(const SbLook &p, minecpp_writer_t *w) {
  minecpp_wr_f32(w, p.yaw);
  minecpp_wr_f32(w, p.pitch);
  minecpp_wr_bool(w, p.on_ground);
  return minecpp_wr_ok(w);
}

bool Decode(SbLook &p, minecpp_reader_t *r) {
  int g = 0;
  if (minecpp_rd_f32(r, &p.yaw) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.pitch) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_bool(r, &g) != MINECPP_BUF_OK) return false;
  p.on_ground = g != 0;
  return true;
}

bool Encode(const SbPosLook &p, minecpp_writer_t *w) {
  minecpp_wr_f64(w, p.x);
  minecpp_wr_f64(w, p.y);
  minecpp_wr_f64(w, p.z);
  minecpp_wr_f32(w, p.yaw);
  minecpp_wr_f32(w, p.pitch);
  minecpp_wr_bool(w, p.on_ground);
  return minecpp_wr_ok(w);
}

bool Decode(SbPosLook &p, minecpp_reader_t *r) {
  int g = 0;
  if (minecpp_rd_f64(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f64(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f64(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.yaw) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.pitch) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_bool(r, &g) != MINECPP_BUF_OK) return false;
  p.on_ground = g != 0;
  return true;
}

bool Encode(const PluginMessage &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.channel.data(), p.channel.size());
  if (!p.data.empty()) minecpp_wr_raw(w, p.data.data(), p.data.size());
  return minecpp_wr_ok(w);
}

bool Decode(PluginMessage &p, minecpp_reader_t *r) {
  if (!RdString(r, 20, &p.channel)) return false;
  p.data.assign(r->p, r->p + r->left);
  r->p += r->left;
  r->left = 0;
  return true;
}

// ---- lote P0 ----

bool Encode(const ServerDifficulty &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.difficulty);
  return minecpp_wr_ok(w);
}

bool Decode(ServerDifficulty &p, minecpp_reader_t *r) {
  uint8_t d = 0;
  if (minecpp_rd_u8(r, &d) != MINECPP_BUF_OK) return false;
  p.difficulty = d;
  return true;
}

bool Encode(const PlayerAbilities &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.flags);
  minecpp_wr_f32(w, p.fly_speed);
  minecpp_wr_f32(w, p.walk_speed);
  return minecpp_wr_ok(w);
}

bool Decode(PlayerAbilities &p, minecpp_reader_t *r) {
  uint8_t f = 0;
  if (minecpp_rd_u8(r, &f) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.fly_speed) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.walk_speed) != MINECPP_BUF_OK) return false;
  p.flags = f;
  return true;
}

bool Encode(const HeldItemChange &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.slot);
  return minecpp_wr_ok(w);
}

bool Decode(HeldItemChange &p, minecpp_reader_t *r) {
  uint8_t s = 0;
  if (minecpp_rd_u8(r, &s) != MINECPP_BUF_OK) return false;
  p.slot = s;
  return true;
}

bool Encode(const Statistics &p, minecpp_writer_t *w) {
  if (p.stats.size() > 0x7FFFFFFF) return false;
  minecpp_wr_varint(w, (int32_t)p.stats.size());
  for (const auto &kv : p.stats) {
    minecpp_wr_strn(w, kv.first.data(), kv.first.size());
    minecpp_wr_varint(w, kv.second);
  }
  return minecpp_wr_ok(w);
}

bool Decode(Statistics &p, minecpp_reader_t *r) {
  int32_t n = 0;
  if (minecpp_rd_varint(r, &n) != MINECPP_BUF_OK || n < 0) return false;
  if (n > 1024) return false;  // teto são (stats vanilla ~ centenas)
  for (int32_t i = 0; i < n; i++) {
    std::string k;
    int32_t v = 0;
    if (!RdString(r, 64, &k)) return false;
    if (minecpp_rd_varint(r, &v) != MINECPP_BUF_OK) return false;
    p.stats.emplace_back(k, v);
  }
  return true;
}

namespace {
bool WrUuid(minecpp_writer_t *w, const uint8_t u[16]) {
  minecpp_wr_raw(w, u, 16);
  return minecpp_wr_ok(w);
}
bool RdUuid(minecpp_reader_t *r, uint8_t u[16]) {
  return minecpp_rd_raw(r, u, 16) == MINECPP_BUF_OK;
}
}  // namespace

bool Encode(const PlayerListItem &p, minecpp_writer_t *w) {
  if (p.action != 0 && p.action != 4) return false;
  minecpp_wr_varint(w, p.action);
  minecpp_wr_varint(w, (int32_t)p.players.size());
  for (const auto &e : p.players) {
    WrUuid(w, e.uuid);
    if (p.action == 4) continue;
    minecpp_wr_strn(w, e.name.data(), e.name.size());
    minecpp_wr_varint(w, (int32_t)e.props.size());
    for (const auto &pr : e.props) {
      minecpp_wr_strn(w, pr.name.data(), pr.name.size());
      minecpp_wr_strn(w, pr.value.data(), pr.value.size());
      minecpp_wr_bool(w, pr.has_sig);
      if (pr.has_sig) minecpp_wr_strn(w, pr.sig.data(), pr.sig.size());
    }
    minecpp_wr_varint(w, e.gamemode);
    minecpp_wr_varint(w, e.ping);
    minecpp_wr_bool(w, e.has_display);
    if (e.has_display) minecpp_wr_strn(w, e.display.data(), e.display.size());
  }
  return minecpp_wr_ok(w);
}

bool Decode(PlayerListItem &p, minecpp_reader_t *r) {
  int32_t n = 0;
  if (minecpp_rd_varint(r, &p.action) != MINECPP_BUF_OK) return false;
  if (p.action != 0 && p.action != 4) return false;
  if (minecpp_rd_varint(r, &n) != MINECPP_BUF_OK || n < 0 || n > 128)
    return false;
  for (int32_t i = 0; i < n; i++) {
    PlayerListEntry e;
    if (!RdUuid(r, e.uuid)) return false;
    if (p.action == 4) {
      p.players.push_back(e);
      continue;
    }
    int32_t np = 0, gm = 0, pg = 0, hd = 0;
    if (!RdString(r, 16, &e.name)) return false;
    if (minecpp_rd_varint(r, &np) != MINECPP_BUF_OK || np < 0 || np > 16)
      return false;
    for (int32_t k = 0; k < np; k++) {
      PlayerListProp pr;
      int sg = 0;
      if (!RdString(r, 64, &pr.name)) return false;
      if (!RdString(r, 32767, &pr.value)) return false;
      if (minecpp_rd_bool(r, &sg) != MINECPP_BUF_OK) return false;
      pr.has_sig = sg != 0;
      if (pr.has_sig && !RdString(r, 1024, &pr.sig)) return false;
      e.props.push_back(std::move(pr));
    }
    if (minecpp_rd_varint(r, &gm) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_varint(r, &pg) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_bool(r, &hd) != MINECPP_BUF_OK) return false;
    e.gamemode = gm;
    e.ping = pg;
    e.has_display = hd != 0;
    if (e.has_display && !RdString(r, 64, &e.display)) return false;
    p.players.push_back(std::move(e));
  }
  return true;
}

bool Encode(const WorldBorder &p, minecpp_writer_t *w) {
  if (p.action != 3) return false;  // só INITIALIZE por enquanto
  minecpp_wr_varint(w, p.action);
  minecpp_wr_f64(w, p.center_x);
  minecpp_wr_f64(w, p.center_z);
  minecpp_wr_f64(w, p.old_size);
  minecpp_wr_f64(w, p.new_size);
  minecpp_wr_varlong(w, p.speed);
  minecpp_wr_varint(w, p.portal_boundary);
  minecpp_wr_varint(w, p.warn_a);
  minecpp_wr_varint(w, p.warn_b);
  return minecpp_wr_ok(w);
}

bool Decode(WorldBorder &p, minecpp_reader_t *r) {
  int64_t sp = 0;
  int32_t a = 0;
  if (minecpp_rd_varint(r, &a) != MINECPP_BUF_OK || a != 3) return false;
  p.action = a;
  if (minecpp_rd_f64(r, &p.center_x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f64(r, &p.center_z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f64(r, &p.old_size) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f64(r, &p.new_size) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varlong(r, &sp) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.portal_boundary) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.warn_a) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.warn_b) != MINECPP_BUF_OK) return false;
  p.speed = sp;
  return true;
}

bool Encode(const Slot &p, minecpp_writer_t *w) {
  minecpp_wr_u16(w, (uint16_t)p.id);
  if (p.id == -1) return minecpp_wr_ok(w);
  minecpp_wr_u8(w, p.count);
  minecpp_wr_u16(w, (uint16_t)p.damage);
  if (!p.nbt) {
    minecpp_wr_u8(w, 0);  // hd.a((fn)null) escreve 1 byte 0x00
    return minecpp_wr_ok(w);
  }
  uint8_t *nb = nullptr;
  size_t nn = 0;
  if (minecpp_nbt_serialize(p.nbt, &nb, &nn) != MINECPP_NBT_OK) {
    w->oom = 1;
    return false;
  }
  minecpp_wr_raw(w, nb, nn);
  free(nb);
  return minecpp_wr_ok(w);
}

bool Decode(Slot &p, minecpp_reader_t *r) {
  uint16_t id = 0;
  uint8_t count = 0;
  uint16_t dmg = 0;
  uint8_t tag0 = 0;
  if (minecpp_rd_u16(r, &id) != MINECPP_BUF_OK) return false;
  p.id = (int16_t)id;
  minecpp_nbt_free(p.nbt);
  p.nbt = nullptr;
  if (p.id == -1) {
    p.count = 0;
    p.damage = 0;
    return true;
  }
  if (minecpp_rd_u8(r, &count) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &dmg) != MINECPP_BUF_OK) return false;
  if (r->left < 1) return false;
  tag0 = r->p[0];
  p.count = count;
  p.damage = (int16_t)dmg;
  if (tag0 == 0) {
    r->p++;
    r->left--;
    return true;
  }
  // NBT presente: parse com o budget do vanilla (2MB, hd.h/fx).
  size_t used = 0;
  if (minecpp_nbt_parse(r->p, r->left, 2u << 20, &p.nbt, &used) !=
      MINECPP_NBT_OK)
    return false;
  r->p += used;
  r->left -= used;
  return true;
}

void FreeSlot(Slot &s) {
  minecpp_nbt_free(s.nbt);
  s.nbt = nullptr;
  s.id = -1;
  s.count = 0;
  s.damage = 0;
}

bool CopySlot(Slot &dst, const Slot &src) {
  minecpp_nbt_tag_t *nbt =
      src.nbt ? minecpp_nbt_clone(src.nbt) : nullptr;
  if (src.nbt && !nbt) {
    FreeSlot(dst);
    return false;
  }
  minecpp_nbt_free(dst.nbt);
  dst.id = src.id;
  dst.count = src.count;
  dst.damage = src.damage;
  dst.nbt = nbt;
  return true;
}

bool SlotsSame(const Slot &a, const Slot &b) {
  if (a.id != b.id || a.damage != b.damage) return false;
  if (!a.nbt || !b.nbt) return a.nbt == b.nbt;
  return minecpp_nbt_equal(a.nbt, b.nbt) != 0;
}

bool Encode(const WindowItems &p, minecpp_writer_t *w) {
  if (p.slots.size() > 127) return false;
  minecpp_wr_u8(w, p.window);
  minecpp_wr_u16(w, (uint16_t)p.slots.size());
  for (const auto &s : p.slots)
    if (!Encode(s, w)) return false;
  return minecpp_wr_ok(w);
}

bool Decode(WindowItems &p, minecpp_reader_t *r) {
  uint8_t win = 0;
  uint16_t n = 0;
  if (minecpp_rd_u8(r, &win) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &n) != MINECPP_BUF_OK || n > 128) return false;
  p.window = win;
  p.slots.clear();
  for (uint16_t i = 0; i < n; i++) {
    Slot s;
    if (!Decode(s, r)) return false;
    p.slots.push_back(std::move(s));
  }
  return true;
}

bool Encode(const SetSlot &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, (uint8_t)p.window);
  minecpp_wr_u16(w, (uint16_t)p.slot);
  return Encode(p.item, w) && minecpp_wr_ok(w);
}

bool Decode(SetSlot &p, minecpp_reader_t *r) {
  uint8_t win = 0;
  uint16_t slot = 0;
  if (minecpp_rd_u8(r, &win) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &slot) != MINECPP_BUF_OK) return false;
  p.window = (int8_t)win;
  p.slot = (int16_t)slot;
  return Decode(p.item, r);
}

bool Encode(const UpdateHealth &p, minecpp_writer_t *w) {
  minecpp_wr_f32(w, p.health);
  minecpp_wr_varint(w, p.food);
  minecpp_wr_f32(w, p.saturation);
  return minecpp_wr_ok(w);
}

bool Decode(UpdateHealth &p, minecpp_reader_t *r) {
  if (minecpp_rd_f32(r, &p.health) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.food) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.saturation) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const SetExperience &p, minecpp_writer_t *w) {
  minecpp_wr_f32(w, p.bar);
  minecpp_wr_varint(w, p.level);
  minecpp_wr_varint(w, p.total);
  return minecpp_wr_ok(w);
}

bool Decode(SetExperience &p, minecpp_reader_t *r) {
  if (minecpp_rd_f32(r, &p.bar) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.level) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.total) != MINECPP_BUF_OK) return false;
  return true;
}

// Seção em bytes de rede (igual ao ChunkData): u16LE + lights.
static void AppendSectionNet(const Section &s, std::vector<uint8_t> *out) {
  const size_t base = out->size();
  out->resize(base + 12288);
  uint8_t *d = out->data() + base;
  for (int i = 0; i < 4096; i++) {
    int id = s.blocks[i];
    if (s.has_add) id |= NibbleGet(s.add, i) << 8;
    const int v = (id << 4) | NibbleGet(s.data, i);
    d[2 * i] = (uint8_t)(v & 0xFF);
    d[2 * i + 1] = (uint8_t)((v >> 8) & 0xFF);
  }
  memcpy(d + 8192, s.block_light, 2048);
  memcpy(d + 10240, s.sky_light, 2048);
}

bool Encode(const MapChunkBulk &p, minecpp_writer_t *w) {
  if (p.chunks.size() > 64) return false;
  minecpp_wr_bool(w, p.sky_light);
  minecpp_wr_varint(w, (int32_t)p.chunks.size());
  for (const auto &c : p.chunks) {
    minecpp_wr_i32(w, c.x);
    minecpp_wr_i32(w, c.z);
    minecpp_wr_u16(w, c.mask);
  }
  for (const auto &c : p.chunks)
    if (!c.data.empty()) minecpp_wr_raw(w, c.data.data(), c.data.size());
  return minecpp_wr_ok(w);
}

bool Decode(MapChunkBulk &p, minecpp_reader_t *r) {
  int sky = 0, n = 0;
  if (minecpp_rd_bool(r, &sky) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &n) != MINECPP_BUF_OK || n < 0 || n > 64)
    return false;
  p.sky_light = sky != 0;
  p.chunks.clear();
  size_t total = 0;
  for (int32_t i = 0; i < n; i++) {
    BulkChunk c;
    uint16_t mask = 0;
    if (minecpp_rd_i32(r, &c.x) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_i32(r, &c.z) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_u16(r, &mask) != MINECPP_BUF_OK) return false;
    c.mask = mask;
    int secs = 0;
    for (int b = 0; b < 16; b++)
      if (mask & (1u << b)) secs++;
    total += (size_t)secs * 12288 + 256;  // biomas sempre (bulk = full)
    p.chunks.push_back(std::move(c));
  }
  if (total > r->left) return false;
  for (auto &c : p.chunks) {
    int secs = 0;
    for (int b = 0; b < 16; b++)
      if (c.mask & (1u << b)) secs++;
    const size_t sz = (size_t)secs * 12288 + 256;
    c.data.assign(r->p, r->p + sz);
    r->p += sz;
    r->left -= sz;
  }
  return true;
}

bool FrameEncode(int32_t id, const uint8_t *payload, size_t n,
                 minecpp_writer_t *out) {  minecpp_writer_t body = {nullptr, 0, 0, 0};
  minecpp_wr_varint(&body, id);
  if (n) minecpp_wr_raw(&body, payload, n);
  if (!minecpp_wr_ok(&body)) {
    free(body.buf);
    return false;
  }
  if (body.len > kMaxFrame) {
    free(body.buf);
    return false;
  }
  minecpp_wr_varint(out, (int32_t)body.len);
  minecpp_wr_raw(out, body.buf, body.len);
  free(body.buf);
  return minecpp_wr_ok(out);
}

bool FrameDecode(minecpp_reader_t *r, int32_t *id, minecpp_reader_t *payload) {
  int32_t len = 0;
  if (minecpp_rd_varint(r, &len) != MINECPP_BUF_OK) return false;
  if (len <= 0 || (size_t)len > kMaxFrame || (size_t)len > r->left)
    return false;
  minecpp_reader_t sub = {r->p, (size_t)len};
  int32_t pid = 0;
  if (minecpp_rd_varint(&sub, &pid) != MINECPP_BUF_OK) return false;
  r->p += (size_t)len;
  r->left -= (size_t)len;
  if (id) *id = pid;
  if (payload) *payload = sub;
  return true;
}

namespace {

// Monta ChunkData 0x21 a partir do Chunk (seções não-nulas, Y asc).
bool BuildChunkData(const Chunk &c, bool continuous, ChunkData *out) {
  out->x = c.x;
  out->z = c.z;
  out->continuous = continuous;
  out->mask = 0;
  out->data.clear();
  for (int y = 0; y < kSectionsPerChunk; y++) {
    const Section *s = c.sections[y];
    if (!s) continue;
    out->mask |= (uint16_t)(1u << y);
    const size_t base = out->data.size();
    out->data.resize(base + 8192 + 2048 + 2048);
    uint8_t *d = out->data.data() + base;
    for (int i = 0; i < 4096; i++) {
      int id = s->blocks[i];
      if (s->has_add) id |= NibbleGet(s->add, i) << 8;
      const int v = (id << 4) | NibbleGet(s->data, i);
      d[2 * i] = (uint8_t)(v & 0xFF);  // u16LE (bytes reais do vanilla)
      d[2 * i + 1] = (uint8_t)((v >> 8) & 0xFF);
    }
    memcpy(d + 8192, s->block_light, 2048);
    memcpy(d + 10240, s->sky_light, 2048);
  }
  if (continuous) {
    if (!c.has_biomes) return false;
    out->data.insert(out->data.end(), c.biomes, c.biomes + 256);
  }
  return true;
}

// Aplica payload 0x21 num Chunk (cria seções; exige tamanhos exatos).
bool ApplyChunkData(const ChunkData &p, Chunk *c) {
  size_t off = 0;
  const auto need = [&](size_t n) { return off + n <= p.data.size(); };
  for (int y = 0; y < kSectionsPerChunk; y++) {
    if (!(p.mask & (1u << y))) continue;
    if (!need(12288)) return false;
    Section *s = c->sections[y];
    if (!s) {
      s = new (std::nothrow) Section();
      if (!s) return false;
      s->y = (uint8_t)y;
      c->sections[y] = s;
    }
    const uint8_t *d = p.data.data() + off;
    s->has_add = false;
    for (int i = 0; i < 4096; i++) {
      // u16LE(id<<4|meta): id 12 bits, meta 4 bits (bytes reais do vanilla).
      const int v = d[2 * i] | (d[2 * i + 1] << 8);
      s->blocks[i] = (uint8_t)((v >> 4) & 0xFF);
      NibbleSet(s->data, i, v & 15);
      const int hi = (v >> 12) & 15;
      if (hi) {
        s->has_add = true;
        NibbleSet(s->add, i, hi);
      }
    }
    memcpy(s->block_light, d + 8192, 2048);
    memcpy(s->sky_light, d + 10240, 2048);
    off += 12288;
  }
  if (p.continuous) {
    if (!need(256)) return false;
    c->has_biomes = true;
    memcpy(c->biomes, p.data.data() + off, 256);
    off += 256;
  }
  if (off != p.data.size()) return false;  // trailing bytes = corrupt
  c->x = p.x;
  c->z = p.z;
  return true;
}

}  // namespace

// Adaptadores públicos (declarados em protocol.h).
bool ChunkDataBuild(const Chunk &c, bool continuous, ChunkData *out) {
  return BuildChunkData(c, continuous, out);
}

bool ChunkDataApply(const ChunkData &p, Chunk *c) {
  if (!c) return false;
  return ApplyChunkData(p, c);
}

bool BulkBuild(const std::vector<const Chunk *> &chunks, bool sky,
               MapChunkBulk *out) {
  if (!out || chunks.empty() || chunks.size() > 64) return false;
  out->sky_light = sky;
  out->chunks.clear();
  for (const Chunk *c : chunks) {
    if (!c || !c->has_biomes) return false;
    BulkChunk b;
    b.x = c->x;
    b.z = c->z;
    b.mask = 0;
    for (int y = 0; y < kSectionsPerChunk; y++) {
      if (!c->sections[y]) continue;
      b.mask |= (uint16_t)(1u << y);
      AppendSectionNet(*c->sections[y], &b.data);
    }
    b.data.insert(b.data.end(), c->biomes, c->biomes + 256);
    out->chunks.push_back(std::move(b));
  }
  return true;
}

bool BulkSplit(const MapChunkBulk &p, std::vector<ChunkData> *out) {
  if (!out) return false;
  out->clear();
  for (const auto &b : p.chunks) {
    ChunkData d;
    d.x = b.x;
    d.z = b.z;
    d.continuous = true;
    d.mask = b.mask;
    d.data = b.data;
    out->push_back(std::move(d));
  }
  return true;
}

// ---- janelas/inventário ----

bool Encode(const ClickWindow &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.window);
  minecpp_wr_u16(w, (uint16_t)p.slot);
  minecpp_wr_u8(w, p.button);
  minecpp_wr_u16(w, (uint16_t)p.action);
  minecpp_wr_u8(w, p.mode);
  return Encode(p.clicked, w) && minecpp_wr_ok(w);
}

bool Decode(ClickWindow &p, minecpp_reader_t *r) {
  uint8_t win = 0, btn = 0, mode = 0;
  uint16_t slot = 0, action = 0;
  if (minecpp_rd_u8(r, &win) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &slot) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &btn) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &action) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &mode) != MINECPP_BUF_OK) return false;
  p.window = win;
  p.slot = (int16_t)slot;
  p.button = btn;
  p.action = (int16_t)action;
  p.mode = mode;
  return Decode(p.clicked, r);
}

bool Encode(const ConfirmTransaction &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.window);
  minecpp_wr_u16(w, (uint16_t)p.action);
  minecpp_wr_bool(w, p.accepted);
  return minecpp_wr_ok(w);
}

bool Decode(ConfirmTransaction &p, minecpp_reader_t *r) {
  uint8_t win = 0;
  uint16_t action = 0;
  int ok = 0;
  if (minecpp_rd_u8(r, &win) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &action) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_bool(r, &ok) != MINECPP_BUF_OK) return false;
  p.window = win;
  p.action = (int16_t)action;
  p.accepted = ok != 0;
  return true;
}

bool Encode(const OpenWindow &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.window);
  minecpp_wr_strn(w, p.type.data(), p.type.size());
  minecpp_wr_strn(w, p.title.data(), p.title.size());
  minecpp_wr_u8(w, p.slots);
  return minecpp_wr_ok(w);
}

bool Decode(OpenWindow &p, minecpp_reader_t *r) {
  uint8_t win = 0, slots = 0;
  if (minecpp_rd_u8(r, &win) != MINECPP_BUF_OK) return false;
  if (!RdString(r, 64, &p.type)) return false;
  if (!RdString(r, 32767, &p.title)) return false;
  if (minecpp_rd_u8(r, &slots) != MINECPP_BUF_OK) return false;
  p.window = win;
  p.slots = slots;
  return true;
}

bool Encode(const CloseWindowPkt &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.window);
  return minecpp_wr_ok(w);
}

bool Decode(CloseWindowPkt &p, minecpp_reader_t *r) {
  uint8_t win = 0;
  if (minecpp_rd_u8(r, &win) != MINECPP_BUF_OK) return false;
  p.window = win;
  return true;
}

bool Encode(const WindowProperty &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.window);
  minecpp_wr_u16(w, (uint16_t)p.prop);
  minecpp_wr_u16(w, (uint16_t)p.value);
  return minecpp_wr_ok(w);
}

bool Decode(WindowProperty &p, minecpp_reader_t *r) {
  uint8_t win = 0;
  uint16_t pr = 0, v = 0;
  if (minecpp_rd_u8(r, &win) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &pr) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &v) != MINECPP_BUF_OK) return false;
  p.window = win;
  p.prop = (int16_t)pr;
  p.value = (int16_t)v;
  return true;
}

bool Encode(const Animation &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u8(w, p.action);
  return minecpp_wr_ok(w);
}

bool Decode(Animation &p, minecpp_reader_t *r) {
  uint8_t a = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &a) != MINECPP_BUF_OK) return false;
  p.action = a;
  return true;
}

bool Encode(const CreativeAction &p, minecpp_writer_t *w) {
  minecpp_wr_u16(w, (uint16_t)p.slot);
  return Encode(p.item, w) && minecpp_wr_ok(w);
}

bool Decode(CreativeAction &p, minecpp_reader_t *r) {
  uint16_t s = 0;
  if (minecpp_rd_u16(r, &s) != MINECPP_BUF_OK) return false;
  p.slot = (int16_t)s;
  return Decode(p.item, r);
}

bool Encode(const SbHeldItem &p, minecpp_writer_t *w) {
  minecpp_wr_u16(w, (uint16_t)p.slot);
  return minecpp_wr_ok(w);
}

bool Decode(SbHeldItem &p, minecpp_reader_t *r) {
  uint16_t s = 0;
  if (minecpp_rd_u16(r, &s) != MINECPP_BUF_OK) return false;
  p.slot = (int16_t)s;
  return true;
}

// ---- blocos ----

bool Encode(const BlockDig &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.status);
  minecpp_wr_pos(w, p.x, p.y, p.z);
  minecpp_wr_u8(w, p.face);
  return minecpp_wr_ok(w);
}

bool Decode(BlockDig &p, minecpp_reader_t *r) {
  uint8_t f = 0;
  if (minecpp_rd_varint(r, &p.status) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_pos(r, &p.x, &p.y, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &f) != MINECPP_BUF_OK) return false;
  p.face = f;
  return true;
}

bool Encode(const BlockPlace &p, minecpp_writer_t *w) {
  minecpp_wr_pos(w, p.x, p.y, p.z);
  minecpp_wr_u8(w, p.dir);
  if (!Encode(p.held, w)) return false;
  minecpp_wr_u8(w, p.cx);
  minecpp_wr_u8(w, p.cy);
  minecpp_wr_u8(w, p.cz);
  return minecpp_wr_ok(w);
}

bool Decode(BlockPlace &p, minecpp_reader_t *r) {
  uint8_t d = 0;
  if (minecpp_rd_pos(r, &p.x, &p.y, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &d) != MINECPP_BUF_OK) return false;
  if (!Decode(p.held, r)) return false;
  uint8_t cx = 0, cy = 0, cz = 0;
  if (minecpp_rd_u8(r, &cx) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &cy) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &cz) != MINECPP_BUF_OK) return false;
  p.dir = d;
  p.cx = cx;
  p.cy = cy;
  p.cz = cz;
  return true;
}

bool Encode(const BlockChangePkt &p, minecpp_writer_t *w) {
  minecpp_wr_pos(w, p.x, p.y, p.z);
  minecpp_wr_varint(w, p.idmeta);
  return minecpp_wr_ok(w);
}

bool Decode(BlockChangePkt &p, minecpp_reader_t *r) {
  if (minecpp_rd_pos(r, &p.x, &p.y, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.idmeta) != MINECPP_BUF_OK || p.idmeta < 0)
    return false;
  return true;
}

bool Encode(const BlockAction &p, minecpp_writer_t *w) {
  minecpp_wr_pos(w, p.x, p.y, p.z);
  minecpp_wr_u8(w, p.b1);
  minecpp_wr_u8(w, p.b2);
  minecpp_wr_varint(w, p.block);
  return minecpp_wr_ok(w);
}

bool Decode(BlockAction &p, minecpp_reader_t *r) {
  uint8_t a = 0, b = 0;
  if (minecpp_rd_pos(r, &p.x, &p.y, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &a) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &b) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.block) != MINECPP_BUF_OK || p.block < 0)
    return false;
  p.b1 = a;
  p.b2 = b;
  return true;
}

bool Encode(const BreakAnim &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_pos(w, p.x, p.y, p.z);
  minecpp_wr_u8(w, (uint8_t)p.stage);
  return minecpp_wr_ok(w);
}

bool Decode(BreakAnim &p, minecpp_reader_t *r) {
  uint8_t s = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_pos(r, &p.x, &p.y, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &s) != MINECPP_BUF_OK) return false;
  p.stage = (int8_t)s;
  return true;
}

bool Encode(const Effect &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.id);
  minecpp_wr_pos(w, p.x, p.y, p.z);
  minecpp_wr_i32(w, p.data);
  minecpp_wr_bool(w, p.norel);
  return minecpp_wr_ok(w);
}

bool Decode(Effect &p, minecpp_reader_t *r) {
  int rel = 0;
  if (minecpp_rd_i32(r, &p.id) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_pos(r, &p.x, &p.y, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.data) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_bool(r, &rel) != MINECPP_BUF_OK) return false;
  p.norel = rel != 0;
  return true;
}

bool Encode(const SoundEffect &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.name.data(), p.name.size());
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  minecpp_wr_f32(w, p.volume);
  minecpp_wr_f32(w, p.pitch);
  return minecpp_wr_ok(w);
}

bool Decode(SoundEffect &p, minecpp_reader_t *r) {
  if (!RdString(r, 64, &p.name)) return false;
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.volume) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.pitch) != MINECPP_BUF_OK) return false;
  return true;
}

// ---- entidades ----

bool Encode(const SpawnMob &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u8(w, p.type);
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  minecpp_wr_u8(w, p.yaw);
  minecpp_wr_u8(w, p.pitch);
  minecpp_wr_u8(w, p.head);
  minecpp_wr_u16(w, (uint16_t)p.vx);
  minecpp_wr_u16(w, (uint16_t)p.vy);
  minecpp_wr_u16(w, (uint16_t)p.vz);
  return Encode(p.meta, w) && minecpp_wr_ok(w);
}

bool Decode(SpawnMob &p, minecpp_reader_t *r) {
  uint16_t vx = 0, vy = 0, vz = 0;
  uint8_t t = 0, yw = 0, pi = 0, hd = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &t) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &yw) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &pi) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &hd) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &vx) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &vy) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &vz) != MINECPP_BUF_OK) return false;
  p.type = t;
  p.yaw = yw;
  p.pitch = pi;
  p.head = hd;
  p.vx = (int16_t)vx;
  p.vy = (int16_t)vy;
  p.vz = (int16_t)vz;
  return Decode(p.meta, r);
}

bool Encode(const SpawnPlayer &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_raw(w, p.uuid, 16);
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  minecpp_wr_u8(w, p.yaw);
  minecpp_wr_u8(w, p.pitch);
  minecpp_wr_u16(w, (uint16_t)p.held);
  return Encode(p.meta, w) && minecpp_wr_ok(w);
}

bool Decode(SpawnPlayer &p, minecpp_reader_t *r) {
  uint16_t held = 0;
  uint8_t yw = 0, pi = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_raw(r, p.uuid, 16) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &yw) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &pi) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &held) != MINECPP_BUF_OK) return false;
  p.yaw = yw;
  p.pitch = pi;
  p.held = (int16_t)held;
  return Decode(p.meta, r);
}

bool Encode(const SpawnObject &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u8(w, p.type);
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  minecpp_wr_u8(w, p.pitch);
  minecpp_wr_u8(w, p.yaw);
  minecpp_wr_i32(w, p.data);
  if (p.has_vel || p.data != 0) {
    minecpp_wr_u16(w, (uint16_t)p.vx);
    minecpp_wr_u16(w, (uint16_t)p.vy);
    minecpp_wr_u16(w, (uint16_t)p.vz);
  }
  return minecpp_wr_ok(w);
}

bool Decode(SpawnObject &p, minecpp_reader_t *r) {
  uint8_t t = 0, pi = 0, yw = 0;
  uint16_t vx = 0, vy = 0, vz = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &t) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &pi) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &yw) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.data) != MINECPP_BUF_OK) return false;
  p.type = t;
  p.pitch = pi;
  p.yaw = yw;
  p.has_vel = p.data != 0;
  if (p.has_vel) {
    if (minecpp_rd_u16(r, &vx) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_u16(r, &vy) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_u16(r, &vz) != MINECPP_BUF_OK) return false;
    p.vx = (int16_t)vx;
    p.vy = (int16_t)vy;
    p.vz = (int16_t)vz;
  }
  return true;
}

bool Encode(const DestroyEntities &p, minecpp_writer_t *w) {
  if (p.eids.size() > 256) return false;
  minecpp_wr_varint(w, (int32_t)p.eids.size());
  for (int32_t e : p.eids) minecpp_wr_varint(w, e);
  return minecpp_wr_ok(w);
}

bool Decode(DestroyEntities &p, minecpp_reader_t *r) {
  int32_t n = 0;
  if (minecpp_rd_varint(r, &n) != MINECPP_BUF_OK || n < 0 || n > 256)
    return false;
  p.eids.clear();
  for (int32_t i = 0; i < n; i++) {
    int32_t e = 0;
    if (minecpp_rd_varint(r, &e) != MINECPP_BUF_OK) return false;
    p.eids.push_back(e);
  }
  return true;
}

bool Encode(const EntityTeleport &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  minecpp_wr_u8(w, p.yaw);
  minecpp_wr_u8(w, p.pitch);
  minecpp_wr_bool(w, p.on_ground);
  return minecpp_wr_ok(w);
}

bool Decode(EntityTeleport &p, minecpp_reader_t *r) {
  uint8_t yw = 0, pi = 0;
  int g = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &yw) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &pi) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_bool(r, &g) != MINECPP_BUF_OK) return false;
  p.yaw = yw;
  p.pitch = pi;
  p.on_ground = g != 0;
  return true;
}

bool Encode(const EntityRelMove &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u8(w, (uint8_t)p.dx);
  minecpp_wr_u8(w, (uint8_t)p.dy);
  minecpp_wr_u8(w, (uint8_t)p.dz);
  minecpp_wr_bool(w, p.on_ground);
  return minecpp_wr_ok(w);
}

bool Decode(EntityRelMove &p, minecpp_reader_t *r) {
  uint8_t x = 0, y = 0, z = 0;
  int g = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_bool(r, &g) != MINECPP_BUF_OK) return false;
  p.dx = (int8_t)x;
  p.dy = (int8_t)y;
  p.dz = (int8_t)z;
  p.on_ground = g != 0;
  return true;
}

bool Encode(const EntityLook &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u8(w, p.yaw);
  minecpp_wr_u8(w, p.pitch);
  minecpp_wr_bool(w, p.on_ground);
  return minecpp_wr_ok(w);
}

bool Decode(EntityLook &p, minecpp_reader_t *r) {
  uint8_t yw = 0, pi = 0;
  int g = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &yw) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &pi) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_bool(r, &g) != MINECPP_BUF_OK) return false;
  p.yaw = yw;
  p.pitch = pi;
  p.on_ground = g != 0;
  return true;
}

bool Encode(const EntityRelMoveLook &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u8(w, (uint8_t)p.dx);
  minecpp_wr_u8(w, (uint8_t)p.dy);
  minecpp_wr_u8(w, (uint8_t)p.dz);
  minecpp_wr_u8(w, p.yaw);
  minecpp_wr_u8(w, p.pitch);
  minecpp_wr_bool(w, p.on_ground);
  return minecpp_wr_ok(w);
}

bool Decode(EntityRelMoveLook &p, minecpp_reader_t *r) {
  uint8_t x = 0, y = 0, z = 0, yw = 0, pi = 0;
  int g = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &yw) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &pi) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_bool(r, &g) != MINECPP_BUF_OK) return false;
  p.dx = (int8_t)x;
  p.dy = (int8_t)y;
  p.dz = (int8_t)z;
  p.yaw = yw;
  p.pitch = pi;
  p.on_ground = g != 0;
  return true;
}

bool Encode(const Velocity &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u16(w, (uint16_t)p.vx);
  minecpp_wr_u16(w, (uint16_t)p.vy);
  minecpp_wr_u16(w, (uint16_t)p.vz);
  return minecpp_wr_ok(w);
}

bool Decode(Velocity &p, minecpp_reader_t *r) {
  uint16_t x = 0, y = 0, z = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u16(r, &z) != MINECPP_BUF_OK) return false;
  p.vx = (int16_t)x;
  p.vy = (int16_t)y;
  p.vz = (int16_t)z;
  return true;
}

bool Encode(const HeadLook &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u8(w, p.head);
  return minecpp_wr_ok(w);
}

bool Decode(HeadLook &p, minecpp_reader_t *r) {
  uint8_t h = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &h) != MINECPP_BUF_OK) return false;
  p.head = h;
  return true;
}

bool Encode(const EntityStatus &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.eid);
  minecpp_wr_u8(w, p.status);
  return minecpp_wr_ok(w);
}

bool Decode(EntityStatus &p, minecpp_reader_t *r) {
  uint8_t s = 0;
  if (minecpp_rd_i32(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &s) != MINECPP_BUF_OK) return false;
  p.status = s;
  return true;
}

bool Encode(const Equipment &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_varint(w, p.slot);
  return Encode(p.item, w) && minecpp_wr_ok(w);
}

bool Decode(Equipment &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.slot) != MINECPP_BUF_OK) return false;
  return Decode(p.item, r);
}

bool Encode(const CollectItem &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.collector);
  minecpp_wr_varint(w, p.collected);
  return minecpp_wr_ok(w);
}

bool Decode(CollectItem &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.collector) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.collected) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const ExpOrb &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  minecpp_wr_varint(w, p.count);
  return minecpp_wr_ok(w);
}

bool Decode(ExpOrb &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.count) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const UseEntity &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.target);
  minecpp_wr_varint(w, p.action);
  if (p.action == 2) {
    minecpp_wr_f32(w, p.hx);
    minecpp_wr_f32(w, p.hy);
    minecpp_wr_f32(w, p.hz);
  }
  return minecpp_wr_ok(w);
}

bool Decode(UseEntity &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.target) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.action) != MINECPP_BUF_OK) return false;
  p.hx = p.hy = p.hz = 0;
  if (p.action == 2) {
    if (minecpp_rd_f32(r, &p.hx) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_f32(r, &p.hy) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_f32(r, &p.hz) != MINECPP_BUF_OK) return false;
  }
  return true;
}

static bool WrMetaVal(minecpp_writer_t *w, const MetaEntry &e) {
  switch (e.type) {
    case 0:
      minecpp_wr_u8(w, (uint8_t)e.i);
      return minecpp_wr_ok(w);
    case 1:
      minecpp_wr_u16(w, (uint16_t)e.i);
      return minecpp_wr_ok(w);
    case 2:
      minecpp_wr_i32(w, (int32_t)e.i);
      return minecpp_wr_ok(w);
    case 3: {
      const float f = (float)e.f;
      minecpp_wr_f32(w, f);
      return minecpp_wr_ok(w);
    }
    case 4:
      minecpp_wr_strn(w, e.s.data(), e.s.size());
      return minecpp_wr_ok(w);
    case 5: {
      minecpp_writer_t sw{nullptr, 0, 0, 0};
      if (!Encode(e.item, &sw)) {
        free(sw.buf);
        return false;
      }
      minecpp_wr_raw(w, sw.buf, sw.len);
      free(sw.buf);
      return minecpp_wr_ok(w);
    }
    case 6:
      minecpp_wr_i32(w, e.px);
      minecpp_wr_i32(w, e.py);
      minecpp_wr_i32(w, e.pz);
      return minecpp_wr_ok(w);
    case 7:
      minecpp_wr_f32(w, e.rx);
      minecpp_wr_f32(w, e.ry);
      minecpp_wr_f32(w, e.rz);
      return minecpp_wr_ok(w);
    default:
      return false;
  }
}

bool Encode(const Metadata &p, minecpp_writer_t *w) {
  for (const auto &e : p.entries) {
    if (e.index > 31 || e.type > 7) return false;
    minecpp_wr_u8(w, (uint8_t)((e.index & 31) | (e.type << 5)));
    if (!WrMetaVal(w, e)) return false;
  }
  minecpp_wr_u8(w, 0x7F);
  return minecpp_wr_ok(w);
}

static bool RdMetaVal(minecpp_reader_t *r, uint8_t type, MetaEntry *e) {
  uint16_t u16 = 0;
  switch (type) {
    case 0: {
      uint8_t b = 0;
      if (minecpp_rd_u8(r, &b) != MINECPP_BUF_OK) return false;
      e->i = b;
      return true;
    }
    case 1:
      if (minecpp_rd_u16(r, &u16) != MINECPP_BUF_OK) return false;
      e->i = (int16_t)u16;
      return true;
    case 2: {
      int32_t v = 0;
      if (minecpp_rd_i32(r, &v) != MINECPP_BUF_OK) return false;
      e->i = v;
      return true;
    }
    case 3: {
      float f = 0;
      if (minecpp_rd_f32(r, &f) != MINECPP_BUF_OK) return false;
      e->f = f;
      return true;
    }
    case 4: {
      char *s = nullptr;
      int n = 0;
      if (minecpp_rd_string(r, 32767, &s, &n) != MINECPP_BUF_OK) return false;
      e->s.assign(s, (size_t)n);
      free(s);
      return true;
    }
    case 5:
      return Decode(e->item, r);
    case 6:
      if (minecpp_rd_i32(r, &e->px) != MINECPP_BUF_OK) return false;
      if (minecpp_rd_i32(r, &e->py) != MINECPP_BUF_OK) return false;
      if (minecpp_rd_i32(r, &e->pz) != MINECPP_BUF_OK) return false;
      return true;
    case 7:
      if (minecpp_rd_f32(r, &e->rx) != MINECPP_BUF_OK) return false;
      if (minecpp_rd_f32(r, &e->ry) != MINECPP_BUF_OK) return false;
      if (minecpp_rd_f32(r, &e->rz) != MINECPP_BUF_OK) return false;
      return true;
    default:
      return false;
  }
}

bool Decode(Metadata &p, minecpp_reader_t *r) {
  p.entries.clear();
  for (int guard = 0; guard < 64; guard++) {
    uint8_t k = 0;
    if (minecpp_rd_u8(r, &k) != MINECPP_BUF_OK) return false;
    if (k == 0x7F) return true;
    MetaEntry e;
    e.index = k & 31;
    e.type = k >> 5;
    if (!RdMetaVal(r, e.type, &e)) return false;
    p.entries.push_back(std::move(e));
  }
  return false;  // sem terminador em 64 entradas = corrupt
}

bool Encode(const Properties &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_i32(w, (int32_t)p.props.size());
  for (const auto &pr : p.props) {
    minecpp_wr_strn(w, pr.key.data(), pr.key.size());
    minecpp_wr_f64(w, pr.value);
    minecpp_wr_varint(w, (int32_t)pr.mods.size());
    for (const auto &m : pr.mods) {
      minecpp_wr_raw(w, m.uuid, 16);
      minecpp_wr_f64(w, m.amount);
      minecpp_wr_u8(w, (uint8_t)m.op);
    }
  }
  return minecpp_wr_ok(w);
}

bool Decode(Properties &p, minecpp_reader_t *r) {
  int32_t n = 0;
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &n) != MINECPP_BUF_OK || n < 0 || n > 64) return false;
  p.props.clear();
  for (int32_t i = 0; i < n; i++) {
    Property pr;
    char *k = nullptr;
    int nm = 0;
    if (minecpp_rd_string(r, 64, &k, nullptr) != MINECPP_BUF_OK) return false;
    pr.key = k;
    free(k);
    if (minecpp_rd_f64(r, &pr.value) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_varint(r, &nm) != MINECPP_BUF_OK || nm < 0 || nm > 16)
      return false;
    for (int32_t m = 0; m < nm; m++) {
      PropertyMod mod;
      uint8_t op = 0;
      if (minecpp_rd_raw(r, mod.uuid, 16) != MINECPP_BUF_OK) return false;
      if (minecpp_rd_f64(r, &mod.amount) != MINECPP_BUF_OK) return false;
      if (minecpp_rd_u8(r, &op) != MINECPP_BUF_OK) return false;
      mod.op = (int8_t)op;
      pr.mods.push_back(mod);
    }
    p.props.push_back(std::move(pr));
  }
  return true;
}

// ---- P1 faltando: Clientbound encode/decode ----

bool Encode(const Respawn &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.dimension);
  minecpp_wr_u8(w, p.difficulty);
  minecpp_wr_u8(w, p.gamemode);
  minecpp_wr_strn(w, p.level_type.data(), p.level_type.size());
  return minecpp_wr_ok(w);
}

bool Decode(Respawn &p, minecpp_reader_t *r) {
  if (minecpp_rd_i32(r, &p.dimension) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.difficulty) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.gamemode) != MINECPP_BUF_OK) return false;
  char *s = nullptr;
  if (minecpp_rd_string(r, 16, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.level_type = s;
  free(s);
  return true;
}

bool Encode(const UseBed &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  return minecpp_wr_ok(w);
}

bool Decode(UseBed &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const SpawnPainting &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_strn(w, p.title.data(), p.title.size());
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  minecpp_wr_u8(w, p.direction);
  return minecpp_wr_ok(w);
}

bool Decode(SpawnPainting &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  char *s = nullptr;
  if (minecpp_rd_string(r, 13, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.title = s;
  free(s);
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.direction) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const AttachEntity &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.vehicle);
  minecpp_wr_i32(w, p.rider);
  minecpp_wr_u8(w, p.leash ? 1 : 0);
  return minecpp_wr_ok(w);
}

bool Decode(AttachEntity &p, minecpp_reader_t *r) {
  if (minecpp_rd_i32(r, &p.vehicle) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.rider) != MINECPP_BUF_OK) return false;
  uint8_t l = 0;
  if (minecpp_rd_u8(r, &l) != MINECPP_BUF_OK) return false;
  p.leash = (l != 0);
  return true;
}

bool Encode(const EntityEffect &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u8(w, p.effect_id);
  minecpp_wr_u8(w, p.amplifier);
  minecpp_wr_varint(w, p.duration);
  minecpp_wr_u8(w, p.hide_particles ? 1 : 0);
  return minecpp_wr_ok(w);
}

bool Decode(EntityEffect &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.effect_id) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.amplifier) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.duration) != MINECPP_BUF_OK) return false;
  uint8_t h = 0;
  if (minecpp_rd_u8(r, &h) != MINECPP_BUF_OK) return false;
  p.hide_particles = (h != 0);
  return true;
}

bool Encode(const RemoveEntityEffect &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u8(w, p.effect_id);
  return minecpp_wr_ok(w);
}

bool Decode(RemoveEntityEffect &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.effect_id) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const EntityRelMove0 &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  return minecpp_wr_ok(w);
}

bool Decode(EntityRelMove0 &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const MultiBlockChange &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.chunk_x);
  minecpp_wr_i32(w, p.chunk_z);
  minecpp_wr_varint(w, (int32_t)p.records.size());
  for (const auto &rec : p.records) {
    minecpp_wr_u16(w, rec.packed);
    minecpp_wr_varint(w, rec.block_id);
  }
  return minecpp_wr_ok(w);
}

bool Decode(MultiBlockChange &p, minecpp_reader_t *r) {
  if (minecpp_rd_i32(r, &p.chunk_x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.chunk_z) != MINECPP_BUF_OK) return false;
  int32_t n = 0;
  if (minecpp_rd_varint(r, &n) != MINECPP_BUF_OK || n < 0 || n > 4096) return false;
  p.records.clear();
  p.records.reserve(n);
  for (int32_t i = 0; i < n; i++) {
    MultiBlockChange::Record rec;
    if (minecpp_rd_u16(r, &rec.packed) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_varint(r, &rec.block_id) != MINECPP_BUF_OK) return false;
    p.records.push_back(rec);
  }
  return true;
}

bool Encode(const Explosion &p, minecpp_writer_t *w) {
  minecpp_wr_f32(w, p.x);
  minecpp_wr_f32(w, p.y);
  minecpp_wr_f32(w, p.z);
  minecpp_wr_f32(w, p.strength);
  minecpp_wr_varint(w, (int32_t)p.records.size());
  for (const auto &rec : p.records) {
    minecpp_wr_u8(w, (uint8_t)rec.dx);
    minecpp_wr_u8(w, (uint8_t)rec.dy);
    minecpp_wr_u8(w, (uint8_t)rec.dz);
  }
  minecpp_wr_f32(w, p.player_motion_x);
  minecpp_wr_f32(w, p.player_motion_y);
  minecpp_wr_f32(w, p.player_motion_z);
  return minecpp_wr_ok(w);
}

bool Decode(Explosion &p, minecpp_reader_t *r) {
  if (minecpp_rd_f32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.strength) != MINECPP_BUF_OK) return false;
  int32_t n = 0;
  if (minecpp_rd_varint(r, &n) != MINECPP_BUF_OK || n < 0 || n > 4096) return false;
  p.records.clear();
  p.records.reserve(n);
  for (int32_t i = 0; i < n; i++) {
    Explosion::Offset rec;
    uint8_t dx = 0, dy = 0, dz = 0;
    if (minecpp_rd_u8(r, &dx) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_u8(r, &dy) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_u8(r, &dz) != MINECPP_BUF_OK) return false;
    rec.dx = (int8_t)dx;
    rec.dy = (int8_t)dy;
    rec.dz = (int8_t)dz;
    p.records.push_back(rec);
  }
  if (minecpp_rd_f32(r, &p.player_motion_x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.player_motion_y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.player_motion_z) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const Particle &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.id);
  minecpp_wr_u8(w, p.long_distance ? 1 : 0);
  minecpp_wr_f32(w, p.x);
  minecpp_wr_f32(w, p.y);
  minecpp_wr_f32(w, p.z);
  minecpp_wr_f32(w, p.ox);
  minecpp_wr_f32(w, p.oy);
  minecpp_wr_f32(w, p.oz);
  minecpp_wr_f32(w, p.speed);
  minecpp_wr_varint(w, p.count);
  for (int32_t d : p.data) {
    minecpp_wr_varint(w, d);
  }
  return minecpp_wr_ok(w);
}

bool Decode(Particle &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.id) != MINECPP_BUF_OK) return false;
  uint8_t ld = 0;
  if (minecpp_rd_u8(r, &ld) != MINECPP_BUF_OK) return false;
  p.long_distance = (ld != 0);
  if (minecpp_rd_f32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.ox) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.oy) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.oz) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.speed) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.count) != MINECPP_BUF_OK) return false;
  p.data.clear();
  p.data.reserve(p.count);
  for (int32_t i = 0; i < p.count; i++) {
    int32_t d = 0;
    if (minecpp_rd_varint(r, &d) != MINECPP_BUF_OK) return false;
    p.data.push_back(d);
  }
  return true;
}

bool Encode(const ChangeGameState &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.reason);
  minecpp_wr_f32(w, p.value);
  return minecpp_wr_ok(w);
}

bool Decode(ChangeGameState &p, minecpp_reader_t *r) {
  if (minecpp_rd_u8(r, &p.reason) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.value) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const SpawnGlobalEntity &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_u8(w, p.type);
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  return minecpp_wr_ok(w);
}

bool Decode(SpawnGlobalEntity &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.type) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const UpdateSign &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  for (int i = 0; i < 4; i++) {
    minecpp_wr_strn(w, p.lines[i].data(), p.lines[i].size());
  }
  return minecpp_wr_ok(w);
}

bool Decode(UpdateSign &p, minecpp_reader_t *r) {
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  for (int i = 0; i < 4; i++) {
    char *s = nullptr;
    if (minecpp_rd_string(r, 15, &s, nullptr) != MINECPP_BUF_OK) return false;
    p.lines[i] = s;
    free(s);
  }
  return true;
}

bool Encode(const UpdateBlockEntity &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  minecpp_wr_u8(w, p.action);
  // NBT encoding seria aqui - por enquanto placeholder
  minecpp_wr_u8(w, 0);  // tag end
  return minecpp_wr_ok(w);
}

bool Decode(UpdateBlockEntity &p, minecpp_reader_t *r) {
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.action) != MINECPP_BUF_OK) return false;
  // NBT decoding seria aqui - por enquanto pula
  uint8_t tag = 0;
  if (minecpp_rd_u8(r, &tag) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const SignEditorOpen &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  return minecpp_wr_ok(w);
}

bool Decode(SignEditorOpen &p, minecpp_reader_t *r) {
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const CombatEvent &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.event);
  if (p.event == 1) {  // end combat
    minecpp_wr_varint(w, p.duration);
    minecpp_wr_varint(w, p.entity_id);
  } else if (p.event == 2) {  // entity dead
    minecpp_wr_varint(w, p.player_id);
    minecpp_wr_varint(w, p.entity_id);
    minecpp_wr_strn(w, p.death_message.data(), p.death_message.size());
  }
  return minecpp_wr_ok(w);
}

bool Decode(CombatEvent &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.event) != MINECPP_BUF_OK) return false;
  if (p.event == 1) {  // end combat
    if (minecpp_rd_varint(r, &p.duration) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_varint(r, &p.entity_id) != MINECPP_BUF_OK) return false;
  } else if (p.event == 2) {  // entity dead
    if (minecpp_rd_varint(r, &p.player_id) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_varint(r, &p.entity_id) != MINECPP_BUF_OK) return false;
    char *s = nullptr;
    if (minecpp_rd_string(r, 256, &s, nullptr) != MINECPP_BUF_OK) return false;
    p.death_message = s;
    free(s);
  }
  return true;
}

// ---- P1 faltando: Serverbound encode/decode ----

bool Encode(const EntityAction &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.eid);
  minecpp_wr_varint(w, p.action);
  minecpp_wr_varint(w, p.jump_boost);
  return minecpp_wr_ok(w);
}

bool Decode(EntityAction &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.eid) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.action) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.jump_boost) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const SteerVehicle &p, minecpp_writer_t *w) {
  minecpp_wr_f32(w, p.sideways);
  minecpp_wr_f32(w, p.forward);
  minecpp_wr_u8(w, p.flags);
  return minecpp_wr_ok(w);
}

bool Decode(SteerVehicle &p, minecpp_reader_t *r) {
  if (minecpp_rd_f32(r, &p.sideways) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.forward) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.flags) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const EnchantItem &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.window);
  minecpp_wr_u8(w, p.enchantment);
  return minecpp_wr_ok(w);
}

bool Decode(EnchantItem &p, minecpp_reader_t *r) {
  if (minecpp_rd_u8(r, &p.window) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.enchantment) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const SbUpdateSign &p, minecpp_writer_t *w) {
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.y);
  minecpp_wr_i32(w, p.z);
  for (int i = 0; i < 4; i++) {
    minecpp_wr_strn(w, p.lines[i].data(), p.lines[i].size());
  }
  return minecpp_wr_ok(w);
}

bool Decode(SbUpdateSign &p, minecpp_reader_t *r) {
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  for (int i = 0; i < 4; i++) {
    char *s = nullptr;
    if (minecpp_rd_string(r, 15, &s, nullptr) != MINECPP_BUF_OK) return false;
    p.lines[i] = s;
    free(s);
  }
  return true;
}

bool Encode(const SbPlayerAbilities &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.flags);
  minecpp_wr_f32(w, p.fly_speed);
  minecpp_wr_f32(w, p.walk_speed);
  return minecpp_wr_ok(w);
}

bool Decode(SbPlayerAbilities &p, minecpp_reader_t *r) {
  if (minecpp_rd_u8(r, &p.flags) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.fly_speed) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_f32(r, &p.walk_speed) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const ClientSettings &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.locale.data(), p.locale.size());
  minecpp_wr_u8(w, p.view_distance);
  minecpp_wr_varint(w, p.chat_mode);
  minecpp_wr_u8(w, p.chat_colors ? 1 : 0);
  minecpp_wr_u8(w, p.skin_parts);
  minecpp_wr_varint(w, p.main_hand);
  return minecpp_wr_ok(w);
}

bool Decode(ClientSettings &p, minecpp_reader_t *r) {
  char *s = nullptr;
  if (minecpp_rd_string(r, 16, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.locale = s;
  free(s);
  if (minecpp_rd_u8(r, &p.view_distance) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.chat_mode) != MINECPP_BUF_OK) return false;
  uint8_t cc = 0;
  if (minecpp_rd_u8(r, &cc) != MINECPP_BUF_OK) return false;
  p.chat_colors = (cc != 0);
  if (minecpp_rd_u8(r, &p.skin_parts) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.main_hand) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const ClientStatus &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.action);
  return minecpp_wr_ok(w);
}

bool Decode(ClientStatus &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.action) != MINECPP_BUF_OK) return false;
  return true;
}

// ---- P2: Clientbound encode/decode ----

bool Encode(const MapData &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.map_id);
  minecpp_wr_u8(w, p.scale);
  minecpp_wr_varint(w, (int32_t)p.icons.size());
  for (int32_t icon : p.icons) {
    minecpp_wr_varint(w, icon);
  }
  minecpp_wr_varint(w, p.columns);
  minecpp_wr_varint(w, p.rows);
  minecpp_wr_i32(w, p.x);
  minecpp_wr_i32(w, p.z);
  minecpp_wr_varint(w, (int32_t)p.data.size());
  for (uint8_t b : p.data) {
    minecpp_wr_u8(w, b);
  }
  return minecpp_wr_ok(w);
}

bool Decode(MapData &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.map_id) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_u8(r, &p.scale) != MINECPP_BUF_OK) return false;
  int32_t n = 0;
  if (minecpp_rd_varint(r, &n) != MINECPP_BUF_OK || n < 0 || n > 256) return false;
  p.icons.clear();
  p.icons.reserve(n);
  for (int32_t i = 0; i < n; i++) {
    int32_t icon = 0;
    if (minecpp_rd_varint(r, &icon) != MINECPP_BUF_OK) return false;
    p.icons.push_back(icon);
  }
  if (minecpp_rd_varint(r, &p.columns) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_varint(r, &p.rows) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  int32_t dlen = 0;
  if (minecpp_rd_varint(r, &dlen) != MINECPP_BUF_OK || dlen < 0 || dlen > 16384) return false;
  p.data.resize(dlen);
  for (int32_t i = 0; i < dlen; i++) {
    uint8_t b = 0;
    if (minecpp_rd_u8(r, &b) != MINECPP_BUF_OK) return false;
    p.data[i] = b;
  }
  return true;
}

bool Encode(const CbTabComplete &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, (int32_t)p.matches.size());
  for (const auto &s : p.matches) {
    minecpp_wr_strn(w, s.data(), s.size());
  }
  return minecpp_wr_ok(w);
}

bool Decode(CbTabComplete &p, minecpp_reader_t *r) {
  int32_t n = 0;
  if (minecpp_rd_varint(r, &n) != MINECPP_BUF_OK || n < 0 || n > 100) return false;
  p.matches.clear();
  p.matches.reserve(n);
  for (int32_t i = 0; i < n; i++) {
    char *s = nullptr;
    if (minecpp_rd_string(r, 256, &s, nullptr) != MINECPP_BUF_OK) return false;
    p.matches.push_back(s);
    free(s);
  }
  return true;
}

bool Encode(const ScoreboardObjective &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.name.data(), p.name.size());
  minecpp_wr_strn(w, p.value.data(), p.value.size());
  minecpp_wr_u8(w, p.action);
  return minecpp_wr_ok(w);
}

bool Decode(ScoreboardObjective &p, minecpp_reader_t *r) {
  char *s = nullptr;
  if (minecpp_rd_string(r, 16, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.name = s;
  free(s);
  if (minecpp_rd_string(r, 32, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.value = s;
  free(s);
  if (minecpp_rd_u8(r, &p.action) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const UpdateScore &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.name.data(), p.name.size());
  minecpp_wr_u8(w, p.action);
  minecpp_wr_strn(w, p.objective.data(), p.objective.size());
  if (p.action == 0) {
    minecpp_wr_varint(w, p.value);
  }
  return minecpp_wr_ok(w);
}

bool Decode(UpdateScore &p, minecpp_reader_t *r) {
  char *s = nullptr;
  if (minecpp_rd_string(r, 40, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.name = s;
  free(s);
  if (minecpp_rd_u8(r, &p.action) != MINECPP_BUF_OK) return false;
  if (minecpp_rd_string(r, 16, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.objective = s;
  free(s);
  if (p.action == 0) {
    if (minecpp_rd_varint(r, &p.value) != MINECPP_BUF_OK) return false;
  }
  return true;
}

bool Encode(const DisplayScoreboard &p, minecpp_writer_t *w) {
  minecpp_wr_u8(w, p.position);
  minecpp_wr_strn(w, p.name.data(), p.name.size());
  return minecpp_wr_ok(w);
}

bool Decode(DisplayScoreboard &p, minecpp_reader_t *r) {
  if (minecpp_rd_u8(r, &p.position) != MINECPP_BUF_OK) return false;
  char *s = nullptr;
  if (minecpp_rd_string(r, 16, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.name = s;
  free(s);
  return true;
}

bool Encode(const Teams &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.name.data(), p.name.size());
  minecpp_wr_u8(w, p.mode);
  if (p.mode == 0 || p.mode == 2) {  // create or update
    minecpp_wr_strn(w, p.display_name.data(), p.display_name.size());
    minecpp_wr_strn(w, p.prefix.data(), p.prefix.size());
    minecpp_wr_strn(w, p.suffix.data(), p.suffix.size());
    minecpp_wr_u8(w, p.friendly_fire);
    minecpp_wr_u8(w, p.name_tag_visibility);
    minecpp_wr_u8(w, p.color);
  }
  if (p.mode == 0 || p.mode == 3 || p.mode == 4) {  // create, add players, remove players
    minecpp_wr_varint(w, (int32_t)p.players.size());
    for (const auto &pl : p.players) {
      minecpp_wr_strn(w, pl.data(), pl.size());
    }
  }
  return minecpp_wr_ok(w);
}

bool Decode(Teams &p, minecpp_reader_t *r) {
  char *s = nullptr;
  if (minecpp_rd_string(r, 16, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.name = s;
  free(s);
  if (minecpp_rd_u8(r, &p.mode) != MINECPP_BUF_OK) return false;
  if (p.mode == 0 || p.mode == 2) {
    if (minecpp_rd_string(r, 32, &s, nullptr) != MINECPP_BUF_OK) return false;
    p.display_name = s;
    free(s);
    if (minecpp_rd_string(r, 16, &s, nullptr) != MINECPP_BUF_OK) return false;
    p.prefix = s;
    free(s);
    if (minecpp_rd_string(r, 16, &s, nullptr) != MINECPP_BUF_OK) return false;
    p.suffix = s;
    free(s);
    if (minecpp_rd_u8(r, &p.friendly_fire) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_u8(r, &p.name_tag_visibility) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_u8(r, &p.color) != MINECPP_BUF_OK) return false;
  }
  if (p.mode == 0 || p.mode == 3 || p.mode == 4) {
    int32_t n = 0;
    if (minecpp_rd_varint(r, &n) != MINECPP_BUF_OK || n < 0 || n > 200) return false;
    p.players.clear();
    p.players.reserve(n);
    for (int32_t i = 0; i < n; i++) {
      if (minecpp_rd_string(r, 16, &s, nullptr) != MINECPP_BUF_OK) return false;
      p.players.push_back(s);
      free(s);
    }
  }
  return true;
}

bool Encode(const Camera &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.entity_id);
  return minecpp_wr_ok(w);
}

bool Decode(Camera &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.entity_id) != MINECPP_BUF_OK) return false;
  return true;
}

bool Encode(const Title &p, minecpp_writer_t *w) {
  minecpp_wr_varint(w, p.action);
  if (p.action == 0 || p.action == 1) {  // title or subtitle
    minecpp_wr_strn(w, p.text.data(), p.text.size());
  } else if (p.action == 2) {  // times
    minecpp_wr_i32(w, p.fade_in);
    minecpp_wr_i32(w, p.stay);
    minecpp_wr_i32(w, p.fade_out);
  }
  return minecpp_wr_ok(w);
}

bool Decode(Title &p, minecpp_reader_t *r) {
  if (minecpp_rd_varint(r, &p.action) != MINECPP_BUF_OK) return false;
  if (p.action == 0 || p.action == 1) {
    char *s = nullptr;
    if (minecpp_rd_string(r, 256, &s, nullptr) != MINECPP_BUF_OK) return false;
    p.text = s;
    free(s);
  } else if (p.action == 2) {
    if (minecpp_rd_i32(r, &p.fade_in) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_i32(r, &p.stay) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_i32(r, &p.fade_out) != MINECPP_BUF_OK) return false;
  }
  return true;
}

bool Encode(const PlayerListHeaderFooter &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.header.data(), p.header.size());
  minecpp_wr_strn(w, p.footer.data(), p.footer.size());
  return minecpp_wr_ok(w);
}

bool Decode(PlayerListHeaderFooter &p, minecpp_reader_t *r) {
  char *s = nullptr;
  if (minecpp_rd_string(r, 256, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.header = s;
  free(s);
  if (minecpp_rd_string(r, 256, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.footer = s;
  free(s);
  return true;
}

bool Encode(const ResourcePackSend &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.url.data(), p.url.size());
  minecpp_wr_strn(w, p.hash.data(), p.hash.size());
  return minecpp_wr_ok(w);
}

bool Decode(ResourcePackSend &p, minecpp_reader_t *r) {
  char *s = nullptr;
  if (minecpp_rd_string(r, 256, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.url = s;
  free(s);
  if (minecpp_rd_string(r, 40, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.hash = s;
  free(s);
  return true;
}

// ---- P2: Serverbound encode/decode ----

bool Encode(const SbTabComplete &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.text.data(), p.text.size());
  minecpp_wr_u8(w, p.has_position ? 1 : 0);
  if (p.has_position) {
    minecpp_wr_i32(w, p.x);
    minecpp_wr_i32(w, p.y);
    minecpp_wr_i32(w, p.z);
  }
  return minecpp_wr_ok(w);
}

bool Decode(SbTabComplete &p, minecpp_reader_t *r) {
  char *s = nullptr;
  if (minecpp_rd_string(r, 256, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.text = s;
  free(s);
  uint8_t hp = 0;
  if (minecpp_rd_u8(r, &hp) != MINECPP_BUF_OK) return false;
  p.has_position = (hp != 0);
  if (p.has_position) {
    if (minecpp_rd_i32(r, &p.x) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_i32(r, &p.y) != MINECPP_BUF_OK) return false;
    if (minecpp_rd_i32(r, &p.z) != MINECPP_BUF_OK) return false;
  }
  return true;
}

bool Encode(const ResourcePackStatus &p, minecpp_writer_t *w) {
  minecpp_wr_strn(w, p.hash.data(), p.hash.size());
  minecpp_wr_varint(w, p.result);
  return minecpp_wr_ok(w);
}

bool Decode(ResourcePackStatus &p, minecpp_reader_t *r) {
  char *s = nullptr;
  if (minecpp_rd_string(r, 40, &s, nullptr) != MINECPP_BUF_OK) return false;
  p.hash = s;
  free(s);
  if (minecpp_rd_varint(r, &p.result) != MINECPP_BUF_OK) return false;
  return true;
}

}  // namespace minecpp::v18::proto
