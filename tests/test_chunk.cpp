// Testes chunk 1.8: parse do chunk vanilla real, flat gen, block API + Add,
// wiring da version_api. Sem framework.
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

#include "minecpp/core/nbt.h"
#include "minecpp/core/region.h"
#include "minecpp/core/version_api.h"
#include "minecpp/v1_8/chunk.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "test-data/vanilla-1.8-flat"
#endif

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

static void TmpPath(char *dst, size_t cap, const char *tag) {
#ifdef _WIN32
  const int pid = _getpid();
#else
  const int pid = (int)getpid();
#endif
  snprintf(dst, cap, "/tmp/minecpp-chunk-%s-%d.mca", tag, pid);
}

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

static uint8_t *Inflate(const uint8_t *in, size_t n, size_t *out_n) {
  size_t cap = n * 16 + 64;
  uint8_t *out = (uint8_t *)malloc(cap);
  z_stream s;
  memset(&s, 0, sizeof s);
  if (inflateInit(&s) != Z_OK) {
    free(out);
    return nullptr;
  }
  s.next_in = (Bytef *)in;
  s.avail_in = (uInt)n;
  for (;;) {
    if (s.total_out >= cap) {
      cap *= 2;
      uint8_t *nb = (uint8_t *)realloc(out, cap);
      if (!nb) {
        free(out);
        inflateEnd(&s);
        return nullptr;
      }
      out = nb;
    }
    s.next_out = out + s.total_out;
    s.avail_out = (uInt)(cap - s.total_out);
    const int r = inflate(&s, Z_NO_FLUSH);
    if (r == Z_STREAM_END) break;
    if (r != Z_OK) {
      free(out);
      inflateEnd(&s);
      return nullptr;
    }
  }
  *out_n = s.total_out;
  inflateEnd(&s);
  return out;
}

// Carrega o chunk vanilla real (-1,-12) em `out_nbt` (parseado) + bytes crus.
static bool LoadReal(minecpp_nbt_tag_t **out_root, uint8_t **out_raw,
                     size_t *out_raw_len) {
  char tmp[256];
  TmpPath(tmp, sizeof tmp, "real");
  size_t flen = 0;
  uint8_t *f = ReadFile(TEST_DATA_DIR "/region/r.-1.-1.mca", &flen);
  if (!f) return false;
  FILE *w = fopen(tmp, "wb");
  fwrite(f, 1, flen, w);
  fclose(w);
  free(f);
  minecpp_region_t *r = minecpp_region_open(tmp, 0, nullptr);
  if (!r) {
    remove(tmp);
    return false;
  }
  uint8_t ver = 0;
  uint8_t *pay = nullptr;
  size_t plen = 0;
  const bool ok =
      minecpp_region_read_raw(r, 31, 20, &ver, &pay, &plen) ==
          MINECPP_REGION_OK &&
      ver == 2;
  minecpp_region_close(r);
  remove(tmp);
  if (!ok) {
    free(pay);
    return false;
  }
  size_t nlen = 0;
  uint8_t *nbt = Inflate(pay, plen, &nlen);
  free(pay);
  if (!nbt) return false;
  minecpp_nbt_tag_t *root = nullptr;
  if (minecpp_nbt_parse(nbt, nlen, 0, &root, nullptr) != MINECPP_NBT_OK) {
    free(nbt);
    return false;
  }
  *out_root = root;
  *out_raw = nbt;
  *out_raw_len = nlen;
  return true;
}

