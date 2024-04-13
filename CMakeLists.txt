cmake_minimum_required(VERSION 3.0.0)
project(untitled VERSION 0.1.0)

set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON")

find_package(PkgConfig)

add_executable(untitled
  src/main.cc
  src/texture_atlas.cc
  src/state.cc
  src/util.cc
  src/renderer.cc
  src/face_collection.cc
  lib/glad/src/glad.c
)

# GLFW
pkg_check_modules(GLFW REQUIRED glfw3)
target_link_libraries(untitled PRIVATE ${GLFW_LIBRARIES})
target_include_directories(untitled PUBLIC ${GLFW_INCLUDE_DIRS})
target_compile_options(untitled PUBLIC ${GLFW_CFLAGS_OTHER})

# Harfbuzz
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
target_link_libraries(untitled PRIVATE ${HARFBUZZ_LIBRARIES})
target_include_directories(untitled PUBLIC ${HARFBUZZ_INCLUDE_DIRS})
target_compile_options(untitled PUBLIC ${HARFBUZZ_CFLAGS_OTHER})

# Freetype2
pkg_check_modules(FREETYPE REQUIRED freetype2)
target_link_libraries(untitled PRIVATE ${FREETYPE_LIBRARIES})
target_include_directories(untitled PUBLIC ${FREETYPE_INCLUDE_DIRS})
target_compile_options(untitled PUBLIC ${FREETYPE_CFLAGS_OTHER})

# libpng
pkg_check_modules(libpng REQUIRED libpng)
target_link_libraries(untitled PRIVATE ${LIBPNG_LIBRARIES})
target_include_directories(untitled PUBLIC ${LIBPNG_INCLUDE_DIRS})
target_compile_options(untitled PUBLIC ${LIBPNG_CFLAGS_OTHER})

# Glad
target_include_directories(untitled PUBLIC lib/glad/include)
set_source_files_properties(lib/glad/src/glad.c PROPERTIES COMPILE_FLAGS "-Wno-error -Wno-all -Wno-extra -Wno-pedantic")

# FontConfig
find_package(Fontconfig REQUIRED)
target_link_libraries(untitled PRIVATE ${Fontconfig_LIBRAIES})
target_include_directories(untitled PUBLIC ${Fontconfig_INCLUDE_DIRS})
target_compile_options(untitled PUBLIC ${Fontconfig_COMPILE_OPTIONS})

set_property(TARGET untitled PROPERTY CXX_STANDARD 11)
set_property(TARGET untitled PROPERTY CMAKE_CXX_STANDARD_REQUIRED True)

target_compile_options(untitled PUBLIC -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -g)
target_link_libraries(untitled PRIVATE glfw X11 dl freetype pthread harfbuzz fontconfig)


# Try to find clang-tidy
find_program(
  CLANG_TIDY_EXE
  NAMES "clang-tidy"
  DOC "Path to clang-tidy executable"
  )
if(NOT CLANG_TIDY_EXE)
  message(STATUS "clang-tidy not found.")
else()
  message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
  set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "-checks=-*,google-*")
endif()

set_target_properties(
  untitled PROPERTIES
  CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
)

# If enabled, generates a compile_commands.json, used by clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS)
