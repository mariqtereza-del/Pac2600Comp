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
add_library(controls STATIC controls.cpp pad_layout.cpp)
target_compile_options(controls PRIVATE /W4 /permissive-)
target_link_libraries(controls PUBLIC SDL3::SDL3)

# The Escape menu (Stage I5): the choices, their explanations and the presses that start a game; no window code.
add_library(menu STATIC menu.cpp menu_text.cpp menu_session.cpp background.cpp pad_feedback.cpp achievements.cpp)
target_compile_options(menu PRIVATE /W4 /permissive-)
target_link_libraries(menu PUBLIC controls framerate crt settings)

# The frame rate of the picture (Stage I9.4): the choices and the arithmetic, no window code.
add_library(framerate STATIC framerate.cpp)
target_compile_options(framerate PRIVATE /W4 /permissive-)

# HDR output (Stage I10): what the screen says and the decisions and arithmetic that follow, no window code.
add_library(hdr STATIC hdr.cpp)
target_compile_options(hdr PRIVATE /W4 /permissive-)

# The CRT filter choices (Stage I11): the names and the order, no window code.
add_library(crt STATIC crt.cpp)
target_compile_options(crt PRIVATE /W4 /permissive-)

# Input recordings (Stage D13): the file format, no window code.
add_library(inputlog STATIC inputlog.cpp)
target_compile_options(inputlog PRIVATE /W4 /permissive-)

# Frame traces (Stage E1): the file format and the fingerprint, no emulator code.
add_library(trace STATIC trace.cpp)
target_compile_options(trace PRIVATE /W4 /permissive-)

# Sound output through SDL (Stage D8).
add_library(audio STATIC audio.cpp)
target_compile_options(audio PRIVATE /W4 /permissive-)
target_link_libraries(audio PUBLIC SDL3::SDL3)

# The only host code that includes Stella headers (ARCHITECTURE.md, T-D1-1).
add_library(emulator STATIC emulator.cpp)
target_link_libraries(emulator PUBLIC stella_core)

# The game itself (Stage F26): the rebuilt logic, the drawing code and the chip models of src/game, with no Stella and no 6502.
add_library(game_native STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/players.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/data/tables.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/maze.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/objects.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/animation.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/scoring.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/ghosts.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/flow.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/collisions.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/pacman.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/bottom.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/top.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/logic/frame_start.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/picture/status_bar.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/sound/sound_logic.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/game.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/objects_view.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/lookahead.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/smooth.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/kernel.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/picture/kernel_positions.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/picture/kernel_maze.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/picture/kernel_score.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../game/tia_video.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/tia_audio.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/renderer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../game/data/rom_check.cpp)
target_compile_options(game_native PRIVATE /W4 /permissive-)
add_library(emulator_native STATIC native_emulator.cpp)
target_compile_options(emulator_native PRIVATE /W4 /permissive-)
target_link_libraries(emulator_native PUBLIC game_native)

# The program's parts (Stage G4.7, split out of main.cpp): arguments, ROM choice, screen, pads, pacing, window handling, headless runs.
# They need an Emulator but not which one: the executables below link the native one or the Stella one.
add_library(host_app STATIC options.cpp rom_choice.cpp screen.cpp symbols.cpp pads.cpp pad_driver.cpp achievement_driver.cpp pacer.cpp window_tracker.cpp window_checks.cpp play_window.cpp headless.cpp menu_view.cpp menu_check.cpp backdrop.cpp backdrop_check.cpp smooth_check.cpp key_pusher.cpp hdr_check.cpp render_setup.cpp gpu_pass.cpp filter_check.cpp crt_filter.cpp crt_lottes.cpp crt_geom.cpp gpu_graph.cpp gdv_chain.cpp gdv_generated.cpp gdv_luts.cpp)
target_compile_options(host_app PRIVATE /W4 /permissive-)
target_link_libraries(host_app PUBLIC SDL3::SDL3 d3d_pin rom_finder layout windowstate audio controls menu inputlog trace framerate hdr crt d3d11 d3dcompiler dxguid)

