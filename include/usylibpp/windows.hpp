#pragma once

#include <string>
#include <optional>
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <wrl/client.h>
#include <shellapi.h>
#include "strings.hpp"

namespace usylibpp::windows {
    inline std::optional<std::wstring> to_wstr(const char* utf8) {
        const auto buffer_size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);

        if (buffer_size == 0) {
            return std::nullopt;
        }

        std::wstring wstr(buffer_size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr.data(), buffer_size);
        return wstr;
    }

    inline std::optional<std::wstring> to_wstr(const std::string& utf8) {
        return to_wstr(utf8.c_str());
    }

    // If its a string type this pointer will only survive to the next call on the thread
    template<strings::wchar_t_compatible T>
    inline const wchar_t* wchar_t_from_compatible(T&& str) {
        using U = std::remove_cvref_t<T>;

        if constexpr (strings::is_wchar_ptr_v<U>) {
            return str;
        }
        else if constexpr (strings::is_wstring_v<U>) {
            return str.c_str();
        }
        else if constexpr (strings::is_string_v<U>) {
            static thread_local std::optional<std::wstring> buffer;
            buffer = to_wstr(str);
            if (!buffer) return L"";
            return buffer->c_str();
        } else if constexpr (strings::is_filesystem_path_v<U>) {
            return str.native().c_str();
        }
    }

    template <strings::wchar_t_compatible T>
    inline std::optional<std::string> to_utf8(T&& _wstr) {
        const auto wstr = wchar_t_from_compatible(std::forward<T>(_wstr));

        const auto buffer_size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);

        if (buffer_size == 0) {
            return std::nullopt;
        }

        std::string utf8_str(buffer_size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8_str.data(), buffer_size - 1, nullptr, nullptr);

        return utf8_str;
    }

    class COMWrapper {
    private:
        HRESULT hr;
    public:
        COMWrapper() : hr(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}

        HRESULT status() {
            return hr;
        }

        ~COMWrapper() {
            if (SUCCEEDED(hr)) CoUninitialize();
        }
    };

    template <strings::wchar_t_compatible T>
    inline bool recycle_file(T&& wstr) {
        using Microsoft::WRL::ComPtr;

        COMWrapper COM{};
        if (FAILED(COM.status())) return false;

        ComPtr<IFileOperation> fileOp;
        HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(fileOp.GetAddressOf()));
        if (FAILED(hr)) return false;

        ComPtr<IShellItem> item;
        hr = SHCreateItemFromParsingName(wchar_t_from_compatible(std::forward<T>(wstr)), nullptr, IID_PPV_ARGS(item.GetAddressOf()));
        if (FAILED(hr)) return false;

        fileOp->SetOperationFlags(FOFX_RECYCLEONDELETE);

        hr = fileOp->DeleteItem(item.Get(), nullptr);
        if (FAILED(hr)) return false;
        
        hr = fileOp->PerformOperations();
        if (FAILED(hr)) return false;

        BOOL anyFailed = FALSE;
        fileOp->GetAnyOperationsAborted(&anyFailed);

        return !anyFailed;
    }

    template <strings::wchar_t_compatible T>
    inline bool open_file_in_default_app(T&& file_path) {
        HINSTANCE result = ShellExecuteW(
            nullptr,
            L"open",
            wchar_t_from_compatible(std::forward<T>(file_path)),
            nullptr,
            nullptr,
            SW_SHOWNORMAL
        );

        return reinterpret_cast<INT_PTR>(result) > 32;
    }

    inline std::optional<std::wstring> current_executable_path() {
        DWORD size = 260;
        std::wstring buffer;
        DWORD copied = 0;

        while (true) {
            buffer.resize(size);
            copied = GetModuleFileNameW(NULL, buffer.data(), size);

            if (copied == 0) {
                return std::nullopt;
            }

            if (copied < size - 1) { 
                break;
            }

            size *= 2;
        };

        buffer.resize(copied);

        return buffer;
    }

    namespace task_dialog {
        inline int create(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon, TASKDIALOG_BUTTON* buttons, UINT buttons_size) {
            TASKDIALOGCONFIG td_config{};
            td_config.cbSize = sizeof(td_config);
            td_config.hwndParent = NULL;
            td_config.pszWindowTitle = title;
            td_config.pszMainInstruction = message;
            td_config.pszContent = mainContent;
            td_config.pszMainIcon = icon;
            td_config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;

            td_config.pButtons = buttons;
            td_config.cButtons = buttons_size;

            int buttonPressed = 0;
            TaskDialogIndirect(&td_config, &buttonPressed, NULL, NULL);
            return buttonPressed;
        }

        inline void ok(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon) {
            TASKDIALOG_BUTTON buttons[] = { { IDOK, L"Ok" } };
            create(title, message, mainContent, icon, buttons, ARRAYSIZE(buttons));
        }

        inline bool confirmation(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon) {
            TASKDIALOG_BUTTON buttons[] = { 
                { IDOK, L"Confirm" },
                { IDCANCEL, L"Cancel" } 
            };
            return create(title, message, mainContent, icon, buttons, ARRAYSIZE(buttons)) == IDOK;
        }
    }
    
    inline void error_message(const std::wstring& title, const std::wstring& message, const std::wstring& message_body) noexcept {
        task_dialog::ok(title.c_str(), message.c_str(), message_body.c_str(), TD_ERROR_ICON);
    }

    inline void warning_message(const std::wstring& title, const std::wstring& message, const std::wstring& message_body) noexcept {
        task_dialog::ok(title.c_str(), message.c_str(), message_body.c_str(), TD_WARNING_ICON);
    }

    inline void info_message(const std::wstring& title, const std::wstring& message, const std::wstring& message_body) noexcept {
        task_dialog::ok(title.c_str(), message.c_str(), message_body.c_str(), TD_INFORMATION_ICON);
    }

    inline bool confirmation_message(const std::wstring& title, const std::wstring& message, const std::wstring& message_body) noexcept {
        return task_dialog::confirmation(title.c_str(), message.c_str(), message_body.c_str(), TD_INFORMATION_ICON);
    }
}