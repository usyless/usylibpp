#pragma once

#include "aliases.hpp" // IWYU pragma: export
#include "macros.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
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
        result.resize((std::basic_string_view<Char>(parts).size() + ... + 0));

        Char* dest = result.data();
        std::basic_string_view<Char> sv;
        #if USYLIBPP_CPLUSPLUS >= 202302L
        if consteval
        #else
        if (std::is_constant_evaluated())
        #endif
        {
            ((sv = std::basic_string_view<Char>(parts), [&sv, &dest]{
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

    template <types::Numeric T>
    requires (!std::is_same_v<T, bool>)
    [[nodiscard]] inline std::string to_string(T val) noexcept {
        static constexpr auto TO_STRING_BUFFER_LENGTH = 128;
        char buffer[TO_STRING_BUFFER_LENGTH];

        #ifndef USYLIBPP_HAS_GLAZE
        // this will never fail
        return std::string{buffer, std::to_chars(buffer, buffer + TO_STRING_BUFFER_LENGTH, val).ptr};
        #else
        if constexpr (has_glaze_40kb<T>) {
            return std::string{buffer, glz::to_chars_40kb(buffer, val)};
        } else {
            return std::string{buffer, std::to_chars(buffer, buffer + TO_STRING_BUFFER_LENGTH, val).ptr};
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

    namespace detail {

    template <typename T>
    concept SupportedChar =
        std::same_as<T, char> || std::same_as<T, wchar_t> ||
        std::same_as<T, char8_t> || std::same_as<T, char16_t> || std::same_as<T, char32_t>;

    static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4);

    template <SupportedChar C>
    inline constexpr size_t utf_width_v = sizeof(C);

    inline constexpr char32_t REPLACEMENT_CP = 0xFFFDu;

    template <SupportedChar C>
    [[nodiscard]] constexpr auto to_unsigned_unit(C c) noexcept {
        if constexpr (sizeof(C) == 1) {
            return static_cast<uint8_t>(c);
        } else if constexpr (sizeof(C) == 2) {
            return static_cast<uint16_t>(c);
        } else {
            return static_cast<uint32_t>(c);
        }
    }

    template <typename T>
    concept NativePathLike = requires(const T& p) {
        typename T::value_type;
        typename T::string_type;
        requires SupportedChar<typename T::value_type>;
        { p.native() } -> std::same_as<const typename T::string_type&>;
    };

    /// Normalizes any supported string-like input (string, string_view, FixedBasicString, C string,
    /// C array, single char/wchar_t/charN_t, or std::filesystem::path) into a non-owning view without allocating.
    template <typename T>
    constexpr auto to_part_view(const T& part) noexcept {
        using Bare = std::remove_cvref_t<T>;
        if constexpr (SupportedChar<Bare>) {
            return std::basic_string_view<Bare>{&part, 1};
        } else if constexpr (std::is_pointer_v<Bare> && SupportedChar<std::remove_cv_t<std::remove_pointer_t<Bare>>>) {
            using Elem = std::remove_cv_t<std::remove_pointer_t<Bare>>;
            return part ? std::basic_string_view<Elem>{part} : std::basic_string_view<Elem>{};
        } else if constexpr (std::is_array_v<Bare> && SupportedChar<std::remove_cv_t<std::remove_extent_t<Bare>>>) {
            using Elem = std::remove_cv_t<std::remove_extent_t<Bare>>;
            return std::basic_string_view<Elem>{part};
        } else if constexpr (NativePathLike<Bare>) {
            return std::basic_string_view<typename Bare::value_type>{part.native()};
        } else if constexpr (std::convertible_to<const Bare&, std::basic_string_view<char>>) {
            return std::basic_string_view<char>{part};
        } else if constexpr (std::convertible_to<const Bare&, std::basic_string_view<wchar_t>>) {
            return std::basic_string_view<wchar_t>{part};
        } else if constexpr (std::convertible_to<const Bare&, std::basic_string_view<char8_t>>) {
            return std::basic_string_view<char8_t>{part};
        } else if constexpr (std::convertible_to<const Bare&, std::basic_string_view<char16_t>>) {
            return std::basic_string_view<char16_t>{part};
        } else if constexpr (std::convertible_to<const Bare&, std::basic_string_view<char32_t>>) {
            return std::basic_string_view<char32_t>{part};
        }
    }

    template <typename T>
    concept StringPart = requires(const T& part) {
        { to_part_view(part) } -> std::same_as<std::basic_string_view<typename decltype(to_part_view(part))::value_type>>;
    };

    /// Decodes the next Unicode scalar value from `sv` starting at `i`, advancing `i` past the
    /// consumed units (or maximal ill-formed subpart, returning U+FFFD).
    template <SupportedChar SrcChar>
    [[nodiscard]] constexpr char32_t decode_next(std::basic_string_view<SrcChar> sv, size_t& i) noexcept {
        const size_t n = sv.size();
        if constexpr (utf_width_v<SrcChar> == 1) {
            const auto b0 = static_cast<uint8_t>(sv[i]);
            if (b0 < 0x80u) {
                ++i;
                return b0;
            }
            if (b0 >= 0xC2u && b0 <= 0xDFu) {
                if (i + 1 < n) {
                    const auto b1 = static_cast<uint8_t>(sv[i + 1]);
                    if ((b1 & 0xC0u) == 0x80u) {
                        i += 2;
                        return (static_cast<char32_t>(b0 & 0x1Fu) << 6u) | static_cast<char32_t>(b1 & 0x3Fu);
                    }
                }
                ++i;
                return REPLACEMENT_CP;
            }
            if (b0 >= 0xE0u && b0 <= 0xEFu) {
                if (i + 1 < n) {
                    const auto b1 = static_cast<uint8_t>(sv[i + 1]);
                    const uint8_t lo = (b0 == 0xE0u) ? 0xA0u : 0x80u;
                    const uint8_t hi = (b0 == 0xEDu) ? 0x9Fu : 0xBFu;
                    if (b1 >= lo && b1 <= hi) {
                        if (i + 2 < n) {
                            const auto b2 = static_cast<uint8_t>(sv[i + 2]);
                            if ((b2 & 0xC0u) == 0x80u) {
                                i += 3;
                                return (static_cast<char32_t>(b0 & 0x0Fu) << 12u) |
                                       (static_cast<char32_t>(b1 & 0x3Fu) << 6u) |
                                       static_cast<char32_t>(b2 & 0x3Fu);
                            }
                        }
                        i += 2;
                        return REPLACEMENT_CP;
                    }
                }
                ++i;
                return REPLACEMENT_CP;
            }
            if (b0 >= 0xF0u && b0 <= 0xF4u) {
                if (i + 1 < n) {
                    const auto b1 = static_cast<uint8_t>(sv[i + 1]);
                    const uint8_t lo = (b0 == 0xF0u) ? 0x90u : 0x80u;
                    const uint8_t hi = (b0 == 0xF4u) ? 0x8Fu : 0xBFu;
                    if (b1 >= lo && b1 <= hi) {
                        if (i + 2 < n) {
                            const auto b2 = static_cast<uint8_t>(sv[i + 2]);
                            if ((b2 & 0xC0u) == 0x80u) {
                                if (i + 3 < n) {
                                    const auto b3 = static_cast<uint8_t>(sv[i + 3]);
                                    if ((b3 & 0xC0u) == 0x80u) {
                                        i += 4;
                                        return (static_cast<char32_t>(b0 & 0x07u) << 18u) |
                                               (static_cast<char32_t>(b1 & 0x3Fu) << 12u) |
                                               (static_cast<char32_t>(b2 & 0x3Fu) << 6u) |
                                               static_cast<char32_t>(b3 & 0x3Fu);
                                    }
                                }
                                i += 3;
                                return REPLACEMENT_CP;
                            }
                        }
                        i += 2;
                        return REPLACEMENT_CP;
                    }
                }
                ++i;
                return REPLACEMENT_CP;
            }
            ++i;
            return REPLACEMENT_CP;
        } else if constexpr (utf_width_v<SrcChar> == 2) {
            const auto u0 = static_cast<uint16_t>(sv[i]);
            if (u0 < 0xD800u || u0 > 0xDFFFu) {
                ++i;
                return u0;
            }
            if (u0 <= 0xDBFFu && i + 1 < n) {
                const auto u1 = static_cast<uint16_t>(sv[i + 1]);
                if (u1 >= 0xDC00u && u1 <= 0xDFFFu) {
                    i += 2;
                    return 0x10000u +
                           ((static_cast<char32_t>(u0 - 0xD800u) << 10u) | static_cast<char32_t>(u1 - 0xDC00u));
                }
            }
            ++i;
            return REPLACEMENT_CP;
        } else {
            const auto u = static_cast<uint32_t>(sv[i]);
            ++i;
            if (u <= 0x10FFFFu && (u < 0xD800u || u > 0xDFFFu)) return u;
            return REPLACEMENT_CP;
        }
    }

    template <SupportedChar DstChar>
    [[nodiscard]] constexpr size_t encoded_units(char32_t cp) noexcept {
        if constexpr (utf_width_v<DstChar> == 1) {
            if (cp < 0x80u) return 1;
            if (cp < 0x800u) return 2;
            if (cp < 0x10000u) return 3;
            return 4;
        } else if constexpr (utf_width_v<DstChar> == 2) {
            return cp < 0x10000u ? 1u : 2u;
        } else {
            return 1u;
        }
    }

    template <SupportedChar DstChar>
    [[nodiscard]] constexpr DstChar* encode_unit(DstChar* out, char32_t cp) noexcept {
        if constexpr (utf_width_v<DstChar> == 1) {
            if (cp < 0x80u) {
                *out++ = static_cast<DstChar>(cp);
            } else if (cp < 0x800u) {
                *out++ = static_cast<DstChar>(0xC0u | (cp >> 6u));
                *out++ = static_cast<DstChar>(0x80u | (cp & 0x3Fu));
            } else if (cp < 0x10000u) {
                *out++ = static_cast<DstChar>(0xE0u | (cp >> 12u));
                *out++ = static_cast<DstChar>(0x80u | ((cp >> 6u) & 0x3Fu));
                *out++ = static_cast<DstChar>(0x80u | (cp & 0x3Fu));
            } else {
                *out++ = static_cast<DstChar>(0xF0u | (cp >> 18u));
                *out++ = static_cast<DstChar>(0x80u | ((cp >> 12u) & 0x3Fu));
                *out++ = static_cast<DstChar>(0x80u | ((cp >> 6u) & 0x3Fu));
                *out++ = static_cast<DstChar>(0x80u | (cp & 0x3Fu));
            }
        } else if constexpr (utf_width_v<DstChar> == 2) {
            if (cp < 0x10000u) {
                *out++ = static_cast<DstChar>(cp);
            } else {
                const char32_t v = cp - 0x10000u;
                *out++ = static_cast<DstChar>(0xD800u | (v >> 10u));
                *out++ = static_cast<DstChar>(0xDC00u | (v & 0x3FFu));
            }
        } else {
            *out++ = static_cast<DstChar>(cp);
        }
        return out;
    }

    template <SupportedChar DstChar, SupportedChar SrcChar>
    [[nodiscard]] constexpr size_t converted_length(std::basic_string_view<SrcChar> sv) noexcept {
        if constexpr (utf_width_v<SrcChar> == utf_width_v<DstChar>) {
            return sv.size();
        } else {
            size_t out = 0;
            size_t i = 0;
            const size_t n = sv.size();
            while (i < n) {
                if (to_unsigned_unit(sv[i]) < 0x80u) {
                    ++out;
                    ++i;
                    continue;
                }
                out += encoded_units<DstChar>(decode_next(sv, i));
            }
            return out;
        }
    }

    template <SupportedChar DstChar, SupportedChar SrcChar>
    [[nodiscard]] constexpr DstChar* write_converted(DstChar* cur, std::basic_string_view<SrcChar> sv) noexcept {
        if (sv.empty()) return cur;
        if constexpr (std::same_as<SrcChar, DstChar>) {
            std::char_traits<DstChar>::copy(cur, sv.data(), sv.size());
            return cur + sv.size();
        } else if constexpr (utf_width_v<SrcChar> == utf_width_v<DstChar>) {
            #if USYLIBPP_CPLUSPLUS >= 202302L
            if consteval
            #else
            if (std::is_constant_evaluated())
            #endif
            {
                for (const SrcChar c : sv) *cur++ = static_cast<DstChar>(c);
            } else {
                std::memcpy(cur, sv.data(), sv.size() * sizeof(DstChar));
                cur += sv.size();
            }
            return cur;
        } else {
            size_t i = 0;
            const size_t n = sv.size();
            while (i < n) {
                const auto u = to_unsigned_unit(sv[i]);
                if (u < 0x80u) {
                    *cur++ = static_cast<DstChar>(u);
                    ++i;
                    continue;
                }
                cur = encode_unit<DstChar>(cur, decode_next(sv, i));
            }
            return cur;
        }
    }

    template <SupportedChar DstChar, SupportedChar SrcChar>
    [[nodiscard]] constexpr size_t write_converted_bounded(
        DstChar* cur, size_t max_units, std::basic_string_view<SrcChar> sv
    ) noexcept {
        if (max_units == 0 || sv.empty()) return 0;
        if constexpr (std::same_as<SrcChar, DstChar>) {
            return sv.copy(cur, max_units);
        } else if constexpr (utf_width_v<SrcChar> == utf_width_v<DstChar>) {
            const size_t n = std::min(sv.size(), max_units);
            #if USYLIBPP_CPLUSPLUS >= 202302L
            if consteval
            #else
            if (std::is_constant_evaluated())
            #endif
            {
                for (size_t i = 0; i < n; ++i) cur[i] = static_cast<DstChar>(sv[i]);
            } else {
                std::memcpy(cur, sv.data(), n * sizeof(DstChar));
            }
            return n;
        } else {
            DstChar* const start = cur;
            size_t rem = max_units;
            size_t i = 0;
            const size_t n = sv.size();
            while (i < n && rem > 0) {
                const auto u = to_unsigned_unit(sv[i]);
                if (u < 0x80u) {
                    *cur++ = static_cast<DstChar>(u);
                    --rem;
                    ++i;
                    continue;
                }
                const char32_t cp = decode_next(sv, i);
                const size_t need = encoded_units<DstChar>(cp);
                if (need > rem) break;
                cur = encode_unit<DstChar>(cur, cp);
                rem -= need;
            }
            return static_cast<size_t>(cur - start);
        }
    }

    template <bool Overwrite, SupportedChar Char, StringPart... Ts>
    constexpr void write_to_string(std::basic_string<Char>& dst, const Ts&... parts) {
        if constexpr (sizeof...(Ts) == 0) {
            if constexpr (Overwrite) dst.clear();
            return;
        } else {
            const auto views = std::tuple{to_part_view(parts)...};
            size_t add = 0;
            [&]<size_t... Is>(std::index_sequence<Is...>) noexcept {
                ((add += converted_length<Char>(std::get<Is>(views))), ...);
            }(std::index_sequence_for<Ts...>{});

            if (add == 0) {
                if constexpr (Overwrite) dst.clear();
                return;
            }

            const size_t old = dst.size();
            const size_t base = Overwrite ? 0u : old;

            #if USYLIBPP_CPLUSPLUS >= 202302L
            if consteval
            #else
            if (std::is_constant_evaluated())
            #endif
            {
                std::basic_string<Char> tmp;
                tmp.reserve(base + add);
                if constexpr (!Overwrite) tmp.append(dst);
                tmp.resize(base + add);
                Char* cur = tmp.data() + base;
                [&]<size_t... Is>(std::index_sequence<Is...>) noexcept {
                    ((cur = write_converted<Char>(cur, std::get<Is>(views))), ...);
                }(std::index_sequence_for<Ts...>{});
                dst = std::move(tmp);
                return;
            }

            // Snapshot same-char views that alias `dst` before resize may reallocate `dst`'s buffer.
            const Char* const old_begin = dst.data();
            const Char* const old_end = old_begin + old;
            std::array<bool, sizeof...(Ts)> self_alias{};
            std::array<size_t, sizeof...(Ts)> self_off{};
            bool any_self_alias = false;
            [&]<size_t... Is>(std::index_sequence<Is...>) noexcept {
                ([&] {
                    const auto sv = std::get<Is>(views);
                    using SrcChar = typename decltype(sv)::value_type;
                    if constexpr (std::same_as<SrcChar, Char>) {
                        const Char* const p = sv.data();
                        if (!sv.empty() && !std::less<const Char*>{}(p, old_begin) && std::less<const Char*>{}(p, old_end)) {
                            self_alias[Is] = true;
                            self_off[Is] = static_cast<size_t>(p - old_begin);
                            any_self_alias = true;
                        }
                    }
                }(), ...);
            }(std::index_sequence_for<Ts...>{});

            if constexpr (Overwrite) {
                if (any_self_alias) {
                    std::basic_string<Char> tmp;
                    #if USYLIBPP_CPLUSPLUS >= 202302L
                    tmp.resize_and_overwrite(add, [&](Char* p, size_t) noexcept {
                        Char* cur = p;
                        [&]<size_t... Is>(std::index_sequence<Is...>) noexcept {
                            ((cur = write_converted<Char>(cur, std::get<Is>(views))), ...);
                        }(std::index_sequence_for<Ts...>{});
                        return add;
                    });
                    #else
                    tmp.resize(add);
                    Char* cur = tmp.data();
                    [&]<size_t... Is>(std::index_sequence<Is...>) noexcept {
                        ((cur = write_converted<Char>(cur, std::get<Is>(views))), ...);
                    }(std::index_sequence_for<Ts...>{});
                    #endif
                    dst = std::move(tmp);
                    return;
                }
            }

            auto fill_dst = [&](Char* p) noexcept {
                Char* cur = p + base;
                [&]<size_t... Is>(std::index_sequence<Is...>) noexcept {
                    ([&] {
                        const auto sv = std::get<Is>(views);
                        if (sv.empty()) return;
                        using SrcChar = typename decltype(sv)::value_type;
                        if constexpr (std::same_as<SrcChar, Char>) {
                            const Char* const src = (!Overwrite && self_alias[Is]) ? (p + self_off[Is]) : sv.data();
                            std::char_traits<Char>::copy(cur, src, sv.size());
                            cur += sv.size();
                        } else {
                            cur = write_converted<Char>(cur, sv);
                        }
                    }(), ...);
                }(std::index_sequence_for<Ts...>{});
            };

            #if USYLIBPP_CPLUSPLUS >= 202302L
            dst.resize_and_overwrite(base + add, [&](Char* p, size_t) noexcept {
                fill_dst(p);
                return base + add;
            });
            #else
            dst.resize(base + add);
            fill_dst(dst.data());
            #endif
        }
    }

    } // namespace detail

    /// Fixed-capacity stack string with implicit `std::basic_string_view` conversion and zero heap allocations.
    template <typename Char, size_t N>
    struct FixedBasicString {
        static_assert(N > 0);
        std::array<Char, N> buf; // NOLINT(cppcoreguidelines-pro-type-member-init)
        size_t len = 0;

        constexpr FixedBasicString() noexcept {
            #if USYLIBPP_CPLUSPLUS >= 202302L
            if consteval { buf.fill(Char{}); }
            #else
            if (std::is_constant_evaluated()) { buf.fill(Char{}); }
            #endif
            buf[0] = Char{};
        }
        constexpr FixedBasicString(const FixedBasicString& other) noexcept {
            #if USYLIBPP_CPLUSPLUS >= 202302L
            if consteval { buf.fill(Char{}); }
            #else
            if (std::is_constant_evaluated()) { buf.fill(Char{}); }
            #endif
            assign(other.view());
        }
        constexpr FixedBasicString& operator=(const FixedBasicString& other) noexcept {
            if (this != &other) assign(other.view());
            return *this;
        }
        constexpr FixedBasicString(FixedBasicString&& other) noexcept {
            #if USYLIBPP_CPLUSPLUS >= 202302L
            if consteval { buf.fill(Char{}); }
            #else
            if (std::is_constant_evaluated()) { buf.fill(Char{}); }
            #endif
            assign(other.view());
        }
        constexpr FixedBasicString& operator=(FixedBasicString&& other) noexcept {
            if (this != &other) assign(other.view());
            return *this;
        }
        constexpr ~FixedBasicString() noexcept = default;
        constexpr FixedBasicString(std::basic_string_view<Char> sv) noexcept {
            #if USYLIBPP_CPLUSPLUS >= 202302L
            if consteval { buf.fill(Char{}); }
            #else
            if (std::is_constant_evaluated()) { buf.fill(Char{}); }
            #endif
            assign(sv);
        }
        constexpr FixedBasicString(const Char* s) noexcept
            : FixedBasicString(s ? std::basic_string_view<Char>{s} : std::basic_string_view<Char>{}) {}
        template <size_t M>
        constexpr FixedBasicString(const FixedBasicString<Char, M>& other) noexcept {
            #if USYLIBPP_CPLUSPLUS >= 202302L
            if consteval { buf.fill(Char{}); }
            #else
            if (std::is_constant_evaluated()) { buf.fill(Char{}); }
            #endif
            assign(other.view());
        }

        constexpr void clear() noexcept {
            len = 0;
            buf[0] = Char{};
        }

        constexpr void assign(std::basic_string_view<Char> sv) noexcept {
            const size_t n = std::min(sv.size(), N - 1);
            for (size_t i = 0; i < n; ++i) buf[i] = sv[i];
            buf[n] = Char{};
            len = n;
        }

        template <detail::StringPart... Ts>
            requires(detail::SupportedChar<Char> && sizeof...(Ts) > 0)
        constexpr void assign(const Ts&... parts) noexcept {
            const auto views = std::tuple{detail::to_part_view(parts)...};
            #if USYLIBPP_CPLUSPLUS >= 202302L
            if consteval
            #else
            if (std::is_constant_evaluated())
            #endif
            {
                FixedBasicString tmp;
                tmp.append_views(views);
                *this = std::move(tmp);
                return;
            }
            const Char* const old_begin = buf.data();
            const Char* const old_end = old_begin + len;
            bool any_self_alias = false;
            [&]<size_t... Is>(std::index_sequence<Is...>) noexcept {
                ([&] {
                    const auto sv = std::get<Is>(views);
                    using SrcChar = typename decltype(sv)::value_type;
                    if constexpr (std::same_as<SrcChar, Char>) {
                        const Char* const p = sv.data();
                        if (!sv.empty() && !std::less<const Char*>{}(p, old_begin) && std::less<const Char*>{}(p, old_end)) {
                            any_self_alias = true;
                        }
                    }
                }(), ...);
            }(std::index_sequence_for<Ts...>{});
            if (any_self_alias) {
                FixedBasicString tmp;
                tmp.append_views(views);
                *this = std::move(tmp);
                return;
            }
            len = 0;
            buf[0] = Char{};
            append_views(views);
        }

        constexpr void push_back(Char c) noexcept {
            if (len + 1 < N) {
                buf[len++] = c;
                buf[len] = Char{};
            }
        }

        constexpr void append(std::basic_string_view<Char> sv) noexcept {
            if (len >= N - 1) return;
            const size_t room = (N - 1) - len;
            const size_t n = std::min(sv.size(), room);
            for (size_t i = 0; i < n; ++i) buf[len + i] = sv[i];
            len += n;
            buf[len] = Char{};
        }

        template <detail::StringPart... Ts>
            requires(detail::SupportedChar<Char> && sizeof...(Ts) > 0)
        constexpr void append(const Ts&... parts) noexcept {
            append_views(std::tuple{detail::to_part_view(parts)...});
        }

        [[nodiscard]] constexpr const Char* data() const noexcept { return buf.data(); }
        [[nodiscard]] constexpr Char* data() noexcept { return buf.data(); }
        [[nodiscard]] constexpr const Char* c_str() const noexcept { return buf.data(); }
        [[nodiscard]] constexpr size_t size() const noexcept { return len; }
        [[nodiscard]] constexpr size_t length() const noexcept { return len; }
        [[nodiscard]] static constexpr size_t capacity() noexcept { return N - 1; }
        [[nodiscard]] constexpr bool empty() const noexcept { return len == 0; }
        [[nodiscard]] constexpr const Char* begin() const noexcept { return buf.data(); }
        [[nodiscard]] constexpr const Char* end() const noexcept { return buf.data() + len; }
        [[nodiscard]] constexpr Char operator[](size_t i) const noexcept { return buf[i]; }
        [[nodiscard]] constexpr std::basic_string_view<Char> view() const noexcept { return {buf.data(), len}; }
        [[nodiscard]] constexpr operator std::basic_string_view<Char>() const noexcept { return view(); }
        [[nodiscard]] std::basic_string<Char> str() const { return std::basic_string<Char>{buf.data(), len}; }

        friend constexpr bool operator==(const FixedBasicString& a, std::basic_string_view<Char> b) noexcept {
            return a.view() == b;
        }
        friend constexpr auto operator<=>(const FixedBasicString& a, std::basic_string_view<Char> b) noexcept {
            return a.view() <=> b;
        }

    private:
        template <typename ViewsTuple>
        constexpr void append_views(const ViewsTuple& views) noexcept {
            [&]<size_t... Is>(std::index_sequence<Is...>) noexcept {
                ([&] {
                    const size_t room = (N - 1) - len;
                    if (room == 0) return;
                    const auto sv = std::get<Is>(views);
                    len += detail::write_converted_bounded<Char>(buf.data() + len, room, sv);
                }(), ...);
            }(std::make_index_sequence<std::tuple_size_v<ViewsTuple>>{});
            buf[len] = Char{};
        }
    };

    template <size_t N>
    using FixedString = FixedBasicString<char, N>;

    template <size_t N>
    using FixedWString = FixedBasicString<wchar_t, N>;

    template <typename Char, size_t N, size_t M>
    [[nodiscard]] constexpr bool operator==(const FixedBasicString<Char, N>& a,
                                            const FixedBasicString<Char, M>& b) noexcept {
        return a.view() == b.view();
    }

    template <typename Char, size_t N, size_t M>
    [[nodiscard]] constexpr auto operator<=>(const FixedBasicString<Char, N>& a,
                                             const FixedBasicString<Char, M>& b) noexcept {
        return a.view() <=> b.view();
    }

    /// Appends `parts...` (any mix of narrow, wide, UTF-8/16/32, FixedBasicString, C string, char, or fs::path)
    /// onto `dst` in one resize, converting encodings directly without intermediate allocations.
    template <detail::SupportedChar Char, detail::StringPart... Ts>
    constexpr void append_to(std::basic_string<Char>& dst, const Ts&... parts) {
        detail::write_to_string<false>(dst, parts...);
    }

    /// Overwrites `dst` with `parts...` in one resize (reusing `dst`'s capacity when possible),
    /// converting encodings directly without intermediate allocations.
    template <detail::SupportedChar Char, detail::StringPart... Ts>
    constexpr void overwrite_to(std::basic_string<Char>& dst, const Ts&... parts) {
        detail::write_to_string<true>(dst, parts...);
    }

    /// Appends `parts...` directly into a fixed-capacity stack string with codepoint-safe truncation.
    template <detail::SupportedChar Char, size_t N, detail::StringPart... Ts>
    constexpr void append_to(FixedBasicString<Char, N>& dst, const Ts&... parts) noexcept {
        if constexpr (sizeof...(Ts) > 0) dst.append(parts...);
    }

    /// Overwrites a fixed-capacity stack string with `parts...` with codepoint-safe truncation.
    template <detail::SupportedChar Char, size_t N, detail::StringPart... Ts>
    constexpr void overwrite_to(FixedBasicString<Char, N>& dst, const Ts&... parts) noexcept {
        if constexpr (sizeof...(Ts) == 0) {
            dst.clear();
        } else {
            dst.assign(parts...);
        }
    }

    /// Constructs a `std::basic_string<Char>` from any mix of string types in a single allocation.
    template <detail::SupportedChar Char, detail::StringPart... Ts>
    [[nodiscard]] constexpr std::basic_string<Char> concat_to(const Ts&... parts) {
        std::basic_string<Char> out;
        detail::write_to_string<false>(out, parts...);
        return out;
    }

    /// Constructs a UTF-8 `std::string` from any mix of string types in a single allocation.
    template <detail::StringPart... Ts>
    [[nodiscard]] constexpr std::string concat_utf8(const Ts&... parts) {
        return concat_to<char>(parts...);
    }

    /// Constructs a wide `std::wstring` from any mix of string types in a single allocation.
    template <detail::StringPart... Ts>
    [[nodiscard]] constexpr std::wstring concat_wide(const Ts&... parts) {
        return concat_to<wchar_t>(parts...);
    }

    [[nodiscard]] constexpr std::string to_utf8(std::wstring_view w) {
        return concat_utf8(w);
    }

    [[nodiscard]] constexpr std::wstring to_wide(std::string_view s) {
        return concat_wide(s);
    }

    template <detail::SupportedChar Char, size_t N = 32, typename T>
        requires(std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
    [[nodiscard]] inline FixedBasicString<Char, N> to_fixed_string(T val) noexcept {
        if constexpr (std::same_as<Char, char>) {
            FixedBasicString<char, N> out;
            const auto [ptr, ec] = std::to_chars(out.buf.data(), out.buf.data() + out.buf.size() - 1, val);
            out.len = (ec == std::errc{}) ? static_cast<size_t>(ptr - out.buf.data()) : 0u;
            out.buf[out.len] = '\0';
            return out;
        } else {
            std::array<char, N> tmp; // NOLINT(cppcoreguidelines-pro-type-member-init)
            const auto [ptr, ec] = std::to_chars(tmp.data(), tmp.data() + tmp.size() - 1, val);
            FixedBasicString<Char, N> out;
            if (ec == std::errc{}) {
                const auto n = static_cast<size_t>(ptr - tmp.data());
                for (size_t i = 0; i < n; ++i) out.buf[i] = static_cast<Char>(static_cast<unsigned char>(tmp[i]));
                out.len = n;
                out.buf[n] = Char{};
            }
            return out;
        }
    }

    template <detail::SupportedChar Char, size_t N = 48, std::floating_point T>
    [[nodiscard]] inline FixedBasicString<Char, N> to_fixed_string(T val, int precision) noexcept {
        if constexpr (std::same_as<Char, char>) {
            FixedBasicString<char, N> out;
            const auto [ptr, ec] = std::to_chars(out.buf.data(), out.buf.data() + out.buf.size() - 1, val, std::chars_format::fixed, precision);
            out.len = (ec == std::errc{}) ? static_cast<size_t>(ptr - out.buf.data()) : 0u;
            out.buf[out.len] = '\0';
            return out;
        } else {
            std::array<char, N> tmp; // NOLINT(cppcoreguidelines-pro-type-member-init)
            const auto [ptr, ec] = std::to_chars(tmp.data(), tmp.data() + tmp.size() - 1, val, std::chars_format::fixed, precision);
            FixedBasicString<Char, N> out;
            if (ec == std::errc{}) {
                const auto n = static_cast<size_t>(ptr - tmp.data());
                for (size_t i = 0; i < n; ++i) out.buf[i] = static_cast<Char>(static_cast<unsigned char>(tmp[i]));
                out.len = n;
                out.buf[n] = Char{};
            }
            return out;
        }
    }

    template <size_t N = 32, typename T>
        requires(std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
    [[nodiscard]] inline FixedString<N> to_fixed_str(T val) noexcept {
        return to_fixed_string<char, N>(val);
    }

    template <size_t N = 48, std::floating_point T>
    [[nodiscard]] inline FixedString<N> to_fixed_str(T val, int precision) noexcept {
        return to_fixed_string<char, N>(val, precision);
    }

    template <size_t N = 32, typename T>
        requires(std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
    [[nodiscard]] inline FixedWString<N> to_fixed_wstr(T val) noexcept {
        return to_fixed_string<wchar_t, N>(val);
    }

    template <size_t N = 48, std::floating_point T>
    [[nodiscard]] inline FixedWString<N> to_fixed_wstr(T val, int precision) noexcept {
        return to_fixed_string<wchar_t, N>(val, precision);
    }

}
