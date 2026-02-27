#pragma once

#include "aliases.hpp" // IWYU pragma: export

#include "macros.hpp" // IWYU pragma: export
#include "opts.hpp" // IWYU pragma: export

#ifdef USYLIBPP_ENABLE_WINDOWS
#include "windows.hpp" // IWYU pragma: export
#include "windows/fs.hpp" // IWYU pragma: export
#include "windows/process.hpp" // IWYU pragma: export
#include "windows/strings.hpp" // IWYU pragma: export
#include "windows/compression.hpp" // IWYU pragma: export
#endif

#include "everything/everything.hpp" // IWYU pragma: export

#include "discord.hpp" // IWYU pragma: export
#include "strings.hpp" // IWYU pragma: export
#include "files.hpp" // IWYU pragma: export
#include "init.hpp" // IWYU pragma: export
#include "time.hpp" // IWYU pragma: export
#include "print.hpp" // IWYU pragma: export
#include "util/worker.hpp" // IWYU pragma: export
#include "util/timer.hpp" // IWYU pragma: export
#include "util/interval.hpp" // IWYU pragma: export
#include <usylibppconfig.hpp> // IWYU pragma: export

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