#pragma once

#if defined(_MSVC_LANG)
    #define USYLIBPP_CPLUSPLUS _MSVC_LANG
#else
    #define USYLIBPP_CPLUSPLUS __cplusplus
#endif

#define USYLIBPP__MAKE_OR(function, default_value) \
template <typename... TArgs, typename... Call> \
requires (sizeof...(TArgs) > 0) \
[[nodiscard]] inline constexpr auto function##_or_default(Call&&... args) \
    noexcept(noexcept(function<TArgs...>(std::forward<Call>(args)...).value_or(default_value))) { \
    return function<TArgs...>(std::forward<Call>(args)...).value_or(default_value); \
} \
 \
template <auto... NTArgs, typename... Call> \
requires (sizeof...(NTArgs) > 0) \
[[nodiscard]] inline constexpr auto function##_or_default(Call&&... args) \
    noexcept(noexcept(function<NTArgs...>(std::forward<Call>(args)...).value_or(default_value))) { \
    return function<NTArgs...>(std::forward<Call>(args)...).value_or(default_value); \
} \
 \
template <typename... Call> \
requires requires (Call&&... a) { function(std::forward<Call>(a)...).value_or(default_value); } \
[[nodiscard]] inline constexpr auto function##_or_default(Call&&... args) \
    noexcept(noexcept(function(std::forward<Call>(args)...).value_or(default_value))) { \
    return function(std::forward<Call>(args)...).value_or(default_value); \
}
