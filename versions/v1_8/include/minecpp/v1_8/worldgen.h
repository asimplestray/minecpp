#pragma once

// World generation 1.8 — Java-compatible (seed, biomes, terrain, structures)
// Baseado em vanilla 1.8 (bks, bkt, bku, bkv, etc.) + Wiki.vg/Protocol.

#include <cstdint>
#include <array>
#include <vector>

#include "minecpp/v1_8/chunk.h"

namespace minecpp::v18::worldgen {

// ---- Java Random (java.util.Random) ----
// next(int bits) = (seed * 0x5DEECE66DL + 0xBL) & ((1L<<48)-1)
// nextInt(n) com rejection sampling para evitar bias.
class JavaRandom {
 public:
  explicit JavaRandom(int64_t seed) : seed_((seed ^ 0x5DEECE66DL) & 0xFFFFFFFFFFFFLL) {}
  int64_t NextLong() {
    seed_ = (seed_ * 0x5DEECE666DL + 0xBL) & 0xFFFFFFFFFFFFLL;
    return seed_;
  }
  int32_t Next(int bits) {
    return (int32_t)(NextLong() >> (48 - bits));
  }
  int32_t NextInt(int32_t n) {
    if (n <= 0) return 0;
    if ((n & -n) == n) return (int32_t)((n * (int64_t)Next(31)) >> 31);
    int32_t bits, val;
    do {
      bits = Next(31);
      val = bits % n;
    } while (bits - val + (n - 1) < 0);
    return val;
  }
  int32_t NextInt(int32_t min, int32_t max) {  // [min, max]
    return min + NextInt(max - min + 1);
  }
  double NextDouble() {
    return ((int64_t)Next(26) << 27 | Next(27)) / (double)(1LL << 53);
  }
  float NextFloat() { return Next(24) / (float)(1 << 24); }
  bool NextBoolean() { return Next(1) != 0; }
  void SetSeed(int64_t s) { seed_ = (s ^ 0x5DEECE666DL) & 0xFFFFFFFFFFFFLL; }

 private:
  int64_t seed_;
};

// ---- Biomes (1.8 vanilla IDs) ----
enum Biome : uint8_t {
  kOcean = 0,
  kPlains = 1,
  kDesert = 2,
  kMountains = 3,
  kForest = 4,
  kTaiga = 5,
  kSwampland = 6,
  kRiver = 7,
  kHell = 8,          // Nether
  kSky = 9,           // End
  kFrozenOcean = 10,
  kFrozenRiver = 11,
  kIcePlains = 12,
  kIceMountains = 13,
  kMushroomIsland = 14,
  kMushroomIslandShore = 15,
  kBeach = 16,
  kDesertHills = 17,
  kForestHills = 18,
  kTaigaHills = 19,
  kSmallerExtremeHills = 20,
  kJungle = 21,
  kJungleHills = 22,
  kJungleEdge = 23,
  kDeepOcean = 24,
  kStoneBeach = 25,
  kColdBeach = 26,
  kBirchForest = 27,
  kBirchForestHills = 28,
  kRoofedForest = 29,
  kTaigaCold = 30,
  kTaigaColdHills = 31,
  kRedwoodTaiga = 32,
  kRedwoodTaigaHills = 33,
  kExtremeHillsPlus = 34,
  kSavanna = 35,
  kSavannaPlateau = 36,
  kMesa = 37,
  kMesaPlateauF = 38,
  kMesaPlateau = 39,
};

// Biome groups for generation
struct BiomeGenSettings {
  float temperature;
  float humidity;
  float height_variation;
  uint8_t top_block;
  uint8_t filler_block;
  int grass_color;
  int foliage_color;
  int water_color;
};

// ---- World Generator ----
struct GeneratorConfig {
  int64_t seed = 0;
  bool generate_structures = true;
  bool generate_biomes = true;
};

class WorldGenerator {
 public:
  explicit WorldGenerator(const GeneratorConfig &cfg);
  ~WorldGenerator();

  // Gera chunk completo (terrain + biomes + structures)
  void GenerateChunk(Chunk *c);

  // Apenas terrain (para chunk jobs assíncronos)
  void GenerateTerrain(Chunk *c);

  // Apenas biomas
  void GenerateBiomes(Chunk *c);

  // Estruturas (villages, temples, etc.)
  void GenerateStructures(Chunk *c);

 private:
  GeneratorConfig cfg_;
  JavaRandom biome_rng_;
  JavaRandom terrain_rng_;
  JavaRandom structure_rng_;

  // Biome generation
  void InitBiomeRNG(int32_t cx, int32_t cz);
  uint8_t GetBiomeAt(const Chunk *c, int x, int z);

  // Terrain generation
  void InitTerrainRNG(int32_t cx, int32_t cz);
  int GetHeight(const Chunk *c, int x, int z, const uint8_t *biomes);
  void GenerateCaves(Chunk *c);
  void GenerateRavines(Chunk *c);
  void GenerateOres(Chunk *c);
  void GenerateLakes(Chunk *c);

  // Structures
  void InitStructureRNG(int32_t cx, int32_t cz);
  bool CanSpawnStructureAt(int32_t cx, int32_t cz, const char *type);
  void GenerateVillage(Chunk *c);
  void GenerateTemple(Chunk *c);
  void GenerateMineshaft(Chunk *c);
  void GenerateStronghold(Chunk *c);

  // Helpers
  int32_t noise2d(int x, int z, double scale, int octaves);
  float biome_noise(int x, int z, double scale);
};

// Função de conveniência para chunk jobs
void GenerateChunk(int32_t cx, int32_t cz, int64_t seed, Chunk *c);

}  // namespace minecpp::v18::worldgen