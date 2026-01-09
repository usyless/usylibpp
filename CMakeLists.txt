cmake_minimum_required(VERSION 3.40)

project(
    usylibpp 
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)
set(CMAKE_SKIP_INSTALL_RULES ON)

string(
    COMPARE EQUAL
    "${CMAKE_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}"
    PROJECT_IS_TOP_LEVEL
)

if (PROJECT_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/usylibppconfig.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/usylibppconfig.hpp
)

add_library(usylibpp_usylibpp INTERFACE)
add_library(usylibpp::usylibpp ALIAS usylibpp_usylibpp)

target_compile_features(usylibpp_usylibpp INTERFACE
    cxx_std_20
)

target_include_directories(usylibpp_usylibpp INTERFACE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

if (WIN32)
    target_compile_definitions(usylibpp_usylibpp INTERFACE
        "WIN32_LEAN_AND_MEAN"
        "_UNICODE"
        "UNICODE"
        "NOMINMAX"
    )

    set(USYLIBPP_ENABLE_TASK_DIALOG
        ON
        CACHE BOOL
        "Enable Windows Task Dialog support.

When enabled, the library injects a Common Controls v6 manifest into the final executable,
and links against Comctl32"
    )

    if(USYLIBPP_ENABLE_TASK_DIALOG)
        target_compile_definitions(usylibpp_usylibpp INTERFACE 
            "USYLIBPP_ENABLE_TASK_DIALOG"
        )
        target_link_libraries(usylibpp_usylibpp INTERFACE 
            Comctl32
        )
    endif()
endif()


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
        usylibpp::usylibpp
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_property(TARGET usylibpp_test PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
    endif()

    if (MSVC)
        target_compile_options(usylibpp_test PRIVATE
            /EHsc
            /W4
            /GR
            /Zc:preprocessor
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