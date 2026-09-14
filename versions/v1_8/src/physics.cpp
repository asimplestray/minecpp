#include "minecpp/v1_8/physics.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace minecpp::v18::physics {

// ---- Block collision shapes ----

// Vanilla 1.8 block bounds (from bku/Block.java)
static const struct BlockBounds {
  int32_t id;
  int32_t meta;
  BlockShape shape;
} kBlockShapes[] = {
  {0, 0, BlockShape::kEmpty},           // air
  {1, 0, BlockShape::kFullBlock},       // stone
  {2, 0, BlockShape::kFullBlock},       // grass
  {3, 0, BlockShape::kFullBlock},       // dirt
  {4, 0, BlockShape::kFullBlock},       // cobblestone
  {5, 0, BlockShape::kFullBlock},       // planks
  {6, 0, BlockShape::kFullBlock},       // sapling
  {7, 0, BlockShape::kFullBlock},       // bedrock
  {8, 0, BlockShape::kLiquid},          // water
  {9, 0, BlockShape::kLiquid},          // stationary water
  {10, 0, BlockShape::kLiquid},         // lava
  {11, 0, BlockShape::kLiquid},         // stationary lava
  {12, 0, BlockShape::kFullBlock},      // sand
  {13, 0, BlockShape::kFullBlock},      // gravel
  {14, 0, BlockShape::kFullBlock},      // gold ore
  {15, 0, BlockShape::kFullBlock},      // iron ore
  {16, 0, BlockShape::kFullBlock},      // coal ore
  {17, 0, BlockShape::kFullBlock},      // log
  {18, 0, BlockShape::kFullBlock},      // leaves
  {19, 0, BlockShape::kFullBlock},      // sponge
  {20, 0, BlockShape::kFullBlock},      // glass
  {21, 0, BlockShape::kFullBlock},      // lapis ore
  {22, 0, BlockShape::kFullBlock},      // lapis block
  {23, 0, BlockShape::kFullBlock},      // dispenser
  {24, 0, BlockShape::kFullBlock},      // sandstone
  {25, 0, BlockShape::kFullBlock},      // noteblock
  {26, 0, BlockShape::kFullBlock},      // bed
  {27, 0, BlockShape::kFullBlock},      // golden rail
  {28, 0, BlockShape::kFullBlock},      // detector rail
  {29, 0, BlockShape::kFullBlock},      // sticky piston
  {30, 0, BlockShape::kFullBlock},      // web
  {31, 0, BlockShape::kFullBlock},      // tallgrass
  {32, 0, BlockShape::kFullBlock},      // deadbush
  {33, 0, BlockShape::kFullBlock},      // piston
  {34, 0, BlockShape::kFullBlock},      // piston head
  {35, 0, BlockShape::kFullBlock},      // wool
  {36, 0, BlockShape::kFullBlock},      // piston extension
  {37, 0, BlockShape::kFullBlock},      // yellow flower
  {38, 0, BlockShape::kFullBlock},      // red flower
  {39, 0, BlockShape::kFullBlock},      // brown mushroom
  {40, 0, BlockShape::kFullBlock},      // red mushroom
  {41, 0, BlockShape::kFullBlock},      // gold block
  {42, 0, BlockShape::kFullBlock},      // iron block
  {43, 0, BlockShape::kFullBlock},      // double slab
  {44, 0, BlockShape::kSlabBottom},     // slab
  {45, 0, BlockShape::kFullBlock},      // brick
  {46, 0, BlockShape::kFullBlock},      // tnt
  {47, 0, BlockShape::kFullBlock},      // bookshelf
  {48, 0, BlockShape::kFullBlock},      // moss stone
  {49, 0, BlockShape::kFullBlock},      // obsidian
  {50, 0, BlockShape::kFullBlock},      // torch
  {51, 0, BlockShape::kFullBlock},      // fire
  {52, 0, BlockShape::kFullBlock},      // mob spawner
  {53, 0, BlockShape::kFullBlock},      // oak stairs
  {54, 0, BlockShape::kFullBlock},      // chest
  {55, 0, BlockShape::kFullBlock},      // redstone wire
  {56, 0, BlockShape::kFullBlock},      // diamond ore
  {57, 0, BlockShape::kFullBlock},      // diamond block
  {58, 0, BlockShape::kFullBlock},      // crafting table
  {59, 0, BlockShape::kFullBlock},      // crops
  {60, 0, BlockShape::kFullBlock},      // farmland
  {61, 0, BlockShape::kFullBlock},      // furnace
  {62, 0, BlockShape::kFullBlock},      // burning furnace
  {63, 0, BlockShape::kFullBlock},      // sign post
  {64, 0, BlockShape::kFullBlock},      // wooden door
  {65, 0, BlockShape::kFullBlock},      // ladder
  {66, 0, BlockShape::kFullBlock},      // rail
  {67, 0, BlockShape::kFullBlock},      // cobblestone stairs
  {68, 0, BlockShape::kFullBlock},      // wall sign
  {69, 0, BlockShape::kFullBlock},      // lever
  {70, 0, BlockShape::kFullBlock},      // stone pressure plate
  {71, 0, BlockShape::kFullBlock},      // iron door
  {72, 0, BlockShape::kFullBlock},      // wooden pressure plate
  {73, 0, BlockShape::kFullBlock},      // redstone ore
  {74, 0, BlockShape::kFullBlock},      // glowing redstone ore
  {75, 0, BlockShape::kFullBlock},      // redstone torch off
  {76, 0, BlockShape::kFullBlock},      // redstone torch on
  {77, 0, BlockShape::kFullBlock},      // stone button
  {78, 0, BlockShape::kFullBlock},      // snow layer
  {79, 0, BlockShape::kFullBlock},      // ice
  {80, 0, BlockShape::kFullBlock},      // snow
  {81, 0, BlockShape::kFullBlock},      // cactus
  {82, 0, BlockShape::kFullBlock},      // clay
  {83, 0, BlockShape::kFullBlock},      // reeds
  {84, 0, BlockShape::kFullBlock},      // jukebox
  {85, 0, BlockShape::kFullBlock},      // fence
  {86, 0, BlockShape::kFullBlock},      // pumpkin
  {87, 0, BlockShape::kFullBlock},      // netherrack
  {88, 0, BlockShape::kFullBlock},      // soul sand
  {89, 0, BlockShape::kFullBlock},      // glowstone
  {90, 0, BlockShape::kFullBlock},      // portal
  {91, 0, BlockShape::kFullBlock},      // jack o lantern
  {92, 0, BlockShape::kFullBlock},      // cake
  {93, 0, BlockShape::kFullBlock},      // redstone repeater off
  {94, 0, BlockShape::kFullBlock},      // redstone repeater on
  {95, 0, BlockShape::kFullBlock},      // locked chest
  {96, 0, BlockShape::kFullBlock},      // trapdoor
  {97, 0, BlockShape::kFullBlock},      // monster egg
  {98, 0, BlockShape::kFullBlock},      // stone brick
  {99, 0, BlockShape::kFullBlock},      // huge brown mushroom
  {100, 0, BlockShape::kFullBlock},     // huge red mushroom
  {101, 0, BlockShape::kFullBlock},     // iron bars
  {102, 0, BlockShape::kFullBlock},     // glass pane
  {103, 0, BlockShape::kFullBlock},     // melon
  {104, 0, BlockShape::kFullBlock},     // pumpkin stem
  {105, 0, BlockShape::kFullBlock},     // melon stem
  {106, 0, BlockShape::kFullBlock},     // vine
  {107, 0, BlockShape::kFullBlock},     // fence gate
  {108, 0, BlockShape::kFullBlock},     // brick stairs
  {109, 0, BlockShape::kFullBlock},     // stone brick stairs
  {110, 0, BlockShape::kFullBlock},     // mycelium
  {111, 0, BlockShape::kFullBlock},     // lily pad
  {112, 0, BlockShape::kFullBlock},     // nether brick
  {113, 0, BlockShape::kFullBlock},     // nether brick fence
  {114, 0, BlockShape::kFullBlock},     // nether brick stairs
  {115, 0, BlockShape::kFullBlock},     // nether wart
  {116, 0, BlockShape::kFullBlock},     // enchantment table
  {117, 0, BlockShape::kFullBlock},     // brewing stand
  {118, 0, BlockShape::kFullBlock},     // cauldron
  {119, 0, BlockShape::kFullBlock},     // end portal
  {120, 0, BlockShape::kFullBlock},     // end portal frame
  {121, 0, BlockShape::kFullBlock},     // end stone
  {122, 0, BlockShape::kFullBlock},     // dragon egg
  {123, 0, BlockShape::kFullBlock},     // redstone lamp off
  {124, 0, BlockShape::kFullBlock},     // redstone lamp on
  {125, 0, BlockShape::kFullBlock},     // double wood slab
  {126, 0, BlockShape::kFullBlock},     // wood slab
  {127, 0, BlockShape::kFullBlock},     // cocoa
  {128, 0, BlockShape::kFullBlock},     // sandstone stairs
  {129, 0, BlockShape::kFullBlock},     // emerald ore
  {130, 0, BlockShape::kFullBlock},     // ender chest
  {131, 0, BlockShape::kFullBlock},     // tripwire hook
  {132, 0, BlockShape::kFullBlock},     // tripwire
  {133, 0, BlockShape::kFullBlock},     // emerald block
  {134, 0, BlockShape::kFullBlock},     // spruce stairs
  {135, 0, BlockShape::kFullBlock},     // birch stairs
  {136, 0, BlockShape::kFullBlock},     // jungle stairs
  {137, 0, BlockShape::kFullBlock},     // command block
  {138, 0, BlockShape::kFullBlock},     // beacon
  {139, 0, BlockShape::kFullBlock},     // cobblestone wall
  {140, 0, BlockShape::kFullBlock},     // flower pot
  {141, 0, BlockShape::kFullBlock},     // carrot
  {142, 0, BlockShape::kFullBlock},     // potato
  {143, 0, BlockShape::kFullBlock},     // wooden button
  {144, 0, BlockShape::kFullBlock},     // skull
  {145, 0, BlockShape::kFullBlock},     // anvil
  {146, 0, BlockShape::kFullBlock},     // trapped chest
  {147, 0, BlockShape::kFullBlock},     // light weighted pressure plate
  {148, 0, BlockShape::kFullBlock},     // heavy weighted pressure plate
  {149, 0, BlockShape::kFullBlock},     // redstone comparator off
  {150, 0, BlockShape::kFullBlock},     // redstone comparator on
  {151, 0, BlockShape::kFullBlock},     // daylight sensor
  {152, 0, BlockShape::kFullBlock},     // redstone block
  {153, 0, BlockShape::kFullBlock},     // nether quartz ore
  {154, 0, BlockShape::kFullBlock},     // hopper
  {155, 0, BlockShape::kFullBlock},     // quartz block
  {156, 0, BlockShape::kFullBlock},     // quartz stairs
  {157, 0, BlockShape::kFullBlock},     // activator rail
  {158, 0, BlockShape::kFullBlock},     // dropper
  {159, 0, BlockShape::kFullBlock},     // stained clay
  {160, 0, BlockShape::kFullBlock},     // stained glass pane
  {161, 0, BlockShape::kFullBlock},     // leaves 2
  {162, 0, BlockShape::kFullBlock},     // log 2
  {163, 0, BlockShape::kFullBlock},     // acacia stairs
  {164, 0, BlockShape::kFullBlock},     // dark oak stairs
  {165, 0, BlockShape::kFullBlock},     // slime
  {166, 0, BlockShape::kFullBlock},     // barrier
  {167, 0, BlockShape::kFullBlock},     // iron trapdoor
  {168, 0, BlockShape::kFullBlock},     // prismarine
  {169, 0, BlockShape::kFullBlock},     // sea lantern
  {170, 0, BlockShape::kFullBlock},     // hay bale
  {171, 0, BlockShape::kFullBlock},     // carpet
  {172, 0, BlockShape::kFullBlock},     // hardened clay
  {173, 0, BlockShape::kFullBlock},     // coal block
  {174, 0, BlockShape::kFullBlock},     // packed ice
  {175, 0, BlockShape::kFullBlock},     // double plant
  {206, 0, BlockShape::kFullBlock},     // purpur block
  {207, 0, BlockShape::kFullBlock},     // purpur pillar
  {208, 0, BlockShape::kFullBlock},     // purpur stairs
  {209, 0, BlockShape::kFullBlock},     // purpur double slab
  {210, 0, BlockShape::kFullBlock},     // purpur slab
  {211, 0, BlockShape::kFullBlock},     // end rod
  {212, 0, BlockShape::kFullBlock},     // chorus plant
  {213, 0, BlockShape::kFullBlock},     // chorus flower
  {214, 0, BlockShape::kFullBlock},     // purpur block
  {215, 0, BlockShape::kFullBlock},     // end stone bricks
  {216, 0, BlockShape::kFullBlock},     // beetroot
  {217, 0, BlockShape::kFullBlock},     // grass path
  {218, 0, BlockShape::kFullBlock},     // end gateway
  {219, 0, BlockShape::kFullBlock},     // repeating command block
  {220, 0, BlockShape::kFullBlock},     // chain command block
  {221, 0, BlockShape::kFullBlock},     // frost
  {222, 0, BlockShape::kFullBlock},     // magma
  {223, 0, BlockShape::kFullBlock},     // nether wart block
  {224, 0, BlockShape::kFullBlock},     // red nether brick
  {225, 0, BlockShape::kFullBlock},     // bone block
  {226, 0, BlockShape::kFullBlock},     // structure void
  {227, 0, BlockShape::kFullBlock},     // observer
  {228, 0, BlockShape::kFullBlock},     // white shulker box
  {229, 0, BlockShape::kFullBlock},     // orange shulker box
  {230, 0, BlockShape::kFullBlock},     // magenta shulker box
  {231, 0, BlockShape::kFullBlock},     // light blue shulker box
  {232, 0, BlockShape::kFullBlock},     // yellow shulker box
  {233, 0, BlockShape::kFullBlock},     // lime shulker box
  {234, 0, BlockShape::kFullBlock},     // pink shulker box
  {235, 0, BlockShape::kFullBlock},     // gray shulker box
  {236, 0, BlockShape::kFullBlock},     // silver shulker box
  {237, 0, BlockShape::kFullBlock},     // cyan shulker box
  {238, 0, BlockShape::kFullBlock},     // purple shulker box
  {239, 0, BlockShape::kFullBlock},     // blue shulker box
  {240, 0, BlockShape::kFullBlock},     // brown shulker box
  {241, 0, BlockShape::kFullBlock},     // green shulker box
  {242, 0, BlockShape::kFullBlock},     // red shulker box
  {243, 0, BlockShape::kFullBlock},     // black shulker box
  {244, 0, BlockShape::kFullBlock},     // white glazed terracotta
};

