#pragma once

#include "../aliases.hpp" // IWYU pragma: export

#include <optional>
#include <thread>
#include <atomic>
#include <future>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <variant>

namespace usylibpp::util {
    enum class WorkerType {
        ReturnDefault,
        ThrowError
    };

    struct WorkerOpts {
        bool drain_queue_on_cancel{true};
        WorkerType type{WorkerType::ReturnDefault};
        bool with_then_chaining{false};
    };

    template <typename T>
    class ChainedResult {
    private:
        std::variant<T, std::exception_ptr> data;

    public:
        template <typename U>
        ChainedResult(U&& value) : data(std::forward<U>(value)) {}
        ChainedResult(std::exception_ptr exc) : data(std::move(exc)) {}

        bool is_exception() const noexcept { return std::holds_alternative<std::exception_ptr>(data); }
        bool has_value() const noexcept { return std::holds_alternative<T>(data); }

        T& get() {
            if (is_exception()) std::rethrow_exception(std::get<std::exception_ptr>(data));
            return std::get<T>(data);
        }
        const T& get() const {
            if (is_exception()) std::rethrow_exception(std::get<std::exception_ptr>(data));
            return std::get<T>(data);
        }

        std::exception_ptr get_exception() const noexcept {
            if (is_exception()) return std::get<std::exception_ptr>(data);
            return nullptr;
        }
    };

    template <>
    class ChainedResult<void> {
    private:
        std::variant<std::monostate, std::exception_ptr> data;

    public:
        ChainedResult() : data(std::monostate{}) {}
        ChainedResult(std::exception_ptr exc) : data(std::move(exc)) {}

        bool is_exception() const noexcept { return std::holds_alternative<std::exception_ptr>(data); }
        bool has_value() const noexcept { return std::holds_alternative<std::monostate>(data); }

        void get() const {
            if (is_exception()) std::rethrow_exception(std::get<std::exception_ptr>(data));
        }

        std::exception_ptr get_exception() const noexcept {
            if (is_exception()) return std::get<std::exception_ptr>(data);
            return nullptr;
        }
    };

    template<typename Ret>
    struct ChainableSharedState {
        std::mutex mtx;
        std::condition_variable cv;
        std::atomic_bool ready{false};
        
        std::optional<ChainedResult<Ret>> result;
        
        // Continuation now takes the result as an argument
        std::function<void(ChainedResult<Ret>&)> continuation;

        template <typename... Args>
        void set_value(Args&&... args) {
            std::function<void(ChainedResult<Ret>&)> cont;
            {
                std::lock_guard lock(mtx);
                if constexpr (std::is_void_v<Ret>) {
                    result.emplace();
                } else {
                    result.emplace(std::forward<Args>(args)...);
                }
                ready = true;
                cont = std::move(continuation);
            }
            cv.notify_all();
            if (cont) cont(*result); // Pass the result directly
        }

        void set_exception(std::exception_ptr e) {
            std::function<void(ChainedResult<Ret>&)> cont;
            {
                std::lock_guard lock(mtx);
                result.emplace(std::move(e));
                ready = true;
                cont = std::move(continuation);
            }
            cv.notify_all();
            if (cont) cont(*result); // Pass the exception result directly
        }
    };

    template<typename Ret>
    class ChainableFuture {
        std::shared_ptr<ChainableSharedState<Ret>> state;

    public:
        explicit ChainableFuture(std::shared_ptr<ChainableSharedState<Ret>> s) : state(std::move(s)) {}
        explicit ChainableFuture() {}

        Ret get() {
            std::unique_lock lock(state->mtx);
            state->cv.wait(lock, [this]() { return state->ready.load(); });
            
            if (state->result->is_exception()) {
                std::rethrow_exception(state->result->get_exception());
            }

            if constexpr (!std::is_void_v<Ret>) {
                return std::move(state->result->get());
            } else {
                state->result->get();
                return;
            }
        }

        template <typename Func>
        auto then(Func&& func) {
            using NextRet = std::conditional_t<std::is_void_v<Ret>, 
                                               std::invoke_result_t<Func>, 
                                               std::invoke_result_t<Func, Ret>>;
            
            auto next_state = std::make_shared<ChainableSharedState<NextRet>>();
            ChainableFuture<NextRet> next_future(next_state);

            auto wrapper = [next_state, f = std::forward<Func>(func)](ChainedResult<Ret>& res) mutable {
                try {
                    if (res.is_exception()) {
                        next_state->set_exception(res.get_exception());
                    } else {
                        if constexpr (std::is_void_v<NextRet>) {
                            if constexpr (std::is_void_v<Ret>) f();
                            else f(std::move(res.get()));
                            next_state->set_value();
                        } else {
                            if constexpr (std::is_void_v<Ret>) next_state->set_value(f());
                            else next_state->set_value(f(std::move(res.get())));
                        }
                    }
                } catch (...) {
                    next_state->set_exception(std::current_exception());
                }
            };

            bool run_now = false;
            {
                std::lock_guard lock(state->mtx);
                if (state->ready) {
                    run_now = true;
                } else {
                    state->continuation = std::move(wrapper);
                }
            }

            if (run_now) {
                wrapper(*state->result);
            }

            return next_future;
        }
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

        template<typename Ret>
        class ChainablePromise {
            std::shared_ptr<ChainableSharedState<Ret>> state;

        public:
            ChainablePromise() : state(std::make_shared<ChainableSharedState<Ret>>()) {}

            ChainableFuture<Ret> get_future() {
                return ChainableFuture<Ret>(state);
            }

            template <typename... Args>
            void set_value(Args&&... args) {
                if constexpr (std::is_void_v<Ret>) {
                    state->set_value();
                } else {
                    state->set_value(std::forward<Args>(args)...);
                }
            }

            void set_exception(std::exception_ptr e) {
                state->set_exception(std::move(e));
            }
        };

        template <typename Ret>
        using future_t = std::conditional_t<opts.with_then_chaining, ChainableFuture<Ret>, std::future<Ret>>;

        template <typename Ret>
        using promise_t = std::conditional_t<opts.with_then_chaining, ChainablePromise<Ret>, std::promise<Ret>>;

        template<typename Fn, typename Ret>
        struct Call : CallBase {
            Fn fn;
            promise_t<Ret> result;

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
        using conditional_return = std::conditional_t<wait_for_completion, Ret, future_t<Ret>>;

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
                        promise_t<Ret> promise; \
                        auto future = promise.get_future(); \
                        promise.set_exception(std::make_exception_ptr(std::runtime_error("Worker is cancelled"))); \
                        return future; \
                    } \
                } else { \
                    if constexpr (wait_for_completion) return Ret{}; \
                    else { \
                        promise_t<Ret> promise; \
                        auto future = promise.get_future(); \
                        if constexpr (std::is_void_v<Ret>) promise.set_value(); \
                        else promise.set_value(Ret{}); \
                        return future; \
                    } \
                } \
            }

            check_cancelled

            future_t<Ret> fut;

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