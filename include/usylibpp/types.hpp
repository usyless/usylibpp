#pragma once

#include <string_view>
#include <string>
#include <filesystem>

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

    /**
     * Done like this to prevent something like 128 bit unsigned integers if they happen to exist
     */
    template <typename T>
    concept UnsignedInteger =
        std::same_as<std::remove_cvref_t<T>, unsigned char> || 
        std::same_as<std::remove_cvref_t<T>, unsigned short> || 
        std::same_as<std::remove_cvref_t<T>, unsigned int> || 
        std::same_as<std::remove_cvref_t<T>, unsigned long> || 
        std::same_as<std::remove_cvref_t<T>, unsigned long long>;
}