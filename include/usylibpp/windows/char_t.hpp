#pragma once

#include "../aliases.hpp" // IWYU pragma: export

namespace usylibpp::windows {
    #ifdef UNICODE
    using WIN_CHAR = wchar_t;
    #else
    using WIN_CHAR = char;
    #endif
}