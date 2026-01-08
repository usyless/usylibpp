#pragma once

#include <string_view>
#include <string>
#include <filesystem>

namespace usylibpp::types {
    template<typename T, typename Char>
    concept StringLike = requires(T a) {
        { std::basic_string_view<Char>(a) };
    };

    template<typename T>
    concept wchar_ptr = std::is_same<std::decay_t<T>, wchar_t*>::value || std::is_same<std::decay_t<T>, const wchar_t*>::value;

    template<typename T>
    concept wstring = std::is_same<std::decay_t<T>, std::wstring>::value;

    template<typename T>
    concept string = std::is_same<std::decay_t<T>, std::string>::value;

    template<typename T>
    concept is_filesystem_path = std::is_same<std::decay_t<T>, std::filesystem::path>::value;

    template <typename T>
    concept wchar_t_strict = wchar_ptr<T> || wstring<T>;

    template <typename T>
    concept wchar_t_compatible = wchar_t_strict<T> || string<T> || is_filesystem_path<T>;
}