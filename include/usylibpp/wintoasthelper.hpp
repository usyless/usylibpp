#pragma once

#include "windows.hpp"
#include "print.hpp"
#include <wintoastlib.h>

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

    struct AutoDeletingWinToastInit {
        AutoDeletingWinToastInit(const std::wstring& app_name, const std::wstring& company_name, const std::wstring& product_name, const std::wstring& sub_product, const std::wstring& version_information) {
            appname = app_name;
            using namespace WinToastLib;

            if (!WinToast::isCompatible()) return;

            WinToast::instance()->setAppName(app_name);
            WinToast::instance()->setAppUserModelId(WinToast::configureAUMI(company_name, product_name, sub_product, version_information));

            if (!WinToast::instance()->initialize()) return;

            _success = true;
        }

        bool success() const noexcept {
            return _success;
        }

        ~AutoDeletingWinToastInit() {
            delete_shortcut(appname);
        }
    private:
        std::wstring appname;
        bool _success = false;
    };
}