#pragma once

#include <iostream>
#include <format>

namespace usylibpp::print {
    template<typename Fmt = const char*, typename... Ts>
    inline void println(Fmt&& fmt = "", Ts&&... args) {
        using Char = std::remove_cv_t<std::remove_reference_t<decltype(fmt[0])>>;

        if constexpr (std::is_same_v<Char, char>) {
            std::cout << std::vformat(std::forward<Fmt>(fmt), std::make_format_args(args...)) <<'\n';
        } else if constexpr (std::is_same_v<Char, wchar_t>) {
            std::wcout << std::vformat(std::forward<Fmt>(fmt), std::make_wformat_args(args...)) << L'\n';
        } else {
            static_assert(!std::is_same_v<Char, Char>, "Unsupported type passed to usylibpp::print::println");
        }
    }
}