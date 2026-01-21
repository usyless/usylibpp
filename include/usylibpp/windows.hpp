#pragma once

#include "macros.hpp"
#include <string>
#include <optional>
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <wrl/client.h>
#include <shellapi.h>
#include <knownfolders.h>
#include <shlobj.h>
#include "types.hpp"
#include "opts.hpp"

namespace usylibpp::windows {
    /**
     * Convert a const char* into a std::wstring
     * Returns std::nullopt if the string is empty or on error
     */
    template <opts options = {}>
    [[nodiscard]] inline auto to_wstr(const char* utf8) -> types::opts_return<options, std::wstring> {
        using STR = std::wstring;

        const auto buffer_size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
        
        if (buffer_size == 0) {
            if constexpr (options.as_optional) return std::optional<STR>{std::nullopt};
            else return STR{};
        }

        STR wstr(buffer_size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr.data(), buffer_size);
        if constexpr (options.as_optional) return std::optional<STR>{std::move(wstr)};
        else return wstr;
    }

    template <opts options = {}>
    [[nodiscard]] inline auto to_wstr(const std::string& utf8) -> types::opts_return<options, std::wstring> {
        return to_wstr<options>(utf8.c_str());
    }

    USYLIBPP__MAKE_OR(to_wstr, std::wstring{})

