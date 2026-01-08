#pragma once

#include <iostream>
#include <format>

namespace usylibpp::print {
    template<typename Fmt, typename... Ts>
    inline void println(Fmt&& fmt, Ts&&... args) {
        using Char = std::remove_cv_t<std::remove_reference_t<decltype(fmt[0])>>;

        []() -> std::basic_ostream<Char>& { 
            if constexpr (std::is_same_v<Char, char>) return std::cout; 
            else return std::wcout; 
        }() << std::vformat(std::forward<Fmt>(fmt), std::make_format_args(args...)) << Char('\n');
    }
}