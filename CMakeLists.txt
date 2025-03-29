cmake_minimum_required(VERSION 3.19)
project(mygame)

find_package(fastgltf CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(VulkanUtilityLibraries CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Stb REQUIRED)

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

add_executable(vulkan_test src/vulkan_test.cpp 
                src/camera.cpp
                src/file.cpp 
                src/game.cpp
                src/math/transform.cpp
                src/memory.cpp
                src/renderer/descriptor.cpp
                src/renderer/frustum_culling.cpp
                src/renderer/image.cpp
                src/renderer/init.cpp 
                src/renderer/loader.cpp
                src/renderer/pipelines/builder.cpp
                src/renderer/pipelines/tile.cpp
                src/renderer/renderer.cpp 
                src/tile.cpp
                ${SHADERS})

add_dependencies(vulkan_test shaders)

target_link_libraries(vulkan_test PRIVATE 
                        fastgltf::fastgltf
                        imgui::imgui
                        SDL3::SDL3 
                        Vulkan::Vulkan 
                        # Vulkan::SafeStruct
                        # Vulkan::LayerSettings
                        # Vulkan::UtilityHeaders
                        # Vulkan::CompilerConfiguration
                        glm::glm-header-only 
                        GPUOpen::VulkanMemoryAllocator 
                        fmt::fmt
                        ${Stb_INCLUDE_DIR})

target_compile_definitions(vulkan_test
  PUBLIC
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_CTOR_INIT
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

