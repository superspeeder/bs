cmake_minimum_required(VERSION 3.26)
project(testpr)

find_package(Vulkan COMPONENTS shaderc_combined)

if (NOT ${Vulkan_FOUND})
    message(FATAL_ERROR "-- Vulkan not found")
endif()

if (Vulkan_shaderc_combined_FOUND)
    set(SHADERC_LIB Vulkan::shaderc_combined)
else()
    set(SHADERC_LIB shaderc_combined)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_executable(testpr main.cpp
        setup.cpp
        setup.hpp)
target_include_directories(testpr PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(testpr Vulkan::Vulkan ${SHADERC_LIB})

option(RENDERDOC "Enable renderdoc support" OFF)
if (RENDERDOC)
    target_compile_definitions(testpr PRIVATE -DRENDERDOC)
endif()