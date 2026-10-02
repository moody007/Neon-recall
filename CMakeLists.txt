# Build graph: neon_core is renderer-independent, while raylib is only needed for the game UI.
cmake_minimum_required(VERSION 3.20)
project(NeonRecall VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(NEON_RECALL_BUILD_GAME "Build the raylib game" ON)
option(NEON_RECALL_BUILD_TESTS "Build headless tests" ON)

# Reusable rules library linked by both the executable and headless tests.
add_library(neon_core src/GameModel.cpp)
target_include_directories(neon_core PUBLIC include)
if(MSVC)
    target_compile_options(neon_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(neon_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NEON_RECALL_BUILD_GAME)
    # Fetch a pinned raylib release so a fresh developer setup needs no global raylib install.
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(raylib)
    add_executable(neon_recall src/main.cpp)
    target_link_libraries(neon_recall PRIVATE neon_core raylib)
endif()

if(NEON_RECALL_BUILD_TESTS)
    # Tests deliberately link only neon_core and therefore require no graphics window.
    enable_testing()
    add_executable(neon_recall_tests tests/tests.cpp)
    target_link_libraries(neon_recall_tests PRIVATE neon_core)
    add_test(NAME neon_recall_logic COMMAND neon_recall_tests)
endif()
