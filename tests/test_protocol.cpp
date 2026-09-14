// Testes protocolo 1.8: goldens do vanilla real + round-trips + framing.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <zlib.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "minecpp/core/buf.h"
#include "minecpp/core/crypto.h"
#include "minecpp/core/nbt.h"
#include "minecpp/core/region.h"
#include "minecpp/v1_8/chunk.h"
#include "minecpp/v1_8/protocol.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "test-data/vanilla-1.8-flat"
#endif
#define NETDIR TEST_DATA_DIR "/net/"

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

static uint8_t *ReadFile(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("FAIL: sem arquivo %s\n", path);
    g_fail++;
    return nullptr;
  }
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *b = (uint8_t *)malloc((size_t)n);
  if (fread(b, 1, (size_t)n, f) != (size_t)n) {
    fclose(f);
    free(b);
    return nullptr;
  }
  fclose(f);
  *len = (size_t)n;
  return b;
}

// Golden = conteúdo do frame (sem len externo). Com compressão ligada o
// vanilla prefixa dataLen (+zlib se >0); na fase de status não há prefixo.
// has_datalen=0 → devolve bytes crus (id+payload).
static uint8_t *Unwrap(const char *name, int has_datalen, size_t *out_n) {
  size_t n = 0;
  const std::string path = std::string(NETDIR) + name;
  uint8_t *raw = ReadFile(path.c_str(), &n);
  if (!raw) return nullptr;
  if (!has_datalen) {
    *out_n = n;
    return raw;
  }
  minecpp_reader_t r = {raw, n};
  int32_t dl = 0;
  if (minecpp_rd_varint(&r, &dl) != MINECPP_BUF_OK || dl < 0) {
    free(raw);
    return nullptr;
  }
  uint8_t *out = nullptr;
  if (dl == 0) {
    *out_n = r.left;
    out = (uint8_t *)malloc(r.left);
    memcpy(out, r.p, r.left);
  } else {
    uLongf cap = (uLongf)dl;
    out = (uint8_t *)malloc(cap + 1);
    if (uncompress(out, &cap, r.p, (uLong)r.left) != Z_OK) {
      free(out);
      out = nullptr;
    } else {
      *out_n = (size_t)cap;
    }
  }
  free(raw);
  return out;
}

static std::vector<uint8_t> EncodePayload(int32_t id, minecpp_writer_t *w) {
  minecpp_writer_t f = {nullptr, 0, 0, 0};
  FrameEncode(id, w->buf, w->len, &f);
  free(w->buf);
  std::vector<uint8_t> v(f.buf, f.buf + f.len);
  free(f.buf);
  return v;
}

// t1: handshake golden byte-exato (nosso encoder = bytes que o vanilla aceita).
static void t_handshake_golden() {
  Handshake h;
  h.proto = 47;
  h.host = "localhost";
  h.port = 25567;
  h.next = 1;
  minecpp_writer_t w = {nullptr, 0, 0, 0};
  ASSERT(Encode(h, &w));
  const std::vector<uint8_t> f = EncodePayload(kSbHandshake, &w);
  const uint8_t exp[] = {0x0F, 0x00, 0x2F, 0x09, 'l', 'o', 'c', 'a',
                         'l',  'h',  'o',  's',  't',  0x63, 0xDF, 0x01};
  ASSERT_EQ(f.size(), sizeof exp);
  ASSERT(f.size() == sizeof exp && memcmp(f.data(), exp, sizeof exp) == 0);
  // Decode de volta.
  minecpp_reader_t r = {f.data(), f.size()};
  int32_t id = -1;
  minecpp_reader_t pay{nullptr, 0};
  ASSERT(FrameDecode(&r, &id, &pay));
  ASSERT_EQ(id, 0);
  Handshake h2;
  ASSERT(Decode(h2, &pay));
  ASSERT_EQ(h2.proto, 47);
  ASSERT(h2.host == "localhost");
  ASSERT_EQ(h2.port, 25567);
  ASSERT_EQ(h2.next, 1);
  ASSERT_EQ(pay.left, 0u);
}

// t2: status/pong goldens do vanilla.
static void t_status_golden() {
  size_t n = 0;
  uint8_t *pkt = Unwrap("status_response.bin", 0, &n);
  ASSERT(pkt);
  if (!pkt) return;
  minecpp_reader_t r = {pkt, n};
  int32_t id = -1;
  ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
  ASSERT_EQ(id, kCbStatusResponse);
  StatusResponse s;
  ASSERT(Decode(s, &r));
  ASSERT(s.json.find("\"name\":\"1.8\"") != std::string::npos);
  ASSERT(s.json.find("\"protocol\":47") != std::string::npos);
  free(pkt);
  pkt = Unwrap("pong.bin", 0, &n);
  ASSERT(pkt);
  r.p = pkt;
  r.left = n;
  ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
  ASSERT_EQ(id, kCbStatusPong);
  Ping p;
  ASSERT(Decode(p, &r));
  ASSERT_EQ(p.payload, 123456789LL);
  free(pkt);
}

