#pragma once

#include "macros.hpp"
#include <functional>
#include <string>
#include <optional>
#include <variant>
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shellapi.h>
#include <knownfolders.h>
#include <shlobj.h>
#include "types.hpp"
#include "windows/strings.hpp"

#ifdef USYLIBPP_ENABLE_WIL
#include <wil/resource.h>
#include <wil/com.h>
#endif

namespace usylibpp::windows {
    /**
     * If dummy = true then COM is not actually initialised again
     */
    template <bool dummy = false>
    class COMWrapper {
    private:
        HRESULT hr;
    public:
        COMWrapper(DWORD co_init_flags = COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) : hr(dummy ? 1 : (CoInitializeEx(nullptr, co_init_flags))) {}

        [[nodiscard]] constexpr HRESULT status() noexcept {
            return hr;
        }

        ~COMWrapper() {
            if constexpr (!dummy) {
                if (SUCCEEDED(hr)) CoUninitialize();
            }
        }
    };

    #ifdef USYLIBPP_ENABLE_WIL
    /**
     * Pass true into ComInitialised to not re-initialise COM
     */
    template <bool ComInitialised = false, types::wchar_t_compatible T>
    [[nodiscard]] inline bool recycle_file(T&& wstr) {
        COMWrapper<ComInitialised> COM{};
        if (FAILED(COM.status())) return false;

        wil::com_ptr<IFileOperation> fileOp;
        HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileOp));
        if (FAILED(hr)) return false;

        wil::com_ptr<IShellItem> item;
        hr = SHCreateItemFromParsingName(wchar_t_from_compatible(std::forward<T>(wstr)), nullptr, IID_PPV_ARGS(&item));
        if (FAILED(hr)) return false;

        fileOp->SetOperationFlags(FOFX_RECYCLEONDELETE);

        hr = fileOp->DeleteItem(item.get(), nullptr);
        if (FAILED(hr)) return false;
        
        hr = fileOp->PerformOperations();
        if (FAILED(hr)) return false;

        BOOL anyFailed = FALSE;
        fileOp->GetAnyOperationsAborted(&anyFailed);

        return !anyFailed;
    }
    #endif

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

    #ifdef USYLIBPP_ENABLE_WIL
    /**
     * Pass true into ComInitialised to not re-initialise COM
     */
    template <bool ComInitialised = false, types::wchar_t_compatible T>
    [[nodiscard]] inline bool show_file_in_explorer(T&& file_path) {
        COMWrapper<ComInitialised> COM{};
        auto hr = COM.status();
        if (FAILED(hr)) return false;
        
        wil::unique_cotaskmem_ptr<ITEMIDLIST> pidlFolder = nullptr;

        hr = SHParseDisplayName(wchar_t_from_compatible(std::forward<T>(file_path)), nullptr, wil::out_param(pidlFolder), 0, nullptr);
        if (FAILED(hr) || !pidlFolder) return false;

        hr = SHOpenFolderAndSelectItems(pidlFolder.get(), 0, nullptr, 0);
        if (FAILED(hr)) return false;

        return true;
    }
    #endif

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

        auto& exe_path = *exe_path_opt;

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
        wil::unique_cotaskmem_string path = nullptr;

        HRESULT hr = SHGetKnownFolderPath(folder, 0, nullptr, &path);
        if (FAILED(hr)) return std::nullopt;

        return path.get();
    }

    USYLIBPP__MAKE_OR(get_known_folder, std::filesystem::path{})

    #ifdef USYLIBPP_ENABLE_WIL
    /**
     * Pass true into ComInitialised to not re-initialise COM
     */
    template <bool ComInitialised = false>
    [[nodiscard]] inline std::optional<std::filesystem::path> get_folder_picker() {
        COMWrapper<ComInitialised> COM{};
        auto hr = COM.status();
        if (FAILED(hr)) return std::nullopt;

        wil::com_ptr<IFileDialog> pfd;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
        if (FAILED(hr)) return std::nullopt;

        DWORD dwOptions{};
        pfd->GetOptions(&dwOptions);
        pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);

        hr = pfd->Show(nullptr);
        if (FAILED(hr)) return std::nullopt;

        wil::com_ptr<IShellItem> psi;
        hr = pfd->GetResult(&psi);
        if (FAILED(hr)) return std::nullopt;

        wil::unique_cotaskmem_string path = nullptr;
        hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &path);

        if (FAILED(hr) or (not path)) return std::nullopt;

        return path.get();
    }

    USYLIBPP__MAKE_OR(get_folder_picker, std::filesystem::path{})
    #endif

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

    [[nodiscard]] inline bool is_window_focused(HWND hwnd) {
        return GetForegroundWindow() == hwnd;
    }

    #ifdef USYLIBPP_ENABLE_WIL
    /**
     * Returns the text in the clipboard from a given hwnd
     * Converts to wstring
     * I don't think the clipboard stuff is thread safe
     */
    [[nodiscard]] inline std::optional<std::variant<std::wstring, std::string>> get_clipboard_text(const HWND hwnd) {
        auto clipboard = wil::open_clipboard(hwnd);
        if (!clipboard) return std::nullopt;

        if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (!hData) return std::nullopt;

            wil::unique_hglobal_locked pszText{hData};
            if (!pszText) return std::nullopt;

            return std::wstring{static_cast<LPCWSTR>(pszText.get())};
        }
        else if (IsClipboardFormatAvailable(CF_TEXT)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (!hData) return std::nullopt;

            wil::unique_hglobal_locked pszText{hData};
            if (!pszText) return std::nullopt;

            return std::string{static_cast<LPCSTR>(pszText.get())};
        }

        return std::nullopt;
    }

    USYLIBPP__MAKE_OR(get_clipboard_text, std::wstring{})

    [[nodiscard]] inline std::optional<std::wstring> get_clipboard_text_as_wstring(const HWND hwnd) {
        auto res = get_clipboard_text(hwnd);
        if (!res) return std::nullopt;

        return std::visit([](auto&& s) -> std::optional<std::wstring> {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, std::wstring>) return s;
            else return to_wstr(s);
        }, *res);
    }

    USYLIBPP__MAKE_OR(get_clipboard_text_as_wstring, std::wstring{})

    [[nodiscard]] inline std::optional<std::string> get_clipboard_text_as_string(const HWND hwnd) {
        auto res = get_clipboard_text(hwnd);
        if (!res) return std::nullopt;

        return std::visit([](auto&& s) -> std::optional<std::string> {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, std::string>) return s;
            else return to_utf8(s);
        }, *res);
    }

    USYLIBPP__MAKE_OR(get_clipboard_text_as_string, std::string{})
    #endif

    /**
     * Get a vector of wstrings for the drag query files in a hDrop
     */
    [[nodiscard]] inline std::vector<std::wstring> get_drag_query_files(const HDROP hDrop) {
        UINT file_count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        if (file_count <= 0) {
            return {};
        }

        std::vector<std::wstring> files;

        for (UINT file_index{0}; file_index < file_count; ++file_index) {
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

    #ifdef USYLIBPP_ENABLE_WIL
    template <bool ComInitialised = false>
    struct TaskbarProgress {
        COMWrapper<ComInitialised> COM{COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE};
        wil::com_ptr<ITaskbarList3> taskbar = nullptr;
        HWND hwnd_ = nullptr;
        HRESULT hr;

        /**
         * You must check the status before using any method
         */
        TaskbarProgress(HWND hwnd) : hr(COM.status()), hwnd_(hwnd) {
            if (FAILED(hr)) return;

            hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar));
            if (FAILED(hr)) taskbar = nullptr;
        }

        [[nodiscard]] constexpr HRESULT status() noexcept {
            return hr;
        }

        bool set_progress(int value, int max) {
            if (!taskbar) return false;

            hr = taskbar->SetProgressState(hwnd_, TBPF_NORMAL);
            if (FAILED(hr)) return false;

            hr = taskbar->SetProgressValue(hwnd_, std::clamp(value, 0, max), max);
            return SUCCEEDED(hr);
        }

        void cancel() {
            if (!taskbar) return;

            taskbar->SetProgressState(hwnd_, TBPF_NOPROGRESS);
            taskbar.reset();
        }

        ~TaskbarProgress() {
            cancel();
        }
    };
    #endif

    namespace admin {
        #ifdef USYLIBPP_ENABLE_WIL
        [[nodiscard]] inline bool is_admin() {
            static bool has_run = false;
            static bool is_admin = false;

            if (has_run) return is_admin;
            has_run = true;

            BOOL isAdmin = FALSE;
            SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

            wil::unique_sid adminGroup;
            if (AllocateAndInitializeSid(&ntAuthority, 2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0,
                &adminGroup)) {
                CheckTokenMembership(nullptr, adminGroup.get(), &isAdmin);
            }

            return (is_admin = static_cast<bool>(isAdmin));
        }
        #endif

        /**
         * Exits the program on success
         */
        [[nodiscard]] inline bool relaunch_as_admin() {
            auto exe_path_option = current_executable_path();

            if (!exe_path_option) return false;

            SHELLEXECUTEINFOW sei{};
            
            sei.cbSize = sizeof(sei);
            sei.lpVerb = L"runas";
            sei.lpFile = exe_path_option->get().c_str();
            sei.hwnd = nullptr;
            sei.nShow = SW_NORMAL;

            if (!ShellExecuteExW(&sei)) return false;

            exit(0);
            return true;
        }
    }
}