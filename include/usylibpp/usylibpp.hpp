#pragma once

#include "macros.hpp"
#include "opts.hpp"

#ifdef USYLIBPP_ENABLE_WINDOWS
#include "windows.hpp"
#ifdef USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE
#include "windows_dark_mode.hpp"
#endif
#endif

#include "discord.hpp"
#include "strings.hpp"
#include "files.hpp"
#include "init.hpp"
#include "time.hpp"
#include "print.hpp"
#include <usylibppconfig.hpp>