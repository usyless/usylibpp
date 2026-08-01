#pragma once

#include "../aliases.hpp" // IWYU pragma: export
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <functional>

namespace usylibpp::util {

/**
 * Pass false to fixed_rate to use fixed delay
 */
struct CancellableIntervalOptions {
    bool fixed_rate = true;
    bool fire_immediately = true;
    bool ignore_callback_exceptions = true;
};
template <CancellableIntervalOptions options = {}>
struct CancellableInterval {
    std::jthread thread;

    template <std::invocable T, class Rep, class Period>
    CancellableInterval(T&& cb, std::chrono::duration<Rep, Period> duration) : thread{[cb = std::forward<T>(cb), duration = std::move(duration)](std::stop_token stoken) {
        std::condition_variable_any cv;
        std::mutex m;
        std::unique_lock lock{m};

        std::stop_callback stop_cb{stoken, [&cv, &m]{
            { std::lock_guard guard{m}; }
            cv.notify_all();
        }};

        if constexpr (!options.fire_immediately) {
            cv.wait_for(lock, duration, [&stoken]{ return stoken.stop_requested(); });
        }

        while (!stoken.stop_requested()) {
            if constexpr (options.fixed_rate) {
                const auto wait_until = std::chrono::steady_clock::now() + duration;
                if constexpr (options.ignore_callback_exceptions) {
                    try { std::invoke(cb); } catch (...) {}
                } else {
                    std::invoke(cb);
                }
                if (std::chrono::steady_clock::now() > wait_until) continue;
                cv.wait_until(lock, wait_until, [&stoken]{ return stoken.stop_requested(); });
            } else {
                if constexpr (options.ignore_callback_exceptions) {
                    try { std::invoke(cb); } catch (...) {}
                } else {
                    std::invoke(cb);
                }
                cv.wait_for(lock, duration, [&stoken]{ return stoken.stop_requested(); });
            }
        }
    }} {}
};
}