AABB GetBlockAABB(int32_t block_id, int32_t meta, int x, int y, int z) {
  // Find block shape
  BlockShape shape = BlockShape::kFullBlock;
  for (const auto &bs : kBlockShapes) {
    if (bs.id == block_id && (bs.meta == 0 || bs.meta == meta)) {
      shape = bs.shape;
      break;
    }
  }

  AABB box;
  switch (shape) {
    case BlockShape::kEmpty:
      box = AABB(0, 0, 0, 0, 0, 0);
      break;
    case BlockShape::kFullBlock:
      box = AABB(0, 0, 0, 1, 1, 1);
      break;
    case BlockShape::kLiquid:
      box = AABB(0, 0, 0, 1, 0.875, 1);  // 7/8 height
      break;
    case BlockShape::kSlabBottom:
      box = AABB(0, 0, 0, 1, 0.5, 1);
      break;
    case BlockShape::kSlabTop:
      box = AABB(0, 0.5, 0, 1, 1, 1);
      break;
    case BlockShape::kSnowLayer:
      box = AABB(0, 0, 0, 1, 0.125, 1);  // 1/8
      break;
    case BlockShape::kCarpet:
      box = AABB(0, 0, 0, 1, 0.0625, 1);  // 1/16
      break;
    case BlockShape::kDaylightSensor:
      box = AABB(0, 0, 0, 1, 0.5, 1);
      break;
    case BlockShape::kFence:
    case BlockShape::kWall:
      box = AABB(0, 0, 0, 1, 1.5, 1);
      break;
    case BlockShape::kEnchantTable:
      box = AABB(0, 0, 0, 1, 0.75, 1);
      break;
    default:
      box = AABB(0, 0, 0, 1, 1, 1);
      break;
  }
  return box.Offset(x, y, z);
}

