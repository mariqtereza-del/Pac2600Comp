# pacman2600 host (Phase D). Stage D2: an empty window (DECISIONS.md T-D2-1).
cmake_minimum_required(VERSION 3.30)
project(pacman2600_host LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# MSVC 14.51 ignores /std:c++23; pin the fixed C++23 mode (DECISIONS.md T-B3-2).
if(MSVC)
  set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "/std:c++23preview")
  set(CMAKE_CXX23_EXTENSION_COMPILE_OPTION "/std:c++23preview")
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party third_party)

# settings.ini reading and writing (Stage D7), and the remembered window state; no window code.
add_library(settings STATIC settings.cpp)
target_compile_options(settings PRIVATE /W4 /permissive-)
add_library(windowstate STATIC windowstate.cpp)
target_compile_options(windowstate PRIVATE /W4 /permissive-)
target_link_libraries(windowstate PUBLIC settings)

# The ROM finding and checking logic, with no window code.
add_library(rom_finder STATIC rom_finder.cpp)
target_compile_options(rom_finder PRIVATE /W4 /permissive-)
target_link_libraries(rom_finder PUBLIC settings)

# Where the picture goes in the window: pure arithmetic, no window code.
add_library(layout STATIC layout.cpp)
target_compile_options(layout PRIVATE /W4 /permissive-)

# The keyboard: key table and state (Stage D10); only SDL's key-code names, no window code.
add_library(controls STATIC controls.cpp)
target_compile_options(controls PRIVATE /W4 /permissive-)
target_link_libraries(controls PUBLIC SDL3::SDL3)

# Input recordings (Stage D13): the file format, no window code.
add_library(inputlog STATIC inputlog.cpp)
target_compile_options(inputlog PRIVATE /W4 /permissive-)

# Sound output through SDL (Stage D8).
add_library(audio STATIC audio.cpp)
target_compile_options(audio PRIVATE /W4 /permissive-)
target_link_libraries(audio PUBLIC SDL3::SDL3)

# The only host code that includes Stella headers (ARCHITECTURE.md, T-D1-1).
add_library(emulator STATIC emulator.cpp)
target_link_libraries(emulator PUBLIC stella_core)

# WIN32 = a normal windowed program (no console window when double-clicked).
add_executable(pacman2600 WIN32 main.cpp)
target_compile_options(pacman2600 PRIVATE /W4 /permissive-)
target_link_libraries(pacman2600 PRIVATE SDL3::SDL3 rom_finder emulator layout windowstate audio controls inputlog)

# Tests for the ROM finder (Stage D3): build/host/test_rom_finder.exe <scratch folder> <rom copy>
add_executable(test_rom_finder ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_rom_finder.cpp)
target_compile_options(test_rom_finder PRIVATE /W4 /permissive-)
target_link_libraries(test_rom_finder PRIVATE rom_finder)

# Tests for the layout (Stage D6): build/host/test_layout.exe
add_executable(test_layout ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_layout.cpp)
target_compile_options(test_layout PRIVATE /W4 /permissive-)
target_link_libraries(test_layout PRIVATE layout)

# Tests for the settings file and the remembered window state (Stage D7): build/host/test_windowstate.exe <scratch folder>
add_executable(test_windowstate ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_windowstate.cpp)
target_compile_options(test_windowstate PRIVATE /W4 /permissive-)
target_link_libraries(test_windowstate PRIVATE windowstate rom_finder)

# Tests for the keyboard (Stage D10): build/host/test_controls.exe
add_executable(test_controls ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_controls.cpp)
target_compile_options(test_controls PRIVATE /W4 /permissive-)
target_link_libraries(test_controls PRIVATE controls)

# Tests for the input recordings (Stage D13): build/host/test_inputlog.exe <scratch folder>
add_executable(test_inputlog ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_inputlog.cpp)
target_compile_options(test_inputlog PRIVATE /W4 /permissive-)
target_link_libraries(test_inputlog PRIVATE inputlog)

# Put SDL3.dll next to the program so a double-click finds it.
add_custom_command(TARGET pacman2600 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:pacman2600>)
