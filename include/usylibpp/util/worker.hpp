#pragma once

#include <thread>
#include <optional>
#include <atomic>
#include <future>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace usylibpp::util {
    enum class WorkerType {
        ReturnDefault,
        ReturnOptional,
        ThrowError
    };
    /**
     * Basic worker in its own thread
     * Once cancelled returns std::nullopt
     */
    template <bool drain_queue_on_cancel, WorkerType type = WorkerType::ReturnDefault>
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
                if constexpr (std::is_void_v<Ret>) {
                    fn();
                    result.set_value();
                } else {
                    result.set_value(fn());
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
                    if constexpr (!drain_queue_on_cancel) { if (cancelled()) break; }
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
        using conditional_return = std::conditional_t<wait_for_completion && !std::is_void_v<Ret>, std::conditional_t<type == WorkerType::ReturnOptional, std::optional<Ret>, Ret>, void>;

        /**
         * First template is return type
         * Second template is whether to wait for the completion of the function
         * If cancelled, return depends on template
         */
        template<typename Ret, bool wait_for_completion, typename Fn>
        auto post(Fn&& fn) -> conditional_return<Ret, wait_for_completion> {
            if (cancelled()) {
                if constexpr (!wait_for_completion || std::is_void_v<Ret>) return;
                else if constexpr (type == WorkerType::ReturnOptional) return std::nullopt;
                else if constexpr (type == WorkerType::ThrowError) throw std::runtime_error("Worker is cancelled");
                else return Ret{};
            }

            auto call = std::make_unique<Call<Fn, Ret>>(std::forward<Fn>(fn));
            [[maybe_unused]] auto fut = call->result.get_future();

            {
                std::lock_guard lock{mtx};
                queue.push(std::move(call));
            }
            cv.notify_one();

            if constexpr (wait_for_completion) {
                if constexpr (std::is_void_v<Ret>) fut.get();
                else return fut.get();
            } else {
                return;
            }
        }
        
        /**
         * Draining the queue depends upon the template argument drain_queue_on_cancel
         */
        void cancel() noexcept {
            running = false;
            cv.notify_all();
        }

        constexpr bool cancelled() const noexcept {
            return !running;
        }

    private:
        std::thread worker;
        std::mutex mtx;
        std::condition_variable cv;
        std::queue<std::unique_ptr<CallBase>> queue;
        std::atomic_bool running{true};
    };
}