#pragma once
// NOLINTBEGIN(misc-unused-alias-decls)

#include "macros.hpp" // IWYU pragma: export
#include "opts.hpp" // IWYU pragma: export

namespace ulp = usylibpp;

#ifdef USYLIBPP_ENABLE_WINDOWS
#include "windows.hpp" // IWYU pragma: export
#include "windows/fs.hpp" // IWYU pragma: export
#include "windows/process.hpp" // IWYU pragma: export
#include "windows/strings.hpp" // IWYU pragma: export

namespace usylibpp {
    namespace win = ulp::windows;
}
namespace usylibpp::windows {
    namespace proc = win::process;
}
#endif

#include "discord.hpp" // IWYU pragma: export
#include "strings.hpp" // IWYU pragma: export
namespace usylibpp {
    namespace str = ulp::strings;
}
#include "files.hpp" // IWYU pragma: export
namespace usylibpp {
    namespace fs = ulp::files;
}
#include "init.hpp" // IWYU pragma: export
#include "time.hpp" // IWYU pragma: export
namespace usylibpp {
    namespace tm = ulp::time;
}
#include "print.hpp" // IWYU pragma: export
#include "util/worker.hpp" // IWYU pragma: export
#include "util/timer.hpp" // IWYU pragma: export
#include <usylibppconfig.hpp>

#ifdef USYLIBPP_ENABLE_WINTOAST
#include <wintoastlib.h>
#include "wintoasthelper.hpp" // IWYU pragma: export
#endif

#ifdef USYLIBPP_ENABLE_WINDOWS_IMAGING
#include "windows/images.hpp" // IWYU pragma: export
#endif

#ifdef USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE
#include "windows/darkmode.hpp" // IWYU pragma: export
#include "windows/task_dialog.hpp" // IWYU pragma: export
#endif
// NOLINTEND(misc-unused-alias-decls)