bool IsBlockSolid(int32_t block_id, int32_t meta) {
  (void)meta;
  // Non-solid blocks
  static const int32_t non_solid[] = {
    0, 6, 8, 9, 10, 11, 31, 32, 37, 38, 39, 40, 50, 51, 55, 59,
    63, 65, 68, 69, 70, 72, 75, 76, 77, 78, 83, 104, 105, 106,
    111, 115, 141, 142, 165, 171, 216
  };
  for (int id : non_solid) if (block_id == id) return false;
  return true;
}

bool IsBlockLiquid(int32_t block_id) {
  return block_id == 8 || block_id == 9 || block_id == 10 || block_id == 11;
}

bool IsBlockClimbable(int32_t block_id, int32_t meta) {
  (void)meta;
  return block_id == 65 || block_id == 106;  // ladder, vine
}

// ---- Entity Physics ----

AABB EntityPhysics::GetMobAABB(uint8_t mob_type, double x, double y, double z) {
  // Vanilla 1.8 mob collision boxes
  double w = 0.6, h = 1.8;
  switch (mob_type) {
    case 50: case 51: case 52: case 54: case 55: case 58:  // creeper, skeleton, spider, zombie, slime, enderman
      w = 0.6; h = 1.8; break;
    case 90: case 91: case 92: case 93: case 94:  // pig, sheep, cow, chicken, squid
      w = 0.9; h = 1.3; break;
    case 10: case 95:  // chicken, bat
      w = 0.4; h = 0.7; break;
    case 1:  // item
      w = 0.25; h = 0.25; break;
    default:
      w = 0.6; h = 1.8; break;
  }
  return AABB(x - w/2, y, z - w/2, x + w/2, y + h, z + w/2);
}

