#pragma once

#ifdef USYLIBPP_ENABLE_WIL
#include "../windows.hpp"
#include "../strings.hpp"
#include <windows.h>
#include <wil/resource.h>
#include <string>

namespace usylibpp::windows::fs {
using wstring_arg = const std::wstring&;
using data_arg = const WIN32_FIND_DATAW&;
template <
    typename F1 = decltype([](wstring_arg, wstring_arg, data_arg){}), 
    typename F2 = decltype([](wstring_arg, wstring_arg, data_arg){}), 
    typename F3 = decltype([](wstring_arg, wstring_arg, data_arg){})
>
requires (std::invocable<F1, wstring_arg, wstring_arg, data_arg> && 
          std::invocable<F2, wstring_arg, wstring_arg, data_arg> && 
          std::invocable<F3, wstring_arg, wstring_arg, data_arg>)
struct DirectoryCallbacks {
    F1 on_file{};
    F2 on_directory{};
    F3 on_other{};
};

template <typename>
struct is_directory_callbacks : std::false_type {};

template <typename F1, typename F2, typename F3>
struct is_directory_callbacks<DirectoryCallbacks<F1, F2, F3>> : std::true_type {};

template <typename T>
concept DirectoryCallbacksType = is_directory_callbacks<std::remove_cvref_t<T>>::value;

struct WalkOpts {
    bool recursive = false;
};

/**
 * Return false from on_directory to not recurse into it if using recursive
 */
template <WalkOpts opts = {}, DirectoryCallbacksType CB>
inline void walk_directory(std::wstring root, CB&& cb) {
    if (!root.ends_with(L'\\')) root.push_back(L'\\');

    root.push_back(L'*');

    WIN32_FIND_DATAW data{};
    const wil::unique_hfind hFind{
        FindFirstFileExW(
            root.c_str(), // check if this needs to be preserved or copied
            FindExInfoBasic,
            &data,
            FindExSearchNameMatch,
            nullptr,
            FIND_FIRST_EX_LARGE_FETCH
        )
    };

    if (!hFind || hFind.get() == INVALID_HANDLE_VALUE) return;

    root.pop_back();
    const HANDLE hFind_ptr = static_cast<HANDLE>(hFind.get());

    do {
        // Skip "." and ".."
        if (data.cFileName[0] == L'.' &&
            (data.cFileName[1] == L'\0' ||
             (data.cFileName[1] == L'.' && data.cFileName[2] == L'\0')))
            continue;

        const DWORD attr = data.dwFileAttributes;

        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            if constexpr (opts.recursive && std::is_same_v<std::invoke_result_t<decltype(std::declval<CB>().on_directory), wstring_arg, wstring_arg, data_arg>, bool>) {
                if (std::invoke(cb.on_directory, root, data.cFileName, data)) {
                    walk_directory<opts>(strings::concat_strings(root, data.cFileName), cb);
                }
            } else {
                std::invoke(cb.on_directory, root, data.cFileName, data);
            }
        }
        else if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
            std::invoke(cb.on_other, root, data.cFileName, data);
        }
        else {
            std::invoke(cb.on_file, root, data.cFileName, data);
        }

    } while (FindNextFileW(hFind_ptr, &data));
}
}
#endif