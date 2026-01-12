#pragma once

#define USYLIBPP__MAKE_OR_TEMPLATE(function, suffix, or) \
template <typename... arg, typename... call> \
[[nodiscard]] inline auto function##suffix(call&&... args) { \
    return function<arg...>(std::forward<call>(args)...).value_or(or); \
}

#define USYLIBPP__MAKE_OR_TEMPLATE_AUTO(function, suffix, or) \
template <auto... arg, typename... call> \
[[nodiscard]] inline auto function##suffix(call&&... args) { \
    return function<arg...>(std::forward<call>(args)...).value_or(or); \
}

#define USYLIBPP__MAKE_OR(function, suffix, or) \
template <typename... call> \
[[nodiscard]] inline auto function##suffix(call&&... args) { \
    return function(std::forward<call>(args)...).value_or(or); \
}

#define USYLIBPP__MAKE_OR_DEFAULT_TEMPLATE(function) USYLIBPP__MAKE_OR_TEMPLATE(function, _or_default, {})
#define USYLIBPP__MAKE_OR_DEFAULT_TEMPLATE_AUTO(function) USYLIBPP__MAKE_OR_TEMPLATE_AUTO(function, _or_default, {})
#define USYLIBPP__MAKE_OR_DEFAULT(function) USYLIBPP__MAKE_OR(function, _or_default, {})