// t3: login packets round-trip + bytes do SetCompression.
static void t_login() {
  {
    LoginStart a;
    a.name = "Minecpp";
    minecpp_writer_t w = {nullptr, 0, 0, 0};
    ASSERT(Encode(a, &w));
    size_t n = 0;
    uint8_t *b = minecpp_wr_take(&w, &n);
    const uint8_t exp[] = {0x07, 'M', 'i', 'n', 'e', 'c', 'p', 'p'};
    ASSERT_EQ(n, sizeof exp);
    ASSERT(memcmp(b, exp, n) == 0);
    minecpp_reader_t r = {b, n};
    LoginStart c;
    ASSERT(Decode(c, &r));
    ASSERT(c.name == "Minecpp");
    free(b);
  }
  {
    LoginSuccess a;
    a.uuid = "069a79f4-44e9-4726-a5be-fca90e38aaf5";
    a.name = "Notch";
    minecpp_writer_t w = {nullptr, 0, 0, 0};
    ASSERT(Encode(a, &w));
    size_t n = 0;
    uint8_t *b = minecpp_wr_take(&w, &n);
    minecpp_reader_t r = {b, n};
    LoginSuccess c;
    ASSERT(Decode(c, &r));
    ASSERT(c.uuid == a.uuid && c.name == "Notch");
    ASSERT_EQ(r.left, 0u);
    free(b);
  }
  {
    SetCompression a;
    a.threshold = 256;
    minecpp_writer_t w = {nullptr, 0, 0, 0};
    ASSERT(Encode(a, &w));
    size_t n = 0;
    uint8_t *b = minecpp_wr_take(&w, &n);
    const uint8_t exp[] = {0x80, 0x02};
    ASSERT_EQ(n, sizeof exp);
    ASSERT(memcmp(b, exp, n) == 0);
    free(b);
  }
  {
    Disconnect a;
    a.reason = "{\"text\":\"hi\"}";
    minecpp_writer_t w = {nullptr, 0, 0, 0};
    ASSERT(Encode(a, &w));
    size_t n = 0;
    uint8_t *b = minecpp_wr_take(&w, &n);
    minecpp_reader_t r = {b, n};
    Disconnect c;
    ASSERT(Decode(c, &r));
    ASSERT(c.reason == a.reason);
    free(b);
  }
}

// t4: play goldens do vanilla.
static void t_play_goldens() {
  size_t n = 0;
  {
    uint8_t *pkt = Unwrap("joingame.bin", 1, &n);
    ASSERT(pkt);
    minecpp_reader_t r = {pkt, n};
    int32_t id = -1;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbJoinGame);
    JoinGame j;
    ASSERT(Decode(j, &r));
    ASSERT_EQ(j.eid, 74);
    ASSERT_EQ(j.mode, 0u);
    ASSERT_EQ(j.dimension, 0);
    ASSERT_EQ(j.difficulty, 1u);
    ASSERT_EQ(j.max_players, 20u);
    ASSERT(j.level_type == "flat");
    ASSERT_EQ(j.reduced_debug, false);
    free(pkt);
  }
  {
    uint8_t *pkt = Unwrap("spawnpos.bin", 1, &n);
    minecpp_reader_t r = {pkt, n};
    int32_t id = -1;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbSpawnPosition);
    SpawnPosition s;
    ASSERT(Decode(s, &r));
    ASSERT_EQ(s.x, 285);  // spawn aleatório do mundo flat capturado
    ASSERT_EQ(s.y, 4);
    ASSERT_EQ(s.z, 645);
    free(pkt);
  }
  {
    uint8_t *pkt = Unwrap("poslook.bin", 1, &n);
    minecpp_reader_t r = {pkt, n};
    int32_t id = -1;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbPlayerPosLook);
    PlayerPosLook p;
    ASSERT(Decode(p, &r));
    ASSERT(p.x > 280 && p.x < 281 && p.z > 645 && p.z < 646);
    ASSERT(p.y >= 4 && p.y < 5);
    ASSERT_EQ(p.yaw, 0.0f);
    ASSERT_EQ(p.pitch, 0.0f);
    ASSERT_EQ(p.flags, 0u);
    free(pkt);
  }
  {
    uint8_t *pkt = Unwrap("keepalive.bin", 1, &n);
    minecpp_reader_t r = {pkt, n};
    int32_t id = -1;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbKeepAlive);
    KeepAlive k;
    ASSERT(Decode(k, &r));
    ASSERT_EQ(r.left, 0u);
    free(pkt);
  }
}

