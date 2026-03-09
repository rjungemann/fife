cmake_minimum_required(VERSION 3.10)

# Set the project name
project(fife)

# Specify the C++ standard (e.g., C++11, C++14, C++17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(vendor/rtmidi)

# Add the executable
add_executable(fife src/main.cpp)
target_compile_options(fife PUBLIC -g -Wno-narrowing -Wno-format)

target_sources(fife PRIVATE vendor/tinyosc/tinyosc.c)

target_include_directories(fife PRIVATE vendor/tinyosc)

target_link_libraries(fife PRIVATE rtmidi)