// t1: parse do chunk real + igualdade semântica do re-emit.
static void t_parse_real() {
  minecpp_nbt_tag_t *root = nullptr;
  uint8_t *raw = nullptr;
  size_t raw_len = 0;
  ASSERT(LoadReal(&root, &raw, &raw_len));
  if (!root) return;
  Chunk *c = ChunkCreate(0, 0);
  ASSERT(c);
  ASSERT_EQ(ChunkFromRoot(c, root), 0);
  ASSERT_EQ(c->x, -1);
  ASSERT_EQ(c->z, -12);
  ASSERT_EQ(c->last_update, 445);
  ASSERT_EQ(c->terrain_populated, false);
  ASSERT_EQ(c->light_populated, false);
  ASSERT_EQ(c->inhabited_time, 0);
  ASSERT(c->tile_ticks == nullptr);
  for (int i = 1; i < 16; i++) ASSERT(c->sections[i] == nullptr);
  ASSERT(c->sections[0] != nullptr);
  ASSERT(c->has_biomes);
  // Camadas flat: y0 bedrock, y1-2 dirt, y3 grass, resto ar.
  for (int x = 0; x < 16; x++) {
    for (int z = 0; z < 16; z++) {
      int32_t meta = -1;
      ASSERT_EQ(BlockGet(c, x, 0, z, &meta), 7);
      ASSERT_EQ(meta, 0);
      ASSERT_EQ(BlockGet(c, x, 1, z, nullptr), 3);
      ASSERT_EQ(BlockGet(c, x, 2, z, nullptr), 3);
      ASSERT_EQ(BlockGet(c, x, 3, z, nullptr), 2);
      ASSERT_EQ(BlockGet(c, x, 4, z, nullptr), 0);
      ASSERT_EQ(BlockGet(c, x, 200, z, nullptr), 0);
      ASSERT_EQ(c->heightmap[HeightIndex(x, z)], 4);
      ASSERT_EQ(c->biomes[HeightIndex(x, z)], 1);
    }
  }
  ASSERT_EQ(BlockGet(c, 16, 0, 0, nullptr), -1);  // fora do chunk
  ASSERT_EQ(BlockGet(c, 0, 256, 0, nullptr), -1);
  // Re-emit semanticamente igual (ordem de chaves difere: hash-order vanilla).
  // Nota: ChunkFromRoot destaca Entities/TileEntities do root (detach), então
  // compara contra um parse fresco dos bytes originais.
  minecpp_nbt_tag_t *back = ChunkToRoot(c);
  ASSERT(back);
  minecpp_nbt_tag_t *fresh = nullptr;
  ASSERT_EQ(minecpp_nbt_parse(raw, raw_len, 0, &fresh, nullptr),
            MINECPP_NBT_OK);
  ASSERT(minecpp_nbt_equal(fresh, back));
  minecpp_nbt_free(fresh);
  // Estabilidade: re-parse do nosso output gera o mesmo.
  uint8_t *b1 = nullptr;
  size_t n1 = 0;
  ASSERT_EQ(minecpp_nbt_serialize(back, &b1, &n1), MINECPP_NBT_OK);
  minecpp_nbt_tag_t *re = nullptr;
  ASSERT_EQ(minecpp_nbt_parse(b1, n1, 0, &re, nullptr), MINECPP_NBT_OK);
  ASSERT(minecpp_nbt_equal(back, re));
  minecpp_nbt_free(re);
  free(b1);
  minecpp_nbt_free(back);
  ChunkFree(c);
  minecpp_nbt_free(root);
  free(raw);
}

// t2: flat gerado bate no conteúdo do chunk vanilla real.
static void t_flat_matches_real() {
  minecpp_nbt_tag_t *root = nullptr;
  uint8_t *raw = nullptr;
  size_t raw_len = 0;
  ASSERT(LoadReal(&root, &raw, &raw_len));
  if (!root) return;
  Chunk *real = ChunkCreate(0, 0);
  ASSERT_EQ(ChunkFromRoot(real, root), 0);
  Chunk *gen = ChunkCreate(-1, -12);
  ChunkGenerateFlat(gen);
  // Mesmo conteúdo de mundo: blocos, heightmap, biomas, skylight.
  for (int x = 0; x < 16; x++) {
    for (int z = 0; z < 16; z++) {
      for (int y = 0; y < 256; y++) {
        int32_t m1 = -1, m2 = -2;
        ASSERT_EQ(BlockGet(gen, x, y, z, &m1), BlockGet(real, x, y, z, &m2));
        ASSERT_EQ(m1, m2);
      }
      ASSERT_EQ(gen->heightmap[HeightIndex(x, z)],
                real->heightmap[HeightIndex(x, z)]);
      ASSERT_EQ(gen->biomes[HeightIndex(x, z)], real->biomes[HeightIndex(x, z)]);
    }
  }
  for (int i = 0; i < 2048; i++)
    ASSERT_EQ(gen->sections[0]->sky_light[i], real->sections[0]->sky_light[i]);
  // Round-trip do gerado é estável.
  minecpp_nbt_tag_t *g1 = ChunkToRoot(gen);
  uint8_t *b = nullptr;
  size_t n = 0;
  ASSERT_EQ(minecpp_nbt_serialize(g1, &b, &n), MINECPP_NBT_OK);
  minecpp_nbt_tag_t *rp = nullptr;
  ASSERT_EQ(minecpp_nbt_parse(b, n, 0, &rp, nullptr), MINECPP_NBT_OK);
  ASSERT(minecpp_nbt_equal(g1, rp));
  minecpp_nbt_free(rp);
  free(b);
  minecpp_nbt_free(g1);
  ChunkFree(gen);
  ChunkFree(real);
  minecpp_nbt_free(root);
  free(raw);
}

