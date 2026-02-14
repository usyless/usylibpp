#pragma once

#ifdef USYLIBPP_ENABLE_WIL
#include "../windows.hpp"
#include "../strings.hpp"
#include <windows.h>
#include <wil/resource.h>
#include <string>

namespace usylibpp::windows::fs {
struct FindDataWrapper {
    WIN32_FIND_DATAW data{};
    bool cancelled{false};

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

    [[nodiscard]] const wchar_t* filename() const noexcept {
        return data.cFileName;
    }

    [[nodiscard]] std::optional<std::string> filename_utf8() const {
        return to_utf8(filename());
    }

    [[nodiscard]] std::string filename_utf8_or_default() const {
        return to_utf8_or_default(filename());
    }

    [[nodiscard]] std::wstring_view filename_view() const noexcept {
        return data.cFileName;
    }

    void cancel_walk_directory() noexcept {
        cancelled = true;
    }
};

using wstring_arg = const std::wstring&;
using data_arg = FindDataWrapper&;
template <
    typename F1 = decltype([](wstring_arg, data_arg){}), 
    typename F2 = decltype([](wstring_arg, data_arg){}), 
    typename F3 = decltype([](wstring_arg, data_arg){})
>
requires (std::invocable<F1, wstring_arg, data_arg> && 
          std::invocable<F2, wstring_arg, data_arg> && 
          std::invocable<F3, wstring_arg, data_arg>)
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
    bool trailing_slash_on_parent = false;
};

/**
 * Return false from on_directory to not recurse into it if using recursive
 */
template <WalkOpts opts = {}, CallbacksType CB>
inline void _walk_directory(std::wstring&& root, CB&& cb, FindDataWrapper& wrapper) {
    if (!root.ends_with(L'\\')) root.push_back(L'\\');

    root.push_back(L'*');

    auto& data = wrapper.data;
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

    if (!hFind) return;

    root.pop_back();
    if constexpr (!opts.trailing_slash_on_parent) {
        root.pop_back();
    }
    const HANDLE hFind_ptr = static_cast<HANDLE>(hFind.get());

    do {
        // Skip "." and ".."
        const auto* filename = data.cFileName;
        if (filename[0] == L'.' &&
            (filename[1] == L'\0' ||
             (filename[1] == L'.' && filename[2] == L'\0')))
            continue;

        const DWORD attr = data.dwFileAttributes;
        
        if (attr & FILE_ATTRIBUTE_DEVICE || attr & FILE_ATTRIBUTE_OFFLINE || attr & FILE_ATTRIBUTE_VIRTUAL) continue;
        else if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
            if constexpr (std::is_same_v<std::invoke_result_t<decltype(std::declval<CB>().on_other), wstring_arg, data_arg>, bool>) {
                if (!std::invoke(cb.on_other, root, wrapper)) break;
            } else {
                std::invoke(cb.on_other, root, wrapper);
            }
        } else if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            if constexpr (opts.recursive) {
                #define recurse \
                if constexpr (opts.trailing_slash_on_parent) { \
                    _walk_directory<opts>(strings::concat_strings(root, filename), cb, wrapper); \
                } else { \
                    _walk_directory<opts>(strings::concat_strings(root, L"\\", filename), cb, wrapper); \
                }

                if constexpr (std::is_same_v<std::invoke_result_t<decltype(std::declval<CB>().on_directory), wstring_arg, data_arg>, bool>) {
                    if (std::invoke(cb.on_directory, root, wrapper)) {
                        recurse
                    }
                } else {
                    std::invoke(cb.on_directory, root, wrapper);
                    recurse
                }
                #undef recurse
            } else {
                std::invoke(cb.on_directory, root, wrapper);
            }
        } else {
            if constexpr (std::is_same_v<std::invoke_result_t<decltype(std::declval<CB>().on_file), wstring_arg, data_arg>, bool>) {
                if (!std::invoke(cb.on_file, root, wrapper)) break;
            } else {
                std::invoke(cb.on_file, root, wrapper);
            }
        }

    } while (!wrapper.cancelled && FindNextFileW(hFind_ptr, &data));
}

/**
 * Return false from on_directory to not recurse into it if using recursive
 */
template <WalkOpts opts = {}, CallbacksType CB>
inline void walk_directory(std::wstring root, CB&& cb) {
    FindDataWrapper wrapper{};
    _walk_directory<opts>(std::move(root), std::forward<CB>(cb), wrapper);
}
}
#endif