#pragma once

#include "aliases.hpp" // IWYU pragma: exports
#if __cplusplus >= 202302L && __has_include(<print>)
#include <print>
#else
#include <iostream>
#include <format>
#endif

namespace usylibpp::print {
    #if __cplusplus >= 202302L
    using std::println;
    using std::print;
    #else
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
        print(fmt, std::forward<Ts>(args)...) << '\n';
    }

    template<typename... Ts>
    inline void println(const std::wformat_string<Ts...>& fmt, Ts&&... args) {
        print(fmt, std::forward<Ts>(args)...) << L'\n';
    }

    inline void println() {
        std::cout << '\n';
    }
    #endif
}