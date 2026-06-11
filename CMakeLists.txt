cmake_minimum_required(VERSION 3.20)

project(
    usylibpp 
    VERSION 0.1.2
    LANGUAGES C CXX
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
set(USYLIBPP_3RD_PARTY_DIR "${PROJECT_SOURCE_DIR}/3rdparty")

configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/usylibppconfig.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/usylibpp_config/usylibppconfig.hpp"
)

add_library(usylibpp_usylibpp INTERFACE)
add_library(usylibpp::usylibpp ALIAS usylibpp_usylibpp)

target_compile_features(usylibpp_usylibpp INTERFACE
    cxx_std_20
)

set(USYLIBPP_ENABLE_PCH
    ON
    CACHE BOOL
    "Enable PCH for quicker compilation"
)

message(STATUS "Using PCH: (USYLIBPP_ENABLE_PCH): ${USYLIBPP_ENABLE_PCH}")

target_include_directories(usylibpp_usylibpp INTERFACE
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/usylibpp_config"
)

if(USYLIBPP_ENABLE_PCH)
    if (PROJECT_IS_TOP_LEVEL)
        target_precompile_headers(usylibpp_usylibpp INTERFACE 
            <pch_top_level.hpp>
        )
    else()
        target_precompile_headers(usylibpp_usylibpp INTERFACE 
            <usylibpp.hpp>
        )
    endif()
endif()

set(USYLIBPP_ENABLE_LTCG
    ON
    CACHE BOOL
    "Enable LTCG for any dependencies that need compilation"
)

message(STATUS "Using LTCG for compiled dependencies: (USYLIBPP_ENABLE_LTCG): ${USYLIBPP_ENABLE_LTCG}")

add_library(usylibpp_common INTERFACE)

if(USYLIBPP_ENABLE_LTCG)
    if(MSVC)
        target_compile_options(usylibpp_common INTERFACE /GL)
        target_link_options(usylibpp_common INTERFACE /LTCG)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(usylibpp_common INTERFACE -flto)
        target_link_options(usylibpp_common INTERFACE -flto)
    endif()
endif()

