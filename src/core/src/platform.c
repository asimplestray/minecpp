#define _GNU_SOURCE  // para CPU_ZERO, CPU_SET, pthread_setaffinity_np, sched_getcpu

#include "minecpp/core/platform.h"

#if MINECPP_PLATFORM_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sched.h>
#  include <pthread.h>
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

int minecpp_thread_set_affinity(unsigned core_id) {
#if MINECPP_PLATFORM_WINDOWS
  HANDLE thread = GetCurrentThread();
  DWORD_PTR mask = (DWORD_PTR)1 << core_id;
  if (SetThreadAffinityMask(thread, mask) == 0) return -1;
  return 0;
#else
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) return -1;
  return 0;
#endif
}

int minecpp_thread_get_affinity(void) {
#if MINECPP_PLATFORM_WINDOWS
  // Windows: nao tem API direta para pegar afinidade atual
  return -1;
#else
  return sched_getcpu();
#endif
}
