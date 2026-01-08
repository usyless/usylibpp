#pragma once

#include <string>
#include <filesystem>
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <wrl/client.h>

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

    inline bool recycle_file(const wchar_t* wstr) {
        using Microsoft::WRL::ComPtr;

        COMWrapper COM{};
        if (FAILED(COM.status())) return false;

        ComPtr<IFileOperation> fileOp;
        HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(fileOp.GetAddressOf()));
        if (FAILED(hr)) return false;

        ComPtr<IShellItem> item;
        hr = SHCreateItemFromParsingName(wstr, nullptr, IID_PPV_ARGS(item.GetAddressOf()));
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
}