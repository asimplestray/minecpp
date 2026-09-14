// minecpp-dump: inspeção de .mca para diff vanilla x nosso.
// Uso: minecpp-dump <r.X.Z.mca> [slotx slotz]
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <zlib.h>

#include "minecpp/core/nbt.h"
#include "minecpp/core/region.h"
#include "minecpp/v1_8/chunk.h"

namespace {

uint8_t *Inflate(const uint8_t *in, size_t n, size_t *out_n) {
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

void DumpChunk(minecpp::v18::Chunk *c) {
  using namespace minecpp::v18;
  printf("chunk (%d,%d) last=%lld terrain=%d light=%d inhabited=%lld\n", c->x,
         c->z, (long long)c->last_update, c->terrain_populated,
         c->light_populated, (long long)c->inhabited_time);
  printf("biomes: %s first=%u\n", c->has_biomes ? "yes" : "no",
         c->has_biomes ? c->biomes[0] : 0);
  printf("heightmap distinct:");
  bool seen_h[256] = {};
  for (int v : c->heightmap)
    if (v >= 0 && v < 256 && !seen_h[v]) {
      seen_h[v] = true;
      printf(" %d", v);
    }
  printf("\n");
  for (int y = 0; y < 16; y++) {
    const Section *s = c->sections[y];
    if (!s) continue;
    std::map<int, int> hist;
    for (int i = 0; i < 4096; i++) {
      int id = s->blocks[i];
      if (s->has_add) id |= NibbleGet(s->add, i) << 8;
      hist[id]++;
    }
    printf("section Y=%d blocks={", s->y);
    bool first = true;
    for (const auto &[id, n] : hist) {
      printf("%s%d:%d", first ? "" : " ", id, n);
      first = false;
    }
    printf("} has_add=%d\n", s->has_add);
  }
  const auto *e = c->entities;
  const auto *te = c->tile_entities;
  printf("entities=%zu tile_entities=%zu tile_ticks=%s\n",
         e ? e->v.list.len : 0, te ? te->v.list.len : 0,
         c->tile_ticks ? "present" : "absent");
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2 || argc == 3 || argc > 4) {
    fprintf(stderr, "uso: %s <r.X.Z.mca> [slotx slotz]\n", argv[0]);
    return 2;
  }
  minecpp_region_err_t err = MINECPP_REGION_OK;
  minecpp_region_t *r = minecpp_region_open(argv[1], 0, &err);
  if (!r) {
    fprintf(stderr, "open: %s\n", minecpp_region_err_name(err));
    return 1;
  }
  printf("chunks=%u\n", minecpp_region_chunk_count(r));
  const int only = argc == 4;
  const int ox = only ? atoi(argv[2]) : -1;
  const int oz = only ? atoi(argv[3]) : -1;
  for (int z = 0; z < 32; z++) {
    for (int x = 0; x < 32; x++) {
      if (only && (x != ox || z != oz)) continue;
      uint8_t ver = 0;
      uint8_t *raw = nullptr;
      size_t n = 0;
      const minecpp_region_err_t e =
          minecpp_region_read_raw(r, x, z, &ver, &raw, &n);
      if (e == MINECPP_REGION_EMPTY) continue;
      if (e != MINECPP_REGION_OK) {
        printf("slot (%d,%d): %s\n", x, z, minecpp_region_err_name(e));
        continue;
      }
      printf("slot (%d,%d): version=%u payload=%zu ts=%u\n", x, z, ver, n,
             minecpp_region_timestamp(r, x, z));
      size_t nn = 0;
      uint8_t *nbt = ver == 2 ? Inflate(raw, n, &nn) : nullptr;
      free(raw);
      if (!nbt) {
        printf("  (sem inflate: version %u)\n", ver);
        continue;
      }
      minecpp_nbt_tag_t *root = nullptr;
      if (minecpp_nbt_parse(nbt, nn, 0, &root, nullptr) != MINECPP_NBT_OK) {
        printf("  nbt parse falhou\n");
        free(nbt);
        continue;
      }
      free(nbt);
      minecpp::v18::Chunk *c = minecpp::v18::ChunkCreate(0, 0);
      if (minecpp::v18::ChunkFromRoot(c, root) != 0) {
        printf("  chunk parse falhou\n");
      } else {
        DumpChunk(c);
      }
      minecpp::v18::ChunkFree(c);
      minecpp_nbt_free(root);
    }
  }
  minecpp_region_close(r);
  return 0;
}
