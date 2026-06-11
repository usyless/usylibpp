#pragma once

#include "aliases.hpp" // IWYU pragma: exports
#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) >= 202302L && __has_include(<print>)
#include <print>

namespace usylibpp::print {
    using std::println;
    using std::print;
}
#elif __has_include(<format>)
#include <iostream>
#include <format>

namespace usylibpp::print {
    template<typename... Ts>
    inline auto& print(const std::format_string<Ts...>& fmt, Ts&&... args) {
        std::cout << std::vformat(fmt.get(), std::make_format_args(args...));
        return std::cout;
    }

    template<typename... Ts>
    inline auto& print(const std::wformat_string<Ts...>& fmt, Ts&&... args) {
        std::wcout << std::vformat(fmt.get(), std::make_wformat_args(args...));
        return std::wcout;
    }

    template<typename... Ts>
    inline void println(const std::format_string<Ts...>& fmt, Ts&&... args) {
        usylibpp::print::print(fmt, std::forward<Ts>(args)...) << '\n';
    }

    template<typename... Ts>
    inline void println(const std::wformat_string<Ts...>& fmt, Ts&&... args) {
        usylibpp::print::print(fmt, std::forward<Ts>(args)...) << L'\n';
    }

    inline void println() {
        std::cout << '\n';
    }
}
#endif