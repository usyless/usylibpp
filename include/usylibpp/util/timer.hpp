#pragma once

#include "../aliases.hpp" // IWYU pragma: export
#include <chrono>
#include "../print.hpp"

namespace usylibpp::util {
class Timer {
private:
    const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    bool ended{false};
public:
    inline void end() noexcept {
        ended = true;
        const auto end = std::chrono::high_resolution_clock::now();
        const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        const std::chrono::duration<double> duration = end - start;
        print::println("Time taken: {}, {}, ({:.9f} seconds)", duration_ms, duration_us, duration.count());
    }

    ~Timer() {
        if (!ended) end();
    }
};
}