#pragma once

#include "../aliases.hpp" // IWYU pragma: export

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
     * Not intended to be inheritable (no virtual destructor)
     * Basic worker in its own thread
     */
    template <WorkerOpts opts>
    class Worker {
    private:
        struct CallBase {
            virtual ~CallBase() = default;
            virtual void execute() noexcept = 0;
            virtual void cancel() noexcept = 0;
        };

        template<typename Fn, typename Ret>
        struct Call : CallBase {
            Fn fn;
            std::promise<Ret> result;

            Call(Fn&& f) : fn(std::move(f)) {}

            void execute() noexcept override final {
                try {
                    if constexpr (std::is_void_v<Ret>) {
                        std::invoke(fn);
                        result.set_value();
                    } else {
                        result.set_value(std::invoke(fn));
                    }
                } catch (...) {
                    try {
                        result.set_exception(std::current_exception());
                    } catch (...) {}
                }
            }

            void cancel() noexcept override final {
                try {
                    if constexpr (opts.type == WorkerType::ThrowError) {
                        result.set_exception(std::make_exception_ptr(std::runtime_error("Queue is being cleared")));
                    } else {
                        if constexpr (std::is_void_v<Ret>) result.set_value();
                        else result.set_value(Ret{});
                    }
                } catch (...) {
                    try {
                        result.set_exception(std::current_exception());
                    } catch (...) {}
                }
            }
        };
    public:
        Worker() = delete;
        Worker(const Worker&) = delete;
        Worker& operator=(const Worker&) = delete;
        Worker(Worker&&) = delete;
        Worker& operator=(Worker&&) = delete;


        explicit Worker(const size_t worker_count) {
            workers.reserve(worker_count);
            for (size_t i = 0; i < worker_count; ++i) {
                workers.emplace_back(&Worker::worker_loop, this);
            }
        }

        ~Worker() {
            cancel();
            for (auto& worker : workers) if (worker.joinable()) worker.join();
        }

        template <typename Ret, bool wait_for_completion>
        using conditional_return = std::conditional_t<wait_for_completion, Ret, std::future<Ret>>;

        /**
         * First template is whether to wait for the completion of the function
         * Second template is return type
         * Returns a future if not waiting for completion
         * If cancelled, either throws, or returns a default value/future
         */
        template<bool wait_for_completion, typename Fn, typename... Args>
        requires std::invocable<Fn&, Args...>
        inline auto post(Fn&& fn, Args&&... args) -> conditional_return<std::invoke_result_t<Fn&, Args...>, wait_for_completion> {
            using Ret = std::invoke_result_t<Fn&, Args...>;

            #pragma push_macro("check_cancelled")
            #undef check_cancelled
            #define check_cancelled \
            if (cancelled()) { \
                if constexpr (opts.type == WorkerType::ThrowError) { \
                    if constexpr (wait_for_completion) throw std::runtime_error("Worker is cancelled"); \
                    else { \
                        std::promise<Ret> promise; \
                        auto future = promise.get_future(); \
                        promise.set_exception(std::make_exception_ptr(std::runtime_error("Worker is cancelled"))); \
                        return future; \
                    } \
                } else { \
                    if constexpr (wait_for_completion) return Ret{}; \
                    else { \
                        std::promise<Ret> promise; \
                        auto future = promise.get_future(); \
                        if constexpr (std::is_void_v<Ret>) promise.set_value(); \
                        else promise.set_value(Ret{}); \
                        return future; \
                    } \
                } \
            }

            check_cancelled

            std::future<Ret> fut;

            if constexpr (sizeof...(Args) == 0)  {
                auto call = std::make_unique<Call<Fn, Ret>>(std::forward<Fn>(fn));
                fut = call->result.get_future();
                {
                    std::lock_guard lock{mtx};
                    check_cancelled
                    queue.push(std::move(call));
                }
            } else {
                auto bound = [f = std::forward<Fn>(fn), ...as = std::forward<Args>(args)]() mutable -> Ret {
                    return std::invoke(f, std::move(as)...);
                };

                auto call = std::make_unique<Call<decltype(bound), Ret>>(std::move(bound));
                fut = call->result.get_future();
                {
                    std::lock_guard lock{mtx};
                    check_cancelled
                    queue.push(std::move(call));
                }
            }
            
            cv.notify_one();

            if constexpr (wait_for_completion) return fut.get();
            else return fut;

            #pragma pop_macro("check_cancelled")
        }
        
        /**
         * Draining the queue depends upon the template argument drain_queue_on_cancel
         */
        inline void cancel() noexcept {
            {
                std::lock_guard lock{mtx};
                running.store(false);
                if constexpr (!opts.drain_queue_on_cancel) {
                    while (!queue.empty()) {
                        queue.front()->cancel();
                        queue.pop();
                    }
                }
            }
            cv.notify_all();
        }

        /**
         * Cancels everything in the current queue, throwing exceptions or returning default values
         */
        inline void clear_queue() noexcept {
            {
                std::lock_guard lock{mtx};
                while (!queue.empty()) {
                    queue.front()->cancel();
                    queue.pop();
                }
            }
        }

        inline constexpr bool cancelled() const noexcept {
            return !running;
        }

    private:
        std::mutex mtx;
        std::condition_variable cv;
        std::atomic_bool running{true};
        std::queue<std::unique_ptr<CallBase>> queue;
        std::vector<std::thread> workers;

        inline void worker_loop() {
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
        }
    };
}