# WIN32 = a normal windowed program (no console window when double-clicked).
# pacman2600 is the game: it plays with the rebuilt game (emulator_native) and contains no Stella.
add_executable(pacman2600 WIN32 main.cpp)
target_compile_options(pacman2600 PRIVATE /W4 /permissive-)
target_link_libraries(pacman2600 PRIVATE host_app emulator_native)

# pacman2600_ref is the same program with Stella inside: the REFERENCE the tests compare with (golden traces are made with it). It is not part of a release.
add_executable(pacman2600_ref WIN32 main.cpp)
target_compile_options(pacman2600_ref PRIVATE /W4 /permissive-)
target_link_libraries(pacman2600_ref PRIVATE host_app emulator)

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

# Tests for the frame traces (Stage E1): build/host/test_trace.exe
add_executable(test_trace ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_trace.cpp)
target_compile_options(test_trace PRIVATE /W4 /permissive-)
target_link_libraries(test_trace PRIVATE trace)

# Tests for the frame rate (Stage I9.4): build/host/test_framerate.exe
add_executable(test_framerate ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_framerate.cpp)
target_compile_options(test_framerate PRIVATE /W4 /permissive-)
target_link_libraries(test_framerate PRIVATE framerate)

# Tests for what goes out to a pad (Stage I15): build/host/test_feedback.exe
add_executable(test_feedback ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_feedback.cpp)
target_compile_options(test_feedback PRIVATE /W4 /permissive-)
target_link_libraries(test_feedback PRIVATE menu trace)

# Tests for the achievements (Stage I20): build/host/test_achievements.exe
add_executable(test_achievements ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_achievements.cpp)
target_compile_options(test_achievements PRIVATE /W4 /permissive-)
target_link_libraries(test_achievements PRIVATE menu trace)

# Keeps Direct3D loaded (Stage I16) and its test: build/host/test_d3d_pin.exe
add_library(d3d_pin STATIC d3d_pin.cpp)
target_compile_options(d3d_pin PRIVATE /W4 /permissive-)
add_executable(test_d3d_pin ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_d3d_pin.cpp)
target_compile_options(test_d3d_pin PRIVATE /W4 /permissive-)
target_include_directories(test_d3d_pin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_d3d_pin PRIVATE d3d_pin)

# Tests for the volume (Stage I12): build/host/test_volume.exe
add_executable(test_volume ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_volume.cpp)
target_compile_options(test_volume PRIVATE /W4 /permissive-)
target_include_directories(test_volume PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_volume PRIVATE audio SDL3::SDL3)

# Tests for the CRT filter choices (Stage I11.2): build/host/test_crt.exe
add_executable(test_crt ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_crt.cpp)
target_compile_options(test_crt PRIVATE /W4 /permissive-)
target_link_libraries(test_crt PRIVATE crt)

# Test of the command line (after Stage I11.2: an option the program does not know is not a ROM file): build/host/test_options.exe
add_executable(test_options ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_options.cpp options.cpp)
target_compile_options(test_options PRIVATE /W4 /permissive-)
target_link_libraries(test_options PRIVATE SDL3::SDL3 layout crt)

# Tests for the HDR decisions (Stage I10.1): build/host/test_hdr.exe
add_executable(test_hdr ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_hdr.cpp)
target_compile_options(test_hdr PRIVATE /W4 /permissive-)
target_link_libraries(test_hdr PRIVATE hdr)

# Tests for the Escape menu (Stage I5): build/host/test_menu.exe
add_executable(test_menu ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_menu.cpp)
target_compile_options(test_menu PRIVATE /W4 /permissive-)
target_link_libraries(test_menu PRIVATE menu)

# Tests for what fills the space around the picture (Stage I7): build/host/test_background.exe
add_executable(test_background ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/host/test_background.cpp)
target_compile_options(test_background PRIVATE /W4 /permissive-)
target_link_libraries(test_background PRIVATE menu)

# Put SDL3.dll next to the program so a double-click finds it.
add_custom_command(TARGET pacman2600 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:pacman2600>)