// ---- PhysicsEngine ----

PhysicsEngine::PhysicsEngine(const Chunk *world_chunk) : chunk_(world_chunk) {}

int32_t PhysicsEngine::GetBlock(int x, int y, int z, int32_t *meta) const {
  if (!chunk_) return 0;
  if (y < 0 || y >= 256) return 0;
  return BlockGet(chunk_, x, y, z, meta);
}

CollisionResult PhysicsEngine::Step(const AABB &entity_box, const PhysicsState &state, double dt) {
  CollisionResult result;
  result.x = state.x;
  result.y = state.y;
  result.z = state.z;
  result.vx = state.vx;
  result.vy = state.vy;
  result.vz = state.vz;

  // Apply gravity
  if (!state.on_ground && !state.in_water && !state.in_lava) {
    result.vy -= 0.08 * dt * 20;  // 0.08 per tick
    if (result.vy < -0.98) result.vy = -0.98;  // terminal velocity
  }

  // Drag
  result.vx *= 0.98;
  result.vz *= 0.98;

  // Move X
  double new_x = result.x + result.vx * dt;
  double new_y = result.y;
  double new_z = result.z;

  // Collide X
  CollideBlocks(entity_box.Offset(new_x - result.x, 0, 0),
                &result.vx, &result.vy, &result.vz,
                &result.hit_x, &result.hit_y, &result.hit_z,
                &result.on_ground, &result.in_water, &result.in_lava);
  result.x = new_x;

  // Move Y
  new_y = result.y + result.vy * dt;
  CollideBlocks(entity_box.Offset(0, new_y - result.y, 0),
                &result.vx, &result.vy, &result.vz,
                &result.hit_x, &result.hit_y, &result.hit_z,
                &result.on_ground, &result.in_water, &result.in_lava);
  result.y = new_y;

  // Move Z
  new_z = result.z + result.vz * dt;
  CollideBlocks(entity_box.Offset(0, 0, new_z - result.z),
                &result.vx, &result.vy, &result.vz,
                &result.hit_x, &result.hit_y, &result.hit_z,
                &result.on_ground, &result.in_water, &result.in_lava);
  result.z = new_z;

  // Fluid physics
  if (result.in_water || result.in_lava) {
    physics::PhysicsState fluid_state;
    fluid_state.x = result.x;
    fluid_state.y = result.y;
    fluid_state.z = result.z;
    fluid_state.vx = result.vx;
    fluid_state.vy = result.vy;
    fluid_state.vz = result.vz;
    fluid_state.on_ground = result.on_ground;
    fluid_state.in_water = result.in_water;
    fluid_state.in_lava = result.in_lava;
    ApplyFluidPhysics(&fluid_state, dt);
    result.vx = fluid_state.vx;
    result.vy = fluid_state.vy;
    result.vz = fluid_state.vz;
  }

  return result;
}

