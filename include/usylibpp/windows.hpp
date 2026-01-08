#pragma once

#include <string>
#include <filesystem>
#include <windows.h>

namespace usylibpp::windows {
    inline std::wstring to_wstr(const char* utf8) {
        int buffer_size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);

        if (buffer_size == 0) {
            return L"";
        }

        std::wstring wstr(buffer_size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &wstr[0], buffer_size);
        return wstr;
    }

    inline std::wstring to_wstr(const std::string& utf8) {
        return to_wstr(utf8.c_str());
    }

    inline std::string to_utf8(const wchar_t* wstr) {
        const int buffer_size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);

        if (buffer_size == 0) {
            throw std::runtime_error("Failed to convert wide string to UTF-8.");
        }

        std::string utf8_str(buffer_size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &utf8_str[0], buffer_size - 1, nullptr, nullptr);

        return utf8_str;
    }

    inline std::string to_utf8(const std::wstring& wstr) {
        return to_utf8(wstr.c_str());
    }

    inline std::string to_utf8(const std::filesystem::path& p) {
        return to_utf8(p.native().c_str());
    }
}