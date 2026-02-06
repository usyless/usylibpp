#pragma once

#include "windows.hpp"
#include "print.hpp"
#include <wintoastlib.h>

namespace usylibpp::wintoast {
    /**
     * Returns true if succeeded
     */
    inline bool delete_shortcut() {
        auto programs_path = usylibpp::windows::get_known_folder(FOLDERID_Programs);
        if (!programs_path) return false;

        auto shortcut_path = programs_path.value() / (WinToastLib::WinToast::instance()->appName() + L".lnk");

        std::error_code ec;
        std::filesystem::remove(shortcut_path, ec);
        if (ec) return false;

        return true;
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
}