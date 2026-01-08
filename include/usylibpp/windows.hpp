#pragma once

#include <string>
#include <optional>
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <wrl/client.h>
#include <shellapi.h>
#include <knownfolders.h>
#include <shlobj.h>
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
        if constexpr (strings::is_wchar_ptr_v<T>) {
            return str;
        } else if constexpr (strings::is_wstring_v<T>) {
            return str.c_str();
        } else if constexpr (strings::is_string_v<T>) {
            static thread_local std::optional<std::wstring> buffer;
            buffer = to_wstr(str);
            if (!buffer) return L"";
            return buffer->c_str();
        } else if constexpr (strings::is_filesystem_path_v<T>) {
            return str.native().c_str();
        } else {
            static_assert(!std::is_same_v<T, T>, "Unsupported type passed to usylibpp::windows::wchar_t_from_compatible, must have forgotten a branch");
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

    // If dummy = true then COM is not actually initialised again
    template <bool dummy = false>
    class COMWrapper {
    private:
        HRESULT hr;
    public:
        COMWrapper() : hr(dummy ? 1 : (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {}

        constexpr HRESULT status() {
            return hr;
        }

        ~COMWrapper() {
            if constexpr (!dummy) {
                if (SUCCEEDED(hr)) CoUninitialize();
            }
        }
    };

    // Pass true into ComInitialised to not re-initialise COM
    template <bool ComInitialised = false, strings::wchar_t_compatible T>
    inline bool recycle_file(T&& wstr) {
        using Microsoft::WRL::ComPtr;

        COMWrapper<ComInitialised> COM{};
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

    // Pass true into ComInitialised to not re-initialise COM
    template <bool ComInitialised = false, strings::wchar_t_compatible T>
    inline bool show_file_in_exporer(T&& file_path) {
        COMWrapper<ComInitialised> COM{};
        auto hr = COM.status();
        if (FAILED(hr)) return false;
        
        PIDLIST_ABSOLUTE pidlFolder = nullptr;

        hr = SHParseDisplayName(wchar_t_from_compatible(std::forward<T>(file_path)), nullptr, &pidlFolder, 0, nullptr);
        if (FAILED(hr) || !pidlFolder) return false;

        hr = SHOpenFolderAndSelectItems(pidlFolder, 0, nullptr, 0);
        CoTaskMemFree(pidlFolder);
        if (FAILED(hr)) return false;

        return true;
    }

    // Caches the result
    inline std::optional<std::reference_wrapper<const std::wstring>> current_executable_path() {
        static bool has_run = false;
        static std::wstring buffer;
        if (has_run) {
            if (buffer.empty()) return std::nullopt;
            return buffer;
        }

        has_run = true;

        DWORD size = 260;
        DWORD copied = 0;

        while (true) {
            buffer.resize(size);
            copied = GetModuleFileNameW(NULL, buffer.data(), size);

            if (copied == 0) return std::nullopt;
            if (copied < (size - 1)) break;

            size *= 2;
        };

        buffer.resize(copied);

        if (buffer.empty()) return std::nullopt;

        return buffer;
    }

    inline bool set_current_cwd_to_executable_directory() {
        auto exe_path_opt = current_executable_path();

        if (!exe_path_opt) return false;

        auto exe_path = exe_path_opt.value();

        const auto pos = exe_path.get().find_last_of(L'\\');
        if (pos != std::wstring::npos) {
            // make a copy here
            std::wstring exe_path_copy{exe_path_opt.value()};
            exe_path_copy.resize(pos);
            if (SetCurrentDirectoryW(exe_path_copy.c_str())) return true;
        }

        return false;
    }

    // Downloads folder by default
    // Pass in any FOLDERID_XXXXXX
    inline std::optional<std::filesystem::path> get_known_folder(const GUID& folder = FOLDERID_Downloads) {
        PWSTR path = nullptr;

        HRESULT hr = SHGetKnownFolderPath(folder, 0, NULL, &path);

        if (FAILED(hr)) return std::nullopt;

        std::filesystem::path folder_path{path};
        CoTaskMemFree(path);
        return folder_path;
    }

    // Pass true into ComInitialised to not re-initialise COM
    template <bool ComInitialised = false>
    inline std::optional<std::filesystem::path> get_folder_picker() {
        using Microsoft::WRL::ComPtr;

        COMWrapper<ComInitialised> COM{};
        auto hr = COM.status();
        if (FAILED(hr)) return std::nullopt;

        ComPtr<IFileDialog> pfd;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
        if (FAILED(hr)) return std::nullopt;

        DWORD dwOptions{};
        pfd->GetOptions(&dwOptions);
        pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);

        hr = pfd->Show(NULL);
        if (FAILED(hr)) return std::nullopt;

        ComPtr<IShellItem> psi;
        hr = pfd->GetResult(&psi);
        if (FAILED(hr)) return std::nullopt;

        PWSTR path = nullptr;
        hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &path);

        if (FAILED(hr) or (not path)) return std::nullopt;

        std::filesystem::path selected_path{path};
        CoTaskMemFree(path);

        return selected_path;
    }

    // No stdin, just stdout and stderr
    inline void show_console_for_gui_app() {
        AllocConsole();
        FILE* fp_stdout;
        freopen_s(&fp_stdout, "CONOUT$", "w", stdout);

        FILE* fp_stderr;
        freopen_s(&fp_stderr, "CONOUT$", "w", stderr);
    }

    // Make sure to pass in just the name without the extension
    // Checks both the current directory and the PATH
    inline bool exe_exists(const std::wstring& exeName) {
        std::error_code ec;
        return std::filesystem::exists(exeName + L".exe", ec) || SearchPathW(NULL, exeName.c_str(), L".exe", 0, NULL, NULL) > 0;
    }

    namespace admin {
        inline bool is_admin() {
            static bool has_run = false;
            static bool is_admin = false;

            if (has_run) return is_admin;
            has_run = true;

            BOOL isAdmin = FALSE;
            PSID adminGroup;
            SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

            if (AllocateAndInitializeSid(&ntAuthority, 2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0,
                &adminGroup)) {
                CheckTokenMembership(NULL, adminGroup, &isAdmin);
                FreeSid(adminGroup);
            }

            return (is_admin = static_cast<bool>(isAdmin));
        }

        // Exits the program on success
        inline bool relaunch_as_admin() {
            auto exe_path_option = current_executable_path();

            if (!exe_path_option) return false;

            SHELLEXECUTEINFOW sei = {};
            
            sei.cbSize = sizeof(sei);
            sei.lpVerb = L"runas";
            sei.lpFile = exe_path_option.value().get().c_str();
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;

            if (!ShellExecuteExW(&sei)) return false;

            exit(0);
            return true;
        }
    }

    namespace task_dialog {
        namespace internal {
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

        template <strings::wchar_t_strict T1, strings::wchar_t_strict T2, strings::wchar_t_strict T3>
        inline void error(T1&& title, T2&& message, T3&& message_body) noexcept {
            internal::ok(
                strings::wchar_t_from_strict(std::forward<T1>(title)), 
                strings::wchar_t_from_strict(std::forward<T2>(message)), 
                strings::wchar_t_from_strict(std::forward<T3>(message_body)), 
                TD_ERROR_ICON
            );
        }

        template <strings::wchar_t_strict T1, strings::wchar_t_strict T2, strings::wchar_t_strict T3>
        inline void warning(T1&& title, T2&& message, T3&& message_body) noexcept {
            internal::ok(
                strings::wchar_t_from_strict(std::forward<T1>(title)), 
                strings::wchar_t_from_strict(std::forward<T2>(message)), 
                strings::wchar_t_from_strict(std::forward<T3>(message_body)), 
                TD_WARNING_ICON
            );
        }

        template <strings::wchar_t_strict T1, strings::wchar_t_strict T2, strings::wchar_t_strict T3>
        inline void info(T1&& title, T2&& message, T3&& message_body) noexcept {
            internal::ok(
                strings::wchar_t_from_strict(std::forward<T1>(title)), 
                strings::wchar_t_from_strict(std::forward<T2>(message)), 
                strings::wchar_t_from_strict(std::forward<T3>(message_body)), 
                TD_INFORMATION_ICON
            );
        }

        template <strings::wchar_t_strict T1, strings::wchar_t_strict T2, strings::wchar_t_strict T3>
        inline bool confirmation(T1&& title, T2&& message, T3&& message_body) noexcept {
            return internal::confirmation(
                strings::wchar_t_from_strict(std::forward<T1>(title)), 
                strings::wchar_t_from_strict(std::forward<T2>(message)), 
                strings::wchar_t_from_strict(std::forward<T3>(message_body)), 
                TD_INFORMATION_ICON
            );
        }
    }
}