// Carrega chunk vanilla real (-1,-12) como struct.
static Chunk *LoadRealChunk() {
  char tmp[256];
#ifdef _WIN32
  snprintf(tmp, sizeof tmp, "/tmp/minecpp-proto-%d.mca", _getpid());
#else
  snprintf(tmp, sizeof tmp, "/tmp/minecpp-proto-%d.mca", (int)getpid());
#endif
  size_t flen = 0;
  uint8_t *f = ReadFile(TEST_DATA_DIR "/region/r.-1.-1.mca", &flen);
  if (!f) return nullptr;
  FILE *w = fopen(tmp, "wb");
  fwrite(f, 1, flen, w);
  fclose(w);
  free(f);
  minecpp_region_t *r = minecpp_region_open(tmp, 0, nullptr);
  if (!r) {
    remove(tmp);
    return nullptr;
  }
  uint8_t ver = 0, *pay = nullptr;
  size_t plen = 0;
  Chunk *c = nullptr;
  if (minecpp_region_read_raw(r, 31, 20, &ver, &pay, &plen) ==
          MINECPP_REGION_OK &&
      ver == 2) {
    uLongf cap = (uLongf)plen * 64 + 1024;  // NBT ~11KB de 255B comprimidos
    uint8_t *nbt = (uint8_t *)malloc((size_t)cap);
    if (uncompress(nbt, &cap, pay, (uLong)plen) == Z_OK) {
      minecpp_nbt_tag_t *root = nullptr;
      if (minecpp_nbt_parse(nbt, (size_t)cap, 0, &root, nullptr) ==
          MINECPP_NBT_OK) {
        c = ChunkCreate(0, 0);
        if (ChunkFromRoot(c, root) != 0) {
          ChunkFree(c);
          c = nullptr;
        }
      }
      minecpp_nbt_free(root);
    }
    free(nbt);
  }
  free(pay);
  minecpp_region_close(r);
  remove(tmp);
  return c;
}

// t5: ChunkData golden do vanilla → struct → blocos conferem.
static void t_chunkdata_golden() {
  size_t n = 0;
  uint8_t *pkt = Unwrap("chunkdata.bin", 1, &n);
  ASSERT(pkt);
  if (!pkt) return;
  minecpp_reader_t r = {pkt, n};
  int32_t id = -1;
  ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
  ASSERT_EQ(id, kCbChunkData);
  ChunkData p;
  ASSERT(Decode(p, &r));
  ASSERT_EQ(r.left, 0u);
  ASSERT_EQ(p.x, 27);
  ASSERT_EQ(p.z, 30);
  ASSERT_EQ(p.continuous, true);
  ASSERT_EQ(p.mask, 1u);
  ASSERT_EQ(p.data.size(), 12544u);
  Chunk *c = ChunkCreate(0, 0);
  ASSERT(ChunkDataApply(p, c));
  ASSERT_EQ(c->x, 27);
  int32_t meta = -1;
  ASSERT_EQ(BlockGet(c, 0, 0, 0, &meta), 7);
  ASSERT_EQ(BlockGet(c, 0, 3, 0, nullptr), 2);
  ASSERT_EQ(BlockGet(c, 0, 4, 0, nullptr), 0);
  ASSERT(c->has_biomes && c->biomes[0] == 1);
  ChunkFree(c);
  free(pkt);
}

// t6: nosso ChunkData encodado == bytes do vanilla, bit a bit.
static void t_chunkdata_matches_vanilla() {
  Chunk *c = LoadRealChunk();
  ASSERT(c);
  if (!c) return;
  ChunkData built;
  ASSERT(ChunkDataBuild(*c, true, &built));
  ASSERT_EQ(built.mask, 1u);
  size_t n = 0;
  uint8_t *pkt = Unwrap("chunkdata.bin", 1, &n);
  minecpp_reader_t r = {pkt, n};
  int32_t id = -1;
  minecpp_rd_varint(&r, &id);
  ChunkData gold;
  ASSERT(Decode(gold, &r));
  // x/z do golden são do spawn (27,30); conteúdo deve ser idêntico (flat).
  ASSERT_EQ(built.data.size(), gold.data.size());
  ASSERT(built.data == gold.data);
  free(pkt);
  // Round-trip do nosso encode.
  minecpp_writer_t w = {nullptr, 0, 0, 0};
  ASSERT(Encode(built, &w));
  size_t m = 0;
  uint8_t *b = minecpp_wr_take(&w, &m);
  minecpp_reader_t r2 = {b, m};
  ChunkData back;
  ASSERT(Decode(back, &r2));
  Chunk *c2 = ChunkCreate(0, 0);
  ASSERT(ChunkDataApply(back, c2));
  for (int y = 0; y < 16; y++)
    ASSERT_EQ(BlockGet(c2, 3, y, 3, nullptr), BlockGet(c, 3, y, 3, nullptr));
  free(b);
  ChunkFree(c2);
  ChunkFree(c);
}

// t8: goldens P0 do login vanilla (net/goldens/).
static uint8_t *UnwrapGold(const char *name, size_t *out_n) {
  const std::string path = std::string(TEST_DATA_DIR) + "/net/goldens/" + name;
  size_t n = 0;
  uint8_t *raw = ReadFile(path.c_str(), &n);
  if (!raw) return nullptr;
  minecpp_reader_t r = {raw, n};
  int32_t dl = 0;
  if (minecpp_rd_varint(&r, &dl) != MINECPP_BUF_OK || dl < 0) {
    free(raw);
    return nullptr;
  }
  uint8_t *out = nullptr;
  if (dl == 0) {
    *out_n = r.left;
    out = (uint8_t *)malloc(r.left ? r.left : 1);
    memcpy(out, r.p, r.left);
  } else {
    uLongf cap = (uLongf)dl;
    out = (uint8_t *)malloc(cap + 1);
    if (uncompress(out, &cap, r.p, (uLong)r.left) != Z_OK) {
      free(out);
      out = nullptr;
    } else {
      *out_n = (size_t)cap;
    }
  }
  free(raw);
  return out;
}

