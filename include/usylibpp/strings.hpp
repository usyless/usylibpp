#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <cstring>
#include <filesystem>

namespace usylibpp::strings {
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
            static_assert(!std::is_same_v<T, T>, "Unsupported type passed to usylibpp::strings::wchar_t_from_strict, must have forgotten a branch");
        }
    }

    template<typename... Ts>
    inline constexpr auto concat_strings(Ts&&... parts) {
        using First = decltype(([](auto&& first, auto&&...) -> auto&& { return first; })(parts...));
        using Char = std::remove_cvref_t<decltype(std::declval<First>()[0])>;

        std::basic_string<Char> result;
        result.resize((std::basic_string_view<Char>(std::forward<Ts>(parts)).size() + ... + 0));

        Char* dest = result.data();
        std::basic_string_view<Char> sv;
        ((sv = std::basic_string_view<Char>(std::forward<Ts>(parts)), memcpy(dest, sv.data(), sv.size() * sizeof(Char)), dest += sv.size()), ...);

        return result;
    }

    inline constexpr void to_lowercase_inplace(std::string& str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
    }

    inline constexpr std::string to_lowercase(const std::string_view str) {
        std::string ret{str};
        to_lowercase_inplace(ret);
        return ret;
    }

    inline constexpr void replace_all_inplace(std::string& str, const std::string_view from, const std::string_view to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }

    inline constexpr std::string replace_all(const std::string_view str, const std::string_view from, const std::string_view to) {
        std::string ret{str};
        replace_all_inplace(ret, from, to);
        return ret;
    }

    /**
     * Does not work for negative numbers or floats
     */
    template <typename N>
    inline constexpr std::optional<N> to_number_positive(const std::string_view str) noexcept {
        N num;
        if (std::from_chars(str.data(), str.data() + str.size(), num).ec == std::errc()) return num;
        return std::nullopt;
    }

    /**
     * Does not work for negative numbers or floats
     * String view only survives to next function call on this thread, make copy into std::string to keep alive
     */
    template <typename T>
    inline std::optional<std::string_view> number_to_string_view_positive(T val) noexcept {
        constexpr auto TO_STRING_BUFFER_LENGTH = 21;

        static thread_local char buffer[TO_STRING_BUFFER_LENGTH];
        auto [ptr, ec] = std::to_chars(buffer, buffer + TO_STRING_BUFFER_LENGTH, val);
        if (ec != std::errc()) return std::nullopt;
        return std::string_view{buffer, static_cast<size_t>(ptr - buffer)};
    }

    inline constexpr void split_by_for_each(const std::string_view input, const unsigned char split_by, const auto& f) noexcept {
        size_t start = 0;
        const auto size = input.size();
        while (start < size) {
            const auto end = input.find(split_by, start);
            if (end == std::string_view::npos) {
                f(input.substr(start));
                break;
            } else {
                f(input.substr(start, end - start));
                start = end + 1;
            }
        }
    }

    inline constexpr void for_each_line(const std::string_view input, const auto& f) noexcept {
        split_by_for_each(input, '\n', f);
    }

    inline constexpr size_t count_of(const std::string_view str, const char c) noexcept {
        size_t count = 0;
        for (const auto a : str) if (a == c) ++count;
        return count;
    }

    template<std::size_t N>
    inline constexpr std::size_t constexpr_strlen(const char (&)[N]) {
        return N;
    }

    inline std::string url_encode(const std::string& url) {
        std::ostringstream escaped;
        escaped << std::hex << std::uppercase << std::setfill('0');
    
        for (char c : url) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
            } else {
                escaped << '%' << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(c));
            }
        }
    
        return escaped.str();
    }
}
