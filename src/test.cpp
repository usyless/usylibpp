#include <usylibpp/usylibpp.hpp>

int main() {
    using namespace usylibpp;
    
    print::println("Hello! Welcome to my silly library");

    print::println("Current time: {}", time::current_datetime());
}
