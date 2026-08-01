#ifdef USYLIBPP_ENABLE_WINDOWS
#include <windows.h> // IWYU pragma: keep
#include <shobjidl.h> // IWYU pragma: keep
#include <shlguid.h> // IWYU pragma: keep
#include <shellapi.h> // IWYU pragma: keep
#include <knownfolders.h> // IWYU pragma: keep
#include <shlobj.h> // IWYU pragma: keep
#endif

#ifdef USYLIBPP_ENABLE_WINDOWS_IMAGING
#include <wincodec.h> // IWYU pragma: keep
#include <wincodecsdk.h> // IWYU pragma: keep
#endif

#ifdef USYLIBPP_ENABLE_WIL
#include <shellapi.h> // IWYU pragma: keep
#include <wil/resource.h> // IWYU pragma: keep
#include <wil/com.h> // IWYU pragma: keep
#endif

#ifdef USYLIBPP_ENABLE_WINTOAST
#include <wintoastlib.h> // IWYU pragma: keep
#endif

#ifdef USYLIBPP_ENABLE_LINUX
#include <unistd.h> // IWYU pragma: keep
#include <sys/types.h> // IWYU pragma: keep
#include <sys/wait.h> // IWYU pragma: keep
#include <fcntl.h> // IWYU pragma: keep
#if defined(__linux__)
#include <sys/prctl.h> // IWYU pragma: keep
#endif
#endif

#include <cerrno> // IWYU pragma: keep
#include <chrono> // IWYU pragma: keep
#include <csignal> // IWYU pragma: keep
#include <concepts> // IWYU pragma: keep
#include <cstdint> // IWYU pragma: keep
#include <type_traits> // IWYU pragma: keep
#include <filesystem> // IWYU pragma: keep
#include <memory> // IWYU pragma: keep
#include <string> // IWYU pragma: keep
#include <optional> // IWYU pragma: keep
#include <thread> // IWYU pragma: keep
#include <atomic> // IWYU pragma: keep
#include <future> // IWYU pragma: keep
#include <mutex> // IWYU pragma: keep
#include <queue> // IWYU pragma: keep
#include <condition_variable> // IWYU pragma: keep
#include <functional> // IWYU pragma: keep
#include <algorithm> // IWYU pragma: keep
#include <cwctype> // IWYU pragma: keep
#include <string_view> // IWYU pragma: keep
#include <cstring> // IWYU pragma: keep
#include <charconv> // IWYU pragma: keep
#include <span> // IWYU pragma: keep
#include <fstream> // IWYU pragma: keep
#include <iostream> // IWYU pragma: keep
#include <locale> // IWYU pragma: keep
#include <format> // IWYU pragma: keep
#include <ctime> // IWYU pragma: keep
#include <iomanip> // IWYU pragma: keep
#include <sstream> // IWYU pragma: keep

#include "glaze.hpp" // IWYU pragma: keep