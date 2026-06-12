# usylibpp
- A simple header-only library with a bunch of random code I don't want to rewrite
- Likely super unstable API
- Likely bugs in the methods
- Likely very specific use-cases to methods


## Basically: Don't use this library it's just for my convenience
- But it has some cool stuff like the windows imaging component wrapper, wintoast wrapper, and the voidtools everything wrapper!
- Probably some more but this readme isn't being kept up to date...

# Installation/Usage


### Cmake:
```cmake
include(FetchContent)

# To enable/disable LTCG: (on by default)
# set(USYLIBPP_ENABLE_LTCG ON CACHE BOOL "" FORCE)
# set(USYLIBPP_ENABLE_LTCG OFF CACHE BOOL "" FORCE)

# To enable/disable windows unicode: (on by default)
# set(USYLIBPP_ENABLE_WINDOWS_UNICODE ON CACHE BOOL "" FORCE)
# set(USYLIBPP_ENABLE_WINDOWS_UNICODE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    usylibpp
    GIT_REPOSITORY https://github.com/usyless/usylibpp.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(usylibpp)

target_link_libraries(${PROJECT_NAME} PRIVATE usylibpp::usylibpp)
```

### Everything else:
Look at the cmake file and come up with whatever is needed... (it is header only afterall)

## Cmake variables

These are printed when configuring, which will be more up to date probably

- `USYLIBPP_ENABLE_PCH` (BOOL, Default: OFF) - Toggles PCH, enable to speed up repeated compilation times!

### WIN32 Specific
- `USYLIBPP_ENABLE_WINDOWS` (BOOL, Default: ON) - Toggles all of the following

- `USYLIBPP_ENABLE_WINDOWS_UNICODE` (BOOL, Default: ON) - Define UNICODE and _UNICODE
- `USYLIBPP_ENABLE_BASIC_WINDOWS_DEFINES` (BOOL, Default: ON) - Adds basic windows defines
- `USYLIBPP_ENABLE_LTCG` (BOOL, Default: ON) - Enables LTCG for compiled dependencies
- `USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE` (BOOL, Default: ON) - Adds a manifest entry and support for the task dialog + win32 darkmode stuff

- `USYLIBPP_ENABLE_WIL` (BOOL, Default: ON) - Includes WIL in the project and enables all functions which depend on it
- `USYLIBPP_WIL_GIT_TAG` (STRING, Default: "v1.0.260126.7")

- `USYLIBPP_ENABLE_WINTOAST` (BOOL, Default: OFF) - Includes WinToast in the project and enables the helpers for it
- `USYLIBPP_WINTOAST_GIT_TAG` (STRING, Default: "v1.3.2")

- `USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING` (BOOL, Default: OFF) - Includes a basic c++ wrapper + helpers for voidtools everything

### Linux Specific (Not very tested)
- `USYLIBPP_ENABLE_LINUX` (BOOL, Default: ON) - Enables the basic windows wrapper stuff that I've also made for linux