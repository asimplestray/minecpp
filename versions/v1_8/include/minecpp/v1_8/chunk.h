#pragma once

// Chunk 1.8 — layout extraído de `bfy` (AnvilChunkLoader) + `bff`
// (ChunkNibbleArray). Ver docs/vanilla-1.8.md §4.
//
// - Section 16³: índice linear YZX `idx = (y<<8)|(z<<4)|x` (bfy lê
//   `x=idx&15, y=idx>>8&15, z=idx>>4&15`; bff nibble `y<<8|z<<4|x`).
// - id 12 bits = `Blocks[i] | Add_nibble<<8`; meta 4 bits = Data nibble.
// - HeightMap/Biomes: índice `x + z*16`.
// - Ordem de escrita canônica = ordem do path de save do bfy
//   (V, xPos, ..., Sections, Biomes, Entities, TileEntities, TileTicks?).
//   Leitura aceita qualquer ordem; `minecpp_nbt_equal` compara sem ordem
//   (o vanilla 1.8 usa HashMap e grava em hash-order).

#include <cstdint>

#include "minecpp/core/nbt.h"

namespace minecpp::v18 {

inline constexpr int kSectionSize = 16;
inline constexpr int kChunkHeight = 256;
inline constexpr int kSectionsPerChunk = 16;

inline int BlockIndex(int x, int y, int z) { return (y << 8) | (z << 4) | x; }
inline int HeightIndex(int x, int z) { return x + z * 16; }

int NibbleGet(const uint8_t *a, int idx);
void NibbleSet(uint8_t *a, int idx, int v);

struct Section {
  uint8_t y = 0;
  uint8_t blocks[4096] = {};
  uint8_t data[2048] = {};
  uint8_t add[2048] = {};
  bool has_add = false;
  uint8_t block_light[2048] = {};
  uint8_t sky_light[2048] = {};
};

struct Chunk {
  ~Chunk();  // libera sections + listas (World usa unique_ptr)
  int32_t x = 0, z = 0;
  int64_t last_update = 0;
  int32_t heightmap[256] = {};
  bool terrain_populated = false;
  bool light_populated = false;
  int64_t inhabited_time = 0;
  Section *sections[kSectionsPerChunk] = {};
  bool has_biomes = false;
  uint8_t biomes[256] = {};
  // Listas NBT preservadas verbatim (ownership do Chunk).
  minecpp_nbt_tag_t *entities = nullptr;
  minecpp_nbt_tag_t *tile_entities = nullptr;
  minecpp_nbt_tag_t *tile_ticks = nullptr;  // nullptr = chave ausente
};

Chunk *ChunkCreate(int32_t x, int32_t z);
void ChunkFree(Chunk *c);

// Preenche do NBT root (`{Level: {...}}`). Destaca (detach) as 3 listas do
// root — o caller continua dono do resto e deve liberar. Retorna 0 ou -1.
int ChunkFromRoot(Chunk *c, minecpp_nbt_tag_t *root);
// Constrói NBT root novo (ownership do caller). Ordem canônica. NULL=NOMEM.
minecpp_nbt_tag_t *ChunkToRoot(const Chunk *c);

// Blocos: id 0..4095, meta 0..15. get fora do chunk → -1; section vazia = ar.
int32_t BlockGet(const Chunk *c, int x, int y, int z, int32_t *meta);
int BlockSet(Chunk *c, int x, int y, int z, int32_t id, int32_t meta);

// Flat "2;7,2x3,2;1": bedrock y0, dirt y1-2, grass y3, skylight, heightmap 4,
// biomas plains(1). Metadados zerados, populado=true.
void ChunkGenerateFlat(Chunk *c);

}  // namespace minecpp::v18
