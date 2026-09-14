add_library(minecpp_v1_10 STATIC
  src/version.cpp
)

target_include_directories(minecpp_v1_10 PUBLIC include)
target_link_libraries(minecpp_v1_10 PUBLIC minecpp_v1_9 minecpp_v1_8 minecpp_core)

add_executable(minecpp-1.10 src/main.cpp)
target_link_libraries(minecpp-1.10 PRIVATE minecpp_v1_10 minecpp_v1_9 minecpp_v1_8 minecpp_core)
set_target_properties(minecpp-1.10 PROPERTIES OUTPUT_NAME "minecpp-1.10")
