#pragma once

#include "windows.hpp"
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
}