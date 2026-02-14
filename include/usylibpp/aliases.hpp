#pragma once
// NOLINTBEGIN(misc-unused-alias-decls)

namespace usylibpp {
    namespace windows {
        namespace process {}
        namespace task_dialog {}
        namespace images {}
        namespace darkmode {}
        namespace fs {}
    }
    namespace time {}
    namespace discord {}
    namespace utils {}
    namespace strings {}
    namespace files {}
    namespace wintoast {}
    namespace types {}
    namespace print {}
    namespace init {}
};

namespace ulp = usylibpp;

namespace usylibpp {
    namespace win = windows;
    namespace str = strings;
    namespace fs = files;
    namespace wt = wintoast;

    namespace proc = win::process;
    namespace img = win::images;
    namespace dark = win::darkmode;
    namespace td = win::task_dialog;
}
// NOLINTEND(misc-unused-alias-decls)