# Third-party libraries, pinned in docs/DEPENDENCIES.md:
#   SDL3 3.4.18   (zlib)  -> target SDL3::SDL3
#   Stella 7.0    (GPL-2) -> target stella_libretro (Stella's own libretro core)
# Can be built on its own (stage B7 check) or added by the main project with add_subdirectory().
cmake_minimum_required(VERSION 3.30)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(pacman2600_third_party LANGUAGES C CXX)
endif()

# --- SDL3: built by its own CMake files; no tests, examples or install rules.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL3 EXCLUDE_FROM_ALL)

# --- Stella libretro core: the source list is read from Stella's own Makefile.common,
# and the settings copy its Makefile for MSVC (platform windows_msvc2017_desktop_x64).
# Stella's code is not changed. See DECISIONS.md T-B7-3.
set(STELLA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/stella/src)
file(READ ${STELLA_SRC}/os/libretro/Makefile.common STELLA_MAKEFILE)
string(REGEX MATCHALL "\\$\\(CORE_DIR\\)/[^ \t\r\n\\\\]+\\.cxx" STELLA_SOURCES "${STELLA_MAKEFILE}")
list(TRANSFORM STELLA_SOURCES REPLACE "^\\$\\(CORE_DIR\\)" "${STELLA_SRC}")
list(LENGTH STELLA_SOURCES STELLA_SOURCE_COUNT)
message(STATUS "Stella libretro core: ${STELLA_SOURCE_COUNT} source files from Makefile.common")

# Build-time fix, the vendored file stays untouched (DECISIONS.md T-B7-4): libretro.cxx maps
# snprintf to _snprintf for every MSVC, which breaks std::snprintf in the bundled json.hpp on
# current compilers. Limit it to pre-2015 MSVC (as zlib does) in a copy under the build folder.
file(READ ${STELLA_SRC}/os/libretro/libretro.cxx STELLA_LIBRETRO_CXX)
set(STELLA_SNPRINTF_OLD "#ifdef _MSC_VER\n#define snprintf _snprintf\n#endif")
string(FIND "${STELLA_LIBRETRO_CXX}" "${STELLA_SNPRINTF_OLD}" STELLA_SNPRINTF_POS)
if(STELLA_SNPRINTF_POS EQUAL -1)
  message(FATAL_ERROR "Stella libretro.cxx no longer contains the expected snprintf mapping; review T-B7-4.")
endif()
string(REPLACE "${STELLA_SNPRINTF_OLD}" "#if defined(_MSC_VER) && _MSC_VER < 1900\n#define snprintf _snprintf\n#endif"
  STELLA_LIBRETRO_CXX "${STELLA_LIBRETRO_CXX}")
set(STELLA_LIBRETRO_PATCHED ${CMAKE_CURRENT_BINARY_DIR}/stella_patched/libretro.cxx)
file(WRITE ${STELLA_LIBRETRO_PATCHED}.tmp "${STELLA_LIBRETRO_CXX}")
file(COPY_FILE ${STELLA_LIBRETRO_PATCHED}.tmp ${STELLA_LIBRETRO_PATCHED} ONLY_IF_DIFFERENT)
list(FIND STELLA_SOURCES ${STELLA_SRC}/os/libretro/libretro.cxx STELLA_LIBRETRO_INDEX)
if(STELLA_LIBRETRO_INDEX EQUAL -1)
  message(FATAL_ERROR "libretro.cxx not found in Stella's source list.")
endif()
list(REMOVE_AT STELLA_SOURCES ${STELLA_LIBRETRO_INDEX})
list(INSERT STELLA_SOURCES ${STELLA_LIBRETRO_INDEX} ${STELLA_LIBRETRO_PATCHED})

add_library(stella_libretro SHARED ${STELLA_SOURCES})
set_target_properties(stella_libretro PROPERTIES
  CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
  EXCLUDE_FROM_ALL ON)
target_include_directories(stella_libretro PRIVATE
  ${STELLA_SRC}/os/libretro ${STELLA_SRC} ${STELLA_SRC}/emucore ${STELLA_SRC}/emucore/elf
  ${STELLA_SRC}/emucore/tia ${STELLA_SRC}/common ${STELLA_SRC}/common/audio
  ${STELLA_SRC}/common/tv_filters ${STELLA_SRC}/common/repository/sqlite
  ${STELLA_SRC}/lib/json ${STELLA_SRC}/lib/httplib ${STELLA_SRC}/lib/sqlite)
target_compile_definitions(stella_libretro PRIVATE
  __LIB_RETRO__ SOUND_SUPPORT HAVE_STDINT_H
  __WIN32__ WINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP)
