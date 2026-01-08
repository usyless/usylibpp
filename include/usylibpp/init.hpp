#pragma once

#include <locale>

/**
 * Helper methods to do with the initialisation of an app, stuff that should run at the start of main
 */
namespace usylibpp::init {
    /**
     * Sets the locale of the app to UTF8, should be called at the start of your app
     * Unsure of effects on unix, however on Windows it allows for reading files using utf8 paths
     * with fstream's
     */
    inline void set_utf8_locale() {
        std::setlocale(LC_ALL, ".UTF8");
        std::locale::global(std::locale(".UTF8"));
    }
}