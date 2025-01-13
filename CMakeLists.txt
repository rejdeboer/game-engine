cmake_minimum_required(VERSION 3.5)
project(mygame)

find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Create your game executable target as usual
add_executable(mygame WIN32 src/main.cpp)

# SDL2::SDL2main may or may not be available. It is e.g. required by Windows GUI applications
if(TARGET SDL2::SDL2main)
    # It has an implicit dependency on SDL2 functions, so it MUST be added before SDL2::SDL2 (or SDL2::SDL2-static)
    target_link_libraries(mygame PRIVATE SDL2::SDL2main)
endif()

# Link to the actual SDL2 library. SDL2::SDL2 is the shared SDL library, SDL2::SDL2-static is the static SDL libarary.
target_link_libraries(mygame PRIVATE SDL2::SDL2 SDL2_mixer::SDL2_mixer)
