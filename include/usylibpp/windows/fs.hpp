#pragma once

#ifdef USYLIBPP_ENABLE_WIL
#include "../aliases.hpp" // IWYU pragma: export

#include "../windows.hpp"
#include "../strings.hpp"
#include "../types.hpp"
#include <windows.h>
#include <wil/resource.h>
#include <string>
#include <functional>

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
    typename F1 = types::noop_t,
    typename F2 = types::noop_t,
    typename F3 = types::noop_t
>
requires (std::invocable<F1&, wstring_arg, data_arg> && 
          std::invocable<F2&, wstring_arg, data_arg> && 
          std::invocable<F3&, wstring_arg, data_arg>)
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
 * Otherwise returning false from any method stops iterating
 * Call cancel_walk_directory on the data to completely cancel the walk even if recursed
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
        
        #pragma push_macro("HANDLE")
        #undef HANDLE
        #define HANDLE(func) \
        if constexpr (!std::is_same_v<decltype(std::declval<CB>().func), types::noop_t>) { \
            if constexpr (std::is_convertible_v<std::invoke_result_t<decltype((std::declval<CB>().func)), wstring_arg, data_arg>, bool>) { \
                if (!std::invoke(cb.func, root, wrapper)) break; \
            } else { \
                std::invoke(cb.func, root, wrapper); \
            } \
        }
        
        if (attr & FILE_ATTRIBUTE_DEVICE || attr & FILE_ATTRIBUTE_OFFLINE || attr & FILE_ATTRIBUTE_VIRTUAL) continue;
        else if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
            HANDLE(on_other)
        } else if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            if constexpr (opts.recursive) {
                if constexpr (!std::is_same_v<decltype(std::declval<CB>().on_directory), types::noop_t>) {
                    #pragma push_macro("recurse")
                    #undef recurse
                    #define recurse \
                    if constexpr (opts.trailing_slash_on_parent) { \
                        _walk_directory<opts>(strings::concat_strings(root, filename), cb, wrapper); \
                    } else { \
                        _walk_directory<opts>(strings::concat_strings(root, L"\\", filename), cb, wrapper); \
                    }

                    if constexpr (std::is_convertible_v<std::invoke_result_t<decltype((std::declval<CB>().on_directory)), wstring_arg, data_arg>, bool>) {
                        if (std::invoke(cb.on_directory, root, wrapper)) {
                            recurse
                        }
                    } else {
                        std::invoke(cb.on_directory, root, wrapper);
                        recurse
                    }
                    #pragma pop_macro("recurse")
                }
            } else {
                HANDLE(on_directory)
            }
        } else {
            HANDLE(on_file)
        }

        #pragma pop_macro("HANDLE")

    } while (!wrapper.cancelled && FindNextFileW(hFind_ptr, &data));
}

/**
 * Return false from on_directory to not recurse into it if using recursive
 */
template <WalkOpts opts = {}, CallbacksType CB, types::wchar_t_compatible T>
inline void walk_directory(T&& _root, CB&& cb) {
    FindDataWrapper wrapper{};
    std::wstring root{wchar_t_from_compatible(std::forward<T>(_root))};
    if (!root.empty())
        _walk_directory<opts>(std::move(root), std::forward<CB>(cb), wrapper);
}
}
#endif