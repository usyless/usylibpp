#pragma once

#define USYLIBPP__MAKE_OR(function, or) \
template <typename... TArgs, typename... Call> \
requires (sizeof...(TArgs) > 0) \
[[nodiscard]] inline auto function##_or_default(Call&&... args) { \
    return function<TArgs...>(std::forward<Call>(args)...).value_or(or); \
} \
 \
template <auto... NTArgs, typename... Call> \
requires (sizeof...(NTArgs) > 0) \
[[nodiscard]] inline auto function##_or_default(Call&&... args) { \
    return function<NTArgs...>(std::forward<Call>(args)...).value_or(or); \
} \
 \
template <typename... Call> \
requires (true) \
[[nodiscard]] inline auto function##_or_default(Call&&... args) { \
    return function(std::forward<Call>(args)...).value_or(or); \
} \
