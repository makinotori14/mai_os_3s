cmake_minimum_required(VERSION 3.10)
project(mai_207)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(det_lib src/det.cpp)
target_include_directories(det_lib PUBLIC src)

add_executable(serial src/serial.cpp)
target_link_libraries(serial det_lib)

add_executable(parallel src/parallel.cpp)
target_link_libraries(parallel det_lib pthread)

add_executable(benchmark tests/benchmark.cpp)
target_link_libraries(benchmark det_lib GTest::gtest GTest::gtest_main pthread)
target_compile_options(benchmark PRIVATE -O2)