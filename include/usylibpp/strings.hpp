#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <cstddef>
#include <cstring>

namespace usylibpp::strings {
    template<typename T>
    concept StringLike = requires(T a) {
        { std::string_view(a) };
    };

    inline constexpr size_t total_string_size() noexcept { 
        return 0; 
    }

    template<StringLike T, StringLike... Ts>
    inline constexpr size_t total_string_size(const T& first, const Ts&... rest) noexcept {
        return std::string_view(first).size() + total_string_size(rest...);
    }

    template<StringLike... Ts>
    inline constexpr std::string concat_strings(const Ts&... parts) {
        std::string result;
        result.resize(total_string_size(parts...));

        char* dest = result.data();
        std::string_view sv;
        ((sv = std::string_view(parts), std::memcpy(dest, sv.data(), sv.size()), dest += sv.size()), ...);

        return result;
    }

    inline constexpr void to_lower_case(std::string& str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) -> char {
            return static_cast<char>(std::tolower(c));
        });
    }
}