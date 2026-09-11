cmake_minimum_required(VERSION 3.28)

project(spotify-lite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Phase 0: minimal native executable. Linking against the Rust bridge
# happens in Phase 2.
add_executable(spotify-lite cpp/app/main.cpp)

# Rust bridge: CMake orchestrates the Cargo build (plans.md section 4).
find_program(CARGO_EXECUTABLE cargo REQUIRED)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(BRIDGE_CARGO_PROFILE_ARGS --release)
else()
  set(BRIDGE_CARGO_PROFILE_ARGS)
endif()
add_custom_target(rust-bridge ALL
  COMMAND ${CARGO_EXECUTABLE} build ${BRIDGE_CARGO_PROFILE_ARGS}
          --manifest-path ${CMAKE_SOURCE_DIR}/Cargo.toml
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Building Rust librespot bridge"
  VERBATIM)
add_dependencies(spotify-lite rust-bridge)
