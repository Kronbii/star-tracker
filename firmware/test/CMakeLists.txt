cmake_minimum_required(VERSION 3.16)
project(star_tracker_core_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../star_tracker/src/core)
file(GLOB CORE_SOURCES ${CORE_DIR}/*.cpp)

add_library(st_core STATIC ${CORE_SOURCES})
target_include_directories(st_core PUBLIC ${CORE_DIR})
target_compile_options(st_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                                       -Wno-sign-conversion -Werror)

option(ST_SANITIZE "Build with address/undefined sanitizers" ON)
if(ST_SANITIZE)
  set(SAN -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_compile_options(st_core PUBLIC ${SAN})
  target_link_options(st_core PUBLIC ${SAN})
endif()

find_package(Threads REQUIRED)

add_executable(core_tests
  test_main.cpp
  test_geometry.cpp
  test_trapezoid.cpp
  test_motion.cpp
  test_intervalometer.cpp
  test_command.cpp
  test_sky.cpp
  test_queue.cpp)
target_link_libraries(core_tests PRIVATE st_core Threads::Threads)
target_compile_options(core_tests PRIVATE -Wall -Wextra -Werror)

add_executable(track_sim track_sim.cpp)
target_link_libraries(track_sim PRIVATE st_core)

enable_testing()
add_test(NAME core_tests COMMAND core_tests)
