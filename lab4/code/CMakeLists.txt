cmake_minimum_required(VERSION 3.10)
project(os_lab4_dynamic_libs CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
add_library(impl_first SHARED src/impl1.cpp)
add_library(impl_second SHARED src/impl2.cpp)

add_executable(program1 src/program1.cpp)
target_link_libraries(program1 impl_first)

add_executable(program2 src/program2.cpp)
target_link_libraries(program2 dl)
