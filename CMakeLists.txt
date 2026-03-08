cmake_minimum_required(VERSION 3.10)

# Set the project name
project(midcat)

# Specify the C++ standard (e.g., C++11, C++14, C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(vendor/rtmidi)

# Add the executable
add_executable(midcat src/main.cpp)

target_link_libraries(midcat PRIVATE rtmidi)

