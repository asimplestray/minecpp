find_package(ZLIB REQUIRED)

add_executable(minecpp-dump minecpp-dump.cpp)
target_link_libraries(minecpp-dump PRIVATE minecpp_v1_8 minecpp_core ZLIB::ZLIB)
set_target_properties(minecpp-dump PROPERTIES OUTPUT_NAME "minecpp-dump")