static bool GoldPacket(const char *name, int32_t want_id,
                       std::vector<uint8_t> *pkt) {
  size_t n = 0;
  // setcompression foi capturado sem dataLen (pré-compressão).
  uint8_t *raw = nullptr;
  if (strcmp(name, "cb_03_setcompression.bin") == 0) {
    const std::string path =
        std::string(TEST_DATA_DIR) + "/net/goldens/" + name;
    raw = ReadFile(path.c_str(), &n);
  } else {
    raw = UnwrapGold(name, &n);
  }
  if (!raw) return false;
  minecpp_reader_t r = {raw, n};
  int32_t id = -1;
  const bool ok = minecpp_rd_varint(&r, &id) == MINECPP_BUF_OK && id == want_id;
  if (ok && pkt) pkt->assign(r.p, r.p + r.left);
  free(raw);
  return ok;
}

static void t_p0_goldens() {
  std::vector<uint8_t> p;
  minecpp_reader_t r{nullptr, 0};
  // LoginSuccess: uuid offline do vanilla == nosso offline_uuid("Goldens").
  ASSERT(GoldPacket("cb_02_1.bin", kCbLoginSuccess, &p));
  r.p = p.data();
  r.left = p.size();
  {
    LoginSuccess d;
    ASSERT(Decode(d, &r));
    uint8_t raw[16];
    char ustr[37];
    ASSERT_EQ(minecpp_offline_uuid("Goldens", raw, ustr), 0);
    ASSERT(d.uuid == ustr);
    ASSERT(d.name == "Goldens");
  }
  // Brand.
  ASSERT(GoldPacket("cb_3f_1.bin", kCbPlugin, &p));
  r.p = p.data();
  r.left = p.size();
  {
    PluginMessage d;
    ASSERT(Decode(d, &r));
    ASSERT(d.channel == "MC|Brand");
    ASSERT(d.data.size() == 8 && memcmp(d.data.data(), "\x07vanilla", 8) == 0);
  }
  // Difficulty / abilities / held / statistics.
  ASSERT(GoldPacket("cb_41_1.bin", kCbDifficulty, &p));
  r.p = p.data();
  r.left = p.size();
  {
    ServerDifficulty d;
    ASSERT(Decode(d, &r));
    ASSERT_EQ(d.difficulty, 1u);
  }
  ASSERT(GoldPacket("cb_39_1.bin", kCbAbilities, &p));
  r.p = p.data();
  r.left = p.size();
  {
    PlayerAbilities d;
    ASSERT(Decode(d, &r));
    ASSERT_EQ(d.flags, 0u);
    ASSERT(d.fly_speed == 0.05f && d.walk_speed == 0.1f);
  }
  ASSERT(GoldPacket("cb_09_1.bin", kCbHeldItem, &p));
  r.p = p.data();
  r.left = p.size();
  {
    HeldItemChange d;
    ASSERT(Decode(d, &r));
    ASSERT_EQ(d.slot, 0u);
  }
  ASSERT(GoldPacket("cb_37_1.bin", kCbStatistics, &p));
  r.p = p.data();
  r.left = p.size();
  {
    Statistics d;
    ASSERT(Decode(d, &r));
    ASSERT(d.stats.empty());
  }
  // PlayerList ×2 idênticos (quirk do vanilla: manda 2 ADDs).
  for (const char *f : {"cb_38_1.bin", "cb_38_2.bin"}) {
    ASSERT(GoldPacket(f, kCbPlayerList, &p));
    r.p = p.data();
    r.left = p.size();
    PlayerListItem d;
    ASSERT(Decode(d, &r));
    ASSERT_EQ(d.action, 0);
    ASSERT_EQ(d.players.size(), 1u);
    ASSERT(d.players[0].name == "Goldens");
    ASSERT(d.players[0].props.empty());
    ASSERT_EQ(d.players[0].gamemode, 0);
    ASSERT_EQ(d.players[0].ping, 0);
    ASSERT(!d.players[0].has_display);
  }
  // Border.
  ASSERT(GoldPacket("cb_44_1.bin", kCbBorder, &p));
  r.p = p.data();
  r.left = p.size();
  {
    WorldBorder d;
    ASSERT(Decode(d, &r));
    ASSERT_EQ(d.action, 3);
    ASSERT(d.center_x == 0 && d.center_z == 0);
    ASSERT(d.old_size == 6e7 && d.new_size == 6e7);
    ASSERT_EQ(d.speed, 0);
    ASSERT_EQ(d.portal_boundary, 29999984);
    ASSERT_EQ(d.warn_a, 5);
    ASSERT_EQ(d.warn_b, 15);
  }
  // WindowItems: 45 vazios. SetSlot: cursor vazio.
  ASSERT(GoldPacket("cb_30_1.bin", kCbWindowItems, &p));
  r.p = p.data();
  r.left = p.size();
  {
    WindowItems d;
    ASSERT(Decode(d, &r));
    ASSERT_EQ(d.window, 0u);
    ASSERT_EQ(d.slots.size(), 45u);
    for (const auto &s : d.slots) ASSERT_EQ(s.id, -1);
  }
  ASSERT(GoldPacket("cb_2f_1.bin", kCbSetSlot, &p));
  r.p = p.data();
  r.left = p.size();
  {
    SetSlot d;
    ASSERT(Decode(d, &r));
    ASSERT_EQ(d.window, -1);
    ASSERT_EQ(d.slot, -1);
    ASSERT_EQ(d.item.id, -1);
  }
  // Bulk: 10 chunks, primeiro (18,40) mask 1, bedrock flat.
  ASSERT(GoldPacket("cb_26_1.bin", kCbBulk, &p));
  r.p = p.data();
  r.left = p.size();
  {
    MapChunkBulk d;
    ASSERT(Decode(d, &r));
    ASSERT_EQ(d.sky_light, true);
    ASSERT_EQ(d.chunks.size(), 10u);
    ASSERT_EQ(d.chunks[0].x, 18);
    ASSERT_EQ(d.chunks[0].z, 40);
    ASSERT_EQ(d.chunks[0].mask, 1u);
    std::vector<ChunkData> parts;
    ASSERT(BulkSplit(d, &parts));
    ASSERT_EQ(parts.size(), 10u);
    Chunk *c = ChunkCreate(0, 0);
    ASSERT(ChunkDataApply(parts[0], c));
    ASSERT_EQ(BlockGet(c, 0, 0, 0, nullptr), 7);
    ASSERT_EQ(BlockGet(c, 0, 3, 0, nullptr), 2);
    ASSERT(c->has_biomes && c->biomes[0] == 1);
    ChunkFree(c);
  }
}

