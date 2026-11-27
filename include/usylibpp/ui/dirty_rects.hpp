#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace usylibpp::ui {

struct PixelRect {
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;

    [[nodiscard]] constexpr bool empty() const noexcept { return right <= left || bottom <= top; }
    [[nodiscard]] constexpr int64_t area() const noexcept {
        return empty() ? 0 : static_cast<int64_t>(right - left) * static_cast<int64_t>(bottom - top);
    }
    bool operator==(const PixelRect&) const = default;
};

/// Converts DIP bounds into clamped integer device-pixel coordinates inflated by `pad_dips`.
[[nodiscard]] inline PixelRect dip_to_pixel_rect(float left, float top, float right, float bottom, float scale,
                                                 uint32_t win_w, uint32_t win_h, float pad_dips = 2.0f) noexcept {
    if (right <= left || bottom <= top || win_w == 0 || win_h == 0 || scale <= 0.0f) return {};
    const auto w = static_cast<int32_t>(win_w);
    const auto h = static_cast<int32_t>(win_h);
    const int32_t l = std::clamp(static_cast<int32_t>(std::floor((left - pad_dips) * scale)), 0, w);
    const int32_t t = std::clamp(static_cast<int32_t>(std::floor((top - pad_dips) * scale)), 0, h);
    const int32_t r = std::clamp(static_cast<int32_t>(std::ceil((right + pad_dips) * scale)), 0, w);
    const int32_t b = std::clamp(static_cast<int32_t>(std::ceil((bottom + pad_dips) * scale)), 0, h);
    if (r <= l || b <= t) return {};
    return PixelRect{.left = l, .top = t, .right = r, .bottom = b};
}

/// Returns pairwise strictly disjoint non-empty rectangles covering all non-empty inputs.
[[nodiscard]] inline std::vector<PixelRect> merge_disjoint_rects(std::span<const PixelRect> a,
                                                                 std::span<const PixelRect> b = {}) {
    std::vector<PixelRect> out;
    out.reserve(a.size() + b.size());
    for (const PixelRect& rc : a) {
        if (!rc.empty()) out.push_back(rc);
    }
    for (const PixelRect& rc : b) {
        if (!rc.empty()) out.push_back(rc);
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < out.size(); ++i) {
            for (size_t j = i + 1; j < out.size();) {
                const PixelRect& x = out[i];
                const PixelRect& y = out[j];
                const bool overlap = x.left < y.right && y.left < x.right && x.top < y.bottom && y.top < x.bottom;
                if (overlap) {
                    out[i].left = std::min(x.left, y.left);
                    out[i].top = std::min(x.top, y.top);
                    out[i].right = std::max(x.right, y.right);
                    out[i].bottom = std::max(x.bottom, y.bottom);
                    out[j] = out.back();
                    out.pop_back();
                    changed = true;
                } else {
                    ++j;
                }
            }
        }
    }
    return out;
}

/// Tracks 2-buffer DXGI flip-sequential dirty regions so every back buffer stays consistent across presents.
class FlipDirtyTracker {
public:
    struct Plan {
        bool skip = false;
        bool full = true;
        std::vector<PixelRect> render_rects;
        std::vector<PixelRect> curr_rects;
    };

    void reset() noexcept {
        full_sync_remaining_ = 2;
        prev_dirty_.clear();
    }

    [[nodiscard]] Plan plan(bool full_redraw, std::span<const PixelRect> curr_dirty, uint32_t win_w, uint32_t win_h) const {
        Plan p;
        if (!full_redraw) {
            p.curr_rects = merge_disjoint_rects(curr_dirty);
        }
        if (!full_redraw && full_sync_remaining_ == 0 && prev_dirty_.empty() && p.curr_rects.empty()) {
            p.skip = true;
            p.full = false;
            return p;
        }
        if (full_redraw || full_sync_remaining_ > 0) {
            p.full = true;
            return p;
        }
        p.render_rects = merge_disjoint_rects(prev_dirty_, p.curr_rects);
        if (p.render_rects.empty()) {
            p.skip = true;
            p.full = false;
            return p;
        }
        int64_t area = 0;
        for (const PixelRect& rc : p.render_rects) area += rc.area();
        const int64_t total_area = static_cast<int64_t>(win_w) * static_cast<int64_t>(win_h);
        if (p.render_rects.size() > 8 || (total_area > 0 && area * 100 > total_area * 70)) {
            p.full = true;
            p.render_rects.clear();
            return p;
        }
        p.full = false;
        return p;
    }

    void commit(bool full_redraw, const Plan& p) {
        if (p.skip) return;
        if (full_redraw || full_sync_remaining_ >= 2) {
            full_sync_remaining_ = 1;
            prev_dirty_.clear();
        } else {
            full_sync_remaining_ = 0;
            prev_dirty_ = p.curr_rects;
        }
    }

    [[nodiscard]] int full_sync_remaining() const noexcept { return full_sync_remaining_; }
    [[nodiscard]] const std::vector<PixelRect>& prev_dirty() const noexcept { return prev_dirty_; }

private:
    int full_sync_remaining_ = 2;
    std::vector<PixelRect> prev_dirty_;
};

} // namespace usylibpp::ui
