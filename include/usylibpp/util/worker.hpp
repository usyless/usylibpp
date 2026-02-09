#pragma once

#include <thread>
#include <atomic>
#include <future>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace usylibpp::util {
    enum class WorkerType {
        ReturnDefault,
        ThrowError
    };

    struct WorkerOpts {
        bool drain_queue_on_cancel{true};
        WorkerType type{WorkerType::ReturnDefault};
    };

    /**
     * Basic worker in its own thread
     */
    template <WorkerOpts opts>
    class Worker {
    private:
        struct CallBase {
            virtual ~CallBase() = default;
            virtual void execute() = 0;
        };

        template<typename Fn, typename Ret>
        struct Call : CallBase {
            Fn fn;
            std::promise<Ret> result;

            Call(Fn&& f) : fn(std::move(f)) {}

            void execute() override final {
                try {
                    if constexpr (std::is_void_v<Ret>) {
                        fn();
                        result.set_value();
                    } else {
                        result.set_value(fn());
                    }
                } catch (...) {
                    result.set_exception(std::current_exception());
                }
            }
        };
    public:
        Worker() : worker {[this] {
            while (true) {
                std::unique_ptr<CallBase> call;

                {
                    std::unique_lock lock{mtx};
                    cv.wait(lock, [this]{ return !queue.empty() || cancelled(); });
                    if constexpr (!opts.drain_queue_on_cancel) { if (cancelled()) break; }
                    else { if (cancelled() && queue.empty()) break; }
                    call = std::move(queue.front());
                    queue.pop();
                }

                call->execute();
            }
        }} {}

        ~Worker() {
            cancel();
            if (worker.joinable()) worker.join();
        }

        template <typename Ret, bool wait_for_completion>
        using conditional_return = std::conditional_t<wait_for_completion, Ret, std::future<Ret>>;

        /**
         * First template is return type
         * Second template is whether to wait for the completion of the function
         * Returns a future if not waiting for completion
         * If cancelled, either throws, or returns a default value/future
         */
        template<bool wait_for_completion, std::invocable Fn>
        auto post(Fn&& fn) -> conditional_return<std::invoke_result_t<Fn&>, wait_for_completion> {
            using Ret = std::invoke_result_t<Fn&>;

            #define check_cancelled \
            if (cancelled()) { \
                if constexpr (opts.type == WorkerType::ThrowError) { \
                    if constexpr (wait_for_completion) throw std::runtime_error("Worker is cancelled"); \
                    else { \
                        std::promise<Ret> promise; \
                        auto fut = promise.get_future(); \
                        promise.set_exception(std::make_exception_ptr(std::runtime_error("Worker is cancelled"))); \
                        return fut; \
                    } \
                } else { \
                    if constexpr (wait_for_completion) return Ret{}; \
                    else { \
                        std::promise<Ret> promise; \
                        auto fut = promise.get_future(); \
                        if constexpr (std::is_void_v<Ret>) promise.set_value(); \
                        else promise.set_value(Ret{}); \
                        return fut; \
                    } \
                } \
            }

            check_cancelled

            auto call = std::make_unique<Call<Fn, Ret>>(std::forward<Fn>(fn));
            [[maybe_unused]] auto fut = call->result.get_future();

            {
                std::lock_guard lock{mtx};
                check_cancelled
                queue.push(std::move(call));
            }
            cv.notify_one();

            if constexpr (wait_for_completion) return fut.get();
            else return fut;
        }
        
        /**
         * Draining the queue depends upon the template argument drain_queue_on_cancel
         */
        void cancel() noexcept {
            {
                std::lock_guard lock{mtx};
                running.store(false);
            }
            cv.notify_all();
        }

        constexpr bool cancelled() const noexcept {
            return !running;
        }

    private:
        std::mutex mtx;
        std::condition_variable cv;
        std::atomic_bool running{true};
        std::queue<std::unique_ptr<CallBase>> queue;
        std::thread worker;
    };
}