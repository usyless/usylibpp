#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <cstddef>
#include <cstring>
#include <filesystem>

namespace usylibpp::strings {
    template<typename T>
    concept StringLike = requires(T a) {
        { std::string_view(a) };
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

    template<wchar_t_compatible T>
    constexpr bool is_wchar_ptr_v = is_wchar_ptr<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_wstring_v = is_wstring<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_string_v = is_string<std::remove_cvref_t<T>>::value;

    template<wchar_t_compatible T>
    constexpr bool is_filesystem_path_v = is_filesystem_path<std::remove_cvref_t<T>>::value;

    inline constexpr size_t total_string_size() noexcept { 
        return 0; 
    }

    template<StringLike T, StringLike... Ts>
    inline constexpr size_t total_string_size(const T& first, const Ts&... rest) noexcept {
        return std::string_view(first).size() + total_string_size(rest...);
    }

    template<StringLike... Ts>
    inline constexpr std::string concat_strings(const Ts&... parts) {
        std::string result;
        result.resize(total_string_size(parts...));

        char* dest = result.data();
        std::string_view sv;
        ((sv = std::string_view(parts), std::memcpy(dest, sv.data(), sv.size()), dest += sv.size()), ...);

        return result;
    }

    inline constexpr void to_lower_case(std::string& str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) -> char {
            return static_cast<char>(std::tolower(c));
        });
    }
}