#pragma once

#define USYLIBPP__MAKE_OR_TEMPLATE(function, or) \
template <typename... arg, typename... call> \
[[nodiscard]] inline auto function##_or_default(call&&... args) { \
    return function<arg...>(std::forward<call>(args)...).value_or(or); \
}

#define USYLIBPP__MAKE_OR_TEMPLATE_AUTO(function, or) \
template <auto... arg, typename... call> \
[[nodiscard]] inline auto function##_or_default(call&&... args) { \
    return function<arg...>(std::forward<call>(args)...).value_or(or); \
}

#define USYLIBPP__MAKE_OR(function, or) \
template <typename... call> \
[[nodiscard]] inline auto function##_or_default(call&&... args) { \
    return function(std::forward<call>(args)...).value_or(or); \
}