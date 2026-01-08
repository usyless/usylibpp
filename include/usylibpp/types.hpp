#pragma once

#include <string_view>
#include <string>
#include <filesystem>

namespace usylibpp::types {
    template<typename T, typename Char>
    concept StringLike = requires(T a) {
        { std::basic_string_view<Char>(a) };
    };

    template <typename T>
    struct is_wchar_ptr : std::is_same<std::remove_cv_t<std::decay_t<T>>, wchar_t*> {};

    template <typename T>
    struct is_wstring : std::is_same<std::remove_cv_t<T>, std::wstring> {};

    template <typename T>
    struct is_string : std::is_same<std::remove_cv_t<T>, std::string> {};

    template <typename T>
    struct is_filesystem_path: std::is_same<std::remove_cv_t<T>, std::filesystem::path> {};

    template <typename T>
    concept wchar_t_compatible =
        is_wchar_ptr<std::remove_cvref_t<T>>::value ||
        is_wstring<std::remove_cvref_t<T>>::value ||
        is_string<std::remove_cvref_t<T>>::value ||
        is_filesystem_path<std::remove_cvref_t<T>>::value;

    template <typename T>
    concept wchar_t_strict =
        is_wchar_ptr<std::remove_cvref_t<T>>::value ||
        is_wstring<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_wchar_ptr_v = is_wchar_ptr<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_wstring_v = is_wstring<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_string_v = is_string<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_filesystem_path_v = is_filesystem_path<std::remove_cvref_t<T>>::value;
}