#include "minecpp/core/version_api.h"

extern "C" const minecpp_version_api_t *minecpp_v1_8_api(void);

// v1_9 = v1_8 + delta. Copia a struct e sobrescreve SO o que mudou.
// Nada de heranca C++. Ver docs/architecture.md secao 3.
extern "C" const minecpp_version_api_t *minecpp_v1_9_api(void) {
  static minecpp_version_api_t api = *minecpp_v1_8_api();
  static bool patched = false;
  if (!patched) {
    api.protocol_version = 110; // 1.9.3/1.9.4
    api.version_name = "1.9.x";
    // TODO: override pacotes/chunks que mudaram na 1.9
    // (ex: EntityTeleport com yaw/pitch, novos blocos end, dual wield).
    patched = true;
  }
  return &api;
}
