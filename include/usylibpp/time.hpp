#pragma once

#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace usylibpp::time {
    /**
     * Thread safe
     */
    inline tm tm_safe(time_t now_time) {
        tm cur_tm{};
        #ifdef WIN32
        localtime_s(&cur_tm, &now_time);
        #else
        localtime_r(&now_time, &cur_tm);
        #endif
        return cur_tm;
    }

    /**
     * Thread safe
     */
    inline tm current_tm_safe() {
        return tm_safe(std::time(nullptr));
    }

    inline std::string current_datetime() {
        std::stringstream ss;
        
        auto cur_tm = current_tm_safe();
        ss << std::put_time(&cur_tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
}