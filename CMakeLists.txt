cmake_minimum_required(VERSION 3.27)
project(CHIP_8__)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules)

set(SDL2_PATH "C:/Users/kobis/SDL2-2.28.5/x86_64-w64-mingw32")

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

add_executable(CHIP_8__ KOBZ_CHIP8PLUS.cpp)

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})