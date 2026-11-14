cmake_minimum_required(VERSION 3.22.1)
project(librecuts_vulkan LANGUAGES CXX)

add_library(librecuts_vulkan SHARED vulkan_runtime.cpp)
target_compile_features(librecuts_vulkan PRIVATE cxx_std_17)
target_compile_options(librecuts_vulkan PRIVATE -Wall -Wextra -Werror=return-type)

find_library(android-lib android REQUIRED)
find_library(log-lib log REQUIRED)
find_library(vulkan-lib vulkan)
if(NOT vulkan-lib)
    message(FATAL_ERROR "Android NDK Vulkan loader library was not found")
endif()
target_link_libraries(librecuts_vulkan PRIVATE ${android-lib} ${log-lib} ${vulkan-lib})
