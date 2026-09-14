#include "minecpp/core/version_api.h"

extern "C" const minecpp_version_api_t *minecpp_v1_9_api(void);

extern "C" const minecpp_version_api_t *minecpp_v1_10_api(void) {
  static minecpp_version_api_t api = *minecpp_v1_9_api();
  static bool patched = false;
  if (!patched) {
    api.protocol_version = 210; // 1.10.x
    api.version_name = "1.10.x";
    // TODO: delta 1.10 (fossils, magma, nether wart block, polar bears).
    patched = true;
  }
  return &api;
}
