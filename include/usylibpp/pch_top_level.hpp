#ifdef USYLIBPP_ENABLE_WINDOWS
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shellapi.h>
#include <knownfolders.h>
#include <shlobj.h>
#endif

#ifdef USYLIBPP_ENABLE_WINDOWS_IMAGING
#include <wincodec.h>
#include <wincodecsdk.h>
#endif

#ifdef USYLIBPP_ENABLE_WIL
#include <shellapi.h>
#include <wil/resource.h>
#include <wil/com.h>
#endif

#ifdef USYLIBPP_ENABLE_WINTOAST
#include <wintoastlib.h>
#endif

#ifdef USYLIBPP_ENABLE_LINUX
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#endif

#include <cerrno>
#include <chrono>
#include <csignal>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <filesystem>
#include <memory>
#include <string>
#include <optional>
#include <thread>
#include <atomic>
#include <future>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <algorithm>
#include <cwctype>
#include <string_view>
#include <cstring>
#include <charconv>
#include <span>
#include <fstream>
#include <iostream>
#include <locale>
#include <format>
#include <ctime>
#include <iomanip>
#include <sstream>