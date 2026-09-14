#pragma once

// Physics 1.8 — Authoritative server-side physics
// AABB collision, gravity, water/lava, movement validation, explosions.

#include <cstdint>
#include <vector>

#include "minecpp/v1_8/chunk.h"

namespace minecpp::v18::physics {

// ---- AABB (Axis-Aligned Bounding Box) ----
struct AABB {
  double min_x, min_y, min_z;
  double max_x, max_y, max_z;

  constexpr AABB() : min_x(0), min_y(0), min_z(0), max_x(0), max_y(0), max_z(0) {}
  constexpr AABB(double x0, double y0, double z0, double x1, double y1, double z1)
      : min_x(x0), min_y(y0), min_z(z0), max_x(x1), max_y(y1), max_z(z1) {}

  // Expand by padding (for entity collision boxes)
  constexpr AABB Expand(double x, double y, double z) const {
    return AABB(min_x - x, min_y - y, min_z - z, max_x + x, max_y + y, max_z + z);
  }

  // Offset
  constexpr AABB Offset(double x, double y, double z) const {
    return AABB(min_x + x, min_y + y, min_z + z, max_x + x, max_y + y, max_z + z);
  }

  // Intersection test
  constexpr bool Intersects(const AABB &other) const {
    return min_x <= other.max_x && max_x >= other.min_x &&
           min_y <= other.max_y && max_y >= other.min_y &&
           min_z <= other.max_z && max_z >= other.min_z;
  }

  // Intersection with offset
  constexpr bool Intersects(const AABB &other, double ox, double oy, double oz) const {
    return min_x + ox <= other.max_x && max_x + ox >= other.min_x &&
           min_y + oy <= other.max_y && max_y + oy >= other.min_y &&
           min_z + oz <= other.max_z && max_z + oz >= other.min_z;
  }

  // Union
  constexpr AABB Union(const AABB &other) const {
    return AABB(
      std::min(min_x, other.min_x), std::min(min_y, other.min_y), std::min(min_z, other.min_z),
      std::max(max_x, other.max_x), std::max(max_y, other.max_y), std::max(max_z, other.max_z)
    );
  }
};

// ---- Block collision shapes ----
// Vanilla 1.8 block bounds (from bku/Block.java)
enum class BlockShape : uint8_t {
  kFullBlock = 0,      // 0,0,0 -> 1,1,1
  kEmpty = 1,          // no collision
  kFlower = 2,         // thin cross
  kTorch = 3,          // thin vertical
  kFire = 4,           // full height thin
  kLiquid = 5,         // 0,0,0 -> 1,0.875,1 (7/8 height)
  kLadder = 6,         // thin vertical on side
  kRail = 7,           // thin bottom
  kCrops = 8,          // 0,0,0 -> 1,1,1 but passable
  kSnowLayer = 9,      // 0,0,0 -> 1,1/8,1
  kCarpet = 10,        // 0,0,0 -> 1,1/16,1
  kDaylightSensor = 11, // 0,0,0 -> 1,1/2,1
  kTrapdoor = 12,      // vertical thin
  kFence = 13,         // 1.5 height with post
  kWall = 14,          // 1.5 height
  kSlabBottom = 15,    // 0,0,0 -> 1,0.5,1
  kSlabTop = 16,       // 0,0.5,0 -> 1,1,1
  kDoubleSlab = 17,    // full block
  kStairs = 18,        // complex shape
  kAnvil = 19,         // complex
  kEnchantTable = 20,  // 0,0,0 -> 1,0.75,1
  kEndPortalFrame = 21, // complex
  kCocoa = 22,         // thin on side
  kPistonExtension = 23,
  kChest = 24,         // full block
  kEnderChest = 25,
  kSign = 26,          // thin
  kSkull = 27,         // thin
  kFlowerPot = 28,     // small
  kItemFrame = 29,     // thin on side
};

// Get collision AABB for block at (x,y,z) relative to block origin
AABB GetBlockAABB(int32_t block_id, int32_t meta, int x, int y, int z);

// Check if block is solid (full cube collision)
bool IsBlockSolid(int32_t block_id, int32_t meta);

// Check if block is liquid
bool IsBlockLiquid(int32_t block_id);

// Check if block is climbable (ladder, vine)
bool IsBlockClimbable(int32_t block_id, int32_t meta);

// ---- Entity Physics ----
struct EntityPhysics {
  // Player collision box: 0.6 x 1.8 x 0.6 centered at feet
  static constexpr double kPlayerWidth = 0.3;
  static constexpr double kPlayerHeight = 1.8;
  static constexpr double kPlayerEyeHeight = 1.62;

