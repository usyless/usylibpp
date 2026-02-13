#pragma once

#include "macros.hpp"
#include <algorithm>
#include <string>
#include <string_view>
#include <cstring>
#include <charconv>
#include "types.hpp"

namespace usylibpp::strings {
    template<types::is_basic_string_view... Ts>
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

    inline constexpr void to_lowercase_inplace(std::string& str) noexcept {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) noexcept -> char { return static_cast<char>(std::tolower(c)); });
    }

    [[nodiscard]] inline constexpr std::string to_lowercase(const std::string_view str) {
        std::string ret{str};
        to_lowercase_inplace(ret);
        return ret;
    }

    /**
     * Don't pass in an rvalue as it return a view into the input
     */
    template <types::CharOrWChar Char, types::is_basic_string_view SV>
    requires (std::same_as<types::string_view_char_t<SV>, Char>)
    inline constexpr std::basic_string_view<Char> trim_left(SV&& _input, const Char character) noexcept {
        const std::basic_string_view<Char> input{_input};
        if (input.empty()) return input;

        const auto input_size = input.size();

        for (size_t i = 0; i < input_size; ++i) {
            if (input[i] != character) return input.substr(i);
        }

        return {};
    }

    /**
     * Don't pass in an rvalue as it return a view into the input
     */
    template <types::CharOrWChar Char, types::is_basic_string_view SV>
    requires (std::same_as<types::string_view_char_t<SV>, Char>)
    inline constexpr std::basic_string_view<Char> trim_right(SV&& _input, const Char character) noexcept {
        const std::basic_string_view<Char> input{_input};
        if (input.empty()) return input;

        size_t end = input.size();
        while (end > 0 && (input[end - 1] == character)) --end;

        return input.substr(0, end);
    }

    /**
     * Don't pass in an rvalue as it return a view into the input
     */
    template <types::CharOrWChar Char, types::is_basic_string_view SV>
    requires (std::same_as<types::string_view_char_t<SV>, Char>)
    inline constexpr std::basic_string_view<Char> trim(SV&& _input, const Char character) noexcept {
        std::basic_string_view<Char> input{_input};
        input = trim_left(input, character);
        input = trim_right(input, character);
        return input;
    }

    template <types::is_basic_string S, types::is_basic_string_view SV1, types::is_basic_string_view SV2>
    requires (std::same_as<typename S::value_type, types::string_view_char_t<SV1>> && std::same_as<types::string_view_char_t<SV1>, types::string_view_char_t<SV2>>)
    inline constexpr void replace_all_inplace(S& str, SV1&& _from, SV2&& _to) {
        using Char = S::value_type;

        const std::basic_string_view<Char> from{_from};
        const std::basic_string_view<Char> to{_to};

        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::basic_string<Char>::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }

    template <types::is_basic_string_view SV1, types::is_basic_string_view SV2, types::is_basic_string_view SV3>
    requires (std::same_as<types::string_view_char_t<SV1>, types::string_view_char_t<SV2>> && std::same_as<types::string_view_char_t<SV2>, types::string_view_char_t<SV3>>)
    [[nodiscard]] inline constexpr auto replace_all(SV1&& str, SV2&& from, SV3&& to) {
        using Char = types::string_view_char_t<SV1>;

        std::basic_string<Char> ret{str};
        replace_all_inplace(ret, std::forward<SV2>(from), std::forward<SV3>(to));
        return ret;
    }

    template <types::Numeric N>
    [[nodiscard]] inline constexpr std::optional<N> to_number(const std::string_view str) noexcept {
        N num;
        if (std::from_chars(str.data(), str.data() + str.size(), num).ec == std::errc{}) return num;
        return std::nullopt;
    }
    
    USYLIBPP__MAKE_OR(to_number, 0)

    /**
     * String view only survives to next function call on this thread, make copy into std::string to keep alive
     */
    template <types::Numeric T>
    [[nodiscard]] inline std::optional<std::string_view> to_string_view(T val) noexcept {
        static constexpr auto TO_STRING_BUFFER_LENGTH = 128;

        static thread_local char buffer[TO_STRING_BUFFER_LENGTH];
        auto [ptr, ec] = std::to_chars(buffer, buffer + TO_STRING_BUFFER_LENGTH, val);
        if (ec != std::errc{}) return std::nullopt;
        return std::string_view{buffer, static_cast<size_t>(ptr - buffer)};
    }

    USYLIBPP__MAKE_OR(to_string_view, std::string_view{})

    /**
     * Stop looping early if false returned from function
     */
    template <types::CharOrWChar Char, typename F, types::is_basic_string_view SV>
    requires (std::invocable<F, std::basic_string_view<Char>> && std::same_as<types::string_view_char_t<SV>, Char>)
    inline constexpr void split_by_for_each(SV&& _input, const Char split_by, F&& f) noexcept(noexcept(std::invoke(f, _input))) {
        std::basic_string_view<Char> input{_input};
        
        size_t start = 0;
        const auto size = input.size();
        while (start < size) {
            const auto end = input.find(split_by, start);
            if (end == std::basic_string_view<Char>::npos) {
                std::invoke(f, input.substr(start));
                return;
            } else {
                if constexpr (std::is_same_v<std::invoke_result_t<F, std::basic_string_view<Char>>, bool>) {
                    if (!std::invoke(f, input.substr(start, end - start))) return;
                } else {
                    std::invoke(f, input.substr(start, end - start));
                }
                start = end + 1;
            }
        }
    }

    template <types::CharOrWChar Char, types::is_basic_string_view SV>
    requires (std::same_as<types::string_view_char_t<SV>, Char>)
    inline constexpr std::vector<std::basic_string_view<Char>> split_by(SV&& _input, const Char split_by) {
        std::vector<std::basic_string_view<Char>> result;
        split_by_for_each(std::forward<SV>(_input), split_by, [&result](auto&& view) {
            result.emplace_back(view);
        });
        return result;
    }

    template <types::is_basic_string_view SV, typename F>
    requires (std::invocable<F, std::basic_string_view<types::string_view_char_t<SV>>>)
    inline constexpr void for_each_line(SV&& input, F&& f) noexcept(noexcept(split_by_for_each(std::forward<SV>(input), types::string_view_char_t<SV>('\n'), std::forward<F>(f)))) {
        split_by_for_each(std::forward<SV>(input), types::string_view_char_t<SV>('\n'), std::forward<F>(f));
    }

    template <types::is_basic_string_view SV>
    inline constexpr std::vector<std::basic_string_view<types::string_view_char_t<SV>>> split_lines(SV&& input) {
        return split_by(std::forward<SV>(input), types::string_view_char_t<SV>('\n'));
    }

    template <types::CharOrWChar Char, types::is_basic_string_view SV>
    requires (std::convertible_to<SV, std::basic_string_view<Char>> && std::same_as<types::string_view_char_t<SV>, Char>)
    [[nodiscard]] inline constexpr size_t count_of(SV&& str, const Char c) noexcept {
        size_t count = 0;
        for (const auto a : str) if (a == c) ++count;
        return count;
    }

    /**
     * Includes the null terminator
     */
    template<types::CharOrWChar Char, std::size_t N>
    [[nodiscard]] inline consteval std::size_t constexpr_strlen(const Char (&)[N]) noexcept {
        return N;
    }

    /**
     * Straight up url encodes a string
     * No url sensitivity, for that use encode_full_urls
     */
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
        auto path_start = rest.find('/');
        if (path_start == std::string_view::npos) path_start = rest.find('?');
        if (path_start == std::string_view::npos) path_start = rest.find('#');
        std::string_view path{};

        if (path_start == std::string_view::npos) {
            encoded_path += rest; // rest is host
        } else {
            encoded_path += rest.substr(0, path_start); // host
            path = rest.substr(path_start);
        }

        // separate path from query
        auto query_start = path.find('?');
        std::string_view raw_query = (query_start != std::string_view::npos) ?
                                        path.substr(query_start + 1) : "";
        
        std::string_view raw_fragment{};
        if(query_start == std::string_view::npos) {
            query_start = path.find('#');

            if (query_start != std::string_view::npos) {
                raw_fragment = path.substr(query_start + 1);
                path = path.substr(0, query_start);
            }
        } else {
            const auto fragment_start = raw_query.find('#');

            if (fragment_start != std::string_view::npos) {
                raw_fragment = raw_query.substr(fragment_start + 1);
                raw_query = raw_query.substr(0, fragment_start);
            }
        }

        // encode path segments
        std::string_view raw_path = path.substr(0, query_start);
        bool did_split = false;
        split_by_for_each(raw_path, '/', [&encoded_path, &did_split](const std::string_view part) {
            did_split = true;
            encoded_path += url_encode(part);
            encoded_path.push_back('/');
        });
        if (did_split && !raw_path.ends_with('/')) encoded_path.pop_back();

        if (!raw_query.empty()) {
            encoded_path.push_back('?');

            split_by_for_each(raw_query, '&', [&encoded_path](const std::string_view part) {
                const auto equals_start = part.find('=');
                if (equals_start == std::string_view::npos) {
                    encoded_path += url_encode(part);
                } else {
                    encoded_path += url_encode(part.substr(0, equals_start));
                    encoded_path.push_back('=');
                    encoded_path += url_encode(part.substr(equals_start + 1));
                }
                encoded_path.push_back('&');
            });
            
            encoded_path.pop_back();
        }

        if (!raw_fragment.empty()) {
            encoded_path.push_back('#');
            encoded_path += url_encode(raw_fragment);
        }

        return encoded_path;
    }

}
