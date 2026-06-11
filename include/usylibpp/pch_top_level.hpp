#ifdef USYLIBPP_ENABLE_WINDOWS
#include <windows.h> // IWYU pragma: export
#include <shobjidl.h> // IWYU pragma: export
#include <shlguid.h> // IWYU pragma: export
#include <shellapi.h> // IWYU pragma: export
#include <knownfolders.h> // IWYU pragma: export
#include <shlobj.h> // IWYU pragma: export
#endif

#ifdef USYLIBPP_ENABLE_WINDOWS_IMAGING
#include <wincodec.h> // IWYU pragma: export
#include <wincodecsdk.h> // IWYU pragma: export
#endif

#ifdef USYLIBPP_ENABLE_WIL
#include <shellapi.h> // IWYU pragma: export
#include <wil/resource.h> // IWYU pragma: export
#include <wil/com.h> // IWYU pragma: export
#endif

#ifdef USYLIBPP_ENABLE_WINTOAST
#include <wintoastlib.h> // IWYU pragma: export
#endif

#ifdef USYLIBPP_ENABLE_LINUX
#include <unistd.h> // IWYU pragma: export
#include <sys/types.h> // IWYU pragma: export
#include <sys/wait.h> // IWYU pragma: export
#include <fcntl.h> // IWYU pragma: export
#if defined(__linux__)
#include <sys/prctl.h> // IWYU pragma: export
#endif
#endif

#include <cerrno> // IWYU pragma: export
#include <chrono> // IWYU pragma: export
#include <csignal> // IWYU pragma: export
#include <concepts> // IWYU pragma: export
#include <cstdint> // IWYU pragma: export
#include <type_traits> // IWYU pragma: export
#include <filesystem> // IWYU pragma: export
#include <memory> // IWYU pragma: export
#include <string> // IWYU pragma: export
#include <optional> // IWYU pragma: export
#include <thread> // IWYU pragma: export
#include <atomic> // IWYU pragma: export
#include <future> // IWYU pragma: export
#include <mutex> // IWYU pragma: export
#include <queue> // IWYU pragma: export
#include <condition_variable> // IWYU pragma: export
#include <functional> // IWYU pragma: export
#include <algorithm> // IWYU pragma: export
#include <cwctype> // IWYU pragma: export
#include <string_view> // IWYU pragma: export
#include <cstring> // IWYU pragma: export
#include <charconv> // IWYU pragma: export
#include <span> // IWYU pragma: export
#include <fstream> // IWYU pragma: export
#include <iostream> // IWYU pragma: export
#include <locale> // IWYU pragma: export
#include <format> // IWYU pragma: export
#include <ctime> // IWYU pragma: export
#include <iomanip> // IWYU pragma: export
#include <sstream> // IWYU pragma: export
