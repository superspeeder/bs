cmake_minimum_required(VERSION 3.26)
project(testpr)

find_package(Vulkan REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_executable(testpr main.cpp
        setup.cpp
        setup.hpp)
target_include_directories(testpr PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(testpr Vulkan::Vulkan)
