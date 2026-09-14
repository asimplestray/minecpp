#include "minecpp/v1_8/chunk.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>

namespace minecpp::v18 {
namespace {

const minecpp_nbt_tag_t *Req(const minecpp_nbt_tag_t *p, const char *name,
                             minecpp_nbt_type_t t) {
  const minecpp_nbt_tag_t *c = minecpp_nbt_get(p, name);
  return (c && c->type == t) ? c : nullptr;
}

bool GetLong(const minecpp_nbt_tag_t *p, const char *name, int64_t *out) {
  const minecpp_nbt_tag_t *c = minecpp_nbt_get(p, name);
  if (!c) return false;
  if (c->type == MINECPP_NBT_LONG) {
    *out = c->v.i64;
    return true;
  }
  if (c->type == MINECPP_NBT_INT) {  // tolera mundos antigos (bfy faz o mesmo)
    *out = c->v.i32;
    return true;
  }
  return false;
}

minecpp_nbt_tag_t *NewList(const char *name) {
  minecpp_nbt_tag_t *t = minecpp_nbt_new(MINECPP_NBT_LIST, name);
  if (t) t->v.list.elem = MINECPP_NBT_END;
  return t;
}

void FreeSections(Chunk *c) {
  for (auto *&s : c->sections) {
    delete s;
    s = nullptr;
  }
}

void FreeLists(Chunk *c) {
  minecpp_nbt_free(c->entities);
  minecpp_nbt_free(c->tile_entities);
  minecpp_nbt_free(c->tile_ticks);
  c->entities = c->tile_entities = c->tile_ticks = nullptr;
}

}  // namespace

Chunk::~Chunk() {
  FreeSections(this);
  FreeLists(this);
}

int NibbleGet(const uint8_t *a, int idx) {
  const uint8_t b = a[idx >> 1];
  return (idx & 1) == 0 ? b & 15 : (b >> 4) & 15;
}

void NibbleSet(uint8_t *a, int idx, int v) {
  uint8_t &b = a[idx >> 1];
  if ((idx & 1) == 0)
    b = (uint8_t)((b & 0xF0) | (v & 15));
  else
    b = (uint8_t)((b & 0x0F) | ((v & 15) << 4));
}

Chunk *ChunkCreate(int32_t x, int32_t z) {
  Chunk *c = new (std::nothrow) Chunk();
  if (!c) return nullptr;
  c->x = x;
  c->z = z;
  c->entities = NewList("Entities");
  c->tile_entities = NewList("TileEntities");
  if (!c->entities || !c->tile_entities) {
    ChunkFree(c);
    return nullptr;
  }
  return c;
}

void ChunkFree(Chunk *c) { delete c; }

int ChunkFromRoot(Chunk *c, minecpp_nbt_tag_t *root) {
  if (!c || !root || root->type != MINECPP_NBT_COMPOUND) return -1;
  const minecpp_nbt_tag_t *level = Req(root, "Level", MINECPP_NBT_COMPOUND);
  if (!level) return -1;

  // Escalares (tolerante a ausente como o vanilla; rígido com tipo errado).
  const minecpp_nbt_tag_t *t = minecpp_nbt_get(level, "xPos");
  if (t && t->type != MINECPP_NBT_INT) return -1;
  const minecpp_nbt_tag_t *tz = minecpp_nbt_get(level, "zPos");
  if (tz && tz->type != MINECPP_NBT_INT) return -1;

  FreeSections(c);
  FreeLists(c);

  if (t) c->x = t->v.i32;
  if (tz) c->z = tz->v.i32;
  GetLong(level, "LastUpdate", &c->last_update);
  GetLong(level, "InhabitedTime", &c->inhabited_time);

  const minecpp_nbt_tag_t *hm = minecpp_nbt_get(level, "HeightMap");
  if (hm) {
    if (hm->type != MINECPP_NBT_INT_ARRAY || hm->v.ints.len != 256) return -1;
    memcpy(c->heightmap, hm->v.ints.data, sizeof c->heightmap);
  } else {
    memset(c->heightmap, 0, sizeof c->heightmap);
  }

  const minecpp_nbt_tag_t *tp = minecpp_nbt_get(level, "TerrainPopulated");
  if (tp && tp->type != MINECPP_NBT_BYTE) return -1;
  c->terrain_populated = tp ? tp->v.i8 != 0 : false;
  const minecpp_nbt_tag_t *lp = minecpp_nbt_get(level, "LightPopulated");
  if (lp && lp->type != MINECPP_NBT_BYTE) return -1;
  c->light_populated = lp ? lp->v.i8 != 0 : false;

  const minecpp_nbt_tag_t *secs = Req(level, "Sections", MINECPP_NBT_LIST);
  if (!secs) return -1;
  if (secs->v.list.len > 0 && secs->v.list.elem != MINECPP_NBT_COMPOUND)
    return -1;
  for (size_t i = 0; i < secs->v.list.len; i++) {
    const minecpp_nbt_tag_t *s = secs->v.list.items[i];
    const minecpp_nbt_tag_t *y = Req(s, "Y", MINECPP_NBT_BYTE);
    const minecpp_nbt_tag_t *blocks = Req(s, "Blocks", MINECPP_NBT_BYTE_ARRAY);
    const minecpp_nbt_tag_t *data = Req(s, "Data", MINECPP_NBT_BYTE_ARRAY);
    const minecpp_nbt_tag_t *bl = Req(s, "BlockLight", MINECPP_NBT_BYTE_ARRAY);
    const minecpp_nbt_tag_t *sl = Req(s, "SkyLight", MINECPP_NBT_BYTE_ARRAY);
    if (!y || !blocks || !data || !bl || !sl) return -1;
    if (blocks->v.bytes.len != 4096 || data->v.bytes.len != 2048 ||
        bl->v.bytes.len != 2048 || sl->v.bytes.len != 2048)
      return -1;
    const int yy = y->v.i8;
    if (yy < 0 || yy > 15) return -1;
    const minecpp_nbt_tag_t *add = minecpp_nbt_get(s, "Add");
    if (add && (add->type != MINECPP_NBT_BYTE_ARRAY ||
                add->v.bytes.len != 2048))
      return -1;
    Section *sec = new (std::nothrow) Section();
    if (!sec) return -1;
    sec->y = (uint8_t)yy;
    memcpy(sec->blocks, blocks->v.bytes.data, 4096);
    memcpy(sec->data, data->v.bytes.data, 2048);
    memcpy(sec->block_light, bl->v.bytes.data, 2048);
    memcpy(sec->sky_light, sl->v.bytes.data, 2048);
    if (add) {
      memcpy(sec->add, add->v.bytes.data, 2048);
      sec->has_add = true;
    }
    delete c->sections[yy];  // Y duplicado: último vence (como o vanilla)
    c->sections[yy] = sec;
  }

  const minecpp_nbt_tag_t *bi = minecpp_nbt_get(level, "Biomes");
  if (bi) {
    if (bi->type != MINECPP_NBT_BYTE_ARRAY || bi->v.bytes.len != 256)
      return -1;
    c->has_biomes = true;
    memcpy(c->biomes, bi->v.bytes.data, 256);
  } else {
    c->has_biomes = false;
  }

  // Listas preservadas verbatim (detach transfere ownership).
  auto *mut = const_cast<minecpp_nbt_tag_t *>(level);
  c->entities = minecpp_nbt_detach(mut, "Entities");
  c->tile_entities = minecpp_nbt_detach(mut, "TileEntities");
  c->tile_ticks = minecpp_nbt_detach(mut, "TileTicks");
  if (!c->entities) c->entities = NewList("Entities");
  if (!c->tile_entities) c->tile_entities = NewList("TileEntities");
  if (!c->entities || !c->tile_entities) return -1;
  for (auto *l : {c->entities, c->tile_entities, c->tile_ticks}) {
    if (l && l->type != MINECPP_NBT_LIST) return -1;
    if (l && l->v.list.len > 0 && l->v.list.elem != MINECPP_NBT_COMPOUND)
      return -1;
  }
  return 0;
}

namespace {

minecpp_nbt_tag_t *NewScalar(minecpp_nbt_type_t t, const char *name) {
  return minecpp_nbt_new(t, name);
}

bool PutChild(minecpp_nbt_tag_t *parent, minecpp_nbt_tag_t *child) {
  if (!child || minecpp_nbt_add(parent, child) != 0) {
    minecpp_nbt_free(child);
    return false;
  }
  return true;
}

bool PutByte(minecpp_nbt_tag_t *p, const char *n, int8_t v) {
  minecpp_nbt_tag_t *t = NewScalar(MINECPP_NBT_BYTE, n);
  if (!t) return false;
  t->v.i8 = v;
  return PutChild(p, t);
}

bool PutInt(minecpp_nbt_tag_t *p, const char *n, int32_t v) {
  minecpp_nbt_tag_t *t = NewScalar(MINECPP_NBT_INT, n);
  if (!t) return false;
  t->v.i32 = v;
  return PutChild(p, t);
}

bool PutLong(minecpp_nbt_tag_t *p, const char *n, int64_t v) {
  minecpp_nbt_tag_t *t = NewScalar(MINECPP_NBT_LONG, n);
  if (!t) return false;
  t->v.i64 = v;
  return PutChild(p, t);
}

bool PutBytes(minecpp_nbt_tag_t *p, const char *n, const uint8_t *d,
              size_t len) {
  minecpp_nbt_tag_t *t = NewScalar(MINECPP_NBT_BYTE_ARRAY, n);
  if (!t) return false;
  t->v.bytes.data = (uint8_t *)malloc(len ? len : 1);
  if (!t->v.bytes.data) {
    minecpp_nbt_free(t);
    return false;
  }
  memcpy(t->v.bytes.data, d, len);
  t->v.bytes.len = len;
  return PutChild(p, t);
}

bool PutInts(minecpp_nbt_tag_t *p, const char *n, const int32_t *d,
             size_t len) {
  minecpp_nbt_tag_t *t = NewScalar(MINECPP_NBT_INT_ARRAY, n);
  if (!t) return false;
  t->v.ints.data = (int32_t *)malloc(len ? len * 4 : 1);
  if (!t->v.ints.data) {
    minecpp_nbt_free(t);
    return false;
  }
  memcpy(t->v.ints.data, d, len * 4);
  t->v.ints.len = len;
  return PutChild(p, t);
}

}  // namespace

minecpp_nbt_tag_t *ChunkToRoot(const Chunk *c) {
  if (!c) return nullptr;
  minecpp_nbt_tag_t *root = minecpp_nbt_new(MINECPP_NBT_COMPOUND, "");
  minecpp_nbt_tag_t *level = minecpp_nbt_new(MINECPP_NBT_COMPOUND, "Level");
  if (!root || !level) {
    minecpp_nbt_free(root);
    minecpp_nbt_free(level);
    return nullptr;
  }
  // Ordem canônica = ordem do path de save do bfy.
  bool ok = PutByte(level, "V", 1) && PutInt(level, "xPos", c->x) &&
            PutInt(level, "zPos", c->z) &&
            PutLong(level, "LastUpdate", c->last_update) &&
            PutInts(level, "HeightMap", c->heightmap, 256) &&
            PutByte(level, "TerrainPopulated", c->terrain_populated) &&
            PutByte(level, "LightPopulated", c->light_populated) &&
            PutLong(level, "InhabitedTime", c->inhabited_time);
  if (!ok) {
    minecpp_nbt_free(root);
    minecpp_nbt_free(level);
    return nullptr;
  }
  minecpp_nbt_tag_t *secs = minecpp_nbt_new(MINECPP_NBT_LIST, "Sections");
  if (!secs) {
    minecpp_nbt_free(root);
    minecpp_nbt_free(level);
    return nullptr;
  }
  for (int y = 0; y < kSectionsPerChunk; y++) {
    const Section *s = c->sections[y];
    if (!s) continue;
    minecpp_nbt_tag_t *sc = minecpp_nbt_new(MINECPP_NBT_COMPOUND, nullptr);
    if (!sc) { ok = false; break; }
    bool has_add = s->has_add;
    if (!has_add) {
      for (int i = 0; i < 2048 && !has_add; i++)
        if (s->add[i]) has_add = true;
    }
    ok = PutByte(sc, "Y", (int8_t)s->y) &&
         PutBytes(sc, "Blocks", s->blocks, 4096) &&
         PutBytes(sc, "Data", s->data, 2048) &&
         (!has_add || PutBytes(sc, "Add", s->add, 2048)) &&
         PutBytes(sc, "BlockLight", s->block_light, 2048) &&
         PutBytes(sc, "SkyLight", s->sky_light, 2048) &&
         minecpp_nbt_add(secs, sc) == 0;
    if (!ok) {
      minecpp_nbt_free(sc);
      break;
    }
  }
  if (!ok || minecpp_nbt_add(level, secs) != 0) {
    if (!ok) minecpp_nbt_free(secs);
    minecpp_nbt_free(root);
    minecpp_nbt_free(level);
    return nullptr;
  }
  if (c->has_biomes && !PutBytes(level, "Biomes", c->biomes, 256)) {
    minecpp_nbt_free(root);
    minecpp_nbt_free(level);
    return nullptr;
  }
  minecpp_nbt_tag_t *ent = minecpp_nbt_clone(c->entities);
  minecpp_nbt_tag_t *tet = minecpp_nbt_clone(c->tile_entities);
  if (!ent || !tet || minecpp_nbt_add(level, ent) != 0 ||
      minecpp_nbt_add(level, tet) != 0) {
    minecpp_nbt_free(ent);
    minecpp_nbt_free(tet);
    minecpp_nbt_free(root);
    minecpp_nbt_free(level);
    return nullptr;
  }
  if (c->tile_ticks) {
    minecpp_nbt_tag_t *tt = minecpp_nbt_clone(c->tile_ticks);
    if (!tt || minecpp_nbt_add(level, tt) != 0) {
      minecpp_nbt_free(tt);
      minecpp_nbt_free(root);
      minecpp_nbt_free(level);
      return nullptr;
    }
  }
  if (minecpp_nbt_add(root, level) != 0) {
    minecpp_nbt_free(level);
    minecpp_nbt_free(root);
    return nullptr;
  }
  return root;
}

int32_t BlockGet(const Chunk *c, int x, int y, int z, int32_t *meta) {
  if (!c || x < 0 || x > 15 || z < 0 || z > 15 || y < 0 || y > 255) return -1;
  const Section *s = c->sections[y >> 4];
  if (!s) {
    if (meta) *meta = 0;
    return 0;
  }
  const int idx = BlockIndex(x, y & 15, z);
  int32_t id = s->blocks[idx];
  if (s->has_add) id |= (int32_t)NibbleGet(s->add, idx) << 8;
  if (meta) *meta = NibbleGet(s->data, idx);
  return id;
}

int BlockSet(Chunk *c, int x, int y, int z, int32_t id, int32_t meta) {
  if (!c || x < 0 || x > 15 || z < 0 || z > 15 || y < 0 || y > 255) return -1;
  if (id < 0 || id > 4095 || meta < 0 || meta > 15) return -1;
  Section *s = c->sections[y >> 4];
  if (!s) {
    s = new (std::nothrow) Section();
    if (!s) return -1;
    s->y = (uint8_t)(y >> 4);
    c->sections[y >> 4] = s;
  }
  const int idx = BlockIndex(x, y & 15, z);
  s->blocks[idx] = (uint8_t)(id & 0xFF);
  if (id > 255) {
    s->has_add = true;
    NibbleSet(s->add, idx, (id >> 8) & 15);
  } else if (s->has_add) {
    NibbleSet(s->add, idx, 0);
  }
  NibbleSet(s->data, idx, meta);
  return 0;
}

void ChunkGenerateFlat(Chunk *c) {
  if (!c) return;
  FreeSections(c);
  FreeLists(c);
  c->last_update = 0;
  c->inhabited_time = 0;
  c->terrain_populated = true;
  c->light_populated = true;
  c->has_biomes = true;
  memset(c->biomes, 1, sizeof c->biomes);  // plains
  for (int i = 0; i < 256; i++) c->heightmap[i] = 4;
  Section *s = new (std::nothrow) Section();
  if (!s) return;
  s->y = 0;
  for (int x = 0; x < 16; x++) {
    for (int z = 0; z < 16; z++) {
      // y0 bedrock(7), y1-2 dirt(3), y3 grass(2); resto ar.
      s->blocks[BlockIndex(x, 0, z)] = 7;
      s->blocks[BlockIndex(x, 1, z)] = 3;
      s->blocks[BlockIndex(x, 2, z)] = 3;
      s->blocks[BlockIndex(x, 3, z)] = 2;
      for (int y = 4; y < 16; y++)
        NibbleSet(s->sky_light, BlockIndex(x, y, z), 15);
    }
  }
  c->sections[0] = s;
  c->entities = NewList("Entities");
  c->tile_entities = NewList("TileEntities");
  c->tile_ticks = nullptr;
}

}  // namespace minecpp::v18
