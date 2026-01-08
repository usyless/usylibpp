#pragma once

#include <locale>

namespace usylibpp::init {
    inline void set_utf8_locale() {
        std::setlocale(LC_ALL, ".UTF8");
        std::locale::global(std::locale(".UTF8"));
    }
}