#pragma once

#include "../aliases.hpp" // IWYU pragma: export
#include "../macros.hpp"
#include <string>
#include <optional>
#include <windows.h>
#include "../types.hpp"
#include "../opts.hpp"

namespace usylibpp::windows {
    /**
     * Convert a const char* into a std::wstring
     * Returns std::nullopt on error or if utf8 is null.
     * NOTE: an EMPTY input is not an error - it yields an engaged, empty string.
     */
    template <opts options = {}>
    [[nodiscard]] inline auto to_wstr(const char* utf8) -> types::opts_return<options, std::wstring> {
        using STR = std::wstring;

        if (utf8 == nullptr) {
            if constexpr (options.as_optional) return std::optional<STR>{std::nullopt};
            else return STR{};
        }

        const auto buffer_size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);

        if (buffer_size == 0) {
            if constexpr (options.as_optional) return std::optional<STR>{std::nullopt};
            else return STR{};
        }

        STR wstr(buffer_size, L'\0');
        const auto written = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr.data(), buffer_size);
        if (written == 0) {
            if constexpr (options.as_optional) return std::optional<STR>{std::nullopt};
            else return STR{};
        }
        wstr.resize(written - 1); // written returns null terminator

        if constexpr (options.as_optional) return std::optional<STR>{std::move(wstr)};
        else return wstr;
    }

    template <opts options = {}>
    [[nodiscard]] inline auto to_wstr(const std::string& utf8) -> types::opts_return<options, std::wstring> {
        return to_wstr<options>(utf8.c_str());
    }

    USYLIBPP__MAKE_OR(to_wstr, std::wstring{})

    template<types::wchar_t_strict T>
    [[nodiscard]] inline constexpr const wchar_t* wchar_t_from_strict(T&& str) noexcept {
        if constexpr (types::wchar_ptr<T>) {
            return str;
        } else if constexpr (types::wstring<T>) {
            return str.c_str();
        } else {
            static_assert(!std::is_same_v<T, T>, "Unsupported type passed to usylibpp::wchar_t_from_strict, must have forgotten a branch");
        }
    }

    /**
     * If its a string type this pointer will only survive to the next call on the thread
     */
    template<types::wchar_t_compatible T>
    [[nodiscard]] inline const wchar_t* wchar_t_from_compatible(T&& str) {
        if constexpr (types::wchar_t_strict<T>) {
            return wchar_t_from_strict(std::forward<T>(str));
        } else if constexpr (types::string<T>) {
            static thread_local std::optional<std::wstring> buffer;
            buffer = to_wstr(str);
            if (!buffer) return L"";
            return buffer->c_str();
        } else if constexpr (types::filesystem_path<T>) {
            // native() returns a reference to a member of `str`. For an rvalue path
            // that member dies at the end of the full-expression, so route it through
            // the same thread-local buffer as the narrow-string branch and keep one
            // uniform lifetime rule for every non-wide input.
            static thread_local std::wstring path_buffer;
            path_buffer = str.native();
            return path_buffer.c_str();
        } else {
            static_assert(!std::is_same_v<T, T>, "Unsupported type passed to usylibpp::wchar_t_from_compatible, must have forgotten a branch");
        }
    }

     /**
     * Convert any compatible wide string into a std::string
     * Returns std::nullopt on error.
     * NOTE: an EMPTY input is not an error - it yields an engaged, empty string.
     */
    template <opts options = {}, types::wchar_t_compatible T>
    [[nodiscard]] inline auto to_utf8(T&& _wstr) -> types::opts_return<options, std::string> {
        using STR = std::string;
        
        const auto wstr = wchar_t_from_compatible(std::forward<T>(_wstr));

        const auto buffer_size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);

        if (buffer_size == 0) {
            if constexpr (options.as_optional) return std::optional<STR>{std::nullopt};
            else return STR{};
        }

        STR utf8_str(buffer_size, '\0');
        const auto written = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8_str.data(), buffer_size, nullptr, nullptr);
        if (written == 0) {
            if constexpr (options.as_optional) return std::optional<STR>{std::nullopt};
            else return STR{};
        }
        utf8_str.resize(written - 1); // written returns null terminator

        if constexpr (options.as_optional) return std::optional<STR>{std::move(utf8_str)};
        else return utf8_str;
    }

    USYLIBPP__MAKE_OR(to_utf8, std::string{})
}