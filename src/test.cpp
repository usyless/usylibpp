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

    #ifdef WIN32
    #endif
}
