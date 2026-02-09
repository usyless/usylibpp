#pragma once

#define USYLIBPP__MAKE_OR(function, or) \
template <typename... TArgs, typename... Call> \
requires (sizeof...(TArgs) > 0) \
[[nodiscard]] inline constexpr auto function##_or_default(Call&&... args) \
    noexcept(noexcept(function<TArgs...>(std::forward<Call>(args)...).value_or(or))) { \
    return function<TArgs...>(std::forward<Call>(args)...).value_or(or); \
} \
 \
template <auto... NTArgs, typename... Call> \
requires (sizeof...(NTArgs) > 0) \
[[nodiscard]] inline constexpr auto function##_or_default(Call&&... args) \
    noexcept(noexcept(function<NTArgs...>(std::forward<Call>(args)...).value_or(or))) { \
    return function<NTArgs...>(std::forward<Call>(args)...).value_or(or); \
} \
 \
template <typename... Call> \
requires (true) \
[[nodiscard]] inline constexpr auto function##_or_default(Call&&... args) \
    noexcept(noexcept(function(std::forward<Call>(args)...).value_or(or))) { \
    return function(std::forward<Call>(args)...).value_or(or); \
} \
