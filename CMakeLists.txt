cmake_minimum_required(VERSION 3.16)
project(chip8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# SDL2: prefer the CMake config package (vcpkg, SDL2 dev packages), fall back
# to pkg-config.
find_package(SDL2 CONFIG QUIET)
if(NOT TARGET SDL2::SDL2)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
endif()

# GCC/Clang get -Wall -Wextra; MSVC gets /W4, UTF-8 sources and no CRT
# deprecation noise for sscanf and friends.
function(chip8_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /utf-8)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
endfunction()

# Windows: put SDL2.dll next to each executable so the build tree runs as is.
function(chip8_copy_runtime_dlls target)
  if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
      COMMAND_EXPAND_LISTS)
  endif()
endfunction()

add_library(chip8_core
  src/cpu.cpp
  src/memory.cpp
  src/display.cpp
  src/input.cpp
  src/rom_loader.cpp
)
chip8_set_warnings(chip8_core)
target_include_directories(chip8_core PUBLIC src)
target_link_libraries(chip8_core PUBLIC SDL2::SDL2)

add_executable(chip8 main.cpp)
chip8_set_warnings(chip8)
# SDL.h renames main() to SDL_main() on Windows; SDL2main supplies the real
# entry point.
if(WIN32 AND TARGET SDL2::SDL2main)
  target_link_libraries(chip8 PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(chip8 PRIVATE chip8_core)
chip8_copy_runtime_dlls(chip8)

# Headless runner used by the test procedure in the README: runs a ROM for a
# fixed number of simulated frames and dumps the framebuffer.
add_executable(chip8_run_rom tests/run_rom.cpp)
chip8_set_warnings(chip8_run_rom)
target_link_libraries(chip8_run_rom PRIVATE chip8_core)
chip8_copy_runtime_dlls(chip8_run_rom)
