#pragma once

#include "aliases.hpp" // IWYU pragma: export
#include "macros.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <functional>
#include <string>
#include <optional>
#include <variant>
#include <vector>
#ifdef USYLIBPP_ENABLE_WINDOWS
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shellapi.h>
#include <knownfolders.h>
#include <shlobj.h>
#include "windows/strings.hpp"
#endif
#include "types.hpp"

#ifdef USYLIBPP_ENABLE_WIL
#include <wil/resource.h>
#include <wil/com.h>
#endif

namespace usylibpp::windows {
    #ifdef USYLIBPP_ENABLE_WINDOWS
    /**
     * If dummy = true then COM is not actually initialised again
     */
    template <bool dummy = false>
    class COMWrapper {
    private:
        HRESULT hr;
        bool should_uninitialise{false};
    public:
        explicit COMWrapper(DWORD co_init_flags = COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) : hr(dummy ? S_FALSE : (CoInitializeEx(nullptr, co_init_flags))) {
            if constexpr (!dummy) {
                if (hr == RPC_E_CHANGED_MODE) {
                    hr = S_FALSE;
                    should_uninitialise = false;
                } else {
                    should_uninitialise = SUCCEEDED(hr);
                }
            }
        }

        COMWrapper(const COMWrapper&) = delete;
        COMWrapper& operator=(const COMWrapper&) = delete;
        COMWrapper(COMWrapper&&) = delete;
        COMWrapper& operator=(COMWrapper&&) = delete;

        [[nodiscard]] constexpr HRESULT status() const noexcept {
            return hr;
        }

        ~COMWrapper() {
            if constexpr (!dummy) {
                if (should_uninitialise) CoUninitialize();
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

    #endif

    namespace internal {
        inline const std::filesystem::path empty_path{};

        [[nodiscard]] inline std::optional<std::filesystem::path> compute_executable_path() {
            #ifdef USYLIBPP_ENABLE_WINDOWS

            DWORD size = 260;
            DWORD copied = 0;
            std::wstring buffer;

            while (true) {
                buffer.resize(size);
                copied = GetModuleFileNameW(nullptr, buffer.data(), size);

                if (copied == 0) return std::nullopt;
                if (copied < (size - 1)) break;

                size *= 2;
            }

            buffer.resize(copied);

            if (buffer.empty()) return std::nullopt;

            return std::filesystem::path{buffer};

            #elif defined(USYLIBPP_ENABLE_LINUX)

            std::error_code ec;
            auto path = std::filesystem::canonical("/proc/self/exe", ec);
            if (ec || path.empty()) return std::nullopt;

            return path;

            #else

            return std::nullopt;
            #endif
        }
    }

    [[nodiscard]] inline std::optional<std::reference_wrapper<const std::filesystem::path>> current_executable_path() {
        static const std::optional<std::filesystem::path> cached = internal::compute_executable_path();
        if (!cached) return std::nullopt;
        return std::cref(*cached);
    }

    USYLIBPP__MAKE_OR(current_executable_path, internal::empty_path)

    [[nodiscard]] inline bool set_cwd_to_executable_directory() {
        auto exe_path_opt = current_executable_path();

        if (!exe_path_opt) return false;

        auto& exe_path = *exe_path_opt;

        #ifdef USYLIBPP_ENABLE_WINDOWS

        const auto parent = exe_path.get().parent_path();
        if (!parent.empty() && SetCurrentDirectoryW(parent.c_str())) return true;

        #endif

        #ifdef USYLIBPP_ENABLE_LINUX
        
        std::error_code ec;
        std::filesystem::current_path(exe_path.get().parent_path(), ec);
        if (!ec) return true;

        #endif

        return false;
    }

    #ifdef USYLIBPP_ENABLE_WINDOWS

    #ifdef USYLIBPP_ENABLE_WIL
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
    inline bool show_console_for_gui_app() {
        if (!AllocConsole() && GetLastError() != ERROR_ACCESS_DENIED) return false; // ACCESS_DENIED == already attached

        FILE* fp_stdout = nullptr;
        FILE* fp_stderr = nullptr;
        const bool ok = (freopen_s(&fp_stdout, "CONOUT$", "w", stdout) == 0) &&
                        (freopen_s(&fp_stderr, "CONOUT$", "w", stderr) == 0);

        std::ios::sync_with_stdio(true);
        std::cout.clear();
        std::cerr.clear();

        return ok;
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
        const UINT file_count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        if (file_count == 0) {
            return {};
        }

        std::vector<std::wstring> files;

        for (UINT file_index{0}; file_index < file_count; ++file_index) {
            UINT len = DragQueryFileW(hDrop, file_index, nullptr, 0);

            if (len == 0) continue;

            ++len; // include null terminator

            std::wstring& buffer = files.emplace_back(len, L'\0');
            UINT copied = DragQueryFileW(hDrop, file_index, buffer.data(), len);

            if (copied == 0) {
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
        HRESULT hr = E_FAIL;

        /**
         * You must check the status before using any method
         */
        explicit TaskbarProgress(HWND hwnd) : hwnd_(hwnd), hr(COM.status()) {
            if (FAILED(hr)) return;

            hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar));
            if (FAILED(hr)) taskbar = nullptr;
        }

        TaskbarProgress(const TaskbarProgress&) = delete;
        TaskbarProgress& operator=(const TaskbarProgress&) = delete;
        TaskbarProgress(TaskbarProgress&&) = delete;
        TaskbarProgress& operator=(TaskbarProgress&&) = delete;

        [[nodiscard]] constexpr HRESULT status() const noexcept {
            return hr;
        }

        bool set_progress(int value, int max) {
            if (!taskbar) return false;
            if (max <= 0) return false;

            hr = taskbar->SetProgressState(hwnd_, TBPF_NORMAL);
            if (FAILED(hr)) return false;

            hr = taskbar->SetProgressValue(hwnd_, static_cast<ULONGLONG>(std::clamp(value, 0, max)), static_cast<ULONGLONG>(max));
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
            static const bool cached = []() -> bool {
                BOOL isAdmin = FALSE;
                SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

                wil::unique_sid adminGroup;
                if (AllocateAndInitializeSid(&ntAuthority, 2,
                    SECURITY_BUILTIN_DOMAIN_RID,
                    DOMAIN_ALIAS_RID_ADMINS,
                    0, 0, 0, 0, 0, 0,
                    &adminGroup)) {
                    if (!CheckTokenMembership(nullptr, adminGroup.get(), &isAdmin)) isAdmin = FALSE;
                }

                return static_cast<bool>(isAdmin);
            }();

            return cached;
        }
        #endif

        /**
         * Exits the program on success
         */
        [[nodiscard]] inline bool relaunch_as_admin(const std::wstring& arguments = {}) {
            auto exe_path_option = current_executable_path();

            if (!exe_path_option) return false;

            SHELLEXECUTEINFOW sei{};

            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOASYNC;
            sei.lpVerb = L"runas";
            sei.lpFile = exe_path_option->get().c_str();
            sei.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
            sei.hwnd = nullptr;
            sei.nShow = SW_NORMAL;

            if (!ShellExecuteExW(&sei)) return false;

            std::exit(0);
            return true;
        }
    }
    #endif
}