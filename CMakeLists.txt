cmake_minimum_required(VERSION 3.28)

project(spotify-lite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Phase 0: minimal native executable. The Rust bridge (rust/librespot-bridge)
# is wired into the CMake build in a later milestone; linking happens in Phase 2.
add_executable(spotify-lite cpp/app/main.cpp)
