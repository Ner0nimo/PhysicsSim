cmake_minimum_required(VERSION 3.20)
project(PhysicsSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_GFX REQUIRED IMPORTED_TARGET SDL2_gfx)

add_executable(physics_sim
  src/main.cpp
  src/app.cpp
  src/particles.cpp
  src/simulation.cpp
  src/spatial_grid.cpp
  src/renderer.cpp
)

target_link_libraries(physics_sim PRIVATE
  PkgConfig::SDL2
  PkgConfig::SDL2_GFX
  Threads::Threads
)

target_compile_options(physics_sim PRIVATE -Wall -Wextra)
