cmake_minimum_required(VERSION 3.16)
project(chip8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# SDL2: prefer the CMake config package, fall back to pkg-config.
find_package(SDL2 CONFIG QUIET)
if(NOT TARGET SDL2::SDL2)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
endif()

add_library(chip8_core
  src/cpu.cpp
  src/memory.cpp
  src/display.cpp
  src/input.cpp
  src/rom_loader.cpp
)
target_compile_options(chip8_core PRIVATE -Wall -Wextra)
target_include_directories(chip8_core PUBLIC src)
target_link_libraries(chip8_core PUBLIC SDL2::SDL2)

add_executable(chip8 main.cpp)
target_compile_options(chip8 PRIVATE -Wall -Wextra)
target_link_libraries(chip8 PRIVATE chip8_core)

# Headless runner used by the test procedure in the README: runs a ROM for a
# fixed number of simulated frames and dumps the framebuffer.
add_executable(chip8_run_rom tests/run_rom.cpp)
target_compile_options(chip8_run_rom PRIVATE -Wall -Wextra)
target_link_libraries(chip8_run_rom PRIVATE chip8_core)
