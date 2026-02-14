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
    namespace internal {
        inline int create(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon, TASKDIALOG_BUTTON* buttons, UINT buttons_size, HWND hwnd = nullptr) {
            TASKDIALOGCONFIG td_config{};
            td_config.cbSize = sizeof(td_config);
            td_config.hwndParent = hwnd;
            td_config.pszWindowTitle = title;
            td_config.pszMainInstruction = message;
            td_config.pszContent = mainContent;
            td_config.pszMainIcon = icon;
            td_config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;

            td_config.pButtons = buttons;
            td_config.cButtons = buttons_size;

            int buttonPressed = 0;
            TaskDialogIndirect(&td_config, &buttonPressed, nullptr, nullptr);
            return buttonPressed;
        }

        inline void ok(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon, HWND hwnd = nullptr) {
            TASKDIALOG_BUTTON buttons[] = { { IDOK, L"Ok" } };
            create(title, message, mainContent, icon, buttons, ARRAYSIZE(buttons), hwnd);
        }

        [[nodiscard]] inline bool confirmation(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon, HWND hwnd = nullptr) {
            TASKDIALOG_BUTTON buttons[] = { 
                { IDOK, L"Confirm" },
                { IDCANCEL, L"Cancel" } 
            };
            return create(title, message, mainContent, icon, buttons, ARRAYSIZE(buttons), hwnd) == IDOK;
        }
    }

    template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
    inline void error(T1&& title, T2&& message, T3&& message_body, HWND hwnd = nullptr) noexcept {
        internal::ok(
            wchar_t_from_strict(std::forward<T1>(title)), 
            wchar_t_from_strict(std::forward<T2>(message)), 
            wchar_t_from_strict(std::forward<T3>(message_body)), 
            TD_ERROR_ICON,
            hwnd
        );
    }

    template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
    inline void warning(T1&& title, T2&& message, T3&& message_body, HWND hwnd = nullptr) noexcept {
        internal::ok(
            wchar_t_from_strict(std::forward<T1>(title)), 
            wchar_t_from_strict(std::forward<T2>(message)), 
            wchar_t_from_strict(std::forward<T3>(message_body)), 
            TD_WARNING_ICON,
            hwnd
        );
    }

    template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
    inline void info(T1&& title, T2&& message, T3&& message_body, HWND hwnd = nullptr) noexcept {
        internal::ok(
            wchar_t_from_strict(std::forward<T1>(title)), 
            wchar_t_from_strict(std::forward<T2>(message)), 
            wchar_t_from_strict(std::forward<T3>(message_body)), 
            TD_INFORMATION_ICON,
            hwnd
        );
    }

    template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
    [[nodiscard]] inline bool confirmation(T1&& title, T2&& message, T3&& message_body, HWND hwnd = nullptr) noexcept {
        return internal::confirmation(
            wchar_t_from_strict(std::forward<T1>(title)), 
            wchar_t_from_strict(std::forward<T2>(message)), 
            wchar_t_from_strict(std::forward<T3>(message_body)), 
            TD_INFORMATION_ICON,
            hwnd
        );
    }
}
#endif