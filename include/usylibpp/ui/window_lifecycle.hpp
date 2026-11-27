#pragma once

#include <cstdint>
#include <optional>

namespace usylibpp::ui {

inline constexpr uint32_t DEFAULT_CARET_BLINK_MS = 530u;
inline constexpr uint32_t CARET_BLINK_INFINITE = 0xFFFFFFFFu;

/// Normalizes Win32 `GetCaretBlinkTime()` into a safe `SetTimer` period:
/// - `INFINITE` (`0xFFFFFFFF`, cursor blink disabled) -> `std::nullopt` (keep caret solid, no timer)
/// - `0` (API failure, which `SetTimer` would clamp to 10ms) -> `DEFAULT_CARET_BLINK_MS`
/// - otherwise -> the reported blink period in milliseconds.
[[nodiscard]] constexpr std::optional<uint32_t> caret_blink_interval_ms(uint32_t raw_blink_ms) noexcept {
    if (raw_blink_ms == CARET_BLINK_INFINITE) return std::nullopt;
    if (raw_blink_ms == 0u) return DEFAULT_CARET_BLINK_MS;
    return raw_blink_ms;
}

/// Decodes `WM_ACTIVATE` (`LOWORD(wp)`, `HIWORD(wp) != 0`) into whether the window is genuinely active.
[[nodiscard]] constexpr bool wm_activate_is_active(uint16_t state, bool minimized) noexcept {
    return state != 0u && !minimized;
}

/// Desired caret visibility and blink-timer state for a window.
struct CaretDecision {
    bool visible = false;
    bool run_timer = false;

    [[nodiscard]] constexpr bool operator==(const CaretDecision&) const noexcept = default;
};

/// Determines whether a window should draw the text caret and run its blink timer.
[[nodiscard]] constexpr CaretDecision caret_timer_decision(bool window_visible, bool minimized, bool fading_out,
                                                           bool window_active, bool field_focused,
                                                           std::optional<uint32_t> blink_ms) noexcept {
    if (!window_visible || minimized || fading_out || !window_active || !field_focused) {
        return {.visible = false, .run_timer = false};
    }
    return {.visible = true, .run_timer = blink_ms.has_value()};
}

/// True when a window is on-screen and should run live background timers (glass, polling, animations, media).
[[nodiscard]] constexpr bool window_can_render_live(bool window_visible, bool minimized, bool fading_out = false) noexcept {
    return window_visible && !minimized && !fading_out;
}

/// True when a finished full-resolution preview decode should be retained in GPU memory.
[[nodiscard]] constexpr bool should_keep_preview_decode(bool window_visible, bool fading_out, int preview_index) noexcept {
    return window_visible && !fading_out && preview_index >= 0;
}

/// True when `WM_CLOSE` / `hide()` is allowed to close the window (blocked while a modal dialog or drag owns it).
[[nodiscard]] constexpr bool should_allow_close(bool in_modal, bool dragging_out = false) noexcept {
    return !in_modal && !dragging_out;
}

} // namespace usylibpp::ui
