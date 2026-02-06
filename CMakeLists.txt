cmake_minimum_required(VERSION 3.20)

project(
    usylibpp 
    VERSION 0.1.2
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

set(USYLIBPP_DEPS_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps")

configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/usylibppconfig.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/usylibpp_config/usylibppconfig.hpp"
)

add_library(usylibpp_usylibpp INTERFACE)
add_library(usylibpp::usylibpp ALIAS usylibpp_usylibpp)

target_compile_features(usylibpp_usylibpp INTERFACE
    cxx_std_20
)

target_include_directories(usylibpp_usylibpp INTERFACE
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/usylibpp_config"
)

if (WIN32)
    target_compile_definitions(usylibpp_usylibpp INTERFACE
        "WIN32_LEAN_AND_MEAN"
        "_UNICODE"
        "UNICODE"
        "NOMINMAX"
    )

    set(USYLIBPP_ENABLE_WINDOWS
        ON
        CACHE BOOL
        "Enable Windows Functions.
        
This includes windows.h along with various other headers, but enables all of the windows wrappers"
    )

    message(STATUS "Windows support (USYLIBPP_ENABLE_WINDOWS): ${USYLIBPP_ENABLE_WINDOWS}")

    if (USYLIBPP_ENABLE_WINDOWS)
        target_compile_definitions(usylibpp_usylibpp INTERFACE "USYLIBPP_ENABLE_WINDOWS")

        set(USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE
            ON
            CACHE BOOL
            "Enable Windows Task Dialog, and win32 dark mode enabling support.

    When enabled, the library injects a Common Controls v6 manifest into the final executable,
    and links against Comctl32"
        )

        message(STATUS "Task dialog/dark mode support (USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE): ${USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE}")

        if(USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE)
            target_compile_definitions(usylibpp_usylibpp INTERFACE "USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE")
            target_link_libraries(usylibpp_usylibpp INTERFACE Comctl32)
        endif()

        set(USYLIBPP_ENABLE_WIL
            ON
            CACHE BOOL
            "Download wil(windows implementation libraries) to enable stuff like the windows::process namespace"
        )

        message(STATUS "WIL support (USYLIBPP_ENABLE_WIL): ${USYLIBPP_ENABLE_WIL}")

        if(USYLIBPP_ENABLE_WIL)
            target_compile_definitions(usylibpp_usylibpp INTERFACE "USYLIBPP_ENABLE_WIL")

            set(USYLIBPP_WIL_GIT_TAG
                "v1.0.260126.7"
                CACHE STRING
                "Git tag to use when checking out wil"
            )

            message(STATUS "WIL git tag (USYLIBPP_WIL_GIT_TAG): ${USYLIBPP_WIL_GIT_TAG}")

            include(FetchContent)

            cmake_policy(SET CMP0169 OLD)

            FetchContent_Declare(
                microsoft_wil
                GIT_REPOSITORY https://github.com/microsoft/wil.git
                GIT_TAG ${USYLIBPP_WIL_GIT_TAG}
                GIT_SHALLOW TRUE
            )

            FetchContent_GetProperties(microsoft_wil)
            if(NOT microsoft_wil_POPULATED)
                FetchContent_Populate(microsoft_wil)
            endif()

            add_library(microsoft_wil INTERFACE)

            target_include_directories(microsoft_wil INTERFACE
                ${microsoft_wil_SOURCE_DIR}/include
            )

            target_link_libraries(usylibpp_usylibpp INTERFACE microsoft_wil)
        endif()

        set(USYLIBPP_ENABLE_WINTOAST
            OFF
            CACHE BOOL
            "Download WinToast (https://github.com/mohabouje/WinToast) to include in your projects conveniently"
        )

        message(STATUS "WinToast (USYLIBPP_ENABLE_WINTOAST): ${USYLIBPP_ENABLE_WINTOAST}")

        if(USYLIBPP_ENABLE_WINTOAST)
            target_compile_definitions(usylibpp_usylibpp INTERFACE "USYLIBPP_ENABLE_WINTOAST")

            set(USYLIBPP_WINTOAST_GIT_TAG
                "v1.3.2"
                CACHE STRING
                "Git tag to use when checking out WinToast"
            )

            message(STATUS "WinToast git tag (USYLIBPP_WINTOAST_GIT_TAG): ${USYLIBPP_WINTOAST_GIT_TAG}")

            include(FetchContent)

            cmake_policy(SET CMP0169 OLD)

            FetchContent_Declare(
                mohabouje_wintoast
                GIT_REPOSITORY https://github.com/mohabouje/WinToast.git
                GIT_TAG ${USYLIBPP_WINTOAST_GIT_TAG}
                GIT_SHALLOW TRUE
            )

            FetchContent_GetProperties(mohabouje_wintoast)
            if(NOT mohabouje_wintoast_POPULATED)
                FetchContent_Populate(mohabouje_wintoast)
            endif()

            add_library(mohabouje_wintoast STATIC
                ${mohabouje_wintoast_SOURCE_DIR}/src/wintoastlib.cpp
            )

            target_compile_features(mohabouje_wintoast PRIVATE 
                cxx_std_20
            )

            target_include_directories(mohabouje_wintoast PUBLIC
                ${mohabouje_wintoast_SOURCE_DIR}/include
            )

            target_link_libraries(mohabouje_wintoast PUBLIC
                psapi
            )

            target_link_libraries(usylibpp_usylibpp INTERFACE mohabouje_wintoast)
        endif()
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
        "${PROJECT_SOURCE_DIR}/src/test.cpp"
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
            /Zc:preprocessor
            /await:strict

            $<$<CONFIG:Debug>:
                /RTC1
                /Zi
                /Od
            >

            $<$<CONFIG:Release>:
                /O2
                /Ob2
                /Oi
                /Oy
                /Gy
                /GL
            >
        )

        target_compile_definitions(usylibpp_test PRIVATE
            _WINDOWS
            UNICODE
            _UNICODE

            $<$<CONFIG:Debug>:
                _DEBUG
            >
            $<$<CONFIG:Release>:
                NDEBUG
            >
        )

        target_link_options(usylibpp_test PRIVATE
            # /SUBSYSTEM:WINDOWS

            $<$<CONFIG:Debug>:
                "/DEBUG"
            >
            $<$<CONFIG:Release>:
                "/OPT:ICF"
                "/OPT:REF"
                "/LTCG"
            >
        )
    endif()
endif()