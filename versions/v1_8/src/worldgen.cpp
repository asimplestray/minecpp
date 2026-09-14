#include "minecpp/v1_8/worldgen.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace minecpp::v18::worldgen {

// ---- Biome settings (vanilla 1.8 values) ----
static const BiomeGenSettings kBiomeSettings[256] = {
  // Default (0)
  {0.5f, 0.5f, 0.0f, 3, 3, 0x4C7D7E, 0x4C7D7E, 0x0000FF},  // Ocean
  {0.8f, 0.4f, 0.1f, 2, 3, 0x90EE90, 0x7CFC00, 0x0000FF},  // Plains
  {2.0f, 0.0f, 0.1f, 12, 12, 0xFFAA00, 0xFFAA00, 0x0000FF},  // Desert
  {0.2f, 0.3f, 0.5f, 2, 3, 0x90EE90, 0x7CFC00, 0x0000FF},  // Mountains
  {0.7f, 0.8f, 0.2f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // Forest
  {0.25f, 0.8f, 0.2f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // Taiga
  {0.8f, 0.9f, 0.1f, 2, 3, 0x6A7039, 0x8A9B3C, 0x7F7F7F},  // Swampland
  {0.5f, 0.5f, 0.0f, 3, 3, 0x4C7D7E, 0x4C7D7E, 0x0000FF},  // River
  {2.0f, 0.0f, 0.0f, 87, 87, 0xFF0000, 0xFF0000, 0xFF0000},  // Hell (Nether)
  {0.5f, 0.5f, 0.0f, 87, 87, 0xFFFFFF, 0xFFFFFF, 0x0000FF},  // Sky (End)
  {0.0f, 0.5f, 0.0f, 79, 80, 0xFFFFFF, 0xFFFFFF, 0x0000FF},  // FrozenOcean
  {0.0f, 0.5f, 0.0f, 79, 80, 0xFFFFFF, 0xFFFFFF, 0x0000FF},  // FrozenRiver
  {0.0f, 0.5f, 0.1f, 80, 80, 0xFFFFFF, 0xFFFFFF, 0x0000FF},  // IcePlains
  {0.0f, 0.5f, 0.5f, 80, 80, 0xFFFFFF, 0xFFFFFF, 0x0000FF},  // IceMountains
  {0.9f, 1.0f, 0.2f, 110, 3, 0xFF00FF, 0xFF00FF, 0x0000FF},  // MushroomIsland
  {0.9f, 1.0f, 0.1f, 110, 3, 0xFF00FF, 0xFF00FF, 0x0000FF},  // MushroomIslandShore
  {0.8f, 0.4f, 0.0f, 12, 12, 0xFAF0BE, 0xFAF0BE, 0x0000FF},  // Beach
  {2.0f, 0.0f, 0.3f, 12, 12, 0xFFAA00, 0xFFAA00, 0x0000FF},  // DesertHills
  {0.7f, 0.8f, 0.3f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // ForestHills
  {0.25f, 0.8f, 0.3f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // TaigaHills
  {0.2f, 0.3f, 0.4f, 2, 3, 0x90EE90, 0x7CFC00, 0x0000FF},  // SmallerExtremeHills
  {0.95f, 0.9f, 0.2f, 2, 3, 0x2E8B57, 0x2E8B57, 0x0000FF},  // Jungle
  {0.95f, 0.9f, 0.4f, 2, 3, 0x2E8B57, 0x2E8B57, 0x0000FF},  // JungleHills
  {0.95f, 0.8f, 0.1f, 2, 3, 0x2E8B57, 0x2E8B57, 0x0000FF},  // JungleEdge
  {0.5f, 0.5f, 0.0f, 3, 3, 0x000080, 0x000080, 0x0000FF},  // DeepOcean
  {0.2f, 0.3f, 0.1f, 1, 1, 0xA9A9A9, 0xA9A9A9, 0x0000FF},  // StoneBeach
  {0.05f, 0.3f, 0.0f, 80, 80, 0xFFFFFF, 0xFFFFFF, 0x0000FF},  // ColdBeach
  {0.6f, 0.6f, 0.2f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // BirchForest
  {0.6f, 0.6f, 0.3f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // BirchForestHills
  {0.7f, 0.8f, 0.2f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // RoofedForest
  {0.25f, 0.8f, 0.2f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // TaigaCold
  {0.25f, 0.8f, 0.3f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // TaigaColdHills
  {0.3f, 0.8f, 0.2f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // RedwoodTaiga
  {0.3f, 0.8f, 0.3f, 2, 3, 0x568203, 0x4E8C0D, 0x0000FF},  // RedwoodTaigaHills
  {0.2f, 0.3f, 0.5f, 2, 3, 0x90EE90, 0x7CFC00, 0x0000FF},  // ExtremeHillsPlus
  {1.2f, 0.0f, 0.1f, 2, 3, 0x90EE90, 0x7CFC00, 0x0000FF},  // Savanna
  {1.2f, 0.0f, 0.2f, 2, 3, 0x90EE90, 0x7CFC00, 0x0000FF},  // SavannaPlateau
  {2.0f, 0.0f, 0.2f, 159, 159, 0xFFAA00, 0xFFAA00, 0x0000FF},  // Mesa
  {2.0f, 0.0f, 0.3f, 159, 159, 0xFFAA00, 0xFFAA00, 0x0000FF},  // MesaPlateauF
  {2.0f, 0.0f, 0.3f, 159, 159, 0xFFAA00, 0xFFAA00, 0x0000FF},  // MesaPlateau
};

WorldGenerator::WorldGenerator(const GeneratorConfig &cfg)
    : cfg_(cfg), biome_rng_(cfg.seed), terrain_rng_(cfg.seed), structure_rng_(cfg.seed) {}

WorldGenerator::~WorldGenerator() = default;

void WorldGenerator::GenerateChunk(Chunk *c) {
  if (!c) return;
  GenerateTerrain(c);
  if (cfg_.generate_biomes) GenerateBiomes(c);
  if (cfg_.generate_structures) GenerateStructures(c);
}

void WorldGenerator::GenerateTerrain(Chunk *c) {
  if (!c) return;
  InitTerrainRNG(c->x, c->z);

  uint8_t biomes[256];
  if (cfg_.generate_biomes) {
    InitBiomeRNG(c->x, c->z);
    for (int x = 0; x < 16; x++) {
      for (int z = 0; z < 16; z++) {
        biomes[HeightIndex(x, z)] = GetBiomeAt(c, x, z);
      }
    }
  } else {
    memset(biomes, kPlains, 256);
  }

  // Base terrain
  for (int x = 0; x < 16; x++) {
    for (int z = 0; z < 16; z++) {
      const int h = GetHeight(c, x, z, biomes);
      c->heightmap[HeightIndex(x, z)] = h;
      for (int y = 0; y <= h; y++) {
        int32_t id = 0, meta = 0;
        if (y == 0) {
          id = 7;  // bedrock
        } else if (y < h - 2) {
          id = 1;  // stone
        } else if (y == h - 2) {
          id = 3;  // dirt
        } else if (y == h - 1) {
          id = 3;  // dirt
        } else {
          id = 2;  // grass
        }
        if (id != 0) BlockSet(c, x, y, z, id, meta);
      }
      // Top block per biome
      const uint8_t biome = biomes[HeightIndex(x, z)];
      const auto &bs = kBiomeSettings[biome];
      if (h > 0) {
        int32_t m = 0;
        BlockGet(c, x, h, z, &m);
        if (m == 2) BlockSet(c, x, h, z, bs.top_block, 0);
      }
    }
  }

  // Caves, ravines, ores, lakes
  GenerateCaves(c);
  GenerateRavines(c);
  GenerateOres(c);
  GenerateLakes(c);

  c->terrain_populated = true;
}

void WorldGenerator::GenerateBiomes(Chunk *c) {
  if (!c) return;
  InitBiomeRNG(c->x, c->z);
  for (int x = 0; x < 16; x++) {
    for (int z = 0; z < 16; z++) {
      c->biomes[HeightIndex(x, z)] = GetBiomeAt(c, x, z);
    }
  }
  c->has_biomes = true;
}

void WorldGenerator::GenerateStructures(Chunk *c) {
  if (!c || !cfg_.generate_structures) return;
  InitStructureRNG(c->x, c->z);
  GenerateVillage(c);
  GenerateTemple(c);
  GenerateMineshaft(c);
  GenerateStronghold(c);
}

// ---- Biome Generation ----
void WorldGenerator::InitBiomeRNG(int32_t cx, int32_t cz) {
  int64_t s = (int64_t)cx * 341873128712LL + (int64_t)cz * 132897987541LL + cfg_.seed;
  biome_rng_.SetSeed(s);
}

uint8_t WorldGenerator::GetBiomeAt(const Chunk *c, int x, int z) {
  // Simplified biome generation using noise
  double nx = (c->x * 16 + x) / 200.0;
  double nz = (c->z * 16 + z) / 200.0;
  double temp = biome_noise((int)(nx * 100), (int)(nz * 100), 0.01);
  double humid = biome_noise((int)(nx * 100) + 10000, (int)(nz * 100) + 10000, 0.01);

  // Map temperature/humidity to biome (simplified Whittaker diagram)
  if (temp < 0.1) {
    return humid > 0.5 ? kIcePlains : kTaiga;
  } else if (temp < 0.3) {
    return humid > 0.5 ? kForest : kPlains;
  } else if (temp < 0.6) {
    if (humid > 0.6) return kJungle;
    if (humid > 0.3) return kForest;
    return kPlains;
  } else if (temp < 1.0) {
    if (humid > 0.6) return kSwampland;
    if (humid > 0.3) return kSavanna;
    return kDesert;
  } else {
    if (humid > 0.6) return kMushroomIsland;
    return kDesert;
  }
}

// ---- Terrain Generation ----
void WorldGenerator::InitTerrainRNG(int32_t cx, int32_t cz) {
  int64_t s = (int64_t)cx * 341873128712LL + (int64_t)cz * 132897987541LL + cfg_.seed + 1;
  terrain_rng_.SetSeed(s);
}

int WorldGenerator::GetHeight(const Chunk *c, int x, int z, const uint8_t *biomes) {
  // Base height from noise
  double nx = (c->x * 16 + x) / 80.0;
  double nz = (c->z * 16 + z) / 80.0;

  double h = 0;
  h += noise2d((int)(nx * 100), (int)(nz * 100), 0.01, 4) * 15;   // Continental
  h += noise2d((int)(nx * 100), (int)(nz * 100), 0.02, 2) * 8;    // Mountains
  h += noise2d((int)(nx * 100), (int)(nz * 100), 0.05, 1) * 3;    // Hills

  // Biome variation
  const uint8_t biome = biomes[HeightIndex(x, z)];
  h += kBiomeSettings[biome].height_variation * 10;

  int height = 63 + (int)h;  // Sea level 63
  return height < 1 ? 1 : (height > 255 ? 255 : height);
}

void WorldGenerator::GenerateCaves(Chunk *c) {
  // Simple cave carving
  for (int x = 0; x < 16; x++) {
    for (int z = 0; z < 16; z++) {
      double nx = (c->x * 16 + x) / 16.0;
      double nz = (c->z * 16 + z) / 16.0;
      for (int y = 1; y < 128; y++) {
        double n = noise2d((int)(nx * 100), (int)(nz * 100), 0.05, 2);
        if (n > 0.7 && n < 0.72) {  // Cave threshold
          BlockSet(c, x, y, z, 0, 0);
        }
      }
    }
  }
}

void WorldGenerator::GenerateRavines(Chunk *c) {
  // Rare ravines
  if (terrain_rng_.NextInt(50) != 0) return;
  int rx = terrain_rng_.NextInt(16);
  int rz = terrain_rng_.NextInt(16);
  int ry = terrain_rng_.NextInt(40) + 10;
  int len = terrain_rng_.NextInt(10) + 5;

  for (int i = 0; i < len; i++) {
    int x = rx + (terrain_rng_.NextInt(3) - 1);
    int z = rz + (terrain_rng_.NextInt(3) - 1);
    int y = ry + (terrain_rng_.NextInt(3) - 1);
    if (x >= 0 && x < 16 && z >= 0 && z < 16 && y > 0 && y < 255) {
      for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
          for (int dz = -2; dz <= 2; dz++) {
            if (dx*dx + dz*dz + dy*dy <= 16) {
              int bx = x + dx, bz = z + dz, by = y + dy;
              if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16 && by > 0 && by < 255) {
                BlockSet(c, bx, by, bz, 0, 0);
              }
            }
          }
        }
      }
    }
  }
}

void WorldGenerator::GenerateOres(Chunk *c) {
  // Coal, iron, gold, diamond, redstone, lapis
  struct Ore { int id, min_y, max_y, count, size; };
  static const Ore ores[] = {
    {16, 0, 128, 20, 16},   // coal
    {15, 0, 64, 20, 8},     // iron
    {14, 0, 32, 2, 8},      // gold
    {56, 0, 16, 1, 7},      // diamond
    {73, 0, 16, 8, 6},      // redstone
    {21, 0, 32, 1, 7},      // lapis
  };

  for (const auto &ore : ores) {
    for (int i = 0; i < ore.count; i++) {
      int x = terrain_rng_.NextInt(16);
      int z = terrain_rng_.NextInt(16);
      int y = terrain_rng_.NextInt(ore.max_y - ore.min_y) + ore.min_y;
      int size = terrain_rng_.NextInt(ore.size) + 1;

      for (int j = 0; j < size; j++) {
        int bx = x + terrain_rng_.NextInt(3) - 1;
        int by = y + terrain_rng_.NextInt(3) - 1;
        int bz = z + terrain_rng_.NextInt(3) - 1;
        if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16 && by > 0 && by < 255) {
          int32_t existing = 0;
          BlockGet(c, bx, by, bz, &existing);
          if (existing == 1) BlockSet(c, bx, by, bz, ore.id, 0);  // replace stone
        }
      }
    }
  }
}

void WorldGenerator::GenerateLakes(Chunk *c) {
  // Water and lava lakes
  if (terrain_rng_.NextInt(10) == 0) {
    int x = terrain_rng_.NextInt(16);
    int z = terrain_rng_.NextInt(16);
    int y = terrain_rng_.NextInt(60) + 10;
    for (int dx = -3; dx <= 3; dx++) {
      for (int dz = -3; dz <= 3; dz++) {
        if (dx*dx + dz*dz <= 9) {
          int bx = x + dx, bz = z + dz;
          if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16) {
            for (int dy = -1; dy <= 1; dy++) {
              int by = y + dy;
              if (by > 0 && by < 255) BlockSet(c, bx, by, bz, 8, 0);  // water
            }
          }
        }
      }
    }
  }
  if (terrain_rng_.NextInt(100) == 0) {
    int x = terrain_rng_.NextInt(16);
    int z = terrain_rng_.NextInt(16);
    int y = terrain_rng_.NextInt(60) + 10;
    for (int dx = -2; dx <= 2; dx++) {
      for (int dz = -2; dz <= 2; dz++) {
        if (dx*dx + dz*dz <= 4) {
          int bx = x + dx, bz = z + dz;
          if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16) {
            for (int dy = -1; dy <= 1; dy++) {
              int by = y + dy;
              if (by > 0 && by < 255) BlockSet(c, bx, by, bz, 10, 0);  // lava
            }
          }
        }
      }
    }
  }
}

// ---- Structures ----
void WorldGenerator::InitStructureRNG(int32_t cx, int32_t cz) {
  int64_t s = (int64_t)cx * 341873128712LL + (int64_t)cz * 132897987541LL + cfg_.seed + 2;
  structure_rng_.SetSeed(s);
}

bool WorldGenerator::CanSpawnStructureAt(int32_t cx, int32_t cz, const char *type) {
  // Structure spacing logic (simplified)
  int64_t h = ((int64_t)cx << 32) ^ (cz & 0xFFFFFFFF) ^ (int64_t)type[0];
  JavaRandom rng(h);
  return rng.NextInt(8) == 0;  // 1/8 chance per chunk
}

void WorldGenerator::GenerateVillage(Chunk *c) {
  if (!CanSpawnStructureAt(c->x, c->z, "village")) return;
  // Simplified: place a well at center
  int cx = 8, cz = 8;
  int y = c->heightmap[HeightIndex(cx, cz)];
  if (y > 0 && y < 250) {
    // Well structure (cobblestone + water)
    BlockSet(c, cx, y, cz, 4, 0);      // cobblestone
    BlockSet(c, cx, y+1, cz, 4, 0);
    BlockSet(c, cx, y+2, cz, 0, 0);    // air
    BlockSet(c, cx+1, y, cz, 4, 0);
    BlockSet(c, cx-1, y, cz, 4, 0);
    BlockSet(c, cx, y, cz+1, 4, 0);
    BlockSet(c, cx, y, cz-1, 4, 0);
    BlockSet(c, cx, y+1, cz+1, 4, 0);
    BlockSet(c, cx, y+1, cz-1, 4, 0);
    BlockSet(c, cx+1, y+1, cz, 4, 0);
    BlockSet(c, cx-1, y+1, cz, 4, 0);
    BlockSet(c, cx, y, cz, 8, 0);      // water inside
  }
}

void WorldGenerator::GenerateTemple(Chunk *c) {
  if (!CanSpawnStructureAt(c->x, c->z, "temple")) return;
  // Desert/jungle temple - simplified
  int cx = 8, cz = 8;
  int y = c->heightmap[HeightIndex(cx, cz)];
  if (y > 0 && y < 250) {
    // Small sandstone pyramid
    for (int dy = 0; dy < 5; dy++) {
      int sz = 5 - dy;
      for (int dx = -sz; dx <= sz; dx++) {
        for (int dz = -sz; dz <= sz; dz++) {
          int bx = cx + dx, bz = cz + dz, by = y + dy;
          if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16 && by > 0 && by < 255) {
            if (dy == 0 || dx == -sz || dx == sz || dz == -sz || dz == sz) {
              BlockSet(c, bx, by, bz, 24, 0);  // sandstone
            }
          }
        }
      }
    }
    // Chest with loot at center bottom
    BlockSet(c, cx, y+1, cz, 54, 0);  // chest
  }
}

void WorldGenerator::GenerateMineshaft(Chunk *c) {
  if (!CanSpawnStructureAt(c->x, c->z, "mineshaft")) return;
  // Corridor
  int x = structure_rng_.NextInt(16);
  int z = structure_rng_.NextInt(16);
  int y = structure_rng_.NextInt(40) + 10;
  int len = structure_rng_.NextInt(20) + 10;
  int dir = structure_rng_.NextInt(4);
  int dx = (dir == 0) ? 1 : (dir == 1) ? -1 : 0;
  int dz = (dir == 2) ? 1 : (dir == 3) ? -1 : 0;

  for (int i = 0; i < len; i++) {
    if (x >= 0 && x < 16 && z >= 0 && z < 16 && y > 0 && y < 255) {
      // 3x3 corridor with wood supports
      for (int ddx = -1; ddx <= 1; ddx++) {
        for (int ddz = -1; ddz <= 1; ddz++) {
          int bx = x + ddx, bz = z + ddz;
          if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16) {
            BlockSet(c, bx, y, bz, 0, 0);      // air
            BlockSet(c, bx, y+1, bz, 0, 0);
            if (ddx == 0 && ddz == 0) {
              BlockSet(c, bx, y-1, bz, 5, 0);  // planks floor
              if (i % 4 == 0) {
                BlockSet(c, bx, y+1, bz, 17, 0); // logs support
                BlockSet(c, bx, y+2, bz, 17, 0);
                BlockSet(c, bx, y+3, bz, 17, 0);
                BlockSet(c, bx, y+4, bz, 5, 0);  // ceiling
              }
            }
          }
        }
      }
    }
    x += dx;
    z += dz;
  }
}

void WorldGenerator::GenerateStronghold(Chunk *c) {
  if (!CanSpawnStructureAt(c->x, c->z, "stronghold")) return;
  // Simplified: portal room
  int cx = 8, cz = 8;
  int y = 30;  // deep
  for (int dx = -5; dx <= 5; dx++) {
    for (int dz = -5; dz <= 5; dz++) {
      int bx = cx + dx, bz = cz + dz;
      if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16) {
        BlockSet(c, bx, y, bz, 98, 0);      // stone bricks floor
        BlockSet(c, bx, y+1, bz, 98, 0);
        BlockSet(c, bx, y+2, bz, 98, 0);
        BlockSet(c, bx, y+3, bz, 98, 0);
        if (dx == 0 && dz == 0) {
          BlockSet(c, bx, y+1, bz, 119, 0); // end portal frame
        }
      }
    }
  }
}

// ---- Noise helpers (free functions in namespace) ----
static int32_t noise2d(int x, int z, double scale, int octaves) {
  double value = 0;
  double amplitude = 1.0;
  double frequency = scale;
  double max = 0;
  for (int i = 0; i < octaves; i++) {
    double nx = x * frequency;
    double nz = z * frequency;
    // Simple Perlin-like noise using sine interpolation
    int ix = (int)nx;
    int iz = (int)nz;
    double fx = nx - ix;
    double fz = nz - iz;
    // Hash-based pseudo-random
    auto hash = [](int a, int b) {
      int64_t h = (int64_t)a * 31 + b;
      h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9LL;
      h = (h ^ (h >> 27)) * 0x94d049bb133111ebLL;
      return (double)(h & 0xFFFFFFFF) / 4294967296.0;
    };
    double v1 = hash(ix, iz);
    double v2 = hash(ix+1, iz);
    double v3 = hash(ix, iz+1);
    double v4 = hash(ix+1, iz+1);
    // Smoothstep
    double u = fx * fx * (3 - 2 * fx);
    double v = fz * fz * (3 - 2 * fz);
    double n = v1*(1-u)*(1-v) + v2*u*(1-v) + v3*(1-u)*v + v4*u*v;
    value += n * amplitude;
    max += amplitude;
    amplitude *= 0.5;
    frequency *= 2.0;
  }
  return (int32_t)(value / max * 2147483647);
}

static float biome_noise(int x, int z, double scale) {
  return noise2d(x, z, scale, 2) / 2147483647.0f;
}

// Member wrappers
int32_t WorldGenerator::noise2d(int x, int z, double scale, int octaves) {
  return ::minecpp::v18::worldgen::noise2d(x, z, scale, octaves);
}

float WorldGenerator::biome_noise(int x, int z, double scale) {
  return ::minecpp::v18::worldgen::biome_noise(x, z, scale);
}

// ---- Convenience function for chunk jobs ----
void GenerateChunk(int32_t cx, int32_t cz, int64_t seed, Chunk *c) {
  GeneratorConfig cfg;
  cfg.seed = seed;
  WorldGenerator gen(cfg);
  c->x = cx;
  c->z = cz;
  gen.GenerateChunk(c);
}

// ---- Nether Generation (dimension -1) ----

void GenerateNetherChunk(int32_t cx, int32_t cz, int64_t seed, Chunk *c) {
  if (!c) return;
  c->x = cx;
  c->z = cz;

  JavaRandom rng(seed + (((int64_t)cx << 32) ^ (cz & 0xFFFFFFFF)));
  // Nether: bedrock roof and floor, netherrack, soul sand, glowstone, lava seas

  // Bedrock ceiling (y=127) and floor (y=0)
  for (int x = 0; x < 16; x++) {
    for (int z = 0; z < 16; z++) {
      BlockSet(c, x, 0, z, 7, 0);      // bedrock floor
      BlockSet(c, x, 127, z, 7, 0);    // bedrock ceiling
    }
  }

  // Main netherrack generation with noise
  for (int x = 0; x < 16; x++) {
    for (int z = 0; z < 16; z++) {
      double nx = (cx * 16 + x) / 80.0;
      double nz = (cz * 16 + z) / 80.0;
      double h = noise2d((int)(nx * 100), (int)(nz * 100), 0.01, 3) * 20;
      int height = 64 + (int)h;  // Nether "sea level" around y=64
      height = height < 2 ? 2 : (height > 126 ? 126 : height);

      c->heightmap[HeightIndex(x, z)] = height;

      for (int y = 1; y <= height; y++) {
        int32_t id = 0;
        if (y <= 4) {
          id = 87;  // netherrack
        } else if (y <= height - 2) {
          id = 87;  // netherrack
        } else {
          id = 87;  // netherrack
        }
        if (id != 0) BlockSet(c, x, y, z, id, 0);
      }
    }
  }

  // Soul sand patches (near lava level)
  for (int i = 0; i < 10; i++) {
    int x = rng.NextInt(16);
    int z = rng.NextInt(16);
    int y = rng.NextInt(10) + 30;
    for (int dx = -2; dx <= 2; dx++) {
      for (int dz = -2; dz <= 2; dz++) {
        int bx = x + dx, bz = z + dz;
        if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16) {
          int32_t existing = 0;
          BlockGet(c, bx, y, bz, &existing);
          if (existing == 87 && rng.NextInt(3) == 0) {
            BlockSet(c, bx, y, bz, 88, 0);  // soul sand
          }
        }
      }
    }
  }

  // Glowstone clusters (on ceiling)
  for (int i = 0; i < 8; i++) {
    int x = rng.NextInt(16);
    int z = rng.NextInt(16);
    int y = 120 + rng.NextInt(6);
    if (y > 126) y = 126;
    int size = rng.NextInt(4) + 1;
    for (int j = 0; j < size; j++) {
      int bx = x + rng.NextInt(3) - 1;
      int bz = z + rng.NextInt(3) - 1;
      int by = y + rng.NextInt(2);
      if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16 && by > 0 && by < 127) {
        int32_t existing = 0;
        BlockGet(c, bx, by, bz, &existing);
        if (existing == 0) BlockSet(c, bx, by, bz, 89, 0);  // glowstone
      }
    }
  }

  // Nether quartz ore
  for (int i = 0; i < 16; i++) {
    int x = rng.NextInt(16);
    int z = rng.NextInt(16);
    int y = rng.NextInt(100) + 10;
    int size = rng.NextInt(8) + 1;
    for (int j = 0; j < size; j++) {
      int bx = x + rng.NextInt(3) - 1;
      int by = y + rng.NextInt(3) - 1;
      int bz = z + rng.NextInt(3) - 1;
      if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16 && by > 0 && by < 127) {
        int32_t existing = 0;
        BlockGet(c, bx, by, bz, &existing);
        if (existing == 87) BlockSet(c, bx, by, bz, 153, 0);  // nether quartz ore
      }
    }
  }

  // Lava seas (y=30-35)
  for (int x = 0; x < 16; x++) {
    for (int z = 0; z < 16; z++) {
      double nx = (cx * 16 + x) / 40.0;
      double nz = (cz * 16 + z) / 40.0;
      double n = noise2d((int)(nx * 100), (int)(nz * 100), 0.02, 2);
      if (n > 0.6) {
        for (int y = 30; y <= 35; y++) {
          int32_t existing = 0;
          BlockGet(c, x, y, z, &existing);
          if (existing == 87 || existing == 0) BlockSet(c, x, y, z, 10, 0);  // lava
        }
      }
    }
  }

  // Nether fortress (simplified - bridge corridors)
  int64_t h = ((int64_t)cx << 32) ^ (cz & 0xFFFFFFFF) ^ 0xF0F7E55;
  JavaRandom fort_rng(h);
  if (fort_rng.NextInt(8) == 0) {
    int fx = rng.NextInt(16);
    int fz = rng.NextInt(16);
    int fy = rng.NextInt(40) + 40;
    int len = rng.NextInt(30) + 20;
    int dir = rng.NextInt(4);
    int dx = (dir == 0) ? 1 : (dir == 1) ? -1 : 0;
    int dz = (dir == 2) ? 1 : (dir == 3) ? -1 : 0;

    for (int i = 0; i < len; i++) {
      if (fx >= 0 && fx < 16 && fz >= 0 && fz < 16 && fy > 0 && fy < 127) {
        // 3x3 bridge with nether brick
        for (int ddx = -1; ddx <= 1; ddx++) {
          for (int ddz = -1; ddz <= 1; ddz++) {
            int bx = fx + ddx, bz = fz + ddz;
            if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16) {
              BlockSet(c, bx, fy, bz, 112, 0);  // nether brick
              BlockSet(c, bx, fy + 1, bz, 0, 0);
              BlockSet(c, bx, fy + 2, bz, 0, 0);
              if (i % 5 == 0 && ddx == 0 && ddz == 0) {
                BlockSet(c, bx, fy + 1, bz, 112, 0);  // support pillar
                BlockSet(c, bx, fy + 2, bz, 112, 0);
              }
            }
          }
        }
      }
      fx += dx;
      fz += dz;
    }
  }

  c->terrain_populated = true;
  c->has_biomes = true;
  memset(c->biomes, kHell, 256);
}

