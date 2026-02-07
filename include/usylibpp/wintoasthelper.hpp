#pragma once

#include "windows.hpp"
#include "print.hpp"
#include <future>
#include <wintoastlib.h>
#include <mutex>
#include <thread>
#include <queue>

namespace usylibpp::wintoast {
    /**
     * Returns true if succeeded
     */
    inline bool delete_shortcut(const std::wstring& app_name) {
        auto programs_path = usylibpp::windows::get_known_folder(FOLDERID_Programs);
        if (!programs_path) return false;

        if (app_name.empty()) return true;

        auto shortcut_path = programs_path.value() / (app_name + L".lnk");

        std::error_code ec;
        std::filesystem::remove(shortcut_path, ec);
        if (ec) return false;

        return true;
    }

    /**
     * Only works if called on the same thread as the initial wintoast setup
     */
    inline bool delete_shortcut() {
        return delete_shortcut(WinToastLib::WinToast::instance()->appName());
    }

    class PrintingWinToastHandler : public WinToastLib::IWinToastHandler {
    public:
        PrintingWinToastHandler() {}

        void toastActivated() const override {
            print::println("Toast activated");
        }
        void toastActivated(int actionIndex) const override {
            print::println("Button clicked: {}", actionIndex);
        }
        void toastActivated(std::wstring response) const override {
            print::println("Toast activated: {}", windows::to_utf8_or_default(response));
        }
        void toastDismissed(WinToastDismissalReason state) const override {
            int val = state;
            print::println("Toast dismissed (WinToastDismissalReason): {}", val);
        }
        void toastFailed() const override {
            print::println("Toast failed");
        }
    };

    class EmptyWinToastHandler : public WinToastLib::IWinToastHandler {
    public:
        EmptyWinToastHandler() {}
        void toastActivated() const override {}
        void toastActivated(int) const override {}
        void toastActivated(std::wstring) const override {}
        void toastDismissed(WinToastDismissalReason) const override {}
        void toastFailed() const override {}
    };

    template <bool delete_on_destruct>
    struct WinToastInit {
        WinToastInit(const std::wstring& app_name, const std::wstring& company_name, const std::wstring& product_name, const std::wstring& sub_product, const std::wstring& version_information, std::optional<WinToastLib::WinToast::ShortcutPolicy> shortcut_policy = std::nullopt) {
            if constexpr (delete_on_destruct) {
                appname = app_name;
            }

            using namespace WinToastLib;

            if (!WinToast::isCompatible()) return;

            if (shortcut_policy) WinToast::instance()->setShortcutPolicy(shortcut_policy.value());
            WinToast::instance()->setAppName(app_name);
            WinToast::instance()->setAppUserModelId(WinToast::configureAUMI(company_name, product_name, sub_product, version_information));

            if (!WinToast::instance()->initialize()) return;

            _success = true;
        }

        bool success() const noexcept {
            return _success;
        }

        ~WinToastInit() {
            if constexpr (delete_on_destruct) {
                delete_shortcut(appname);
            }
        }
    private:
        std::wstring appname;
        bool _success = false;
    };

    template <bool delete_on_destruct>
    class ToastWorker {
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

            void execute() override {
                if constexpr (std::is_void_v<Ret>) {
                    fn();
                    result.set_value();
                } else {
                    result.set_value(fn());
                }
            }
        };
    public:
        ToastWorker(const std::wstring& app_name, const std::wstring& company_name, const std::wstring& product_name, const std::wstring& sub_product, const std::wstring& version_information, std::optional<WinToastLib::WinToast::ShortcutPolicy> shortcut_policy = std::nullopt) {
            worker = std::thread([=, this] { thread_main(app_name, company_name, product_name, sub_product, version_information, shortcut_policy); });
        }

        ~ToastWorker() {
            running = false;
            cv.notify_all();
            if (worker.joinable()) worker.join();
        }

        bool success() {
            return post<bool>([this]{ return _toast->success(); });
        }

        bool isInitialized() {
            return post<bool>([]{ return WinToastLib::WinToast::instance()->isInitialized(); });
        }

        bool hideToast(INT64 id) {
            return post<bool>([id]{ return WinToastLib::WinToast::instance()->hideToast(id); });
        }

        INT64 showToast(WinToastLib::WinToastTemplate const& toast, WinToastLib::IWinToastHandler* eventHandler, WinToastLib::WinToast::WinToastError* error = nullptr) {
            return post<INT64>([&toast, eventHandler, error]{ return WinToastLib::WinToast::instance()->showToast(toast, eventHandler, error); });
        }

        void clear() {
            post<void>([]{ WinToastLib::WinToast::instance()->clear(); });
        }

        enum WinToastLib::WinToast::ShortcutResult createShortcut() {
            return post<WinToastLib::WinToast::ShortcutResult>([]{ return WinToastLib::WinToast::instance()->createShortcut(); });
        }

        std::wstring appName() {
            return post<std::wstring>([]{ return WinToastLib::WinToast::instance()->appName(); });
        }

        std::wstring appUserModelId() {
            return post<std::wstring>([]{ return WinToastLib::WinToast::instance()->appUserModelId(); });
        }

    private:
        std::unique_ptr<WinToastInit<delete_on_destruct>> _toast;
        std::thread worker;
        std::mutex mtx;
        std::condition_variable cv;
        std::queue<std::unique_ptr<CallBase>> queue;
        std::atomic<bool> running = true;

        void thread_main(const std::wstring& app_name, const std::wstring& company_name, const std::wstring& product_name, const std::wstring& sub_product, const std::wstring& version_information, std::optional<WinToastLib::WinToast::ShortcutPolicy> shortcut_policy = std::nullopt) {
            _toast = std::make_unique<WinToastInit<delete_on_destruct>>(app_name, company_name, product_name, sub_product, version_information, shortcut_policy);
            if (!_toast->success()) {
                running = false;
                return;
            }

            while (true) {
                std::unique_ptr<CallBase> call;

                {
                    std::unique_lock lock{mtx};
                    cv.wait(lock, [this]{ return !queue.empty() || !running; });
                    if (!running) break;
                    call = std::move(queue.front());
                    queue.pop();
                }

                call->execute();
            }
        }

        template<typename Ret, typename Fn>
        Ret post(Fn&& fn) {
            if (!running) {
                if constexpr (std::is_void_v<Ret>) return;
                else return Ret{};
            }

            auto call = std::make_unique<Call<Fn, Ret>>(std::forward<Fn>(fn));
            auto fut = call->result.get_future();

            {
                std::lock_guard lock{mtx};
                queue.push(std::move(call));
            }
            cv.notify_one();

            if constexpr (std::is_void_v<Ret>) {
                fut.get();
            } else {
                return fut.get();
            }
        }
    };
}