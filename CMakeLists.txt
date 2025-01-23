cmake_minimum_required(VERSION 3.5)
project(mygame)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fast SSE2 floating point operations
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast")

add_executable(mygame src/main.cpp)

target_link_libraries(mygame PRIVATE SDL3::SDL3)
