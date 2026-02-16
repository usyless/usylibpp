#pragma once

#include "aliases.hpp" // IWYU pragma: exports
#include "types.hpp"
#include <iostream>
#include <format>

namespace usylibpp::print {
    /**
     * Will fail silently on mismatched paranthesis and args
     * Prints to cout or wcout depending on the args passed in
     */
    template<types::is_basic_string_view Fmt, typename... Ts>
    inline auto& print(Fmt&& fmt = "", Ts&&... args) {
        // using Char = typename std::basic_string_view<std::remove_cvref_t<decltype(fmt[0])>>::value_type;
        using Char = types::string_view_char_t<Fmt>;

        if constexpr (std::is_same_v<Char, char>) {
            std::cout << std::vformat(std::forward<Fmt>(fmt), std::make_format_args(args...));
            return std::cout;
        } else if constexpr (std::is_same_v<Char, wchar_t>) {
            std::wcout << std::vformat(std::forward<Fmt>(fmt), std::make_wformat_args(args...));
            return std::wcout;
        } else {
            static_assert(!std::is_same_v<Char, Char>, "Unsupported type passed to usylibpp::print::print");
        }
    }

    /**
     * Will fail silently on mismatched paranthesis and args
     * Prints to cout or wcout depending on the args passed in
     */
    template<types::is_basic_string_view Fmt = decltype(""), typename... Ts>
    inline void println(Fmt&& fmt = "", Ts&&... args) {
        using Char = types::string_view_char_t<Fmt>;

        print(std::forward<Fmt>(fmt), std::forward<Ts>(args)...) << Char('\n');
    }
}