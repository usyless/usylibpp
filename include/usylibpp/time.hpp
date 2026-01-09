#pragma once

#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace usylibpp::time {
    /**
     * Thread safe
     */
    inline tm tm_safe(time_t time = std::time(nullptr)) {
        tm cur_tm{};
        #ifdef WIN32
        localtime_s(&cur_tm, &time);
        #else
        localtime_r(&now_time, &cur_tm);
        #endif
        return cur_tm;
    }

    inline auto datetime_stream(const tm& tm) {
        return std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    }

    inline auto datetime_stream(time_t time = std::time(nullptr)) {
        return datetime_stream(tm_safe(time));
    }

    inline std::string datetime_string(time_t time = std::time(nullptr)) {
        std::stringstream ss;
        ss << datetime_stream(time);
        return ss.str();
    }
}