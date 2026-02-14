#pragma once

#include "aliases.hpp" // IWYU pragma: export
#include <optional>
#include <type_traits>

namespace usylibpp {
    struct opts {
        bool as_optional = true;
    };

    namespace types {
        template <opts O, typename R>
        using opts_return = std::conditional_t<O.as_optional, std::optional<R>, R>;
    }
}