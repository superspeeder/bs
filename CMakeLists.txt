cmake_minimum_required(VERSION 3.26)
project(testpr)

find_package(Vulkan REQUIRED)

add_executable(testpr main.cpp)
target_include_directories(testpr PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(testpr Vulkan::Vulkan)
