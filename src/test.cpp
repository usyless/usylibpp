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
        auto lol = "THis IS moSTLY upperCASE";
        print::println("strings::to_lowercase before: {}, after: {}", lol, strings::to_lowercase(lol));
    }
    {
        auto lol = "this has THIS STrING and once again THIS STrING";
        print::println("strings::replace_all before: {}, after: {}", lol, strings::replace_all(lol, "THIS STrING", "not that string"));
    }

    #ifdef WIN32
    #endif
}
