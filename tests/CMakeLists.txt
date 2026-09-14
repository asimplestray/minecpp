find_package(ZLIB REQUIRED)

add_executable(test_nbt test_nbt.c)
target_link_libraries(test_nbt PRIVATE minecpp_core ZLIB::ZLIB)
target_compile_definitions(test_nbt PRIVATE
  TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/test-data/vanilla-1.8-flat")

add_test(NAME nbt COMMAND test_nbt)

add_executable(test_region test_region.c)
target_link_libraries(test_region PRIVATE minecpp_core ZLIB::ZLIB)
target_compile_definitions(test_region PRIVATE
  TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/test-data/vanilla-1.8-flat")

add_test(NAME region COMMAND test_region)

add_executable(test_chunk test_chunk.cpp)
target_link_libraries(test_chunk PRIVATE minecpp_v1_8 minecpp_core ZLIB::ZLIB)
target_compile_definitions(test_chunk PRIVATE
  TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/test-data/vanilla-1.8-flat")

add_test(NAME chunk COMMAND test_chunk)

add_executable(test_thread_pool test_thread_pool.c)
target_link_libraries(test_thread_pool PRIVATE minecpp_core)

add_test(NAME thread_pool COMMAND test_thread_pool)

add_executable(test_buf test_buf.c)
target_link_libraries(test_buf PRIVATE minecpp_core)

add_test(NAME buf COMMAND test_buf)

add_executable(test_protocol test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE minecpp_v1_8 minecpp_core ZLIB::ZLIB)
target_compile_definitions(test_protocol PRIVATE
  TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/test-data/vanilla-1.8-flat")

add_test(NAME protocol COMMAND test_protocol)

add_executable(test_mqueue test_mqueue.c)
target_link_libraries(test_mqueue PRIVATE minecpp_core)

add_test(NAME mqueue COMMAND test_mqueue)

add_executable(test_crypto test_crypto.c)
target_link_libraries(test_crypto PRIVATE minecpp_core)

add_test(NAME crypto COMMAND test_crypto)

add_executable(test_server18 test_server18.cpp)
target_link_libraries(test_server18 PRIVATE minecpp_v1_8 minecpp_core ZLIB::ZLIB)
target_compile_definitions(test_server18 PRIVATE
  TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/test-data/vanilla-1.8-flat")

add_test(NAME server18 COMMAND test_server18)
