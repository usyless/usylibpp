#include <usylibpp/usylibpp.hpp>

int main() {
    using namespace usylibpp;

    print::println("Hello! Welcome to my silly library\n");

    print::println("Time functions:");
    print::println("time::datetime_string: {}", time::datetime_string());
    print::println();

    print::println("String functions:");
    print::println("strings::concat_strings (chars): {}", strings::concat_strings("hello", " ", "there!"));
    print::println("strings::constexpr_strlen (\"hi!\"): {}", strings::constexpr_strlen("hi!"));
    // print::println(L"strings::concat_strings (wide chars): {}", strings::concat_strings(L"hello", L" ", L"there!"));
    {
        auto str = "THis IS moSTLY upperCASE";
        print::println("strings::to_lowercase before: {}, after: {}", str, strings::to_lowercase(str));
    }
    {
        auto str = "this has THIS STrING and once again THIS STrING";
        print::println("strings::replace_all before: {}, after: {}", str, strings::replace_all(str, "THIS STrING", "not that string"));
    }
    print::println("strings::to_number_or_default<size_t> {}", strings::to_number_or_default<size_t>("1234567"));
    print::println("strings::to_number_or_default<double> {}", strings::to_number_or_default<double>("-123.456"));
    print::println("strings::to_string_view_or_default {}", strings::to_string_view_or_default(12234ULL));
    {
        auto str = "?this_is_a_get=lol a space??&ts=!!!%";
        print::println("strings::url_encode before: {}, after: {}", str, strings::url_encode(str));
    }
    {
        auto str = "https://example.com/what lol/true?this_is_a_get=lol a=space??&ts=!!!%#fragment";
        print::println("strings::encode_full_url before: {}, after: {}", str, strings::encode_full_url(str));
    }
    {
        auto str = "https://example.com/what lol/true?this_is_a_get=lol a=space??&ts=!!!%#fragment";
        print::println("strings::encode_full_url before: {}, after: {}", str, strings::encode_full_url(str));
    }
    {
        auto str = "https://example.com/what lol/true/#fragment";
        print::println("strings::encode_full_url before: {}, after: {}", str, strings::encode_full_url(str));
    }
    {
        auto str = "https://example.com?hi#fragment";
        print::println("strings::encode_full_url before: {}, after: {}", str, strings::encode_full_url(str));
    }
    print::println();

    #ifdef WIN32
    print::println("Windows functions:");
    // These break the vscode terminal
    // {
    //     auto str = "a not wide string: 你好";
    //     print::println("windows::to_wstr input: {}", str);
    //     print::println(L"windows::to_wstr output: {}", *windows::to_wstr(str));
    // }
    // {
    //     auto str = L"a not wide string: 你好";
    //     print::println(L"windows::to_utf8 input: {}", str);
    //     print::println("windows::to_utf8 output: {}", *windows::to_utf8(str));
    // }
    print::println("windows::to_utf8<{{.as_optional = false}}>: {}", windows::to_utf8<{.as_optional = false}>(std::wstring(L"This is a test wide string")));
    print::println("windows::current_executable_path_or_default: {}", windows::to_utf8_or_default(windows::current_executable_path_or_default().get()));
    print::println("windows::set_cwd_to_executable_directory: {}", windows::set_cwd_to_executable_directory());
    print::println("windows::get_known_folder_or_default: {}", windows::to_utf8_or_default(windows::get_known_folder_or_default()));
    print::println("windows::exe_exists(L\"usylibpp_test\"): {}", windows::exe_exists(L"usylibpp_test"));
    print::println("windows::exe_exists(L\"usylibpp\"): {}", windows::exe_exists(L"usylibpp"));
    print::println("windows::exe_exists(L\"ffmpeg\"): {}", windows::exe_exists(L"ffmpeg"));
    print::println("windows::admin::is_admin: {}", windows::admin::is_admin());
    #endif

    #ifdef USYLIBPP_ENABLE_WINTOAST
    using namespace WinToastLib;
    print::println("WinToast stuff:");

    wintoast::ToastWorker<true, util::WorkerType::ReturnDefault> toast{L"usylibpp_test", L"usy", L"usylibpp", L"usylibpp_test", L"20260206"};
    if (!toast.success()) {
        print::println("Failed to setup wintoast!");
    } else {
        WinToastTemplate templ(WinToastTemplate::Text02);
        templ.setTextField(L"Hello!", WinToastTemplate::FirstLine);
        templ.setTextField(L"This is a test toast.", WinToastTemplate::SecondLine);
        templ.setDuration(WinToastTemplate::Duration::Short);

        // new usage is fine here as it takes shared ownership after
        const auto id = toast.showToast(templ, new wintoast::PrintingWinToastHandler);

        const auto& appname = toast.appName();
        auto appnamefuture = toast.appName<false>();
        print::println("App name: {}", windows::to_utf8_or_default(appname));
        print::println("App name future: {}", windows::to_utf8_or_default(appnamefuture.get()));
        
        if (id < 0) {
            print::println("Toast failed");
        } else {
            print::println("Toast shown, id = {} (sleeping for 5 seconds)", id);
            Sleep(5000);
        }
    }
    #endif
    #ifdef USYLIBPP_ENABLE_WINDOWS_IMAGING
    print::println("Windows imaging stuff:");
    {
        auto supported_file_extensions = windows::images::get_all_supported_file_extensions();
        print::print("WIC supported file extensions: ");
        for (const auto& format : supported_file_extensions) {
            print::print("{}, ", windows::to_utf8_or_default(format));
        }
        print::println("");
    }
    {
        auto wic_test_png_result = windows::images::decode_image<false, windows::images::DecodedImageType::Gray>(L"test.png");
        if (!wic_test_png_result) print::println("Failed to decode test.png! Ensure it exists");
        else {
            auto& res = wic_test_png_result.value();
            print::println("Decoded test.png, data size: {}, channels: {}, width: {}, height: {}", res.data.size(), res.channels, res.dimensions.width, res.dimensions.height);
        }
    }
    {
        windows::COMWrapper<false> COM{COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE};
        auto wic_test_png_result = windows::images::decode_image_threadlocal<windows::images::DecodedImageType::Gray>(L"test.png");
        if (!wic_test_png_result) print::println("Failed to decode test.png! Ensure it exists");
        else {
            auto& res = wic_test_png_result.value();
            print::println("Decoded test.png, data size: {}, channels: {}, width: {}, height: {}", res.buffer_size, res.channels, res.dimensions.width, res.dimensions.height);
        }
    }
    #endif
}
