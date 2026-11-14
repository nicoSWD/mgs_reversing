cmake_minimum_required(VERSION 3.24)
project(PsxRecomp C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3 URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.2.0.tar.gz)
FetchContent_MakeAvailable(SDL3)

set(GEN_DIR ${CMAKE_SOURCE_DIR}/generated)
file(GLOB GENERATED_SOURCES CONFIGURE_DEPENDS ${GEN_DIR}/main_*.c)
if(NOT GENERATED_SOURCES)
    message(FATAL_ERROR "No generated code in ${GEN_DIR}. Run tools/psxrecomp.py first (see README).")
endif()

# the recompiled game: plain C, huge goto-heavy functions
add_library(game_code STATIC ${GENERATED_SOURCES})
target_include_directories(game_code PUBLIC runtime/include ${GEN_DIR})
target_compile_options(game_code PRIVATE -w -fno-strict-aliasing -O1)
option(PSX_TRACE "record the last functions entered (debugging)" OFF)
if(PSX_TRACE)
    target_compile_definitions(game_code PUBLIC PSX_TRACE_CALLS)
endif()

add_executable(psxrecomp-mgs
    runtime/src/main.cpp runtime/src/core.cpp runtime/src/hw.cpp runtime/src/bios.cpp runtime/src/gte.cpp
    runtime/src/gpu.cpp runtime/src/cdrom.cpp runtime/src/sio.cpp runtime/src/spu.cpp runtime/src/mdec.cpp
    runtime/src/host.cpp games/mgs/game.cpp)
target_include_directories(psxrecomp-mgs PRIVATE runtime/include runtime/src)
target_compile_options(psxrecomp-mgs PRIVATE -O2 -Wall -Wno-unused-function)
target_link_libraries(psxrecomp-mgs PRIVATE game_code SDL3::SDL3)
set_target_properties(psxrecomp-mgs PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