    template<types::wchar_t_strict T>
    [[nodiscard]] inline constexpr const wchar_t* wchar_t_from_strict(T&& str) {
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
            return str.native().c_str();
        } else {
            static_assert(!std::is_same_v<T, T>, "Unsupported type passed to usylibpp::wchar_t_from_compatible, must have forgotten a branch");
        }
    }

     /**
     * Convert any compatible wide string into a std::string
     * Returns std::nullopt if the string is empty or on error
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

        STR utf8_str(buffer_size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8_str.data(), buffer_size - 1, nullptr, nullptr);

        if constexpr (options.as_optional) return std::optional<STR>{std::move(utf8_str)};
        else return utf8_str;
    }

    USYLIBPP__MAKE_OR(to_utf8, std::string{})

    /**
     * If dummy = true then COM is not actually initialised again
     */
    template <bool dummy = false>
    class COMWrapper {
    private:
        HRESULT hr;
    public:
        COMWrapper() : hr(dummy ? 1 : (CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {}

        [[nodiscard]] constexpr HRESULT status() {
            return hr;
        }

        ~COMWrapper() {
            if constexpr (!dummy) {
                if (SUCCEEDED(hr)) CoUninitialize();
            }
        }
    };

    /**
     * Pass true into ComInitialised to not re-initialise COM
     */
    template <bool ComInitialised = false, types::wchar_t_compatible T>
    [[nodiscard]] inline bool recycle_file(T&& wstr) {
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

    template <types::wchar_t_compatible T>
    [[nodiscard]] inline bool open_file_in_default_app(T&& file_path) {
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

    /**
     * Pass true into ComInitialised to not re-initialise COM
     */
    template <bool ComInitialised = false, types::wchar_t_compatible T>
    [[nodiscard]] inline bool show_file_in_explorer(T&& file_path) {
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

    /**
     * Caches the result
     */
    [[nodiscard]] inline std::optional<std::reference_wrapper<const std::filesystem::path>> current_executable_path() {
        static bool has_run = false;
        static std::filesystem::path path;
        if (has_run) {
            if (path.empty()) return std::nullopt;
            return path;
        }

        has_run = true;

        DWORD size = 260;
        DWORD copied = 0;
        std::wstring buffer;

        while (true) {
            buffer.resize(size);
            copied = GetModuleFileNameW(nullptr, buffer.data(), size);

            if (copied == 0) return std::nullopt;
            if (copied < (size - 1)) break;

            size *= 2;
        };

        buffer.resize(copied);

        if (buffer.empty()) return std::nullopt;

        path = buffer;

        return path;
    }

    namespace internal {
        inline const std::filesystem::path empty_path{};
    }

    USYLIBPP__MAKE_OR(current_executable_path, internal::empty_path)

    [[nodiscard]] inline bool set_cwd_to_executable_directory() {
        auto exe_path_opt = current_executable_path();

        if (!exe_path_opt) return false;

        auto& exe_path = exe_path_opt.value();

        const auto pos = exe_path.get().native().find_last_of(L'\\');
        if (pos != std::wstring::npos) {
            // make a copy here
            std::wstring exe_path_copy{exe_path.get()};
            exe_path_copy.resize(pos);
            if (SetCurrentDirectoryW(exe_path_copy.c_str())) return true;
        }

        return false;
    }

    /**
     * Downloads folder by default
     * Pass in any FOLDERID_XXXXXX
     */
    [[nodiscard]] inline std::optional<std::filesystem::path> get_known_folder(const GUID& folder = FOLDERID_Downloads) {
        PWSTR path = nullptr;

        HRESULT hr = SHGetKnownFolderPath(folder, 0, nullptr, &path);

        if (FAILED(hr)) return std::nullopt;

        std::filesystem::path folder_path{path};
        CoTaskMemFree(path);
        return folder_path;
    }

    USYLIBPP__MAKE_OR(get_known_folder, std::filesystem::path{})

    /**
     * Pass true into ComInitialised to not re-initialise COM
     */
    template <bool ComInitialised = false>
    [[nodiscard]] inline std::optional<std::filesystem::path> get_folder_picker() {
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

        hr = pfd->Show(nullptr);
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

    USYLIBPP__MAKE_OR(get_folder_picker, std::filesystem::path{})

    /**
     * No stdin, just stdout and stderr
     */
    inline void show_console_for_gui_app() {
        AllocConsole();
        FILE* fp_stdout;
        freopen_s(&fp_stdout, "CONOUT$", "w", stdout);

        FILE* fp_stderr;
        freopen_s(&fp_stderr, "CONOUT$", "w", stderr);
    }

    /**
     * Make sure to pass in just the name without the extension
     * Checks both the current directory and the PATH
     */
    [[nodiscard]] inline bool exe_exists(const std::wstring& exeName) {
        std::error_code ec;
        return std::filesystem::exists(exeName + L".exe", ec) || SearchPathW(nullptr, exeName.c_str(), L".exe", 0, nullptr, nullptr) > 0;
    }

    /**
     * Returns the text in the clipboard from a given hwnd
     * Converts to wstring
     * I don't think the clipboard stuff is thread safe
     */
    [[nodiscard]] inline std::wstring get_clipboard_text(const HWND hwnd) {
        if (!OpenClipboard(hwnd)) return {};

        std::wstring text;

        if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                LPCWSTR pszText = static_cast<LPCWSTR>(GlobalLock(hData));
                if (pszText) {
                    text = pszText;
                    GlobalUnlock(hData);
                }
            }
        }
        else if (IsClipboardFormatAvailable(CF_TEXT)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                LPCSTR pszText = static_cast<LPCSTR>(GlobalLock(hData));
                if (pszText) {
                    text = to_wstr_or_default(pszText);
                    GlobalUnlock(hData);
                }
            }
        }

        CloseClipboard();

        return text;
    }

    /**
     * Get a vector of wstrings for the drag query files in a hDrop
     */
    inline std::vector<std::wstring> get_drag_query_files(const HDROP hDrop) {
        UINT file_count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        if (file_count <= 0) {
            return {};
        }

        std::vector<std::wstring> files;

        for (std::size_t file_index{0}; file_index < file_count; ++file_index) {
            UINT len = DragQueryFileW(hDrop, file_index, NULL, 0);

            if (len == 0) continue;

            ++len; // include null terminator

            std::wstring& buffer = files.emplace_back(len, L'\0');
            UINT copied = DragQueryFileW(hDrop, file_index, buffer.data(), len);

            if (copied <= 0) {
                files.pop_back();
                continue;
            }

            buffer.resize(copied);
        }

        return files;
    }

    namespace admin {
        [[nodiscard]] inline bool is_admin() {
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
                CheckTokenMembership(nullptr, adminGroup, &isAdmin);
                FreeSid(adminGroup);
            }

            return (is_admin = static_cast<bool>(isAdmin));
        }

        /**
         * Exits the program on success
         */
        [[nodiscard]] inline bool relaunch_as_admin() {
            auto exe_path_option = current_executable_path();

            if (!exe_path_option) return false;

            SHELLEXECUTEINFOW sei{};
            
            sei.cbSize = sizeof(sei);
            sei.lpVerb = L"runas";
            sei.lpFile = exe_path_option.value().get().c_str();
            sei.hwnd = nullptr;
            sei.nShow = SW_NORMAL;

            if (!ShellExecuteExW(&sei)) return false;

            exit(0);
            return true;
        }
    }

    #ifdef USYLIBPP_ENABLE_TASK_DIALOG_DARK_MODE
    // Embed the manifest to enable task dialogs
    #pragma comment(linker, \
    "\"/manifestdependency:type='win32' " \
    "name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' " \
    "processorArchitecture='*' " \
    "publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")

    namespace task_dialog {
        namespace internal {
            inline int create(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon, TASKDIALOG_BUTTON* buttons, UINT buttons_size) {
                TASKDIALOGCONFIG td_config{};
                td_config.cbSize = sizeof(td_config);
                td_config.hwndParent = nullptr;
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

            inline void ok(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon) {
                TASKDIALOG_BUTTON buttons[] = { { IDOK, L"Ok" } };
                create(title, message, mainContent, icon, buttons, ARRAYSIZE(buttons));
            }

            [[nodiscard]] inline bool confirmation(PCWSTR title, PCWSTR message, PCWSTR mainContent, PCWSTR icon) {
                TASKDIALOG_BUTTON buttons[] = { 
                    { IDOK, L"Confirm" },
                    { IDCANCEL, L"Cancel" } 
                };
                return create(title, message, mainContent, icon, buttons, ARRAYSIZE(buttons)) == IDOK;
            }
        }

        template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
        inline void error(T1&& title, T2&& message, T3&& message_body) noexcept {
            internal::ok(
                wchar_t_from_strict(std::forward<T1>(title)), 
                wchar_t_from_strict(std::forward<T2>(message)), 
                wchar_t_from_strict(std::forward<T3>(message_body)), 
                TD_ERROR_ICON
            );
        }

        template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
        inline void warning(T1&& title, T2&& message, T3&& message_body) noexcept {
            internal::ok(
                wchar_t_from_strict(std::forward<T1>(title)), 
                wchar_t_from_strict(std::forward<T2>(message)), 
                wchar_t_from_strict(std::forward<T3>(message_body)), 
                TD_WARNING_ICON
            );
        }

        template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
        inline void info(T1&& title, T2&& message, T3&& message_body) noexcept {
            internal::ok(
                wchar_t_from_strict(std::forward<T1>(title)), 
                wchar_t_from_strict(std::forward<T2>(message)), 
                wchar_t_from_strict(std::forward<T3>(message_body)), 
                TD_INFORMATION_ICON
            );
        }

        template <types::wchar_t_strict T1, types::wchar_t_strict T2, types::wchar_t_strict T3>
        [[nodiscard]] inline bool confirmation(T1&& title, T2&& message, T3&& message_body) noexcept {
            return internal::confirmation(
                wchar_t_from_strict(std::forward<T1>(title)), 
                wchar_t_from_strict(std::forward<T2>(message)), 
                wchar_t_from_strict(std::forward<T3>(message_body)), 
                TD_INFORMATION_ICON
            );
        }
    }

    namespace darkmode {
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
        inline bool allow_dark_mode_for_window(HWND hWnd, bool allow) {
            if (g_darkModeSupported) return _AllowDarkModeForWindow(hWnd, allow);
            return false;
        }

        inline constexpr bool windows_build_supports_darkmode(DWORD buildNumber) {
            return buildNumber >= 17763;
        }

        /**
        * Call this to initialise the dark mode win32 stuff, then call allow_dark_mode_for_window on your individual hwnd
        */
        inline void init_dark_mode() {
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
}