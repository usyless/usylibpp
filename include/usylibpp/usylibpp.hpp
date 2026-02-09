#pragma once

#include "macros.hpp"
#include "opts.hpp"

#ifdef USYLIBPP_ENABLE_WINDOWS
#include "windows.hpp"
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