#include "minecpp/core/platform.h"

#if MINECPP_PLATFORM_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <unistd.h>
#endif

int minecpp_platform_init(void) {
#if MINECPP_PLATFORM_WINDOWS
  WSADATA wsa;
  return WSAStartup(MAKEWORD(2, 2), &wsa);
#else
  return 0;
#endif
}

void minecpp_platform_shutdown(void) {
#if MINECPP_PLATFORM_WINDOWS
  WSACleanup();
#endif
}

unsigned minecpp_cpu_count(void) {
#if MINECPP_PLATFORM_WINDOWS
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwNumberOfProcessors >= 2 ? (unsigned)si.dwNumberOfProcessors : 2u;
#else
  long n = sysconf(_SC_NPROCESSORS_ONLN);
  return n >= 2 ? (unsigned)n : 2u;
#endif
}