  // Mob collision boxes (vanilla 1.8)
  static AABB GetMobAABB(uint8_t mob_type, double x, double y, double z);

  // Item entity: 0.25 x 0.25 x 0.25
  static constexpr AABB ItemAABB() { return AABB(-0.125, 0, -0.125, 0.125, 0.25, 0.125); }
  // Falling block: full block
  static constexpr AABB FallingBlockAABB() { return AABB(0, 0, 0, 1, 1, 1); }
  // TNT: 0.98 x 0.98 x 0.98
  static constexpr AABB TNTAABB() { return AABB(-0.49, 0, -0.49, 0.49, 0.98, 0.49); }
};

struct PhysicsState {
  double x = 0, y = 0, z = 0;
  double vx = 0, vy = 0, vz = 0;
  bool on_ground = false;
  bool in_water = false;
  bool in_lava = false;
  bool on_ladder = false;
  double water_level = 0;  // height of water surface
  double lava_level = 0;
};

// Collision result
struct CollisionResult {
  bool collided = false;
  double x = 0, y = 0, z = 0;  // new position after collision
  double vx = 0, vy = 0, vz = 0;  // new velocity
  bool hit_x = false, hit_y = false, hit_z = false;
  bool on_ground = false;
  bool in_water = false;
  bool in_lava = false;
};

// Physics engine
class PhysicsEngine {
 public:
  explicit PhysicsEngine(const Chunk *world_chunk);  // single chunk for now
  ~PhysicsEngine() = default;

  // Step entity physics for one tick (1/20s)
  CollisionResult Step(const AABB &entity_box, const PhysicsState &state, double dt = 0.05);

  // Player-specific step with input
  CollisionResult StepPlayer(const AABB &box, const PhysicsState &state,
                             double move_x, double move_z, bool jump, bool sprint, bool sneak);

  // Validate player movement (anti-cheat)
  // Returns true if movement is valid, false if impossible
  bool ValidateMovement(const PhysicsState &old_state, const PhysicsState &new_state,
                        double max_speed, bool allow_flight);

 private:
  const Chunk *chunk_;

  // Get block at world coordinates
  int32_t GetBlock(int x, int y, int z, int32_t *meta) const;

  // Check collisions with blocks in AABB
void CollideBlocks(const AABB &box,
                                 double *vx, double *vy, double *vz,
                                 bool *hit_x, bool *hit_y, bool *hit_z,
                                 bool *on_ground, bool *in_water, bool *in_lava);

  // Water/lava physics
  void ApplyFluidPhysics(PhysicsState *state, double dt);
  double GetFluidHeight(int x, int y, int z, bool *is_lava) const;
};

// ---- Explosions ----
struct ExplosionResult {
  std::vector<AABB> affected_blocks;  // blocks destroyed
  std::vector<physics::AABB> affected_entities;  // entities damaged
  double center_x, center_y, center_z;
  float power;
};

ExplosionResult CreateExplosion(const Chunk *world, double x, double y, double z,
                                float power, bool create_fire, bool damage_blocks);

// Apply explosion to entity (knockback + damage)
void ApplyExplosionKnockback(PhysicsState *state, const ExplosionResult &explosion,
                             const AABB &entity_box);

}  // namespace minecpp::v18::physics