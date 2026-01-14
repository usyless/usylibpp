#include <usylibpp/usylibpp.hpp>

int main() {
    using namespace usylibpp;

    print::println("Hello! Welcome to my silly library\n");

    print::println("Time functions:");
    print::println("time::datetime_string: {}", time::datetime_string());
    print::println();

    print::println("String functions:");
    print::println("strings::concat_strings (chars): {}", strings::concat_strings("hello", " ", "there!"));
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
    print::println("windows::to_utf8<{{.as_optional = false}}>: {}", windows::to_utf8<{.as_optional = false}>(L"This is a test wide string"));
    print::println("windows::current_executable_path_or_default: {}", windows::to_utf8_or_default(windows::current_executable_path_or_default().get()));
    print::println("windows::set_cwd_to_executable_directory: {}", windows::set_cwd_to_executable_directory());
    print::println("windows::get_known_folder_or_default: {}", windows::to_utf8_or_default(windows::get_known_folder_or_default()));
    print::println("windows::exe_exists(L\"usylibpp_test\"): {}", windows::exe_exists(L"usylibpp_test"));
    print::println("windows::exe_exists(L\"usylibpp\"): {}", windows::exe_exists(L"usylibpp"));
    print::println("windows::exe_exists(L\"ffmpeg\"): {}", windows::exe_exists(L"ffmpeg"));
    print::println("windows::admin::is_admin: {}", windows::admin::is_admin());
    #endif
}
