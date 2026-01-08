cmake_minimum_required(VERSION 3.40)

project(
    usylibpp 
    VERSION 0.1
    LANGUAGES CXX
)

string(
    COMPARE EQUAL
    "${CMAKE_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}"
    PROJECT_IS_TOP_LEVEL
)

if (PROJECT_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

add_library(usylibpp INTERFACE)

target_compile_features(usylibpp INTERFACE
    cxx_std_20
)

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



if (PROJECT_IS_TOP_LEVEL)
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

    if (MSVC)
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
    endif()
endif()