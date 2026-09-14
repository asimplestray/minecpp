#include "minecpp/core/version_api.h"

#include <cstdlib>

#include "minecpp/v1_8/chunk.h"

namespace {

using minecpp::v18::Chunk;

void *V18ChunkCreate(int32_t x, int32_t z) {
  return minecpp::v18::ChunkCreate(x, z);
}

void V18ChunkFree(void *chunk) {
  minecpp::v18::ChunkFree(static_cast<Chunk *>(chunk));
}

int V18ChunkFromNbt(void *chunk, const uint8_t *data, size_t len) {
  if (!chunk || !data) return -1;
  minecpp_nbt_tag_t *root = nullptr;
  if (minecpp_nbt_parse(data, len, 0, &root, nullptr) != MINECPP_NBT_OK)
    return -1;
  const int rc = minecpp::v18::ChunkFromRoot(static_cast<Chunk *>(chunk), root);
  minecpp_nbt_free(root);
  return rc;
}

int V18ChunkToNbt(const void *chunk, minecpp_buffer_t *out) {
  if (!chunk || !out) return -1;
  minecpp_nbt_tag_t *root =
      minecpp::v18::ChunkToRoot(static_cast<const Chunk *>(chunk));
  if (!root) return -1;
  uint8_t *buf = nullptr;
  size_t len = 0;
  const minecpp_nbt_err_t e = minecpp_nbt_serialize(root, &buf, &len);
  minecpp_nbt_free(root);
  if (e != MINECPP_NBT_OK) return -1;
  out->data = buf;
  out->len = len;
  out->cap = len;
  return 0;
}

int32_t V18BlockGet(const void *chunk, int x, int y, int z, int32_t *meta) {
  return minecpp::v18::BlockGet(static_cast<const Chunk *>(chunk), x, y, z,
                                meta);
}

int V18BlockSet(void *chunk, int x, int y, int z, int32_t id, int32_t meta) {
  return minecpp::v18::BlockSet(static_cast<Chunk *>(chunk), x, y, z, id,
                                meta);
}

}  // namespace

extern "C" const minecpp_version_api_t *minecpp_v1_8_api(void) {
  static const minecpp_version_api_t api = {
      47,  // protocolo 1.8.x
      "1.8.x",
      V18ChunkCreate,
      V18ChunkFree,
      V18ChunkFromNbt,
      V18ChunkToNbt,
      V18BlockGet,
      V18BlockSet,
  };
  return &api;
}