// ---- End Generation (dimension 1) ----

void GenerateEndChunk(int32_t cx, int32_t cz, int64_t seed, Chunk *c) {
  if (!c) return;
  c->x = cx;
  c->z = cz;

  JavaRandom rng(seed + (((int64_t)cx << 32) ^ (cz & 0xFFFFFFFF)) + 1);

  // End: floating islands of end stone, obsidian pillars at center, end cities far out
  double dist_from_center = sqrt((double)cx * cx + (double)cz * cz);

  // Main island (center 8x8 chunks)
  bool is_main_island = (cx >= -4 && cx <= 3 && cz >= -4 && cz <= 3);

  if (is_main_island) {
    // Large central island
    for (int x = 0; x < 16; x++) {
      for (int z = 0; z < 16; z++) {
        double gx = cx * 16 + x;
        double gz = cz * 16 + z;
        double d = sqrt(gx * gx + gz * gz);
        double h = 0;

        if (d < 40) {
          h = 60 + noise2d((int)(gx * 2), (int)(gz * 2), 0.05, 3) * 10;
        } else if (d < 80) {
          h = 40 + noise2d((int)(gx * 2), (int)(gz * 2), 0.05, 3) * 8;
        } else {
          h = 20 + noise2d((int)(gx * 2), (int)(gz * 2), 0.05, 3) * 5;
        }

        int height = (int)h;
        height = height < 1 ? 1 : (height > 255 ? 255 : height);
        c->heightmap[HeightIndex(x, z)] = height;

        for (int y = 0; y <= height; y++) {
          int32_t id = 0;
          if (y == 0) {
            id = 7;  // bedrock at very bottom
          } else if (y <= 3) {
            id = 121;  // end stone
          } else {
            id = 121;  // end stone
          }
          if (id != 0) BlockSet(c, x, y, z, id, 0);
        }
      }
    }

    // Obsidian pillars at (0,0) area
    if (cx == 0 && cz == 0) {
      for (int i = 0; i < 10; i++) {
        int px = rng.NextInt(16);
        int pz = rng.NextInt(16);
        int h = c->heightmap[HeightIndex(px, pz)];
        for (int y = h + 1; y < h + 20 + rng.NextInt(20); y++) {
          if (y < 255) BlockSet(c, px, y, pz, 49, 0);  // obsidian
        }
        // End crystal on top
        if (h + 20 + rng.NextInt(20) < 255) {
          // Note: end crystal is an entity, not a block
        }
      }

      // Bedrock portal (exit portal) at center
      for (int dx = -2; dx <= 2; dx++) {
        for (int dz = -2; dz <= 2; dz++) {
          int bx = 7 + dx, bz = 7 + dz;
          if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16) {
            BlockSet(c, bx, 63, bz, 7, 0);  // bedrock
            if (dx == 0 && dz == 0) BlockSet(c, bx, 64, bz, 122, 0); // end portal
          }
        }
      }
    }
  } else if (dist_from_center > 1000) {
    // Outer islands - end cities
    if (rng.NextInt(20) == 0) {
      // Generate end city structure
      int cx_city = rng.NextInt(16);
      int cz_city = rng.NextInt(16);
      int base_y = 40 + rng.NextInt(30);

      // Tower base
      for (int dx = -4; dx <= 4; dx++) {
        for (int dz = -4; dz <= 4; dz++) {
          int bx = cx_city + dx, bz = cz_city + dz;
          if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16) {
            BlockSet(c, bx, base_y, bz, 206, 0);  // purpur block
            BlockSet(c, bx, base_y + 1, bz, 206, 0);
          }
        }
      }

      // Tower
      for (int dy = 0; dy < 30; dy++) {
        int size = 4 - dy / 8;
        if (size < 1) size = 1;
        for (int dx = -size; dx <= size; dx++) {
          for (int dz = -size; dz <= size; dz++) {
            int bx = cx_city + dx, bz = cz_city + dz, by = base_y + 2 + dy;
            if (bx >= 0 && bx < 16 && bz >= 0 && bz < 16 && by < 255) {
              if (dx == -size || dx == size || dz == -size || dz == size) {
                BlockSet(c, bx, by, bz, 206, 0);  // purpur block walls
              }
            }
          }
        }
      }

      // End ship (rare)
      if (rng.NextInt(10) == 0) {
        int sx = cx_city + 15;
        int sz = cz_city + 15;
        if (sx < 16 && sz < 16) {
          for (int dx = 0; dx < 10; dx++) {
            for (int dz = -2; dz <= 2; dz++) {
              int bx = sx + dx, bz = sz + dz, by = base_y + 5;
              if (bx < 16 && bz >= 0 && bz < 16 && by < 255) {
                BlockSet(c, bx, by, bz, 206, 0);
              }
            }
          }
        }
      }
    }
  }

  // Chorus plants on outer islands
  if (dist_from_center > 500 && rng.NextInt(5) == 0) {
    for (int i = 0; i < 5; i++) {
      int x = rng.NextInt(16);
      int z = rng.NextInt(16);
      int y = 50 + rng.NextInt(50);
      for (int dy = 0; dy < 10 + rng.NextInt(10); dy++) {
        int by = y + dy;
        if (by < 255) BlockSet(c, x, by, z, 208, 0);  // chorus plant
      }
    }
  }

  c->terrain_populated = true;
  c->has_biomes = true;
  memset(c->biomes, kSky, 256);
}

}  // namespace minecpp::v18::worldgen