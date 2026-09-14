add_library(minecpp_v1_9 STATIC
  src/version.cpp
)

target_include_directories(minecpp_v1_9 PUBLIC include)
# Delta: linka a base. 90% dos simbolos reutilizados sem copiar codigo.
target_link_libraries(minecpp_v1_9 PUBLIC minecpp_v1_8 minecpp_core)

add_executable(minecpp-1.9 src/main.cpp)
target_link_libraries(minecpp-1.9 PRIVATE minecpp_v1_9 minecpp_v1_8 minecpp_core)
set_target_properties(minecpp-1.9 PROPERTIES OUTPUT_NAME "minecpp-1.9")
