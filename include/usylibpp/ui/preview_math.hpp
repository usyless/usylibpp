#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace usylibpp::ui {

enum class MediaTickStatus : uint8_t { NoChange, FrameReady, DeviceLost };

/// GIF Graphic Control Extension disposal method (values 0..3 per GIF89a).
enum class GifDisposal : uint8_t {
    Unspecified = 0,
    Keep = 1,
    RestoreBg = 2,
    RestorePrev = 3,
};

struct FitBox {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

[[nodiscard]] inline bool is_gif_path(std::wstring_view path) noexcept {
    if (path.size() < 4) return false;
    const std::wstring_view ext = path.substr(path.size() - 4);
    auto const lower_ascii = [](wchar_t c) noexcept -> wchar_t {
        return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c + (L'a' - L'A')) : c;
    };
    return ext[0] == L'.' && lower_ascii(ext[1]) == L'g' && lower_ascii(ext[2]) == L'i' && lower_ascii(ext[3]) == L'f';
}

/// Strips extended-length Win32 path prefixes (`\\?\` / `\\?\UNC\`) before passing URLs to Media Foundation.
[[nodiscard]] inline std::wstring normalize_mf_source_path(std::wstring_view path) {
    if (path.starts_with(LR"(\\?\UNC\)")) {
        std::wstring out = LR"(\\)";
        out.append(path.substr(8));
        return out;
    }
    if (path.starts_with(LR"(\\?\)")) {
        return std::wstring{path.substr(4)};
    }
    return std::wstring{path};
}

/// Builds a safe synthetic `file:///media.<ext>` URL hint for `IMFMediaEngineEx::SetSourceFromByteStream`
/// so Media Foundation can sniff container format without parsing `#`, `?`, `%`, or >2084-char local paths.
[[nodiscard]] inline std::wstring make_mf_extension_hint_url(std::wstring_view path) {
    std::wstring out = L"file:///media";
    const size_t slash = path.find_last_of(L"\\/");
    const size_t dot = path.find_last_of(L'.');
    if (dot != std::wstring_view::npos && (slash == std::wstring_view::npos || dot > slash)) {
        const std::wstring_view ext = path.substr(dot + 1);
        if (!ext.empty() && ext.size() <= 16u) {
            bool ascii_alnum = true;
            for (const wchar_t c : ext) {
                const bool ok = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9');
                if (!ok) {
                    ascii_alnum = false;
                    break;
                }
            }
            if (ascii_alnum) {
                out.push_back(L'.');
                for (const wchar_t c : ext) {
                    out.push_back((c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c + (L'a' - L'A')) : c);
                }
            }
        }
    }
    return out;
}

/// Normalizes a raw GIF delay in 10ms centiseconds per browser convention (`<= 1cs` -> `100ms`), clamped to `60s`.
[[nodiscard]] inline uint32_t normalize_gif_delay_ms(uint32_t delay_cs) noexcept {
    if (delay_cs < 2u) return 100u;
    return std::min<uint32_t>(delay_cs * 10u, 60000u);
}

/// Parses NETSCAPE2.0 / ANIMEXTS1.0 loop count (0 = infinite). Accepts both `[3, 1, lo, hi]` and `[1, lo, hi]`.
[[nodiscard]] inline std::optional<uint32_t> parse_netscape_loop_count(std::span<const uint8_t> data) noexcept {
    if (data.size() >= 4 && data[0] == 3 && data[1] == 1) {
        return static_cast<uint32_t>(data[2]) | (static_cast<uint32_t>(data[3]) << 8u);
    }
    if (data.size() >= 3 && data[0] == 1) {
        return static_cast<uint32_t>(data[1]) | (static_cast<uint32_t>(data[2]) << 8u);
    }
    return std::nullopt;
}

