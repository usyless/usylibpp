#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace usylibpp::ui {

enum class ThumbFit : uint8_t { Crop, Contain };

struct FitRect {
    float left = 0.0f, top = 0.0f, width = 0.0f, height = 0.0f;
    bool operator==(const FitRect&) const = default;
};

inline ThumbFit parse_thumb_fit(std::string_view s) noexcept {
    return s == "contain" ? ThumbFit::Contain : ThumbFit::Crop;
}

/// Where an image of the given size is drawn inside `tile`. Crop may extend past the tile; contain stays inside it.
inline FitRect fit_image(float image_w, float image_h, const FitRect& tile, ThumbFit fit) noexcept {
    if (image_w <= 0.0f || image_h <= 0.0f || tile.width <= 0.0f || tile.height <= 0.0f) return tile;
    const float sx = tile.width / image_w;
    const float sy = tile.height / image_h;
    const float s = fit == ThumbFit::Crop ? std::max(sx, sy) : std::min(sx, sy);
    const float w = image_w * s;
    const float h = image_h * s;
    return {.left = tile.left + ((tile.width - w) * 0.5f), .top = tile.top + ((tile.height - h) * 0.5f), .width = w, .height = h};
}

} // namespace usylibpp::ui
