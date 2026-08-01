#pragma once

#include "aliases.hpp" // IWYU pragma: export
#include "macros.hpp"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <vector>
#include <string>
#include <string_view>
#include <cstring>
#include <charconv>
#include <functional>
#include <optional>
#include "types.hpp"
#include "glaze.hpp" // IWYU pragma: keep

namespace usylibpp::strings {
    template<types::is_basic_string_view... Ts>
    requires (
        sizeof...(Ts) > 1) && 
        (std::same_as<
            types::string_view_char_t<Ts>, 
            types::string_view_char_t<std::tuple_element_t<0, std::tuple<Ts...>>>
        > && ...)
    [[nodiscard]] inline constexpr auto concat_strings(Ts&&... parts) {
        using First = std::tuple_element_t<0, std::tuple<Ts...>>;
        using Char = types::string_view_char_t<First>;

        std::basic_string<Char> result;
        result.resize((std::basic_string_view<Char>(std::forward<Ts>(parts)).size() + ... + 0));

        Char* dest = result.data();
        std::basic_string_view<Char> sv;
        #if USYLIBPP_CPLUSPLUS >= 202302L
        if consteval
        #else
        if (std::is_constant_evaluated())
        #endif
        {
            ((sv = std::basic_string_view<Char>(std::forward<Ts>(parts)), [&sv, &dest]{
                for (::size_t i = 0; i < sv.size(); ++i) {
                    dest[i] = sv[i];
                }
            }(), dest += sv.size()), ...);
        } else {
            ((sv = std::basic_string_view<Char>(parts),
              (sv.empty() ? (void)0 : (void)::memcpy(dest, sv.data(), sv.size() * sizeof(Char))),
              dest += sv.size()), ...);
        }

        return result;
    }

    template <types::is_basic_string_view SV>
    [[nodiscard]] inline constexpr bool is_valid_ascii(SV&& _str) noexcept {
        using Char = types::string_view_char_t<SV>;
        using cast = std::conditional_t<std::is_same_v<Char, wchar_t>, wchar_t, unsigned char>;

        std::basic_string_view<Char> str{_str};
        return std::all_of(str.begin(), str.end(), [](Char c) noexcept { return static_cast<cast>(c) < 128; });
    }

    /**
     * Super basic ascii string conversion from string <=> wstring
     * Does not perform any utf8/utf16 conversion
     * Returns nullopt if not a valid conversion
     */
    template <types::is_basic_string_view SV>
    [[nodiscard]] inline constexpr std::optional<std::basic_string<std::conditional_t<std::is_same_v<types::string_view_char_t<SV>, char>, wchar_t, char>>> ascii_convert_string(SV&& _str) {
        using Char = types::string_view_char_t<SV>;
        using other_char = std::conditional_t<std::is_same_v<Char, char>, wchar_t, char>;

        std::basic_string_view<Char> str{_str};
        if (!is_valid_ascii(str)) return std::nullopt;

        std::basic_string<other_char> out;
        out.reserve(str.size());
        for (const auto c : str) out.push_back(static_cast<other_char>(c));
        return out;
    }

