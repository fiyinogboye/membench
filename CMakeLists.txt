cmake_minimum_required(VERSION 3.16)
project(membench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(MB_NATIVE "Compile with -march=native" ON)

find_package(Threads REQUIRED)

add_executable(membench src/main.cpp)
target_link_libraries(membench PRIVATE Threads::Threads)

if(MSVC)
  target_compile_options(membench PRIVATE /O2 /W4)
else()
  target_compile_options(membench PRIVATE -O3 -Wall -Wextra)
  if(MB_NATIVE AND NOT APPLE)
    target_compile_options(membench PRIVATE -march=native)
  endif()
endif()
