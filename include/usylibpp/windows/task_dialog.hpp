#pragma once

#ifdef USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE
#include "../aliases.hpp" // IWYU pragma: export
#include <windows.h>
#include <shobjidl.h>
#include "../types.hpp"

#pragma comment(linker, \
"\"/manifestdependency:type='win32' " \
"name='Microsoft.Windows.Common-Controls' " \
"version='6.0.0.0' " \
"processorArchitecture='*' " \
"publicKeyToken='6595b64144ccf1df' " \
"language='*'\"")

namespace usylibpp::windows::task_dialog {
    inline constexpr int TIMED_OUT = 0x7000;

    namespace internal {
        inline HRESULT CALLBACK TaskDialogTimerProc(HWND hwnd, UINT uNotification, WPARAM wParam, LPARAM, LONG_PTR dwRefData) {
            if (uNotification == TDN_TIMER) {
                const DWORD elapsedMs = (DWORD)wParam;
                const DWORD timeoutMs = (DWORD)dwRefData;

                if (elapsedMs >= timeoutMs) {
                    SendMessage(hwnd, TDM_CLICK_BUTTON, TIMED_OUT, 0);
                    return S_OK;
                }
                return S_FALSE;
            }
            return S_OK;
        }

        inline int create(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon, TASKDIALOG_BUTTON* buttons, UINT buttons_size, HWND hwnd = nullptr, DWORD timeoutMs = 0) {
            TASKDIALOGCONFIG td_config{};
            td_config.cbSize = sizeof(td_config);
            td_config.hwndParent = hwnd;
            td_config.pszWindowTitle = title;
            td_config.pszMainInstruction = message;
            td_config.pszContent = mainContent;
            td_config.pszMainIcon = icon;
            td_config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;

            if (timeoutMs > 0) {
                td_config.dwFlags |= TDF_CALLBACK_TIMER;
                td_config.pfCallback = TaskDialogTimerProc;
                td_config.lpCallbackData = (LONG_PTR)timeoutMs;
            }

            td_config.pButtons = buttons;
            td_config.cButtons = buttons_size;

            int buttonPressed = 0;
            TaskDialogIndirect(&td_config, &buttonPressed, nullptr, nullptr);
            return buttonPressed;
        }

        inline int ok(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon, HWND hwnd = nullptr, DWORD timeoutMs = 0) {
            TASKDIALOG_BUTTON buttons[] = { { IDOK, L"Ok" }, { TIMED_OUT, L"" } };
            const UINT count = (timeoutMs > 0) ? 2u : 1u;
            return create(title, message, mainContent, icon, buttons, count, hwnd, timeoutMs);
        }

        [[nodiscard]] inline bool confirmation(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon, HWND hwnd = nullptr, DWORD timeoutMs = 0) {
            TASKDIALOG_BUTTON buttons[] = {
                { IDOK, L"Confirm" },
                { IDCANCEL, L"Cancel" },
                { TIMED_OUT, L"" }
            };
            const UINT count = (timeoutMs > 0) ? 3u : 2u;
            return create(title, message, mainContent, icon, buttons, count, hwnd, timeoutMs) == IDOK;
        }
    }

    template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
    inline void error(T1&& title, T2&& message, T3&& message_body, HWND hwnd = nullptr, DWORD timeoutMs = 0) noexcept {
        internal::ok(
            wchar_t_from_strict(std::forward<T1>(title)), 
            wchar_t_from_strict(std::forward<T2>(message)), 
            wchar_t_from_strict(std::forward<T3>(message_body)), 
            TD_ERROR_ICON,
            hwnd,
            timeoutMs
        );
    }

    template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
    inline void warning(T1&& title, T2&& message, T3&& message_body, HWND hwnd = nullptr, DWORD timeoutMs = 0) noexcept {
        internal::ok(
            wchar_t_from_strict(std::forward<T1>(title)), 
            wchar_t_from_strict(std::forward<T2>(message)), 
            wchar_t_from_strict(std::forward<T3>(message_body)), 
            TD_WARNING_ICON,
            hwnd,
            timeoutMs
        );
    }

    template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
    inline void info(T1&& title, T2&& message, T3&& message_body, HWND hwnd = nullptr, DWORD timeoutMs = 0) noexcept {
        internal::ok(
            wchar_t_from_strict(std::forward<T1>(title)), 
            wchar_t_from_strict(std::forward<T2>(message)), 
            wchar_t_from_strict(std::forward<T3>(message_body)), 
            TD_INFORMATION_ICON,
            hwnd,
            timeoutMs
        );
    }

    template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
    [[nodiscard]] inline bool confirmation(T1&& title, T2&& message, T3&& message_body, HWND hwnd = nullptr, DWORD timeoutMs = 0) noexcept {
        return internal::confirmation(
            wchar_t_from_strict(std::forward<T1>(title)), 
            wchar_t_from_strict(std::forward<T2>(message)), 
            wchar_t_from_strict(std::forward<T3>(message_body)), 
            TD_INFORMATION_ICON,
            hwnd,
            timeoutMs
        );
    }
}
#endif