CollisionResult PhysicsEngine::StepPlayer(const AABB &box, const PhysicsState &state,
                                          double move_x, double move_z, bool jump, bool sprint, bool sneak) {
  CollisionResult result = Step(box, state);

  // Apply player movement input
  double speed = 0.1;
  if (sprint) speed *= 1.3;
  if (state.in_water) speed *= 0.5;
  if (state.on_ladder) speed *= 0.15;

  // Movement direction relative to yaw
  double yaw_rad = state.x; // placeholder - would need actual yaw
  double forward_x = -sin(yaw_rad);
  double forward_z = cos(yaw_rad);
  double right_x = cos(yaw_rad);
  double right_z = sin(yaw_rad);

  double accel_x = (move_z * forward_x + move_x * right_x) * speed;
  double accel_z = (move_z * forward_z + move_x * right_z) * speed;

  result.vx += accel_x;
  result.vz += accel_z;

  // Jump
  if (jump && (state.on_ground || state.in_water || state.on_ladder)) {
    result.vy = 0.42;
    if (state.in_water) result.vy = 0.3;
    result.on_ground = false;
  }

  // Sneak prevents falling off edges
  if (sneak && result.on_ground) {
    // Check if moving would fall
    AABB test_box = box.Offset(result.vx * 0.05, 0, result.vz * 0.05);
    test_box.min_y -= 0.1;  // check below
    int32_t meta = 0;
    bool would_fall = true;
    int min_x = (int)floor(test_box.min_x);
    int max_x = (int)floor(test_box.max_x);
    int min_z = (int)floor(test_box.min_z);
    int max_z = (int)floor(test_box.max_z);
    for (int x = min_x; x <= max_x && would_fall; x++) {
      for (int z = min_z; z <= max_z && would_fall; z++) {
        int32_t id = GetBlock(x, (int)floor(test_box.min_y) - 1, z, &meta);
        if (IsBlockSolid(id, meta)) would_fall = false;
      }
    }
    if (would_fall) {
      result.vx = 0;
      result.vz = 0;
    }
  }

  // Re-run collision with new velocities
  physics::PhysicsState final_state;
  final_state.x = result.x;
  final_state.y = result.y;
  final_state.z = result.z;
  final_state.vx = result.vx;
  final_state.vy = result.vy;
  final_state.vz = result.vz;
  final_state.on_ground = result.on_ground;
  final_state.in_water = result.in_water;
  final_state.in_lava = result.in_lava;
  return Step(box, final_state);
}