/// Scales `(w, h)` proportionally so `max(w, h) <= max_px` without ever upscaling (`max_px == 0` keeps native size).
[[nodiscard]] inline std::pair<uint32_t, uint32_t> fit_scaled_size(uint32_t w, uint32_t h, uint32_t max_px) noexcept {
    if (w == 0 || h == 0) return {0u, 0u};
    const uint32_t longest = std::max(w, h);
    if (max_px == 0 || longest <= max_px) return {w, h};
    const double s = static_cast<double>(max_px) / static_cast<double>(longest);
    const uint32_t sw = std::max(1u, static_cast<uint32_t>(std::lround(static_cast<double>(w) * s)));
    const uint32_t sh = std::max(1u, static_cast<uint32_t>(std::lround(static_cast<double>(h) * s)));
    return {sw, sh};
}

/// Computes the centered preview rectangle in DIPs for media `(src_w, src_h)` fitted into `area`,
/// honouring the original item dimensions `(meta_w, meta_h)` when a smaller stand-in thumbnail is displayed.
[[nodiscard]] inline FitBox compute_preview_box(
    uint32_t src_w, uint32_t src_h, uint32_t meta_w, uint32_t meta_h, float dpi_scale, FitBox area) noexcept {
    const float safe_scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    const float bw = static_cast<float>(std::max(1u, src_w));
    const float bh = static_cast<float>(std::max(1u, src_h));
    const float native = (meta_w > 0 && meta_h > 0) ? static_cast<float>(std::max(meta_w, meta_h)) : std::max(bw, bh);
    const float k = std::max({native, bw, bh}) / std::max(bw, bh) / safe_scale;
    const float nw = bw * k;
    const float nh = bh * k;
    const float avail_w = std::max(1.0f, area.width);
    const float avail_h = std::max(1.0f, area.height);
    const float s = std::min({1.0f, avail_w / nw, avail_h / nh});
    const float w = std::max(1.0f, std::round(nw * s * safe_scale)) / safe_scale;
    const float h = std::max(1.0f, std::round(nh * s * safe_scale)) / safe_scale;
    const float x = std::round((area.left + ((avail_w - w) * 0.5f)) * safe_scale) / safe_scale;
    const float y = std::round((area.top + ((avail_h - h) * 0.5f)) * safe_scale) / safe_scale;
    return FitBox{.left = x, .top = y, .width = w, .height = h};
}

/// Pure-C++ premultiplied 32bpp BGRA GIF logical-screen compositor with sub-rect clipping and disposal 0..3.
class GifCompositor {
public:
    GifCompositor(uint32_t canvas_w, uint32_t canvas_h)
        : cw_{canvas_w}, ch_{canvas_h}, canvas_(static_cast<size_t>(canvas_w) * canvas_h * 4u, 0) {}

    [[nodiscard]] uint32_t width() const noexcept { return cw_; }
    [[nodiscard]] uint32_t height() const noexcept { return ch_; }
    [[nodiscard]] const std::vector<uint8_t>& canvas() const noexcept { return canvas_; }

    void reset() noexcept {
        std::ranges::fill(canvas_, uint8_t{0});
        prev_patch_.clear();
    }

    /// Composites one decoded `32bppPBGRA` frame onto the canvas, stores the resulting frame pixels into `out_pbgra`,
    /// and applies the frame's disposal rule for subsequent frames.
    bool apply_frame(
        uint32_t left, uint32_t top, uint32_t fw, uint32_t fh,
        std::span<const uint8_t> frame_pbgra, GifDisposal disposal, std::vector<uint8_t>& out_pbgra) {
        return apply_frame_impl(left, top, fw, fh, frame_pbgra, disposal, &out_pbgra);
    }

