add_library(minecpp_core STATIC
  src/platform.c
  src/event.c
  src/scheduler.c
  src/sync.c
  src/thread_pool.c
  src/nbt.c
  src/region.c
  src/buf.c
  src/mqueue.c
  src/crypto.c
  src/net.c
)

target_include_directories(minecpp_core PUBLIC include)
target_link_libraries(minecpp_core PUBLIC Threads::Threads)

find_package(ZLIB REQUIRED)
target_link_libraries(minecpp_core PUBLIC ZLIB::ZLIB)

# OpenSSL = RSA/AES do login online. Sem ele: offline-mode apenas.
find_package(OpenSSL)
if(OpenSSL_FOUND)
  target_compile_definitions(minecpp_core PUBLIC MINECPP_WITH_OPENSSL)
  target_link_libraries(minecpp_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  message(STATUS "minecpp: OpenSSL ON (login com cripto suportado)")
else()
  message(STATUS "minecpp: OpenSSL OFF (apenas offline-mode)")
endif()

# POSIX.1-2008 em todo o core+testes (fseeko, getpid...). Precisa valer antes
# de qualquer system header em cada TU — por isso global, não por arquivo.
if(UNIX)
  target_compile_definitions(minecpp_core PUBLIC _POSIX_C_SOURCE=200809L)
endif()

# MSVC precisa disso para __cplusplus correto + UTF-8. GCC/Clang ignoram.
if(MSVC)
  target_compile_options(minecpp_core PUBLIC /Zc:__cplusplus /utf-8)
endif()
