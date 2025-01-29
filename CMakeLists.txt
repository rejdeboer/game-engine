cmake_minimum_required(VERSION 3.5)
project(mygame)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")

add_executable(mygame src/main.cpp src/tile.cpp src/memory.cpp)
add_executable(vulkan_test src/vulkan_test.cpp src/renderer.cpp)
add_executable(test src/test.cpp)

target_link_libraries(mygame PRIVATE SDL3::SDL3 EnTT::EnTT SDL3_image::SDL3_image)
target_link_libraries(vulkan_test PRIVATE SDL3::SDL3 Vulkan::Vulkan)
target_link_libraries(test PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)
