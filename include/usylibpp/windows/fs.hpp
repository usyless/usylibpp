#pragma once

#ifdef USYLIBPP_ENABLE_WIL
#include "../windows.hpp"
#include "../strings.hpp"
#include <windows.h>
#include <wil/resource.h>
#include <string>

namespace usylibpp::windows::fs {
struct FindDataWrapper {
    const WIN32_FIND_DATAW& data;

    [[nodiscard]] auto date_modified() const noexcept {
        return wil::filetime::to_int64(data.ftLastWriteTime);
    }

    [[nodiscard]] auto creation_time() const noexcept {
        return wil::filetime::to_int64(data.ftCreationTime);
    }

    [[nodiscard]] auto access_time() const noexcept {
        return wil::filetime::to_int64(data.ftLastAccessTime);
    }

    [[nodiscard]] uint64_t file_size() const noexcept {
        return (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    }
};

using wstring_arg = const std::wstring&;
using data_arg = const FindDataWrapper&;
template <
    typename F1 = decltype([](wstring_arg, wstring_arg, data_arg){}), 
    typename F2 = decltype([](wstring_arg, wstring_arg, data_arg){}), 
    typename F3 = decltype([](wstring_arg, wstring_arg, data_arg){})
>
requires (std::invocable<F1, wstring_arg, wstring_arg, data_arg> && 
          std::invocable<F2, wstring_arg, wstring_arg, data_arg> && 
          std::invocable<F3, wstring_arg, wstring_arg, data_arg>)
struct Callbacks {
    F1 on_file{};
    F2 on_directory{};
    F3 on_other{};
};

template <typename>
struct is_callbacks : std::false_type {};

template <typename F1, typename F2, typename F3>
struct is_callbacks<Callbacks<F1, F2, F3>> : std::true_type {};

template <typename T>
concept CallbacksType = is_callbacks<std::remove_cvref_t<T>>::value;

struct WalkOpts {
    bool recursive = false;
};

/**
 * Return false from on_directory to not recurse into it if using recursive
 */
template <WalkOpts opts = {}, CallbacksType CB>
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
                if (std::invoke(cb.on_directory, root, data.cFileName, FindDataWrapper{data})) {
                    walk_directory<opts>(strings::concat_strings(root, data.cFileName), cb);
                }
            } else {
                std::invoke(cb.on_directory, root, data.cFileName, FindDataWrapper{data});
            }
        }
        else if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
            std::invoke(cb.on_other, root, data.cFileName, FindDataWrapper{data});
        }
        else if (attr & FILE_ATTRIBUTE_DEVICE || attr & FILE_ATTRIBUTE_OFFLINE) continue;
        else {
            std::invoke(cb.on_file, root, data.cFileName, FindDataWrapper{data});
        }

    } while (FindNextFileW(hFind_ptr, &data));
}
}
#endif