// t9: round-trips do lote P0.
static void t_p0_roundtrips() {
  minecpp_writer_t w{nullptr, 0, 0, 0};
  size_t n = 0;
  uint8_t *b = nullptr;
  minecpp_reader_t r{nullptr, 0};
  // Slot com item + WindowItems mista.
  {
    Slot it;
    it.id = 1;
    it.count = 64;
    it.damage = 0;
    WindowItems a;
    a.window = 0;
    a.slots = std::vector<Slot>(3);
    a.slots[1] = it;
    ASSERT(Encode(a, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    WindowItems c;
    ASSERT(Decode(c, &r));
    ASSERT_EQ(c.slots.size(), 3u);
    ASSERT_EQ(c.slots[0].id, -1);
    ASSERT_EQ(c.slots[1].id, 1);
    ASSERT_EQ(c.slots[1].count, 64u);
    ASSERT_EQ(r.left, 0u);
    free(b);
  }
  // PlayerList REMOVE + Statistics com entradas.
  {
    PlayerListItem a;
    a.action = 4;
    PlayerListEntry e;
    memset(e.uuid, 0xAB, 16);
    a.players.push_back(e);
    ASSERT(Encode(a, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    PlayerListItem c;
    ASSERT(Decode(c, &r));
    ASSERT_EQ(c.action, 4);
    ASSERT_EQ(c.players.size(), 1u);
    ASSERT(c.players[0].uuid[0] == 0xAB && c.players[0].name.empty());
    free(b);
    Statistics s;
    s.stats = {{"stat.jump", 5}, {"stat.walkOneCm", 123}};
    ASSERT(Encode(s, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    Statistics s2;
    ASSERT(Decode(s2, &r));
    ASSERT_EQ(s2.stats.size(), 2u);
    ASSERT(s2.stats[0].first == "stat.jump" && s2.stats[0].second == 5);
    free(b);
  }
  // Health/XP.
  {
    UpdateHealth a;
    a.health = 13.5f;
    a.food = 17;
    a.saturation = 3.25f;
    ASSERT(Encode(a, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    UpdateHealth c;
    ASSERT(Decode(c, &r));
    ASSERT(c.health == 13.5f && c.food == 17 && c.saturation == 3.25f);
    free(b);
    SetExperience e;
    e.bar = 0.5f;
    e.level = 7;
    e.total = 100;
    ASSERT(Encode(e, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    SetExperience e2;
    ASSERT(Decode(e2, &r));
    ASSERT(e2.bar == 0.5f && e2.level == 7 && e2.total == 100);
    free(b);
  }
  // BulkBuild de 2 flats → decode → split → blocos conferem.
  {
    Chunk *c1 = ChunkCreate(0, 0);
    Chunk *c2 = ChunkCreate(1, 0);
    ChunkGenerateFlat(c1);
    ChunkGenerateFlat(c2);
    MapChunkBulk bulk;
    ASSERT(BulkBuild({c1, c2}, true, &bulk));
    ASSERT_EQ(bulk.chunks.size(), 2u);
    ASSERT_EQ(bulk.chunks[0].mask, 1u);
    ASSERT_EQ(bulk.chunks[0].data.size(), 12544u);
    ASSERT(Encode(bulk, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    MapChunkBulk back;
    ASSERT(Decode(back, &r));
    ASSERT_EQ(back.chunks.size(), 2u);
    ASSERT(back.chunks[0].data == bulk.chunks[0].data);
    std::vector<ChunkData> parts;
    ASSERT(BulkSplit(back, &parts));
    Chunk *t = ChunkCreate(0, 0);
    ASSERT(ChunkDataApply(parts[1], t));
    ASSERT_EQ(t->x, 1);
    ASSERT_EQ(BlockGet(t, 0, 0, 0, nullptr), 7);
    ChunkFree(t);
    ChunkFree(c1);
    ChunkFree(c2);
    free(b);
  }
}

// t10: pacotes de bloco (P1b) + Slot NBT esquema vanilla (byte 0x00).
static void t_blocks() {
  minecpp_writer_t w{nullptr, 0, 0, 0};
  size_t n = 0;
  uint8_t *b = nullptr;
  minecpp_reader_t r{nullptr, 0};
  // Slot com NBT: bytes exatos (id+count+damage+root NBT, sem len u16!).
  {
    Slot it;
    it.id = 1;
    it.count = 5;
    it.damage = 2;
    minecpp_nbt_tag_t *t = minecpp_nbt_new(MINECPP_NBT_COMPOUND, "");
    minecpp_nbt_tag_t *c = minecpp_nbt_new(MINECPP_NBT_BYTE, "test");
    c->v.i8 = 1;
    ASSERT_EQ(minecpp_nbt_add(t, c), 0);
    it.nbt = t;
    ASSERT(Encode(it, &w));
    b = minecpp_wr_take(&w, &n);
    const uint8_t exp[] = {0x00, 0x01, 0x05, 0x00, 0x02, 0x0A, 0x00,
                           0x00, 0x01, 0x00, 0x04, 't',  'e',  's',
                           't',  0x01, 0x00};
    ASSERT_EQ(n, sizeof exp);
    ASSERT(memcmp(b, exp, n) == 0);
    r.p = b;
    r.left = n;
    Slot back;
    ASSERT(Decode(back, &r));
    ASSERT_EQ(back.id, 1);
    ASSERT_EQ(back.count, 5u);
    ASSERT_EQ(back.damage, 2);
    ASSERT(back.nbt);
    const minecpp_nbt_tag_t *k = minecpp_nbt_get(back.nbt, "test");
    ASSERT(k && k->type == MINECPP_NBT_BYTE && k->v.i8 == 1);
    ASSERT_EQ(r.left, 0u);
    FreeSlot(back);
    free(b);
    minecpp_nbt_free(t);
  }
  // Slot sem NBT: 1 byte 0x00 (hd.a((fn)null)).
  {
    Slot it;
    it.id = 3;
    it.count = 64;
    ASSERT(Encode(it, &w));
    b = minecpp_wr_take(&w, &n);
    const uint8_t exp[] = {0x00, 0x03, 0x40, 0x00, 0x00, 0x00};
    ASSERT_EQ(n, sizeof exp);
    ASSERT(memcmp(b, exp, n) == 0);
    free(b);
  }
  // BlockDig / BlockPlace round-trip.
  {
    BlockDig a;
    a.status = 2;
    a.x = -8;
    a.y = 3;
    a.z = -184;
    a.face = 1;
    ASSERT(Encode(a, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    BlockDig c;
    ASSERT(Decode(c, &r));
    ASSERT_EQ(c.status, 2u);
    ASSERT_EQ(c.x, -8);
    ASSERT_EQ(c.y, 3);
    ASSERT_EQ(c.z, -184);
    ASSERT_EQ(c.face, 1u);
    ASSERT_EQ(r.left, 0u);
    free(b);
    BlockPlace p;
    p.x = 1;
    p.y = 64;
    p.z = -2;
    p.dir = 1;
    p.held.id = 3;
    p.held.count = 5;
    p.cx = 8;
    p.cy = 8;
    p.cz = 8;
    ASSERT(Encode(p, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    BlockPlace q;
    ASSERT(Decode(q, &r));
    ASSERT_EQ(q.x, 1);
    ASSERT_EQ(q.dir, 1u);
    ASSERT_EQ(q.held.id, 3);
    ASSERT_EQ(q.held.count, 5u);
    ASSERT_EQ(q.cx, 8u);
    ASSERT_EQ(r.left, 0u);
    FreeSlot(q.held);
    free(b);
  }
  // Change/Action/Anim(-1=FF)/Effect/Sound.
  {
    BlockChangePkt a;
    a.x = 1;
    a.y = 2;
    a.z = 3;
    a.idmeta = (2 << 4);
    ASSERT(Encode(a, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    BlockChangePkt c;
    ASSERT(Decode(c, &r) && c.idmeta == 32);
    free(b);
    BreakAnim an;
    an.eid = 7;
    an.x = 1;
    an.stage = -1;
    ASSERT(Encode(an, &w));
    b = minecpp_wr_take(&w, &n);
    ASSERT(b[n - 1] == 0xFF);  // writeByte(-1)
    r.p = b;
    r.left = n;
    BreakAnim an2;
    ASSERT(Decode(an2, &r) && an2.stage == -1 && an2.eid == 7);
    free(b);
    Effect e;
    e.id = 2001;
    e.data = 3;
    ASSERT(Encode(e, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    Effect e2;
    ASSERT(Decode(e2, &r) && e2.id == 2001 && e2.data == 3);
    free(b);
    SoundEffect s;
    s.name = "dig.grass";
    s.volume = 1;
    ASSERT(Encode(s, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    SoundEffect s2;
    ASSERT(Decode(s2, &r) && s2.name == "dig.grass");
    free(b);
    BlockAction ba;
    ba.b1 = 1;
    ba.b2 = 2;
    ba.block = 54;
    ASSERT(Encode(ba, &w));
    b = minecpp_wr_take(&w, &n);
    r.p = b;
    r.left = n;
    BlockAction ba2;
    ASSERT(Decode(ba2, &r) && ba2.b1 == 1 && ba2.block == 54);
    free(b);
  }
}

// t11: entidades — goldens reais ( bodies crus, sem dataLen).
static uint8_t *GoldRaw(const char *name, size_t *out_n) {
  const std::string path = std::string(TEST_DATA_DIR) + "/net/goldens/" + name;
  return ReadFile(path.c_str(), out_n);
}

static const MetaEntry *MetaFind(const Metadata &m, uint8_t index) {
  for (const auto &e : m.entries)
    if (e.index == index) return &e;
  return nullptr;
}

static void t_entities() {
  size_t n = 0;
  minecpp_reader_t r{nullptr, 0};
  int32_t id = -1;
  // SpawnMob slime.
  {
    uint8_t *b = GoldRaw("entity_slime_0f.bin", &n);
    ASSERT(b);
    r.p = b;
    r.left = n;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbSpawnMob);
    SpawnMob m;
    ASSERT(Decode(m, &r));
    ASSERT_EQ(m.type, 55u);
    ASSERT_EQ(m.x, (int32_t)(-1019.5 * 32));
    ASSERT_EQ(m.y, 4 * 32);
    ASSERT_EQ(m.vx, 0);
    // metadata inline (parte do 0x0F).
    const MetaEntry *e0 = MetaFind(m.meta, 16);
    ASSERT(e0 && e0->type == 0 && e0->i == 4);
    e0 = MetaFind(m.meta, 6);
    ASSERT(e0 && e0->type == 3 && e0->f == 16.0);
    e0 = MetaFind(m.meta, 1);
    ASSERT(e0 && e0->type == 1 && e0->i == 300);
    ASSERT_EQ(r.left, 0u);
    free(b);
  }
  // Metadata standalone + Properties.
  {
    uint8_t *b = GoldRaw("entity_slime_1c.bin", &n);
    r.p = b;
    r.left = n;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbMetadata);
    int32_t meid = 0;  // pacote tem eid; Metadata pura não
    ASSERT_EQ(minecpp_rd_varint(&r, &meid), MINECPP_BUF_OK);
    ASSERT_EQ(meid, 198243);
    Metadata md;
    ASSERT(Decode(md, &r));
    const MetaEntry *e = MetaFind(md, 6);
    ASSERT(e && e->f == 20.0);
    e = MetaFind(md, 10);
    ASSERT(e && e->type == 0 && e->i == 0);
    e = MetaFind(md, 17);
    ASSERT(e && e->type == 3 && e->f == 0.0);
    e = MetaFind(md, 18);
    ASSERT(e && e->type == 2 && e->i == 0);
    free(b);
    b = GoldRaw("entity_slime_20.bin", &n);
    r.p = b;
    r.left = n;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbProperties);
    Properties pr;
    ASSERT(Decode(pr, &r));
    ASSERT_EQ(pr.props.size(), 2u);
    ASSERT(pr.props[0].key == "generic.movementSpeed");
    ASSERT(pr.props[0].value == (double)0.1f);  // vanilla grava f32 em f64!
    free(b);
  }
  // SpawnPlayer: uuid + pos + held 0 + metadata com health 20.
  {
    uint8_t *b = GoldRaw("player_0c.bin", &n);
    r.p = b;
    r.left = n;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbSpawnPlayer);
    SpawnPlayer sp;
    ASSERT(Decode(sp, &r));
    ASSERT_EQ(sp.held, 0);
    const MetaEntry *e = MetaFind(sp.meta, 6);
    ASSERT(e && e->f == 20.0);
    e = MetaFind(sp.meta, 1);
    ASSERT(e && e->i == 300);
    ASSERT_EQ(r.left, 0u);
    free(b);
  }
  // Item dropado: tipo 2, data 1, com velocidade.
  {
    uint8_t *b = GoldRaw("item_0e.bin", &n);
    r.p = b;
    r.left = n;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbSpawnObject);
    SpawnObject o;
    ASSERT(Decode(o, &r));
    ASSERT_EQ(o.type, 2u);
    ASSERT_EQ(o.data, 1);
    ASSERT(o.has_vel);
    ASSERT_EQ(r.left, 0u);
    free(b);
  }
  // Velocity + HeadLook sanity.
  {
    uint8_t *b = GoldRaw("entity_vel_12.bin", &n);
    r.p = b;
    r.left = n;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbVelocity);
    Velocity v;
    ASSERT(Decode(v, &r));
    ASSERT_EQ(r.left, 0u);
    free(b);
    b = GoldRaw("entity_head_19.bin", &n);
    r.p = b;
    r.left = n;
    ASSERT_EQ(minecpp_rd_varint(&r, &id), MINECPP_BUF_OK);
    ASSERT_EQ(id, kCbHeadLook);
    HeadLook h;
    ASSERT(Decode(h, &r));
    ASSERT_EQ(r.left, 0u);
    free(b);
  }
  // Builder cobre os 8 tipos + round-trip.
  {
    Metadata a;
    MetaEntry e;
    e.index = 0;
    e.type = 0;
    e.i = 5;
    a.entries.push_back(e);
    e = MetaEntry();
    e.index = 1;
    e.type = 1;
    e.i = 300;
    a.entries.push_back(e);
    e = MetaEntry();
    e.index = 2;
    e.type = 2;
    e.i = -7;
    a.entries.push_back(e);
    e = MetaEntry();
    e.index = 6;
    e.type = 3;
    e.f = 20.0;
    a.entries.push_back(e);
    e = MetaEntry();
    e.index = 3;
    e.type = 4;
    e.s = "hi";
    a.entries.push_back(e);
    e = MetaEntry();
    e.index = 4;
    e.type = 6;
    e.px = 1;
    e.py = 2;
    e.pz = 3;
    a.entries.push_back(e);
    e = MetaEntry();
    e.index = 5;
    e.type = 7;
    e.rx = 1.5f;
    e.ry = 2.5f;
    e.rz = 3.5f;
    a.entries.push_back(e);
    minecpp_writer_t w{nullptr, 0, 0, 0};
    ASSERT(Encode(a, &w));
    size_t m = 0;
    uint8_t *bb = minecpp_wr_take(&w, &m);
    minecpp_reader_t rr{bb, m};
    Metadata back;
    ASSERT(Decode(back, &rr));
    ASSERT_EQ(back.entries.size(), 7u);
    ASSERT_EQ(back.entries[0].i, 5);
    ASSERT_EQ(back.entries[1].i, 300);
    ASSERT_EQ(back.entries[2].i, -7);
    ASSERT(back.entries[3].f == 20.0);
    ASSERT(back.entries[4].s == "hi");
    ASSERT_EQ(back.entries[5].px, 1);
    ASSERT(back.entries[6].rx == 1.5f);
    free(bb);
  }
  // Destroy/teleport/collect/equipment round-trips rápidos.
  {
    DestroyEntities d;
    d.eids = {1, 300, 70000};
    minecpp_writer_t w{nullptr, 0, 0, 0};
    ASSERT(Encode(d, &w));
    size_t m = 0;
    uint8_t *bb = minecpp_wr_take(&w, &m);
    minecpp_reader_t rr{bb, m};
    DestroyEntities d2;
    ASSERT(Decode(d2, &rr));
    ASSERT_EQ(d2.eids.size(), 3u);
    ASSERT_EQ(d2.eids[2], 70000);
    free(bb);
    CollectItem c;
    c.collector = 9;
    c.collected = 10;
    ASSERT(Encode(c, &w));
    bb = minecpp_wr_take(&w, &m);
    rr.p = bb;
    rr.left = m;
    CollectItem c2;
    ASSERT(Decode(c2, &rr));
    ASSERT_EQ(c2.collector, 9);
    ASSERT_EQ(c2.collected, 10);
    free(bb);
  }
}

// t7: framing guards.
static void t_frame_guards() {
  minecpp_writer_t w = {nullptr, 0, 0, 0};
  const uint8_t pay[] = {0xAA};
  ASSERT(FrameEncode(0x21, pay, sizeof pay, &w));
  size_t n = 0;
  uint8_t *b = minecpp_wr_take(&w, &n);
  ASSERT_EQ(n, 3u);  // len + id + 1
  minecpp_reader_t r = {b, n};
  int32_t id = -1;
  minecpp_reader_t sub{nullptr, 0};
  ASSERT(FrameDecode(&r, &id, &sub));
  ASSERT_EQ(id, 0x21);
  ASSERT_EQ(sub.left, 1u);
  free(b);
  // len 0, len negativo, truncado.
  uint8_t z[] = {0x00};
  r.p = z;
  r.left = 1;
  ASSERT(!FrameDecode(&r, &id, &sub));
  uint8_t neg[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  r.p = neg;
  r.left = sizeof neg;
  ASSERT(!FrameDecode(&r, &id, &sub));
  uint8_t cut[] = {0x05, 0x00};
  r.p = cut;
  r.left = sizeof cut;
  ASSERT(!FrameDecode(&r, &id, &sub));
}

int main() {
  t_handshake_golden();
  t_status_golden();
  t_login();
  t_play_goldens();
  t_chunkdata_golden();
  t_chunkdata_matches_vanilla();
  t_frame_guards();
  t_p0_goldens();
  t_p0_roundtrips();
  t_blocks();
  t_entities();
  if (g_fail == 0) printf("protocol: todas as verificações passaram\n");
  return g_fail ? 1 : 0;
}