    /// Advances the internal canvas state by one frame without copying out the full canvas (used during seek fast-forward).
    bool advance_frame(
        uint32_t left, uint32_t top, uint32_t fw, uint32_t fh,
        std::span<const uint8_t> frame_pbgra, GifDisposal disposal) {
        return apply_frame_impl(left, top, fw, fh, frame_pbgra, disposal, nullptr);
    }

private:
    bool apply_frame_impl(
        uint32_t left, uint32_t top, uint32_t fw, uint32_t fh,
        std::span<const uint8_t> frame_pbgra, GifDisposal disposal, std::vector<uint8_t>* out_pbgra) {
        if (cw_ == 0 || ch_ == 0 || fw == 0 || fh == 0) return false;
        const size_t frame_stride = static_cast<size_t>(fw) * 4u;
        if (frame_pbgra.size() < frame_stride * fh) return false;

        const uint32_t copy_w = left < cw_ ? std::min(fw, cw_ - left) : 0u;
        const uint32_t copy_h = top < ch_ ? std::min(fh, ch_ - top) : 0u;

        if (disposal == GifDisposal::RestorePrev && copy_w > 0 && copy_h > 0) {
            save_patch(left, top, copy_w, copy_h);
        }

        const size_t canvas_stride = static_cast<size_t>(cw_) * 4u;
        for (uint32_t y = 0; y < copy_h; ++y) {
            const uint8_t* src = frame_pbgra.data() + (static_cast<size_t>(y) * frame_stride);
            uint8_t* dst = canvas_.data() + (static_cast<size_t>(top + y) * canvas_stride) + (static_cast<size_t>(left) * 4u);
            for (uint32_t x = 0; x < copy_w; ++x, src += 4, dst += 4) {
                const uint8_t a = src[3];
                if (a == 255) {
                    std::memcpy(dst, src, 4);
                } else if (a > 0) {
                    const uint32_t inv = 255u - a;
                    dst[0] = static_cast<uint8_t>(std::min<uint32_t>(255u, src[0] + (((dst[0] * inv) + 127u) / 255u)));
                    dst[1] = static_cast<uint8_t>(std::min<uint32_t>(255u, src[1] + (((dst[1] * inv) + 127u) / 255u)));
                    dst[2] = static_cast<uint8_t>(std::min<uint32_t>(255u, src[2] + (((dst[2] * inv) + 127u) / 255u)));
                    dst[3] = static_cast<uint8_t>(std::min<uint32_t>(255u, a + (((dst[3] * inv) + 127u) / 255u)));
                }
            }
        }

        if (out_pbgra) {
            *out_pbgra = canvas_;
        }

        if (copy_w > 0 && copy_h > 0) {
            if (disposal == GifDisposal::RestoreBg) {
                for (uint32_t y = 0; y < copy_h; ++y) {
                    uint8_t* row = canvas_.data() + (static_cast<size_t>(top + y) * canvas_stride) + (static_cast<size_t>(left) * 4u);
                    std::memset(row, 0, static_cast<size_t>(copy_w) * 4u);
                }
            } else if (disposal == GifDisposal::RestorePrev) {
                restore_patch(left, top, copy_w, copy_h);
            }
        }
        return true;
    }

    void save_patch(uint32_t left, uint32_t top, uint32_t copy_w, uint32_t copy_h) {
        const size_t patch_row = static_cast<size_t>(copy_w) * 4u;
        const size_t canvas_stride = static_cast<size_t>(cw_) * 4u;
        prev_patch_.resize(patch_row * copy_h);
        for (uint32_t y = 0; y < copy_h; ++y) {
            const uint8_t* src = canvas_.data() + (static_cast<size_t>(top + y) * canvas_stride) + (static_cast<size_t>(left) * 4u);
            std::memcpy(prev_patch_.data() + (static_cast<size_t>(y) * patch_row), src, patch_row);
        }
    }

