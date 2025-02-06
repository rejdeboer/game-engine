cmake_minimum_required(VERSION 3.19)
project(mygame)

find_package(fmt CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
file(GLOB SHADERS ${SHADER_DIR}/*.vert ${SHADER_DIR}/*.frag)

foreach(SHADER IN LISTS SHADERS)
    get_filename_component(FILENAME ${SHADER} NAME)
    add_custom_command(OUTPUT ${SHADER_DIR}/spv/${FILENAME}.spv
        COMMAND glslangValidator ${SHADER} -V -o ${SHADER_DIR}/spv/${FILENAME}.spv
        DEPENDS ${SHADER}
        COMMENT "Compiling ${FILENAME}")
list(APPEND SPV_SHADERS ${SHADER_DIR}/spv/${FILENAME}.spv)
endForeach()

add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})

add_executable(mygame src/main.cpp src/tile.cpp src/memory.cpp)
add_executable(vulkan_test src/vulkan_test.cpp 
                src/file.cpp 
                src/renderer/descriptor.cpp
                src/renderer/image.cpp
                src/renderer/init.cpp 
                src/renderer/renderer.cpp 
                ${SHADERS})
add_executable(test src/test.cpp)

add_dependencies(vulkan_test shaders)

target_link_libraries(mygame PRIVATE SDL3::SDL3 EnTT::EnTT SDL3_image::SDL3_image)
target_link_libraries(vulkan_test PRIVATE 
                        SDL3::SDL3 
                        Vulkan::Vulkan 
                        glm::glm-header-only 
                        GPUOpen::VulkanMemoryAllocator 
                        fmt::fmt)
target_link_libraries(test PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)

