add_library(minecpp_v1_8 STATIC
  src/version.cpp
  src/chunk.cpp
  src/worldgen.cpp
  src/protocol.cpp
  src/server.cpp
  src/net_thread.cpp
)

target_include_directories(minecpp_v1_8 PUBLIC include)
target_link_libraries(minecpp_v1_8 PUBLIC minecpp_core)

add_executable(minecpp-1.8 src/main.cpp)
target_link_libraries(minecpp-1.8 PRIVATE minecpp_v1_8 minecpp_core)

# Binario final: minecpp-1.8 (release baixa esse artefato, nao o source).
set_target_properties(minecpp-1.8 PROPERTIES OUTPUT_NAME "minecpp-1.8")