    template <types::is_basic_string S>
    requires (types::CharOrWChar<typename S::value_type>)
    inline constexpr void to_lowercase_inplace(S& str) noexcept {
        using Char = S::value_type;
        if constexpr (std::is_same_v<Char, char>) {
            #pragma push_macro("tolower")
            #undef tolower
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) noexcept -> char { return static_cast<char>(std::tolower(c)); });
            #pragma pop_macro("tolower")
        } else {
            std::transform(str.begin(), str.end(), str.begin(), [](wchar_t c) noexcept -> wchar_t { return static_cast<wchar_t>(std::towlower(c)); });
        }
    }

    template <types::is_basic_string_view SV>
    [[nodiscard]] inline constexpr std::basic_string<types::string_view_char_t<SV>> to_lowercase(SV&& str) {
        std::basic_string<types::string_view_char_t<SV>> ret{str};
        to_lowercase_inplace(ret);
        return ret;
    }

    /**
     * Don't pass in an rvalue as it return a view into the input
     */
    template <types::CharOrWChar Char, types::is_basic_string_view SV>
    requires (std::same_as<types::string_view_char_t<SV>, Char> && !types::owning_rvalue_string<SV>)
    [[nodiscard]] inline constexpr std::basic_string_view<Char> trim_left(SV&& _input, const Char character) noexcept {
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
    requires (std::same_as<types::string_view_char_t<SV>, Char> && !types::owning_rvalue_string<SV>)
    [[nodiscard]] inline constexpr std::basic_string_view<Char> trim_right(SV&& _input, const Char character) noexcept {
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
    requires (std::same_as<types::string_view_char_t<SV>, Char> && !types::owning_rvalue_string<SV>)
    [[nodiscard]] inline constexpr std::basic_string_view<Char> trim(SV&& _input, const Char character) noexcept {
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

        if (from.empty()) return;

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

    /**
     * Returns nullopt unless the WHOLE string parses as a number.
     */
    template <types::Numeric N>
    requires (!std::is_same_v<N, bool>)
    [[nodiscard]] inline constexpr std::optional<N> to_number(const std::string_view str) noexcept {
        if (str.empty()) return std::nullopt;
        N num{};
        const auto* const last = str.data() + str.size();
        #ifndef USYLIBPP_HAS_GLAZE
        const auto res = std::from_chars(str.data(), last, num);
        #else
        const auto res = [&] {
            if constexpr (std::is_floating_point_v<N>) return glz::from_chars<false>(str.data(), last, num);
            else return std::from_chars(str.data(), last, num);
        }();
        #endif
        if (res.ec == std::errc{} && res.ptr == last) return num;
        return std::nullopt;
    }
    
    USYLIBPP__MAKE_OR(to_number, 0)


    #ifdef USYLIBPP_HAS_GLAZE
    template<typename T>
    concept has_glaze_40kb = requires(char* buf, T val) {
        { glz::to_chars_40kb(buf, val) } -> std::same_as<char*>;
    };
    #endif

    /**
     * String view only survives to next function call on this thread, make copy into std::string to keep alive
     */
    template <types::Numeric T>
    requires (!std::is_same_v<T, bool>)
    [[nodiscard]] inline std::string_view to_string_view(T val) noexcept {
        static constexpr auto TO_STRING_BUFFER_LENGTH = 128;
        static thread_local char buffer[TO_STRING_BUFFER_LENGTH];

        #ifndef USYLIBPP_HAS_GLAZE
        // this will never fail
        return std::string_view{buffer, std::to_chars(buffer, buffer + TO_STRING_BUFFER_LENGTH, val).ptr};
        #else
        if constexpr (has_glaze_40kb<T>) {
            return std::string_view{buffer, glz::to_chars_40kb(buffer, val)};
        } else {
            return std::string_view{buffer, std::to_chars(buffer, buffer + TO_STRING_BUFFER_LENGTH, val).ptr};
        }
        #endif
    }

    /**
     * Stop looping early if false returned from function
     */
    template <types::CharOrWChar Char, typename F, types::is_basic_string_view SV>
    requires (std::invocable<F&, std::basic_string_view<Char>> && std::same_as<types::string_view_char_t<SV>, Char>)
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
                if constexpr (std::is_convertible_v<std::invoke_result_t<F&, std::basic_string_view<Char>>, bool>) {
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
    [[nodiscard]] inline constexpr std::vector<std::basic_string_view<Char>> split_by(SV&& _input, const Char split_by) {
        std::vector<std::basic_string_view<Char>> result;
        split_by_for_each(std::forward<SV>(_input), split_by, [&result](auto&& view) {
            result.emplace_back(view);
        });
        return result;
    }

    template <types::is_basic_string_view SV, typename F>
    requires (std::invocable<F&, std::basic_string_view<types::string_view_char_t<SV>>>)
    inline constexpr void for_each_line(SV&& input, F&& f) noexcept(noexcept(split_by_for_each(std::forward<SV>(input), types::string_view_char_t<SV>('\n'), std::forward<F>(f)))) {
        split_by_for_each(std::forward<SV>(input), types::string_view_char_t<SV>('\n'), std::forward<F>(f));
    }

    template <types::is_basic_string_view SV>
    [[nodiscard]] inline constexpr std::vector<std::basic_string_view<types::string_view_char_t<SV>>> split_lines(SV&& input) {
        return split_by(std::forward<SV>(input), types::string_view_char_t<SV>('\n'));
    }

    template <types::is_basic_string_view SV, typename F>
    requires (std::invocable<F&, types::string_view_char_t<SV>> && std::is_convertible_v<std::invoke_result_t<F&, types::string_view_char_t<SV>>, bool>)
    [[nodiscard]] inline constexpr size_t count_if(SV&& str, F&& f) noexcept(noexcept(std::invoke(f, std::declval<types::string_view_char_t<SV>>()))) {
        size_t count = 0;
        for (const auto a : std::basic_string_view<types::string_view_char_t<SV>>{str}) if (std::invoke(f, a)) ++count;
        return count;
    }

    template <types::CharOrWChar Char, types::is_basic_string_view SV>
    requires (std::convertible_to<SV, std::basic_string_view<Char>> && std::same_as<types::string_view_char_t<SV>, Char>)
    [[nodiscard]] inline constexpr size_t count_of(SV&& str, const Char c) noexcept {
        size_t count = 0;
        for (const auto a : std::basic_string_view<Char>{str}) if (a == c) ++count;
        return count;
    }

    template<types::is_basic_string_view SV>
    [[nodiscard]] inline consteval std::size_t strlen(SV&& str) {
        return std::basic_string_view<types::string_view_char_t<SV>>{str}.size();
    }

    /**
     * Straight up url encodes a string
     * No url sensitivity, for that use encode_full_urls
     */
    [[nodiscard]] inline 
    #if USYLIBPP_CPLUSPLUS >= 202302L
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