bool PhysicsEngine::ValidateMovement(const PhysicsState &old_state, const PhysicsState &new_state,
                                     double max_speed, bool allow_flight) {
  // Check horizontal speed
  double dx = new_state.x - old_state.x;
  double dz = new_state.z - old_state.z;
  double horizontal_dist = sqrt(dx * dx + dz * dz);
  if (horizontal_dist > max_speed && !allow_flight) return false;

  // Check vertical speed (no flight)
  double dy = new_state.y - old_state.y;
  if (dy > 0.5 && !allow_flight && !old_state.in_water && !old_state.on_ladder) return false;

  // Check if moved through blocks (simplified)
  // Full validation would raycast
  return true;
}

void PhysicsEngine::CollideBlocks(const AABB &box,
                                  double *vx, double *vy, double *vz,
                                  bool *hit_x, bool *hit_y, bool *hit_z,
                                  bool *on_ground, bool *in_water, bool *in_lava) {
  *on_ground = false;
  *in_water = false;
  *in_lava = false;
  *hit_x = *hit_y = *hit_z = false;

  int min_x = (int)floor(box.min_x);
  int max_x = (int)floor(box.max_x);
  int min_y = (int)floor(box.min_y);
  int max_y = (int)floor(box.max_y);
  int min_z = (int)floor(box.min_z);
  int max_z = (int)floor(box.max_z);

  for (int bx = min_x; bx <= max_x; bx++) {
    for (int by = min_y; by <= max_y; by++) {
      for (int bz = min_z; bz <= max_z; bz++) {
        int32_t meta = 0;
        int32_t id = GetBlock(bx, by, bz, &meta);
        if (id == 0) continue;

        AABB block_box = GetBlockAABB(id, meta, bx, by, bz);

        if (box.Intersects(block_box)) {
          // Collision detected
          if (IsBlockLiquid(id)) {
            *in_water = (id == 8 || id == 9);
            *in_lava = (id == 10 || id == 11);
            continue;
          }

          // Calculate overlap
          double overlap_x = std::min(box.max_x, block_box.max_x) - std::max(box.min_x, block_box.min_x);
          double overlap_y = std::min(box.max_y, block_box.max_y) - std::max(box.min_y, block_box.min_y);
          double overlap_z = std::min(box.max_z, block_box.max_z) - std::max(box.min_z, block_box.min_z);

          // Find smallest overlap axis
          if (overlap_x <= overlap_y && overlap_x <= overlap_z) {
            // Push out X
            *vx = 0;
            *hit_x = true;
          } else if (overlap_y <= overlap_z) {
            // Push out Y
            if (*vy > 0) {
              *on_ground = false;
            } else {
              *on_ground = true;
            }
            *vy = 0;
            *hit_y = true;
          } else {
            // Push out Z
            *vz = 0;
            *hit_z = true;
          }
        }
      }
    }
  }
}

