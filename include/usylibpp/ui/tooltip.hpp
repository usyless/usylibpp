#pragma once

#include "anim.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

#if defined(_WIN32) && defined(USYLIBPP_ENABLE_UI)
#include "gfx.hpp"
#endif

namespace usylibpp::ui {

enum class TooltipHoverAction : uint8_t {
    None = 0,
    StartTimer,
    ShowNow,
};

/// Platform-independent delayed-hover tooltip state machine with smooth fade-in/out
/// and warm instant-reshow when hovering items in rapid succession.
template <typename RectT>
struct HoverTooltipBase {
    static constexpr uint32_t DELAY_MS = 380u;
    static constexpr int64_t WARM_WINDOW_MS = 500;

    RectT anchor{};
    RectT pending_anchor{};
    std::wstring title;
    std::wstring subtitle;
    bool shown = false;
    AnimFloat alpha;
    bool warm_active = false;
    int64_t warm_until_ms = 0;
    uint64_t revision = 0;

    [[nodiscard]] static int64_t now_ms() noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    [[nodiscard]] bool is_warm(int64_t t_ms = now_ms()) const noexcept {
        return shown || (warm_active && t_ms <= warm_until_ms);
    }

    /// Updates hover state when the hovered item changes.
    /// Returns `ShowNow` when a tooltip was just showing (or within `WARM_WINDOW_MS`) and the
    /// newly hovered item is eligible; caller should immediately build text and call `show_now()`.
    [[nodiscard]] TooltipHoverAction on_hover_changed_state(const RectT* anchor_rect, bool eligible,
                                                            bool animate, int64_t t_ms = now_ms()) noexcept {
        const bool was_shown = shown;
        const bool warm = was_shown || (warm_active && t_ms <= warm_until_ms);
        if (!warm) warm_active = false;
        pending_anchor = anchor_rect ? *anchor_rect : RectT{};
        if (eligible && warm) {
            warm_active = true;
            warm_until_ms = t_ms + WARM_WINDOW_MS;
            return TooltipHoverAction::ShowNow;
        }
        if (was_shown) {
            shown = false;
            warm_active = true;
            warm_until_ms = t_ms + WARM_WINDOW_MS;
            alpha.set(0.0f, animate);
        } else if (!animate) {
            alpha.snap(0.0f);
        }
        return eligible ? TooltipHoverAction::StartTimer : TooltipHoverAction::None;
    }

    /// Immediately displays a tooltip for `pending_anchor` (typically after `ShowNow`).
    /// If `allow` is false or both strings are empty, hides any previously visible tooltip instead.
    /// Returns true if tooltip visual state changed and a repaint or animation is needed.
    bool show_now(bool allow, std::wstring new_title, std::wstring new_subtitle,
                  bool animate, int64_t t_ms = now_ms()) noexcept {
        if (!allow || (new_title.empty() && new_subtitle.empty())) {
            const bool was_vis = shown || alpha.value > 0.008f;
            if (shown) {
                shown = false;
                warm_active = true;
                warm_until_ms = t_ms + WARM_WINDOW_MS;
            }
            alpha.set(0.0f, animate);
            return was_vis;
        }
        const bool keep_alpha = shown || alpha.value > 0.008f;
        anchor = pending_anchor;
        title = std::move(new_title);
        subtitle = std::move(new_subtitle);
        shown = true;
        warm_active = true;
        warm_until_ms = t_ms + WARM_WINDOW_MS;
        ++revision;
        if (!keep_alpha) alpha.snap(0.0f);
        alpha.set(1.0f, animate);
        return true;
    }

    /// Commits a tooltip when the initial hover delay timer fires.
    bool on_timer_state(bool allow, std::wstring new_title, std::wstring new_subtitle,
                        bool animate, int64_t t_ms = now_ms()) noexcept {
        if (!allow || (new_title.empty() && new_subtitle.empty())) return false;
        anchor = pending_anchor;
        title = std::move(new_title);
        subtitle = std::move(new_subtitle);
        shown = true;
        warm_active = true;
        warm_until_ms = t_ms + WARM_WINDOW_MS;
        ++revision;
        alpha.snap(0.0f);
        alpha.set(1.0f, animate);
        return true;
    }

    /// Explicitly dismisses the tooltip and resets the warm instant-reshow window.
    void clear_state(bool animate = false) noexcept {
        shown = false;
        warm_active = false;
        warm_until_ms = 0;
        alpha.set(0.0f, animate);
    }

    bool step(float dt) noexcept {
        return alpha.step(dt, shown ? 32.0f : 36.0f, 0.008f);
    }

    [[nodiscard]] bool visible() const noexcept {
        return alpha.value > 0.008f && (!title.empty() || !subtitle.empty());
    }
};

#if defined(_WIN32) && defined(USYLIBPP_ENABLE_UI)
/// Win32 + Direct2D delayed-hover tooltip state with smooth fade-in/out, instant warm reshow,
/// and zero idle repaints.
struct HoverTooltip : HoverTooltipBase<D2D1_RECT_F> {
    /// Updates hover state and manages `timer_id` on `hwnd`.
    /// Returns true if the caller should immediately build tooltip text and call `show_now()`.
    [[nodiscard]] bool on_hover_changed(HWND hwnd, UINT_PTR timer_id, const D2D1_RECT_F* anchor_rect,
                                        bool eligible, bool animate, int64_t t_ms = now_ms()) noexcept {
        if (hwnd) KillTimer(hwnd, timer_id);
        const TooltipHoverAction act = on_hover_changed_state(anchor_rect, eligible, animate, t_ms);
        if (act == TooltipHoverAction::StartTimer && hwnd) {
            SetTimer(hwnd, timer_id, DELAY_MS, nullptr);
        }
        return act == TooltipHoverAction::ShowNow;
    }

    bool on_timer(HWND hwnd, UINT_PTR timer_id, bool allow, std::wstring new_title,
                  std::wstring new_subtitle, bool animate, int64_t t_ms = now_ms()) noexcept {
        if (hwnd) KillTimer(hwnd, timer_id);
        return on_timer_state(allow, std::move(new_title), std::move(new_subtitle), animate, t_ms);
    }

    void clear(HWND hwnd, UINT_PTR timer_id, bool animate = false) noexcept {
        if (hwnd) KillTimer(hwnd, timer_id);
        clear_state(animate);
    }
};
#endif

} // namespace usylibpp::ui
