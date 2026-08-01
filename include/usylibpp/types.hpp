#pragma once

#include "aliases.hpp" // IWYU pragma: export
#include <string_view>
#include <string>
#include <filesystem>
#include <type_traits>
#include <concepts>

namespace usylibpp::types {
    template<typename T, typename Char>
    concept StringLike = requires(T a) {
        { std::basic_string_view<Char>(a) };
    };

    template<class T>
    concept wchar_ptr = std::is_pointer_v<std::decay_t<T>> && std::is_same_v<std::remove_cvref_t<std::remove_pointer_t<std::decay_t<T>>>, wchar_t>;

    template<typename T>
    concept wstring = std::is_same_v<std::decay_t<T>, std::wstring>;

    template<typename T>
    concept string = std::is_same_v<std::decay_t<T>, std::string> || (std::is_pointer_v<std::decay_t<T>> && std::is_same_v<std::remove_cvref_t<std::remove_pointer_t<std::decay_t<T>>>, char>);

    template<typename T>
    concept filesystem_path = std::is_same_v<std::decay_t<T>, std::filesystem::path>;

    template <typename T>
    concept wchar_t_strict = wchar_ptr<T> || wstring<T>;

    template <typename T>
    concept wchar_t_compatible = wchar_t_strict<T> || string<T> || filesystem_path<T>;
    
    template <typename T>
    concept Numeric = std::is_arithmetic_v<T>;

    template <typename T>
    concept CharOrWChar = std::is_same_v<T, char> || std::is_same_v<T, wchar_t>;

    template <typename S>
    concept is_basic_string =
        requires { typename std::remove_cvref_t<S>::value_type; } &&
        std::same_as<std::remove_cvref_t<S>, std::basic_string<typename std::remove_cvref_t<S>::value_type>>;
    
    template <typename S>
    struct string_view_char;

    template <typename S>
        requires std::is_convertible_v<S, std::basic_string_view<char>>
    struct string_view_char<S> {
        using type = char;
    };

    template <typename S>
        requires std::is_convertible_v<S, std::basic_string_view<wchar_t>>
    struct string_view_char<S> {
        using type = wchar_t;
    };

    template <typename S>
    using string_view_char_t = typename string_view_char<S>::type;

    template <typename S>
    concept is_basic_string_view = requires { typename string_view_char<S>::type; };

    template <typename SV>
    concept owning_rvalue_string =
        std::is_same_v<SV, std::remove_cvref_t<SV>> && is_basic_string<SV>;

    struct noop_t {
        template <typename... Args>
        void operator()(Args&&...) const noexcept {}
    };
}