void PhysicsEngine::ApplyFluidPhysics(PhysicsState *state, double dt) {
  if (state->in_water) {
    // Water buoyancy
    state->vy += 0.02 * dt * 20;
    if (state->vy > 0.3) state->vy = 0.3;

    // Water drag
    state->vx *= 0.9;
    state->vy *= 0.9;
    state->vz *= 0.9;
  }
  if (state->in_lava) {
    // Lava damage would be handled elsewhere
    state->vy += 0.02 * dt * 20;
    if (state->vy > 0.3) state->vy = 0.3;
    state->vx *= 0.5;
    state->vy *= 0.5;
    state->vz *= 0.5;
  }
}

double PhysicsEngine::GetFluidHeight(int x, int y, int z, bool *is_lava) const {
  int32_t meta = 0;
  int32_t id = GetBlock(x, y, z, &meta);
  if (id == 8 || id == 9) {  // water
    *is_lava = false;
    return y + 0.875;  // water height
  }
  if (id == 10 || id == 11) {  // lava
    *is_lava = true;
    return y + 0.875;
  }
  return 0;
}

// ---- Explosions ----

ExplosionResult CreateExplosion(const Chunk *world, double x, double y, double z,
                                float power, bool, bool damage_blocks) {
  ExplosionResult result;
  result.center_x = x;
  result.center_y = y;
  result.center_z = z;
  result.power = power;

  if (!world) return result;

  // Radius in blocks
  int radius = (int)ceil(power * 2);
  (void)radius;  // used in loop below
  (void)damage_blocks;  // for future use

  // Raycast in 16 directions (like vanilla)
  for (int dx = -1; dx <= 1; dx++) {
    for (int dy = -1; dy <= 1; dy++) {
      for (int dz = -1; dz <= 1; dz++) {
        if (dx == 0 && dy == 0 && dz == 0) continue;
        double dir_x = dx, dir_y = dy, dir_z = dz;
        double len = sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
        dir_x /= len; dir_y /= len; dir_z /= len;

        double rx = x, ry = y, rz = z;
        for (int step = 0; step < radius * 2; step++) {
          int bx = (int)floor(rx);
          int by = (int)floor(ry);
          int bz = (int)floor(rz);

          int32_t meta = 0;
          int32_t id = BlockGet(world, bx, by, bz, &meta);
          if (id != 0 && IsBlockSolid(id, meta)) {
            double dist = sqrt((bx - x) * (bx - x) + (by - y) * (by - y) + (bz - z) * (bz - z));
            float block_resistance = 1.0f;
            // Block resistance values
            if (id == 7) block_resistance = 18000000;  // bedrock
            else if (id == 49) block_resistance = 6000;  // obsidian
            else if (id == 119) block_resistance = 3600000;  // end portal
            else if (id == 46) block_resistance = 0;  // TNT

            if (dist * block_resistance < power * 10) {
              if (damage_blocks) {
                result.affected_blocks.push_back(AABB(bx, by, bz, bx + 1, by + 1, bz + 1));
              }
            }
            break;
          }
          rx += dir_x * 0.3;
          ry += dir_y * 0.3;
          rz += dir_z * 0.3;
        }
      }
    }
  }

  return result;
}

void ApplyExplosionKnockback(PhysicsState *state, const ExplosionResult &explosion,
                             const AABB &entity_box) {
  double dx = (entity_box.min_x + entity_box.max_x) / 2 - explosion.center_x;
  double dy = (entity_box.min_y + entity_box.max_y) / 2 - explosion.center_y;
  double dz = (entity_box.min_z + entity_box.max_z) / 2 - explosion.center_z;
  double dist = sqrt(dx * dx + dy * dy + dz * dz);

  if (dist == 0) return;

  double exposure = 1.0;  // simplified
  double impact = (1.0 - dist / (explosion.power * 2)) * exposure;
  if (impact <= 0) return;

  impact = impact * impact * 0.5 + 0.2;
  double knockback = impact * explosion.power * 0.5;

  state->vx += dx / dist * knockback;
  state->vy += dy / dist * knockback + 0.1;  // slight upward
  state->vz += dz / dist * knockback;
}

}  // namespace minecpp::v18::physics