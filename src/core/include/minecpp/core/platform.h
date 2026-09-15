#pragma once

// Abstracao de plataforma. NADA de <epoll.h> / <windows.h> fora de platform_*.c.
// Todo o resto do codigo inclui so este header.

#if defined(_WIN32) || defined(_WIN64)
#  define MINECPP_PLATFORM_WINDOWS 1
#  define MINECPP_PLATFORM_LINUX 0
#else
#  define MINECPP_PLATFORM_WINDOWS 0
#  define MINECPP_PLATFORM_LINUX 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
#  define MINECPP_ARCH_X64 1
#else
#  define MINECPP_ARCH_X64 0
#endif

#define MINECPP_CACHE_LINE 64

#if defined(_MSC_VER)
#  define MINECPP_ALIGNAS(n) __declspec(align(n))
#else
#  define MINECPP_ALIGNAS(n) __attribute__((aligned(n)))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Inicializacao global da plataforma (WSAStartup no Windows, noop no Linux).
// Deve ser chamada uma vez no main antes de qualquer thread/net.
int minecpp_platform_init(void);
void minecpp_platform_shutdown(void);

// Numero de threads hardware. Retorna >= 2 sempre (fallback seguro).
unsigned minecpp_cpu_count(void);

// Seta afinidade da thread atual para um core logico (0..cpu_count-1).
// Retorna 0 ok, -1 erro (core invalido ou sem permissao).
int minecpp_thread_set_affinity(unsigned core_id);

// Obtem ID do core logico onde a thread atual esta rodando (Linux: sched_getcpu).
// Retorna -1 se nao suportado.
int minecpp_thread_get_affinity(void);

#ifdef __cplusplus
}
#endif
