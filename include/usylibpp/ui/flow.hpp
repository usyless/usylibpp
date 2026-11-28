#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

#if defined(_WIN32) && defined(USYLIBPP_ENABLE_UI)
#include <d2d1_3.h>

#include <functional>
#include <utility>
#endif

namespace usylibpp::ui {

struct FlowItem {
    float width = 0.0f;
    float height = 0.0f;
    bool grow = false;
};

struct FlowPlacement {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    int line = 0;
    bool oversize = false;
};

struct FlowResult {
    std::vector<FlowPlacement> placements;
    float height = 0.0f;
};

/// Places items left to right into `out` (`out.size() >= items.size()`), starting a new line when the next
/// item no longer fits in `avail`. Returns total layout height. Zero heap allocations.
inline float flow_layout_into(std::span<const FlowItem> items, float gap_x, float gap_y, float avail,
                              std::span<FlowPlacement> out) noexcept {
    constexpr float tolerance = 0.001f;
    size_t line_begin = 0;
    float y = 0.0f;
    int line = 0;
    while (line_begin < items.size()) {
        size_t line_end = line_begin;
        float used = 0.0f;
        while (line_end < items.size()) {
            const float w = std::min(items[line_end].width, avail);
            const float next = used + (line_end > line_begin ? gap_x : 0.0f) + w;
            if (line_end > line_begin && (next > avail + tolerance || items[line_end].width > avail + tolerance)) break;
            used = next;
            ++line_end;
            if (items[line_end - 1].width > avail + tolerance) break;
        }

        int growers = 0;
        float line_h = 0.0f;
        for (size_t i = line_begin; i < line_end; ++i) {
            if (items[i].grow) ++growers;
            line_h = std::max(line_h, items[i].height);
        }
        const float extra = growers > 0 ? std::max(0.0f, avail - used) / static_cast<float>(growers) : 0.0f;

        float x = 0.0f;
        for (size_t i = line_begin; i < line_end; ++i) {
            const float w = std::min(items[i].width, avail) + (items[i].grow ? extra : 0.0f);
            if (i < out.size()) {
                out[i] = FlowPlacement{.x = x, .y = y, .width = w, .line = line, .oversize = items[i].width > avail + tolerance};
            }
            x += w + gap_x;
        }

        y += line_h + gap_y;
        ++line;
        line_begin = line_end;
    }

    return items.empty() ? 0.0f : y - gap_y;
}

/// Places items left to right, starting a new line when the next item no longer fits in `avail`.
/// An item wider than `avail` gets a line of its own, is clamped to `avail` and flagged `oversize`.
/// Items marked `grow` share the unused width of their line equally.
inline FlowResult flow_layout(std::span<const FlowItem> items, float gap_x, float gap_y, float avail) {
    FlowResult result;
    result.placements.resize(items.size());
    result.height = flow_layout_into(items, gap_x, gap_y, avail, result.placements);
    return result;
}

#if defined(_WIN32) && defined(USYLIBPP_ENABLE_UI)
/// Inline-buffered flow container for Direct2D draw callbacks.
/// Stores up to `InlineCap` items inline on the stack without heap allocation, spilling to `std::vector` only if exceeded.
template <size_t InlineCap = 8>
class BasicFlow {
public:
    using DrawFn = std::function<void(D2D1_RECT_F)>;

    BasicFlow(float gap_x, float gap_y) noexcept : gap_x_{gap_x}, gap_y_{gap_y} {}

    void add(float w, float h, DrawFn draw, bool grow = false) {
        if (heap_items_.empty() && count_ < InlineCap) {
            inline_items_[count_] = FlowItem{.width = w, .height = h, .grow = grow};
            inline_draws_[count_] = std::move(draw);
            ++count_;
            return;
        }
        if (heap_items_.empty()) {
            heap_items_.reserve(InlineCap * 2);
            heap_draws_.reserve(InlineCap * 2);
            heap_items_.assign(inline_items_.begin(), inline_items_.begin() + static_cast<std::ptrdiff_t>(count_));
            for (size_t i = 0; i < count_; ++i) {
                heap_draws_.push_back(std::move(inline_draws_[i]));
            }
            count_ = 0;
        }
        heap_items_.push_back(FlowItem{.width = w, .height = h, .grow = grow});
        heap_draws_.push_back(std::move(draw));
    }

    template <size_t OtherCap>
    void add_group(BasicFlow<OtherCap> inner, float avail, bool grow = false) {
        const float group_w = std::min(inner.natural_width(), avail);
        const float group_h = inner.height(group_w);
        add(
            group_w, group_h,
            [inner = std::move(inner)](D2D1_RECT_F r) { inner.draw(r.left, r.top, r.right - r.left); },
            grow);
    }

    [[nodiscard]] size_t size() const noexcept {
        return heap_items_.empty() ? count_ : heap_items_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    [[nodiscard]] float natural_width() const noexcept {
        const std::span<const FlowItem> items = items_span();
        float total = 0.0f;
        for (const FlowItem& item : items) {
            total += item.width;
        }
        return items.empty() ? 0.0f : (total + (gap_x_ * static_cast<float>(items.size() - 1)));
    }

    [[nodiscard]] float height(float avail) const noexcept {
        return flow_layout_into(items_span(), gap_x_, gap_y_, avail, {});
    }

    void draw(float x, float y, float avail) const {
        const std::span<const FlowItem> items = items_span();
        const std::span<const DrawFn> draws = draws_span();
        if (items.size() <= InlineCap) {
            std::array<FlowPlacement, InlineCap> stack_placements; // NOLINT(cppcoreguidelines-pro-type-member-init)
            const std::span<FlowPlacement> out{stack_placements.data(), items.size()};
            flow_layout_into(items, gap_x_, gap_y_, avail, out);
            for (size_t i = 0; i < items.size(); ++i) {
                const FlowPlacement& p = out[i];
                draws[i](D2D1_RECT_F{.left = x + p.x, .top = y + p.y, .right = x + p.x + p.width, .bottom = y + p.y + items[i].height});
            }
            return;
        }
        const FlowResult layout = flow_layout(items, gap_x_, gap_y_, avail);
        for (size_t i = 0; i < items.size(); ++i) {
            const FlowPlacement& p = layout.placements[i];
            draws[i](D2D1_RECT_F{.left = x + p.x, .top = y + p.y, .right = x + p.x + p.width, .bottom = y + p.y + items[i].height});
        }
    }

private:
    [[nodiscard]] std::span<const FlowItem> items_span() const noexcept {
        if (!heap_items_.empty()) {
            return heap_items_;
        }
        return {inline_items_.data(), count_};
    }

    [[nodiscard]] std::span<const DrawFn> draws_span() const noexcept {
        if (!heap_draws_.empty()) {
            return heap_draws_;
        }
        return {inline_draws_.data(), count_};
    }

    float gap_x_;
    float gap_y_;
    size_t count_ = 0;
    std::array<FlowItem, InlineCap> inline_items_{};
    std::array<DrawFn, InlineCap> inline_draws_{};
    std::vector<FlowItem> heap_items_;
    std::vector<DrawFn> heap_draws_;
};

using Flow = BasicFlow<8>;
#endif

} // namespace usylibpp::ui
