cmake_minimum_required(VERSION 3.16)

project(fifo_im_chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(im_server im_server.cpp)
add_executable(im_client im_client.cpp)

