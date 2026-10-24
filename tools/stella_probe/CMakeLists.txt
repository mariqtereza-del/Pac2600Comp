# stella_probe: measuring tool built on the Stella 7.0 core (Stage C3, DECISIONS.md T-C3-1).
cmake_minimum_required(VERSION 3.30)
project(stella_probe LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# MSVC 14.51 ignores /std:c++23; pin the fixed C++23 mode (DECISIONS.md T-B3-2).
if(MSVC)
  set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "/std:c++23preview")
  set(CMAKE_CXX23_EXTENSION_COMPILE_OPTION "/std:c++23preview")
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party third_party)

add_executable(stella_probe main.cpp)
target_link_libraries(stella_probe PRIVATE stella_core)
# A linker map file lets a crash offset printed by the probe be traced to a function.
target_link_options(stella_probe PRIVATE /MAP)

# stella_timing: per-scanline timing measurements (Stage C4, DECISIONS.md T-C4-1).
add_executable(stella_timing timing.cpp)
target_link_libraries(stella_timing PRIVATE stella_core)
target_link_options(stella_timing PRIVATE /MAP)

# stella_sound: sound register changes, audio and pictures (Stage C5, DECISIONS.md T-C5-1).
add_executable(stella_sound sound.cpp)
target_link_libraries(stella_sound PRIVATE stella_core)
target_link_options(stella_sound PRIVATE /MAP)

# stella_script: scripted inputs, per-frame fingerprints, switches and RAM (Stage C6, DECISIONS.md T-C6-1).
add_executable(stella_script script.cpp)
target_link_libraries(stella_script PRIVATE stella_core)
target_link_options(stella_script PRIVATE /MAP)

# stella_bot: search-based player that writes input scripts (Stage C7, DECISIONS.md T-C7-1).
add_executable(stella_bot bot.cpp)
target_link_libraries(stella_bot PRIVATE stella_core)
target_link_options(stella_bot PRIVATE /MAP)