// t3: block API com id > 255 (caminho Add) + round-trip.
static void t_add_path() {
  Chunk *c = ChunkCreate(0, 0);
  ASSERT_EQ(BlockSet(c, 5, 60, 5, 0xABC, 7), 0);
  int32_t meta = -1;
  ASSERT_EQ(BlockGet(c, 5, 60, 5, &meta), 0xABC);
  ASSERT_EQ(meta, 7);
  ASSERT(c->sections[3] != nullptr && c->sections[3]->has_add);
  minecpp_nbt_tag_t *root = ChunkToRoot(c);
  const minecpp_nbt_tag_t *level = minecpp_nbt_get(root, "Level");
  const minecpp_nbt_tag_t *secs = minecpp_nbt_get(level, "Sections");
  ASSERT_EQ(secs->v.list.len, 1u);
  ASSERT(minecpp_nbt_get(secs->v.list.items[0], "Add") != nullptr);
  uint8_t *b = nullptr;
  size_t n = 0;
  ASSERT_EQ(minecpp_nbt_serialize(root, &b, &n), MINECPP_NBT_OK);
  minecpp_nbt_tag_t *re = nullptr;
  ASSERT_EQ(minecpp_nbt_parse(b, n, 0, &re, nullptr), MINECPP_NBT_OK);
  Chunk *c2 = ChunkCreate(0, 0);
  ASSERT_EQ(ChunkFromRoot(c2, re), 0);
  meta = -1;
  ASSERT_EQ(BlockGet(c2, 5, 60, 5, &meta), 0xABC);
  ASSERT_EQ(meta, 7);
  ASSERT_EQ(BlockSet(c2, 5, 60, 5, 1, 0), 0);  // volta p/ id baixo
  ASSERT_EQ(BlockGet(c2, 5, 60, 5, &meta), 1);
  ASSERT_EQ(BlockSet(c, 0, 0, 0, 4096, 0), -1);  // id inválido
  ASSERT_EQ(BlockSet(c, 0, 0, 0, 1, 16), -1);  // meta inválida
  minecpp_nbt_free(re);
  free(b);
  minecpp_nbt_free(root);
  ChunkFree(c2);
  ChunkFree(c);
}

// t4: wiring da version_api (o que v1_9/v1_10 herdam).
extern "C" const minecpp_version_api_t *minecpp_v1_8_api(void);

static void t_version_api() {
  const minecpp_version_api_t *api = minecpp_v1_8_api();
  ASSERT_EQ(api->protocol_version, 47);
  minecpp_nbt_tag_t *root = nullptr;
  uint8_t *raw = nullptr;
  size_t raw_len = 0;
  ASSERT(LoadReal(&root, &raw, &raw_len));
  if (!root) return;
  void *c = api->chunk_create(-1, -12);
  ASSERT(c);
  ASSERT_EQ(api->chunk_from_nbt(c, raw, raw_len), 0);
  int32_t meta = -1;
  ASSERT_EQ(api->block_get(c, 0, 3, 0, &meta), 2);
  ASSERT_EQ(api->block_set(c, 0, 3, 0, 1, 0), 0);
  ASSERT_EQ(api->block_get(c, 0, 3, 0, nullptr), 1);
  minecpp_buffer_t out{nullptr, 0, 0};
  ASSERT_EQ(api->chunk_to_nbt(c, &out), 0);
  ASSERT(out.data && out.len > 0);
  minecpp_nbt_tag_t *re = nullptr;
  ASSERT_EQ(minecpp_nbt_parse(out.data, out.len, 0, &re, nullptr),
            MINECPP_NBT_OK);
  free(out.data);
  minecpp_nbt_free(re);
  api->chunk_free(c);
  minecpp_nbt_free(root);
  free(raw);
}

int main() {
  t_parse_real();
  t_flat_matches_real();
  t_add_path();
  t_version_api();
  if (g_fail == 0) printf("chunk: todas as verificações passaram\n");
  return g_fail ? 1 : 0;
}
