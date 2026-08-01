#pragma once

#include "aliases.hpp" // IWYU pragma: export
#include <iostream>
#include <locale>
#include <clocale>

/**
 * Helper methods to do with the initialisation of an app, stuff that should run at the start of main
 */
namespace usylibpp::init {
    /**
     * Sets the locale of the app to UTF8, should be called at the start of your app
     * Unsure of effects on unix, however on Windows it allows for reading files using utf8 paths
     * with fstream's
     */
    inline bool set_utf8_locale() noexcept {
        std::setlocale(LC_ALL, ".UTF8");
        for (const char* name : {".UTF8", "C.UTF-8", "en_US.UTF-8", "UTF-8"}) {
            try {
                std::locale::global(std::locale(name));
                return true;
            } catch (...) {}
        }
        return false;
    }

    /**
     * Enable a faster cout and cin
     * I forgot the implications but i think you shouldnt be using
     * the C-style out and in stuff at the same time to prevent issues
     */
    inline void quicker_cin_cout() noexcept {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);
    }
}