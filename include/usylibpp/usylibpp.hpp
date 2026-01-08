#pragma once

namespace usylibpp::info {
    inline constexpr auto version = "0.1";
}

#ifdef WIN32
#include "windows.hpp"
#endif

#include "strings.hpp"
#include "files.hpp"
#include "init.hpp"
#include "time.hpp"
#include "print.hpp"
