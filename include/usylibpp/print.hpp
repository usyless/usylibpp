#pragma once

#include <iostream>
#include <format>

namespace usylibpp::print {
    /**
     * Will fail silently on mismatched paranthesis and args
     * Prints to cout or wcout depending on the args passed in
     */
    template<typename Fmt = const char*, typename... Ts>
    inline void print(Fmt&& fmt = "", Ts&&... args) {
        // using Char = typename std::basic_string_view<std::remove_cvref_t<decltype(fmt[0])>>::value_type;
        using Char = std::remove_cvref_t<decltype(fmt[0])>;

        if constexpr (std::is_same_v<Char, char>) {
            std::cout << std::vformat(std::forward<Fmt>(fmt), std::make_format_args(args...));
        } else if constexpr (std::is_same_v<Char, wchar_t>) {
            std::wcout << std::vformat(std::forward<Fmt>(fmt), std::make_wformat_args(args...));
        } else {
            static_assert(!std::is_same_v<Char, Char>, "Unsupported type passed to usylibpp::print::print");
        }
    }

    /**
     * Will fail silently on mismatched paranthesis and args
     * Prints to cout or wcout depending on the args passed in
     */
    template<typename Fmt = const char*, typename... Ts>
    inline void println(Fmt&& fmt = "", Ts&&... args) {
        using Char = std::remove_cvref_t<decltype(fmt[0])>;

        print(fmt, args..., Char('\n'));
    }
}