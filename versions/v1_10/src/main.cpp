#include <cstdio>

#include "minecpp/core/platform.h"
#include "minecpp/core/thread_pool.h"
#include "minecpp/core/version_api.h"

extern "C" const minecpp_version_api_t *minecpp_v1_10_api(void);

int main() {
  if (minecpp_platform_init() != 0) return 1;
  minecpp_thread_pool_init(0);
  const minecpp_version_api_t *api = minecpp_v1_10_api();
  std::printf("minecpp-%s protocolo=%d workers=%u cpu=%u\n", api->version_name,
              api->protocol_version, minecpp_thread_pool_workers(),
              minecpp_cpu_count());
  minecpp_thread_pool_shutdown();
  minecpp_platform_shutdown();
  return 0;
}
