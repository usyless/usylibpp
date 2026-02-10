#pragma once

#include "windows.hpp"
#include "print.hpp"
#include <wintoastlib.h>

#include "util/worker.hpp"

namespace usylibpp::wintoast {
    /**
     * Returns true if succeeded
     */
    inline bool delete_shortcut(const std::wstring& app_name) {
        if (app_name.empty()) return false;

        auto programs_path = usylibpp::windows::get_known_folder(FOLDERID_Programs);
        if (!programs_path) return false;

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
        WinToastInit() = delete;
        WinToastInit(const WinToastInit&) = delete;
        WinToastInit& operator=(const WinToastInit&) = delete;

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

    /**
     * If wintoast fails to intialise, the return types are dependent on cancel_return_type
     */
    template <bool delete_on_destruct, util::WorkerType cancel_return_type>
    class ToastWorker {
    public:
        ToastWorker() = delete;
        ToastWorker(const ToastWorker&) = delete;
        ToastWorker& operator=(const ToastWorker&) = delete;
        ToastWorker(ToastWorker&&) = delete;
        ToastWorker& operator=(ToastWorker&&) = delete;

        ToastWorker(const std::wstring& app_name, const std::wstring& company_name, const std::wstring& product_name, const std::wstring& sub_product, const std::wstring& version_information, std::optional<WinToastLib::WinToast::ShortcutPolicy> shortcut_policy = std::nullopt) {
            // ref is fine as it waits until completion
            worker.template post<true>([&, this] { 
                _toast = std::make_unique<WinToastInit<delete_on_destruct>>(app_name, company_name, product_name, sub_product, version_information, shortcut_policy);
                if (!_toast->success()) worker.cancel();
            });
        }

        template <bool wait_for_completion = true>
        auto success() {
            return worker.template post<wait_for_completion>([this]{ return _toast->success(); });
        }

        template <bool wait_for_completion = true>
        auto isInitialized() {
            return worker.template post<wait_for_completion>([]{ return WinToastLib::WinToast::instance()->isInitialized(); });
        }

        template <bool wait_for_completion = true>
        auto hideToast(INT64 id) {
            return worker.template post<wait_for_completion>([id]{ return WinToastLib::WinToast::instance()->hideToast(id); });
        }
        
        template <bool wait_for_completion = true>
        auto showToast(WinToastLib::WinToastTemplate const& toast, WinToastLib::IWinToastHandler* eventHandler, WinToastLib::WinToast::WinToastError* error = nullptr) {
            if constexpr (wait_for_completion) {
                return worker.template post<true>([&toast, eventHandler, error]{ return WinToastLib::WinToast::instance()->showToast(toast, eventHandler, error); });
            } else {
                return worker.template post<true>([toast, eventHandler, error]{ return WinToastLib::WinToast::instance()->showToast(toast, eventHandler, error); });
            }
        }

        template <bool wait_for_completion = true>
        auto clear() {
            return worker.template post<wait_for_completion>([]{ WinToastLib::WinToast::instance()->clear(); });
        }

        template <bool wait_for_completion = true>
        auto createShortcut() {
            return worker.template post<wait_for_completion>([]{ return WinToastLib::WinToast::instance()->createShortcut(); });
        }

        template <bool wait_for_completion = true>
        auto appName() {
            return worker.template post<wait_for_completion>([]{ return WinToastLib::WinToast::instance()->appName(); });
        }

        template <bool wait_for_completion = true>
        auto appUserModelId() {
            return worker.template post<wait_for_completion>([]{ return WinToastLib::WinToast::instance()->appUserModelId(); });
        }

    private:
        static constexpr util::WorkerOpts worker_opts{
            .drain_queue_on_cancel = false, 
            .type = cancel_return_type
        };
        util::Worker<worker_opts> worker{1};
        std::unique_ptr<WinToastInit<delete_on_destruct>> _toast;
    };
}