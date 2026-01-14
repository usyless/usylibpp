#pragma once

#include "macros.hpp"
#include <algorithm>
#include <string>
#include <string_view>
#include <cstring>
#include <charconv>
#include "types.hpp"

#ifdef USYLIBPP_ENABLE_WINDOWS
namespace usylibpp::windows {
    std::optional<std::wstring> to_wstr(const char* utf8);
    std::optional<std::wstring> to_wstr(const std::string& utf8);
}
#endif

namespace usylibpp::strings {
    #ifdef USYLIBPP_ENABLE_WINDOWS
    template<types::wchar_t_strict T>
    [[nodiscard]] inline constexpr const wchar_t* wchar_t_from_strict(T&& str) {
        if constexpr (types::wchar_ptr<T>) {
            return str;
        } else if constexpr (types::wstring<T>) {
            return str.c_str();
        } else {
            static_assert(!std::is_same_v<T, T>, "Unsupported type passed to usylibpp::strings::wchar_t_from_strict, must have forgotten a branch");
        }
    }

    /**
     * If its a string type this pointer will only survive to the next call on the thread
     */
    template<types::wchar_t_compatible T>
    [[nodiscard]] inline const wchar_t* wchar_t_from_compatible(T&& str) {
        if constexpr (types::wchar_t_strict<T>) {
            return wchar_t_from_strict(std::forward<T>(str));
        } else if constexpr (types::string<T>) {
            static thread_local std::optional<std::wstring> buffer;
            buffer = windows::to_wstr(str);
            if (!buffer) return L"";
            return buffer->c_str();
        } else if constexpr (types::filesystem_path<T>) {
            return str.native().c_str();
        } else {
            static_assert(!std::is_same_v<T, T>, "Unsupported type passed to usylibpp::strings::wchar_t_from_compatible, must have forgotten a branch");
        }
    }
    #endif

    template<typename... Ts>
    [[nodiscard]] inline constexpr auto concat_strings(Ts&&... parts) {
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

    [[nodiscard]] inline constexpr std::string to_lowercase(const std::string_view str) {
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

    [[nodiscard]] inline constexpr std::string replace_all(const std::string_view str, const std::string_view from, const std::string_view to) {
        std::string ret{str};
        replace_all_inplace(ret, from, to);
        return ret;
    }

    template <types::UnsignedInteger N>
    [[nodiscard]] inline constexpr std::optional<N> to_number(const std::string_view str) noexcept {
        N num;
        if (std::from_chars(str.data(), str.data() + str.size(), num).ec == std::errc()) return num;
        return std::nullopt;
    }
    
    USYLIBPP__MAKE_OR(to_number, 0)

    /**
     * String view only survives to next function call on this thread, make copy into std::string to keep alive
     */
    template <types::UnsignedInteger T>
    [[nodiscard]] inline std::optional<std::string_view> to_string_view(T val) noexcept {
        constexpr auto TO_STRING_BUFFER_LENGTH = 21;

        static thread_local char buffer[TO_STRING_BUFFER_LENGTH];
        auto [ptr, ec] = std::to_chars(buffer, buffer + TO_STRING_BUFFER_LENGTH, val);
        if (ec != std::errc()) return std::nullopt;
        return std::string_view{buffer, static_cast<size_t>(ptr - buffer)};
    }

    USYLIBPP__MAKE_OR(to_string_view, std::string_view{})

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

    [[nodiscard]] inline constexpr size_t count_of(const std::string_view str, const char c) noexcept {
        size_t count = 0;
        for (const auto a : str) if (a == c) ++count;
        return count;
    }

    /**
     * Includes the null terminator
     */
    template<std::size_t N>
    [[nodiscard]] inline constexpr std::size_t constexpr_strlen(const char (&)[N]) {
        return N;
    }

    [[nodiscard]] inline 
    #if __cplusplus >= 202302L
    constexpr 
    #endif
    std::string url_encode(const std::string_view url) {
        static constexpr char hex[] = "0123456789ABCDEF";

        std::string out;
        out.reserve(url.size() * 3); // worst case

        for (unsigned char c : url) {
            if ((c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                out.push_back(c);
            } else {
                out.push_back('%');
                out.push_back(hex[c >> 4]);
                out.push_back(hex[c & 0xF]);
            }
        }

        return out;
    }

    /**
     * Won't work for name:host url's
     * Probably has other bugs
     */
    [[nodiscard]] inline constexpr std::string encode_full_url(const std::string_view url) {
        std::string encoded_path{};

        const auto scheme_end = url.find("://");
        std::string_view rest = url;
        
        // separate scheme
        if (scheme_end != std::string_view::npos) {
            encoded_path += url.substr(0, scheme_end + 3); // keep ://
            rest = url.substr(scheme_end + 3);
        }

        // separate host from path parts
        const auto path_start = rest.find('/');
        std::string_view path{};

        if (path_start == std::string_view::npos) {
            encoded_path += rest; // rest is host
        } else {
            encoded_path += rest.substr(0, path_start); // host
            path = rest.substr(path_start);
        }

        // separate path from query
        const auto query_start = path.find('?');
        std::string_view raw_path = path.substr(0, query_start);
        std::string_view raw_query{};

        if (query_start != std::string_view::npos) {
            raw_query = path.substr(query_start + 1);
        }

        // separate query from fragment
        const auto fragment_start = raw_query.find('#');
        std::string_view raw_fragment{};

        if (fragment_start != std::string_view::npos) {
            raw_fragment = raw_query.substr(fragment_start + 1);
            raw_query = raw_query.substr(0, fragment_start);
        }

        // encode path segments
        split_by_for_each(raw_path, '/', [&encoded_path](const std::string_view part) {
            encoded_path += url_encode(part);
            encoded_path.push_back('/');
        });
        if (!raw_path.ends_with('/')) encoded_path.pop_back();

        if (!raw_query.empty()) {
            encoded_path.push_back('?');
            encoded_path += url_encode(raw_query);
        }

        if (!raw_fragment.empty()) {
            encoded_path.push_back('#');
            encoded_path += url_encode(raw_fragment);
        }

        return encoded_path;
    }

}
