cmake_minimum_required(VERSION 3.40)

project(usylibpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
# set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(usylibpp INTERFACE)

target_include_directories(usylibpp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(usylibpp INTERFACE
    "WIN32_LEAN_AND_MEAN"
    "_UNICODE"
    "UNICODE"
    "NOSERVICE"
    "NOMCX"
    "NOIME"
    "NOMINMAX"
)




set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/builddebug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/build")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/builddebug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/build")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/builddebug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/build")


add_executable(usylibpp_test 
    src/test.cpp
)

target_link_libraries(usylibpp_test PRIVATE 
    usylibpp
)

target_compile_options(usylibpp_test PRIVATE
    /EHsc
    /W4
    /GR
    /Zc:preprocessor
    /GL
    /await:strict

    # /Zi
    # /DEBUG
    $<$<CONFIG:Debug>:/RTC1>
    $<$<CONFIG:Debug>:/Zi>
    $<$<CONFIG:Debug>:/Od>

    $<$<CONFIG:Release>:/O2>
    $<$<CONFIG:Release>:/Ob2>
    $<$<CONFIG:Release>:/Oi>
    $<$<CONFIG:Release>:/Oy>
    $<$<CONFIG:Release>:/Gy>
)