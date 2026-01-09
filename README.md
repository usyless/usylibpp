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
