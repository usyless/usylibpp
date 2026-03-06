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
        #ifdef WIN32
        localtime_s(&cur_tm, &time);
        #else
        localtime_r(&time, &cur_tm);
        #endif
        return cur_tm;
    }

    [[nodiscard]] inline auto datetime_stream(const ::tm& tm) {
        return std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    }
    inline auto datetime_stream(const ::tm&&) = delete;

    [[nodiscard]] inline std::string datetime_string(::time_t time = std::time(nullptr)) {
        std::stringstream ss;
        const auto tm = tm_safe(time);
        ss << datetime_stream(tm);
        return ss.str();
    }
}