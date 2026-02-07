# usylibpp
- A simple header-only library with a bunch of random code I don't want to rewrite
- Likely super unstable API
- Likely bugs in the methods
- Likely very specific use-cases to methods


## Basically: Don't use this library it's just for my convenience

# Usage
Cmake:
```cmake
include(FetchContent)

FetchContent_Declare(
    usylibpp
    GIT_REPOSITORY https://github.com/usyless/usylibpp.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(usylibpp)

target_link_libraries(${PROJECT_NAME} PRIVATE usylibpp::usylibpp)
```

## Cmake variables


### WIN32 Specific
- `USYLIBPP_ENABLE_WINDOWS` (BOOL, Default: ON) - Toggles all of the following

- `USYLIBPP_ENABLE_BASIC_WINDOWS_DEFINES` (BOOL, Default: ON) - Adds basic windows defines
- `USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE` (BOOL, Default: ON) - Adds a manifest entry and support for the task dialog + win32 darkmode stuff

- `USYLIBPP_ENABLE_WIL` (BOOL, Default: ON) - Includes WIL in the project and enables all functions which depend on it
- `USYLIBPP_WIL_GIT_TAG` (STRING, Default: "v1.0.260126.7")

- `USYLIBPP_ENABLE_WINTOAST` (BOOL, Default: OFF) - Includes WinToast in the project and enables the helpers for it
- `USYLIBPP_WINTOAST_GIT_TAG` (STRING, Default: "v1.3.2")