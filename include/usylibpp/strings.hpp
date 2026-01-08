#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <cstring>
#include <filesystem>

namespace usylibpp::strings {
    template<typename T, typename Char>
    concept StringLike = requires(T a) {
        { std::basic_string_view<Char>(a) } -> std::same_as<std::basic_string_view<Char>>;
    };

    template <typename T>
    struct is_wchar_ptr : std::false_type {};

    template <>
    struct is_wchar_ptr<const wchar_t*> : std::true_type {};

    template <>
    struct is_wchar_ptr<wchar_t*> : std::true_type {};

    template <typename T>
    struct is_wstring : std::is_same<std::remove_cv_t<T>, std::wstring> {};

    template <typename T>
    struct is_string : std::is_same<std::remove_cv_t<T>, std::string> {};

    template <typename T>
    struct is_filesystem_path: std::is_same<std::remove_cv_t<T>, std::filesystem::path> {};

    template <typename T>
    concept wchar_t_compatible =
        strings::is_wchar_ptr<std::remove_cvref_t<T>>::value ||
        strings::is_wstring<std::remove_cvref_t<T>>::value ||
        strings::is_string<std::remove_cvref_t<T>>::value ||
        strings::is_filesystem_path<std::remove_cvref_t<T>>::value;

    template <typename T>
    concept wchar_t_strict =
        strings::is_wchar_ptr<std::remove_cvref_t<T>>::value ||
        strings::is_wstring<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_wchar_ptr_v = is_wchar_ptr<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_wstring_v = is_wstring<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_string_v = is_string<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_filesystem_path_v = is_filesystem_path<std::remove_cvref_t<T>>::value;

    template<strings::wchar_t_strict T>
    inline constexpr const wchar_t* wchar_t_from_strict(T&& str) {
        if constexpr (strings::is_wchar_ptr_v<T>) {
            return str;
        } else if constexpr (strings::is_wstring_v<T>) {
            return str.c_str();
        } else {
            static_assert(!std::is_same_v<T, T>, "Unsupported type passed to wchar_t_from_strict, must have forgotten a branch");
        }
    }

    // inline constexpr size_t total_string_size() noexcept { 
    //     return 0; 
    // }

    // template<typename Char, StringLike<Char> T, StringLike<Char>... Ts>
    // inline constexpr size_t total_string_size(T&& first, Ts&&... rest) noexcept {
    //     return std::basic_string_view<Char>(std::forward<T>(first)).size() + total_string_size(std::forward<Ts>(rest)...);
    // }

    template<typename Char, StringLike<Char>... Ts>
    inline constexpr std::basic_string<Char> concat_strings(Ts&&... parts) {
        std::basic_string<Char> result;
        result.resize((std::basic_string_view<Char>(std::forward<Ts>(parts)).size() + ... + 0));

        Char* dest = result.data();
        std::basic_string_view<Char> sv;
        ((sv = std::basic_string_view<Char>(std::forward<Ts>(parts)), memcpy(dest, sv.data(), sv.size() * sizeof(Char)), dest += sv.size()), ...);

        return result;
    }

    inline constexpr void to_lower_case(std::string& str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) -> char {
            return static_cast<char>(std::tolower(c));
        });
    }
}