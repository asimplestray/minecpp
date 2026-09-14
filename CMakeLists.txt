cmake_minimum_required(VERSION 3.25)

project(minecpp LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Um binario por versao. Repo monolito, release por versao.
option(MINECPP_BUILD_V1_8 "Build server 1.8.x (base legacy)" ON)
option(MINECPP_BUILD_V1_9 "Build server 1.9.x (delta sobre 1.8)" ON)
option(MINECPP_BUILD_V1_10 "Build server 1.10.x (delta sobre 1.9)" ON)
option(MINECPP_ENABLE_TESTS "Enable CTest" ON)
option(MINECPP_WARNINGS_AS_ERRORS "Warnings as errors" ON)

find_package(Threads REQUIRED)

# Flags de warning portateis (GCC/Clang/MSVC). Detalhe em cmake/warnings.cmake
include(cmake/warnings.cmake)

add_subdirectory(src/core)

if(MINECPP_BUILD_V1_8)
  add_subdirectory(versions/v1_8)
  add_subdirectory(tools)
endif()

if(MINECPP_BUILD_V1_9)
  if(NOT MINECPP_BUILD_V1_8)
    message(FATAL_ERROR "v1_9 precisa de v1_8 (delta). Ative MINECPP_BUILD_V1_8.")
  endif()
  add_subdirectory(versions/v1_9)
endif()

if(MINECPP_BUILD_V1_10)
  if(NOT MINECPP_BUILD_V1_9)
    message(FATAL_ERROR "v1_10 precisa de v1_9 (delta). Ative MINECPP_BUILD_V1_9.")
  endif()
  add_subdirectory(versions/v1_10)
endif()

if(MINECPP_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