target_compile_definitions(usylibpp_common INTERFACE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_link_libraries(usylibpp_usylibpp INTERFACE usylibpp_common)

if (WIN32)
    set(USYLIBPP_ENABLE_WINDOWS
        ON
        CACHE BOOL
        "Enable Windows Functions.
        
This includes windows.h along with various other headers, but enables all of the windows wrappers"
    )

    message(STATUS "Windows support (USYLIBPP_ENABLE_WINDOWS): ${USYLIBPP_ENABLE_WINDOWS}")

    if (USYLIBPP_ENABLE_WINDOWS)
        target_compile_definitions(usylibpp_usylibpp INTERFACE "USYLIBPP_ENABLE_WINDOWS")

        set(USYLIBPP_ENABLE_WINDOWS_UNICODE
            ON
            CACHE BOOL
            "Enable unicode for windows stuff"
        )

        message(STATUS "Using unicode defines: (USYLIBPP_ENABLE_WINDOWS_UNICODE): ${USYLIBPP_ENABLE_WINDOWS_UNICODE}")

        set(USYLIBPP_ENABLE_BASIC_WINDOWS_DEFINES
            ON
            CACHE BOOL
            "Define lean and mean, unicode, and nominmax"
        )

        message(STATUS "Basic windows defines (USYLIBPP_ENABLE_BASIC_WINDOWS_DEFINES): ${USYLIBPP_ENABLE_BASIC_WINDOWS_DEFINES}")

        if(USYLIBPP_ENABLE_BASIC_WINDOWS_DEFINES)
            target_compile_definitions(usylibpp_usylibpp INTERFACE
                "WIN32_LEAN_AND_MEAN"
                "NOMINMAX"
            )
        endif()

        if(USYLIBPP_ENABLE_WINDOWS_UNICODE)
            target_compile_definitions(usylibpp_common INTERFACE 
                "_UNICODE"
                "UNICODE"
            )
        endif()

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
                "${mohabouje_wintoast_SOURCE_DIR}/src/wintoastlib.cpp"
            )

            target_compile_features(mohabouje_wintoast PRIVATE 
                cxx_std_20
            )

            if (MSVC)
                target_compile_options(mohabouje_wintoast PRIVATE
                    /EHsc
                    /Zc:preprocessor

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
                    >
                )
            elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
                target_compile_options(mohabouje_wintoast PRIVATE
                    $<$<CONFIG:Debug>:
                        -g
                        -O0
                    >

                    $<$<CONFIG:Release>:
                        -O3
                        -ffunction-sections
                        -fdata-sections
                        -fomit-frame-pointer
                    >
                )
            endif()

            target_link_libraries(mohabouje_wintoast PUBLIC usylibpp_common)

            target_include_directories(mohabouje_wintoast PUBLIC
                ${mohabouje_wintoast_SOURCE_DIR}/include
            )

            target_link_libraries(mohabouje_wintoast PUBLIC
                psapi
            )

            target_link_libraries(usylibpp_usylibpp INTERFACE mohabouje_wintoast)
        endif()

        set(USYLIBPP_ENABLE_WINDOWS_IMAGING
            OFF
            CACHE BOOL
            "Enable windows imaging components and wrappers"
        )

        message(STATUS "Windows imaging components (USYLIBPP_ENABLE_WINDOWS_IMAGING): ${USYLIBPP_ENABLE_WINDOWS_IMAGING}")

        if(USYLIBPP_ENABLE_WINDOWS_IMAGING)
            target_compile_definitions(usylibpp_usylibpp INTERFACE "USYLIBPP_ENABLE_WINDOWS_IMAGING")
        endif()

        set(USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING
            OFF
            CACHE BOOL
            "Enable use of the voidtools everything sdk"
        )

        message(STATUS "Voidtools Everything SDK Support (USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING): ${USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING}")

        if(USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING)
            target_compile_definitions(usylibpp_usylibpp INTERFACE "USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING")

            set(USYLIBPP_VOIDTOOLS_EVERYTHING_DIR "${USYLIBPP_3RD_PARTY_DIR}/voidtools-everything-sdk")

            add_library(voidtools_everything STATIC 
                "${USYLIBPP_VOIDTOOLS_EVERYTHING_DIR}/src/Everything.c"
            )

            target_compile_features(voidtools_everything PRIVATE 
                c_std_17
            )

            target_include_directories(voidtools_everything PUBLIC 
                "${USYLIBPP_VOIDTOOLS_EVERYTHING_DIR}/include"
            )

            target_compile_definitions(voidtools_everything PRIVATE
                $<$<CONFIG:Debug>:BZ_NO_STDIO>
            )

            if (MSVC)
                target_compile_options(voidtools_everything PRIVATE
                    /EHsc

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
                    >
                )
            elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
                target_compile_options(voidtools_everything PRIVATE
                    $<$<CONFIG:Debug>:
                        -g
                        -O0
                    >

                    $<$<CONFIG:Release>:
                        -O3
                        -ffunction-sections
                        -fdata-sections
                        -fomit-frame-pointer
                    >
                )
            endif()

            target_link_libraries(voidtools_everything PUBLIC usylibpp_common)
            target_link_libraries(usylibpp_usylibpp INTERFACE voidtools_everything)
        endif()
    endif()
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(USYLIBPP_ENABLE_LINUX
        ON
        CACHE BOOL
        "Enable Linux Functions.
        
Will likely be a very small subset of the windows wrappers that I've needed"
    )

    message(STATUS "Linux support (USYLIBPP_ENABLE_LINUX): ${USYLIBPP_ENABLE_LINUX}")

    if (USYLIBPP_ENABLE_LINUX)
        target_compile_definitions(usylibpp_usylibpp INTERFACE "USYLIBPP_ENABLE_LINUX")
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

    set(USYLIBPP_ENABLE_LTCG ON CACHE BOOL "" FORCE)
    set(USYLIBPP_ENABLE_WINDOWS_UNICODE ON CACHE BOOL "" FORCE)

    target_link_libraries(usylibpp_test PRIVATE 
        usylibpp::usylibpp
    )

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

        target_link_options(usylibpp_test PRIVATE
            # /SUBSYSTEM:WINDOWS

            $<$<CONFIG:Debug>:
                "/DEBUG"
            >
            $<$<CONFIG:Release>:
                "/OPT:ICF"
                "/OPT:REF"
            >
        )
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(usylibpp_test PRIVATE
            $<$<CONFIG:Debug>:
                -g
                -O0
            >

            $<$<CONFIG:Release>:
                -O3
                -ffunction-sections
                -fdata-sections
                -fomit-frame-pointer
            >
        )

        target_link_options(usylibpp_test PRIVATE 
            $<$<CONFIG:Release>:
                -Wl,--gc-sections
                -s
            >
        )
    endif()
endif()