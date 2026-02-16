#pragma once

#ifdef USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE
#include "../aliases.hpp" // IWYU pragma: export

#include <windows.h>
#include <shobjidl.h>

#pragma comment(linker, \
"\"/manifestdependency:type='win32' " \
"name='Microsoft.Windows.Common-Controls' " \
"version='6.0.0.0' " \
"processorArchitecture='*' " \
"publicKeyToken='6595b64144ccf1df' " \
"language='*'\"")

namespace usylibpp::windows::darkmode {
    // Wouldnt have been possible without
    // https://github.com/ysc3839/win32-darkmode/blob/master/win32-darkmode/DarkMode.h

    enum PreferredAppMode {
        Default,
        AllowDark,
        ForceDark,
        ForceLight,
        Max
    };

    using fnRtlGetNtVersionNumbers = void (WINAPI *)(LPDWORD major, LPDWORD minor, LPDWORD build);
    // 1809 17763
    using fnShouldAppsUseDarkMode = bool (WINAPI *)(); // ordinal 132
    using fnAllowDarkModeForWindow = bool (WINAPI *)(HWND hWnd, bool allow); // ordinal 133
    using fnAllowDarkModeForApp = bool (WINAPI *)(bool allow); // ordinal 135, in 1809
    using fnRefreshImmersiveColorPolicyState = void (WINAPI *)(); // ordinal 104
    using fnIsDarkModeAllowedForWindow = bool (WINAPI *)(HWND hWnd); // ordinal 137
    using fnOpenNcThemeData = HTHEME(WINAPI *)(HWND hWnd, LPCWSTR pszClassList); // ordinal 49
    // 1903 18362
    using fnSetPreferredAppMode = PreferredAppMode (WINAPI *)(PreferredAppMode appMode); // ordinal 135, in 1903

    inline fnAllowDarkModeForWindow _AllowDarkModeForWindow = nullptr;

    inline bool g_darkModeSupported = false;

    /**
    * Call init_dark_mode before this
    * Enable dark mode for an indiviaul hwnd
    */
    inline bool allow_dark_mode_for_window(HWND hWnd, bool allow) noexcept {
        if (g_darkModeSupported) return _AllowDarkModeForWindow(hWnd, allow);
        return false;
    }

    [[nodiscard]] inline constexpr bool windows_build_supports_darkmode(DWORD buildNumber) noexcept {
        return buildNumber >= 17763;
    }

    /**
    * Call this to initialise the dark mode win32 stuff, then call allow_dark_mode_for_window on your individual hwnd
    */
    inline void init_dark_mode() noexcept {
        auto RtlGetNtVersionNumbers = reinterpret_cast<fnRtlGetNtVersionNumbers>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetNtVersionNumbers"));
        if (!RtlGetNtVersionNumbers) return;

        DWORD major = 0, minor = 0, buildNumber = 0;
        RtlGetNtVersionNumbers(&major, &minor, &buildNumber);
        buildNumber &= ~0xF0000000;

        if (!(major == 10 && minor == 0 && windows_build_supports_darkmode(buildNumber))) return;

        HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!hUxtheme) return;

        auto _OpenNcThemeData = reinterpret_cast<fnOpenNcThemeData>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(49)));
        auto _RefreshImmersiveColorPolicyState = reinterpret_cast<fnRefreshImmersiveColorPolicyState>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(104)));
        auto _ShouldAppsUseDarkMode = reinterpret_cast<fnShouldAppsUseDarkMode>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132)));
        _AllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133)));

        auto ord135 = GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
        fnAllowDarkModeForApp _AllowDarkModeForApp = nullptr;
        fnSetPreferredAppMode _SetPreferredAppMode = nullptr;
        if (buildNumber < 18362) _AllowDarkModeForApp = reinterpret_cast<fnAllowDarkModeForApp>(ord135);
        else _SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(ord135);

        auto _IsDarkModeAllowedForWindow = reinterpret_cast<fnIsDarkModeAllowedForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(137)));

        if (_OpenNcThemeData &&
            _RefreshImmersiveColorPolicyState &&
            _ShouldAppsUseDarkMode &&
            _AllowDarkModeForWindow &&
            (_AllowDarkModeForApp || _SetPreferredAppMode) &&
            _IsDarkModeAllowedForWindow) {
                
            g_darkModeSupported = true;

            if (_AllowDarkModeForApp) _AllowDarkModeForApp(true);
            else if (_SetPreferredAppMode) _SetPreferredAppMode(true ? AllowDark : Default);

            _RefreshImmersiveColorPolicyState();
        }
    }
}
#endif