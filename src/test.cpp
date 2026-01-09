#include <usylibpp/usylibpp.hpp>

int main() {
    using namespace usylibpp;
    
    print::println("Hello! Welcome to my silly library\n");

    print::println("Time functions:");
    print::println("time::datetime_string: {}", time::datetime_string());
    print::println();

    print::println("String functions:");
    print::println("strings::concat_strings (chars): {}", strings::concat_strings("hello", " ", "there!"));
    print::println(L"strings::concat_strings (wide chars): {}", strings::concat_strings(L"hello", L" ", L"there!"));
    {
        auto str = "THis IS moSTLY upperCASE";
        print::println("strings::to_lowercase before: {}, after: {}", str, strings::to_lowercase(str));
    }
    {
        auto str = "this has THIS STrING and once again THIS STrING";
        print::println("strings::replace_all before: {}, after: {}", str, strings::replace_all(str, "THIS STrING", "not that string"));
    }
    print::println("strings::to_number_positive<size_t> {}", *strings::to_number_positive<size_t>("1234567"));
    {
        auto str = "?this_is_a_get=lol a space??&ts=!!!%";
        print::println("strings::url_encode before: {}, after: {}", str, strings::url_encode(str));
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
    print::println("windows::current_executable_path: {}", *windows::to_utf8(windows::current_executable_path()->get()));
    print::println("windows::get_known_folder: {}", *windows::to_utf8(*windows::get_known_folder()));
    print::println("windows::exe_exists(L\"usylibpp_test\"): {}", windows::exe_exists(L"usylibpp_test"));
    print::println("windows::exe_exists(L\"usylibpp\"): {}", windows::exe_exists(L"usylibpp"));
    print::println("windows::exe_exists(L\"ffmpeg\"): {}", windows::exe_exists(L"ffmpeg"));
    print::println("windows::admin::is_admin: {}", windows::admin::is_admin());
    #endif
}
