cmake_minimum_required(VERSION 3.20)
project(lockstep CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "/std:c++23preview")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(lockstep lockstep_main.cpp machine.cpp natives.cpp natives_frame.cpp natives_objects.cpp ../game/logic/players.cpp ../game/data/tables.cpp ../game/logic/maze.cpp ../game/logic/objects.cpp ../game/logic/animation.cpp ../game/logic/scoring.cpp ../game/logic/ghosts.cpp ../game/logic/flow.cpp ../game/logic/collisions.cpp ../game/logic/pacman.cpp ../game/logic/bottom.cpp ../game/logic/top.cpp ../game/logic/frame_start.cpp ../game/picture/status_bar.cpp ../game/sound/sound_logic.cpp)
add_executable(lockstep_native native_main.cpp ../game/logic/players.cpp ../game/data/tables.cpp ../game/logic/maze.cpp ../game/logic/objects.cpp ../game/logic/animation.cpp ../game/logic/scoring.cpp ../game/logic/ghosts.cpp ../game/logic/flow.cpp ../game/logic/collisions.cpp ../game/logic/pacman.cpp ../game/logic/bottom.cpp ../game/logic/top.cpp ../game/logic/frame_start.cpp ../game/picture/status_bar.cpp ../game/sound/sound_logic.cpp ../game/game.cpp ../game/objects_view.cpp ../game/lookahead.cpp ../game/kernel.cpp ../game/picture/kernel_positions.cpp ../game/picture/kernel_maze.cpp ../game/picture/kernel_score.cpp ../game/tia_video.cpp ../game/renderer.cpp ../game/tia_audio.cpp)
# Stage G5a: the same program with a counter on every cartridge read (a test tool; the game's own builds never define the macro).
get_target_property(NATIVE_SRCS lockstep_native SOURCES)
add_executable(lockstep_native_reads ${NATIVE_SRCS})
target_compile_definitions(lockstep_native_reads PRIVATE PACMAN_COUNT_CARTRIDGE_READS=1)
add_executable(test_rom_check ../../tests/game/test_rom_check.cpp ../game/data/rom_check.cpp)
add_executable(test_tia_video ../../tests/game/test_tia_video.cpp ../game/tia_video.cpp)
add_executable(test_kernel ../../tests/game/test_kernel.cpp ../game/kernel.cpp ../game/tia_video.cpp ../game/picture/kernel_positions.cpp ../game/picture/kernel_maze.cpp ../game/picture/kernel_score.cpp ../game/logic/players.cpp ../game/data/tables.cpp ../game/logic/maze.cpp ../game/logic/objects.cpp ../game/logic/animation.cpp ../game/logic/scoring.cpp ../game/logic/ghosts.cpp ../game/logic/flow.cpp ../game/logic/collisions.cpp ../game/logic/pacman.cpp ../game/logic/bottom.cpp ../game/logic/top.cpp ../game/logic/frame_start.cpp ../game/picture/status_bar.cpp ../game/sound/sound_logic.cpp)
add_executable(test_tia_audio ../../tests/game/test_tia_audio.cpp ../game/tia_audio.cpp)

add_executable(test_tables ../../tests/game/test_tables.cpp ../game/data/tables.cpp)
# Stage G5c: the whole game in a program that contains no ROM code at all (no rom_check, no reading of any cartridge file): it must reproduce the recorded pictures and sounds.
get_target_property(NOROM_SRCS lockstep_native SOURCES)
list(REMOVE_ITEM NOROM_SRCS native_main.cpp)
add_executable(test_no_rom ../../tests/game/test_no_rom.cpp ${NOROM_SRCS})
# Stage H2: unit tests of the game modules with made-up states (no ROM, no recording).
add_executable(test_game_modules ../../tests/game/test_game_modules.cpp ${NOROM_SRCS})
# Stage I9.1: the picture in two layers (with and without Pac-Man and the ghosts).
add_executable(test_layers ../../tests/game/test_layers.cpp ${NOROM_SRCS})
# Stage I9.2: the description of the moving objects, checked against the game's own picture.
add_executable(test_objects_view ../../tests/game/test_objects_view.cpp ${NOROM_SRCS})
# Stage I9.2: the look-ahead (the next frames with the controls held).
add_executable(test_lookahead ../../tests/game/test_lookahead.cpp ${NOROM_SRCS})
