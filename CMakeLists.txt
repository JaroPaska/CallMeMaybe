cmake_minimum_required(VERSION 3.20)

project(CallMeMaybe VERSION 1.0.0 LANGUAGES CXX)

add_library(cmm INTERFACE)
add_library(CallMeMaybe::cmm ALIAS cmm)

target_include_directories(cmm INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_options(cmm INTERFACE
    -std=c++26
    -freflection
    -fno-exceptions
    -fno-rtti
)

option(CMM_BUILD_EXAMPLES "Build CallMeMaybe example and test executables" ON)

if(CMM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()