    void restore_patch(uint32_t left, uint32_t top, uint32_t copy_w, uint32_t copy_h) noexcept {
        const size_t patch_row = static_cast<size_t>(copy_w) * 4u;
        if (prev_patch_.size() < patch_row * copy_h) return;
        const size_t canvas_stride = static_cast<size_t>(cw_) * 4u;
        for (uint32_t y = 0; y < copy_h; ++y) {
            uint8_t* dst = canvas_.data() + (static_cast<size_t>(top + y) * canvas_stride) + (static_cast<size_t>(left) * 4u);
            std::memcpy(dst, prev_patch_.data() + (static_cast<size_t>(y) * patch_row), patch_row);
        }
    }

    uint32_t cw_ = 0;
    uint32_t ch_ = 0;
    std::vector<uint8_t> canvas_;
    std::vector<uint8_t> prev_patch_;
};

/// Throttled seek state machine so scrubbing does not flood Media Foundation with redundant SetCurrentTime calls
/// and displayed scrubber position tracks interactive drags without getting permanently stuck on missed events.
class SeekState {
public:
    static constexpr int64_t TIMEOUT_MS = 180;

    void reset() noexcept {
        in_flight_ = false;
        show_target_ = false;
        pending_.reset();
        started_ms_ = 0;
        target_s_ = 0.0;
    }

    /// Requests seeking to `target_s`. Returns the immediate seek target if caller should issue `SetCurrentTime` now,
    /// or `std::nullopt` if queued behind an active seek.
    [[nodiscard]] std::optional<double> request(double target_s, double duration_s, int64_t now_ms) noexcept {
        if (duration_s <= 0.0 || !std::isfinite(target_s)) return std::nullopt;
        const double clamped = std::clamp(target_s, 0.0, duration_s);
        target_s_ = clamped;
        show_target_ = true;

        if (in_flight_ && (now_ms - started_ms_ < TIMEOUT_MS)) {
            pending_ = clamped;
            return std::nullopt;
        }
        in_flight_ = true;
        started_ms_ = now_ms;
        pending_.reset();
        return clamped;
    }

    /// Called if issuing `SetCurrentTime` failed immediately.
    void cancel_current() noexcept {
        in_flight_ = false;
        show_target_ = false;
        pending_.reset();
    }

    /// Marks the active seek completed and returns any queued newer target to dispatch next.
    [[nodiscard]] std::optional<double> on_seeked(int64_t now_ms) noexcept {
        in_flight_ = false;
        if (pending_) {
            const double next = *pending_;
            pending_.reset();
            in_flight_ = true;
            started_ms_ = now_ms;
            target_s_ = next;
            show_target_ = true;
            return next;
        }
        return std::nullopt;
    }

    /// Clears stale displayed seek target once playback resumes or the post-seek frame has landed.
    void on_frame_presented(bool playing) noexcept {
        if (!in_flight_ && !pending_ && playing) {
            show_target_ = false;
        }
    }

    void settle_paused_display() noexcept {
        if (!in_flight_ && !pending_) {
            show_target_ = false;
        }
    }

    [[nodiscard]] double displayed_time(double engine_time_s, bool playing, double duration_s) const noexcept {
        if (show_target_ && (!playing || in_flight_ || pending_.has_value())) {
            return duration_s > 0.0 ? std::clamp(target_s_, 0.0, duration_s) : 0.0;
        }
        if (!std::isfinite(engine_time_s) || engine_time_s < 0.0) return 0.0;
        return duration_s > 0.0 ? std::clamp(engine_time_s, 0.0, duration_s) : engine_time_s;
    }

    [[nodiscard]] bool in_flight() const noexcept { return in_flight_; }
    [[nodiscard]] bool has_pending() const noexcept { return pending_.has_value(); }
    [[nodiscard]] bool active() const noexcept { return in_flight_ || pending_.has_value(); }
    [[nodiscard]] int64_t started_ms() const noexcept { return started_ms_; }

private:
    bool in_flight_ = false;
    bool show_target_ = false;
    std::optional<double> pending_;
    int64_t started_ms_ = 0;
    double target_s_ = 0.0;
};

} // namespace usylibpp::ui
