#include <usylibpp/usylibpp.hpp>

int main() {
    using namespace usylibpp;
    
    print::println("Hello! Welcome to my silly library");

    print::println();
    print::println("Time functions:");
    print::println("time::current_datetime: {}", time::current_datetime());

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

    #ifdef WIN32
    #endif
}
