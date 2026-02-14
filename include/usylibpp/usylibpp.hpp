#pragma once

#include "macros.hpp"
#include "opts.hpp"

#ifdef USYLIBPP_ENABLE_WINDOWS
#include "windows.hpp"
#include "windows/fs.hpp"
#endif

#include "discord.hpp"
#include "strings.hpp"
#include "files.hpp"
#include "init.hpp"
#include "time.hpp"
#include "print.hpp"
#include <usylibppconfig.hpp>

#ifdef USYLIBPP_ENABLE_WINTOAST
#include <wintoastlib.h>
#include "wintoasthelper.hpp"
#endif

#ifdef USYLIBPP_ENABLE_WINDOWS_IMAGING
#include "windows/images.hpp"
#endif

#ifdef USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE
#include "windows/darkmode.hpp"
#include "windows/task_dialog.hpp"
#endif