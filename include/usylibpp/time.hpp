#pragma once

#include "aliases.hpp" // IWYU pragma: export
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace usylibpp::time {
    /**
     * Thread safe
     */
    [[nodiscard]] inline ::tm tm_safe(::time_t time = std::time(nullptr)) noexcept {
        ::tm cur_tm{};
        #if defined(_WIN32)
        localtime_s(&cur_tm, &time);
        #else
        localtime_r(&time, &cur_tm);
        #endif
        return cur_tm;
    }

    [[nodiscard]] inline auto datetime_stream(const ::tm& tm, const char* fmt = "%Y-%m-%d %H:%M:%S") {
        return std::put_time(&tm, fmt);
    }

    inline auto datetime_stream(const ::tm&&, const char* = nullptr) = delete;

    [[nodiscard]] inline std::string datetime_string(::time_t time = std::time(nullptr), const char* fmt = "%Y-%m-%d %H:%M:%S") {
        std::stringstream ss;
        const auto tm = tm_safe(time);
        ss << datetime_stream(tm, fmt);
        return ss.str();
    }
}