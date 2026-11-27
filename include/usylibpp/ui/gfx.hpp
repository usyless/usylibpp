#pragma once

#include "dirty_rects.hpp"

#include <windows.h>
#include <d2d1_3.h>
#include <d2d1_3helper.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <dwrite_3.h>
#include <dxgi1_6.h>
#include <wincodec.h>
#include <wil/com.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace usylibpp::ui {

/// Per-monitor color space, HDR status, and Windows SDR content brightness reference.
struct MonitorColorInfo {
    DXGI_COLOR_SPACE_TYPE color_space = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    float sdr_white_nits = 80.0f;  // Windows "SDR content brightness" in nits (80..480+)
    float sdr_white_scale = 1.0f;  // sdr_white_nits / 80.0f (linear scRGB SDR white reference)
    float max_luminance = 80.0f;   // panel peak luminance (nits) reported by DXGI_OUTPUT_DESC1
    float min_luminance = 0.0f;
    bool is_hdr = false;           // true when monitor is outputting HDR10 ST.2084 / BT.2020
};

/// Queries the Windows "SDR content brightness" scale (`sdr_nits / 80.0`, >= 1.0) for `hmon` via DisplayConfig.
[[nodiscard]] inline float query_monitor_sdr_white_scale(HMONITOR hmon) noexcept {
    if (!hmon) return 1.0f;
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hmon, &mi)) return 1.0f;

    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    LONG rc = ERROR_INSUFFICIENT_BUFFER;
    for (int attempt = 0; attempt < 3 && rc == ERROR_INSUFFICIENT_BUFFER; ++attempt) {
        UINT32 num_paths = 0;
        UINT32 num_modes = 0;
        rc = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &num_paths, &num_modes);
        if (rc != ERROR_SUCCESS || num_paths == 0) return 1.0f;
        try {
            paths.resize(num_paths);
            modes.resize(num_modes);
        } catch (...) {
            return 1.0f;
        }
        rc = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &num_paths, paths.data(), &num_modes, modes.data(), nullptr);
        if (rc == ERROR_SUCCESS) {
            paths.resize(num_paths);
        }
    }
    if (rc != ERROR_SUCCESS) return 1.0f;

    for (const DISPLAYCONFIG_PATH_INFO& path : paths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME src_name{};
        src_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        src_name.header.size = sizeof(src_name);
        src_name.header.adapterId = path.sourceInfo.adapterId;
        src_name.header.id = path.sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&src_name.header) != ERROR_SUCCESS) continue;
        if (_wcsicmp(src_name.viewGdiDeviceName, mi.szDevice) != 0) continue;

        DISPLAYCONFIG_SDR_WHITE_LEVEL white_lvl{};
        white_lvl.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
        white_lvl.header.size = sizeof(white_lvl);
        white_lvl.header.adapterId = path.targetInfo.adapterId;
        white_lvl.header.id = path.targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&white_lvl.header) == ERROR_SUCCESS && white_lvl.SDRWhiteLevel > 0) {
            return std::clamp(static_cast<float>(white_lvl.SDRWhiteLevel) / 1000.0f, 1.0f, 20.0f);
        }
        break;
    }
    return 1.0f;
}

/// Queries DXGI 1.6 (`IDXGIOutput6`) color space / luminance metadata and DisplayConfig SDR white level.
[[nodiscard]] inline MonitorColorInfo query_monitor_color_info(HMONITOR hmon, IDXGIOutput* output = nullptr) noexcept {
    MonitorColorInfo info{};
    if (output) {
        wil::com_ptr<IDXGIOutput6> out6;
        if (SUCCEEDED(output->QueryInterface(IID_PPV_ARGS(&out6))) && out6) {
            DXGI_OUTPUT_DESC1 desc1{};
            if (SUCCEEDED(out6->GetDesc1(&desc1))) {
                info.color_space = desc1.ColorSpace;
                info.is_hdr = (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
                if (desc1.MaxLuminance > 0.0f) info.max_luminance = desc1.MaxLuminance;
                info.min_luminance = desc1.MinLuminance;
                if (!hmon) hmon = desc1.Monitor;
            }
        }
    }
    if (hmon) {
        info.sdr_white_scale = query_monitor_sdr_white_scale(hmon);
        info.sdr_white_nits = info.sdr_white_scale * 80.0f;
    }
    return info;
}

/// Process-wide graphics device shared by every window. Recreated wholesale on device loss or when
/// the adapter set changes (eGPU unplug, driver update, TDR); `epoch()` bumps on every recreation so
/// owners of device-bound resources (bitmaps) know to drop them.
class Gfx {
public:
    bool init() {
        const D2D1_FACTORY_OPTIONS opts{};
        if (!d2d_factory_ && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory7), &opts, reinterpret_cast<void**>(&d2d_factory_)))) return false;
        if (!dwrite_ && FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory3), reinterpret_cast<IUnknown**>(&dwrite_)))) return false;
        if (!wic_ && FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_)))) return false;
        return create_device(nullptr);
    }

    /// Drops the D3D/D2D GPU device when all windows are hidden to keep idle VRAM & RAM tiny.
    void release_device() noexcept {
        d2d_device_.reset();
        dxgi_device_.reset();
        frame_query_.reset();
        if (d3d_ctx_) {
            d3d_ctx_->ClearState();
            d3d_ctx_->Flush();
        }
        d3d_ctx_.reset();
        d3d_.reset();
        dxgi_factory_.reset();
        ++epoch_;
        if (on_device_changed) on_device_changed();
    }

    bool ensure_device(HWND hint = nullptr) {
        if (ready() && !device_lost()) return true;
        if (!d2d_factory_ || !dwrite_ || !wic_) return init();
        return create_device(hint);
    }

    [[nodiscard]] ID2D1Factory7* d2d_factory() const noexcept { return d2d_factory_.get(); }
    [[nodiscard]] IDWriteFactory3* dwrite() const noexcept { return dwrite_.get(); }
    [[nodiscard]] IWICImagingFactory* wic() const noexcept { return wic_.get(); }
    [[nodiscard]] ID3D11Device* d3d() const noexcept { return d3d_.get(); }
    [[nodiscard]] ID3D11DeviceContext* d3d_context() const noexcept { return d3d_ctx_.get(); }
    [[nodiscard]] IDXGIFactory6* dxgi_factory() const noexcept { return dxgi_factory_.get(); }
    [[nodiscard]] IDXGIDevice* dxgi_device() const noexcept { return dxgi_device_.get(); }
    [[nodiscard]] ID2D1Device6* d2d_device() const noexcept { return d2d_device_.get(); }
    [[nodiscard]] uint64_t epoch() const noexcept { return epoch_; }
    [[nodiscard]] bool ready() const noexcept { return d2d_device_ != nullptr; }

    /// Called by windows when presenting fails or the adapter may have changed.
    bool recover(HWND hint) {
        return create_device(hint);
    }

    [[nodiscard]] bool device_lost() const noexcept {
        return !d3d_ || FAILED(d3d_->GetDeviceRemovedReason());
    }

    [[nodiscard]] bool is_warp() const noexcept { return warp_; }
    [[nodiscard]] LUID adapter_luid() const noexcept { return luid_; }

    /// Refreshes `dxgi_factory_` in place when DXGI reports topology/session staleness (`IsCurrent() == FALSE`)
    /// without tearing down a healthy D3D11/D2D device.
    bool refresh_dxgi_factory() noexcept {
        if (dxgi_factory_ && dxgi_factory_->IsCurrent()) return true;
        wil::com_ptr<IDXGIFactory6> fresh;
        if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&fresh))) || !fresh) return false;
        dxgi_factory_ = std::move(fresh);
        return true;
    }

    /// Returns the adapter matching the active D3D11 device's LUID from the current DXGI factory.
    [[nodiscard]] wil::com_ptr<IDXGIAdapter1> current_adapter() noexcept {
        if (!refresh_dxgi_factory() || !dxgi_factory_) return nullptr;
        return find_adapter(dxgi_factory_.get(), luid_);
    }

    /// True when the D3D device was lost, its adapter disappeared, or `hwnd` now sits on another GPU's monitor.
    /// Refreshes a stale `IDXGIFactory6` (e.g. after a UAC Secure Desktop switch) in place rather than
    /// triggering a full device rebuild when the GPU itself has not changed.
    [[nodiscard]] bool adapter_stale(HWND hwnd) noexcept {
        if (device_lost()) return true;
        if (!refresh_dxgi_factory()) return true;
        if (warp_) return false;
        if (const auto a = window_adapter(dxgi_factory_.get(), hwnd)) {
            DXGI_ADAPTER_DESC1 d{};
            if (SUCCEEDED(a->GetDesc1(&d))) return d.AdapterLuid.LowPart != luid_.LowPart || d.AdapterLuid.HighPart != luid_.HighPart;
        }
        return !find_adapter(dxgi_factory_.get(), luid_);
    }

    std::function<void()> on_device_changed;

private:
    static wil::com_ptr<IDXGIAdapter1> find_adapter(IDXGIFactory6* f, LUID luid) noexcept {
        if (!f) return nullptr;
        for (UINT i = 0;; ++i) {
            wil::com_ptr<IDXGIAdapter1> a;
            if (FAILED(f->EnumAdapters1(i, &a)) || !a) break;
            DXGI_ADAPTER_DESC1 d{};
            if (SUCCEEDED(a->GetDesc1(&d)) && d.AdapterLuid.LowPart == luid.LowPart && d.AdapterLuid.HighPart == luid.HighPart) {
                return a;
            }
        }
        return nullptr;
    }
    static wil::com_ptr<IDXGIAdapter1> window_adapter(IDXGIFactory6* f, HWND hwnd) noexcept {
        if (!f || !hwnd) return nullptr;
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        for (UINT i = 0;; ++i) {
            wil::com_ptr<IDXGIAdapter1> a;
            if (FAILED(f->EnumAdapters1(i, &a)) || !a) break;
            for (UINT j = 0;; ++j) {
                wil::com_ptr<IDXGIOutput> o;
                if (FAILED(a->EnumOutputs(j, &o)) || !o) break;
                DXGI_OUTPUT_DESC od{};
                if (SUCCEEDED(o->GetDesc(&od)) && od.Monitor == mon) return a;
            }
        }
        return nullptr;
    }

    bool create_device(HWND hint) {
        d2d_device_.reset();
        dxgi_device_.reset();
        frame_query_.reset();
        if (d3d_ctx_) {
            d3d_ctx_->ClearState();
            d3d_ctx_->Flush();
        }
        d3d_ctx_.reset();
        d3d_.reset();
        dxgi_factory_.reset();

        wil::com_ptr<IDXGIFactory6> factory;
        if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return false;
        wil::com_ptr<IDXGIAdapter1> adapter = window_adapter(factory.get(), hint);
        if (!adapter) factory->EnumAdapters1(0, &adapter);

        static constexpr std::array<D3D_FEATURE_LEVEL, 4> levels = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
        const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        HRESULT hr = E_FAIL;
        warp_ = false;
        if (adapter) hr = D3D11CreateDevice(adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, levels.data(), static_cast<UINT>(levels.size()), D3D11_SDK_VERSION, &d3d_, nullptr, &d3d_ctx_);
        if (FAILED(hr)) hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels.data(), static_cast<UINT>(levels.size()), D3D11_SDK_VERSION, &d3d_, nullptr, &d3d_ctx_);
        if (FAILED(hr)) {
            warp_ = true;
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels.data(), static_cast<UINT>(levels.size()), D3D11_SDK_VERSION, &d3d_, nullptr, &d3d_ctx_);
        }
        if (FAILED(hr)) return false;

        if (const auto mt = d3d_.try_query<ID3D10Multithread>()) {
            mt->SetMultithreadProtected(TRUE);
        }

        if (FAILED(d3d_->QueryInterface(IID_PPV_ARGS(&dxgi_device_)))) return false;
        if (const auto d1 = dxgi_device_.try_query<IDXGIDevice1>()) d1->SetMaximumFrameLatency(1);

        wil::com_ptr<IDXGIAdapter> used;
        if (SUCCEEDED(dxgi_device_->GetAdapter(&used))) {
            DXGI_ADAPTER_DESC d{};
            if (SUCCEEDED(used->GetDesc(&d))) luid_ = d.AdapterLuid;
        }
        dxgi_factory_ = std::move(factory);

        if (FAILED(d2d_factory_->CreateDevice(dxgi_device_.get(), &d2d_device_))) return false;
        // Keep the D2D glyph/bitmap caches small outside Liquid Glass.
        apply_glass_texture_budget();
        ++epoch_;
        if (on_device_changed) on_device_changed();
        return true;
    }

public:
    void add_glass_target() noexcept {
        ++glass_target_count_;
        apply_glass_texture_budget();
    }

    void remove_glass_target() noexcept {
        if (glass_target_count_ > 0) --glass_target_count_;
        apply_glass_texture_budget();
    }

    [[nodiscard]] bool liquid_glass_registered() const noexcept { return liquid_glass_registered_; }
    void mark_liquid_glass_registered() noexcept { liquid_glass_registered_ = true; }
    [[nodiscard]] bool is_effect_registered(const GUID& clsid) const noexcept {
        return std::ranges::any_of(registered_effects_, [&](const GUID& g) noexcept {
            return std::memcmp(&g, &clsid, sizeof(GUID)) == 0;
        });
    }
    void mark_effect_registered(const GUID& clsid) noexcept {
        if (is_effect_registered(clsid)) return;
        try {
            registered_effects_.push_back(clsid);
        } catch (...) {
        }
    }

    void wait_gpu() noexcept {
        if (!d3d_ || !d3d_ctx_) return;
        if (!frame_query_) {
            const D3D11_QUERY_DESC qd{.Query = D3D11_QUERY_EVENT, .MiscFlags = 0};
            if (FAILED(d3d_->CreateQuery(&qd, &frame_query_)) || !frame_query_) return;
        }
        d3d_ctx_->End(frame_query_.get());
        d3d_ctx_->Flush();
        BOOL done = FALSE;
        const ULONGLONG deadline = GetTickCount64() + 100u;
        for (uint32_t spins = 0; d3d_ctx_->GetData(frame_query_.get(), &done, sizeof(done), 0) == S_FALSE; ++spins) {
            if (spins < 64u) {
                YieldProcessor();
            } else {
                if (GetTickCount64() >= deadline || device_lost()) break;
                SwitchToThread();
            }
        }
    }

private:
    void apply_glass_texture_budget() noexcept {
        if (d2d_device_) {
            d2d_device_->SetMaximumTextureMemory((glass_target_count_ > 0 ? 128ull : 32ull) << 20u);
        }
    }

    wil::com_ptr<ID2D1Factory7> d2d_factory_;
    wil::com_ptr<IDWriteFactory3> dwrite_;
    wil::com_ptr<IWICImagingFactory> wic_;
    wil::com_ptr<ID3D11Device> d3d_;
    wil::com_ptr<ID3D11DeviceContext> d3d_ctx_;
    wil::com_ptr<ID3D11Query> frame_query_;
    wil::com_ptr<IDXGIFactory6> dxgi_factory_;
    wil::com_ptr<IDXGIDevice> dxgi_device_;
    wil::com_ptr<ID2D1Device6> d2d_device_;
    LUID luid_{};
    bool warp_ = false;
    bool liquid_glass_registered_ = false;
    std::vector<GUID> registered_effects_;
    int glass_target_count_ = 0;
    uint64_t epoch_ = 0;
};

/// Per-window swapchain composed through DirectComposition (premultiplied alpha so DWM backdrops show through).
class WindowTarget {
public:
    WindowTarget() = default;
    WindowTarget(const WindowTarget&) = delete;
    WindowTarget& operator=(const WindowTarget&) = delete;
    WindowTarget(WindowTarget&&) = delete;
    WindowTarget& operator=(WindowTarget&&) = delete;
    ~WindowTarget() {
        sync_glass_budget(false);
        release();
    }

    bool attach(Gfx& gfx, HWND hwnd, bool liquid_glass = false) noexcept {
        if (gfx_ != &gfx) {
            sync_glass_budget(false);
            gfx_ = &gfx;
        }
        hwnd_ = hwnd;
        liquid_glass_ = liquid_glass;
        sync_glass_budget(liquid_glass_);
        return rebuild();
    }

    void set_liquid_glass(bool on) noexcept {
        if (liquid_glass_ == on) return;
        liquid_glass_ = on;
        sync_glass_budget(on);
        dirty_tracker_.reset();
    }

    [[nodiscard]] bool liquid_glass() const noexcept { return liquid_glass_; }

    bool rebuild() noexcept {
        if (!gfx_ || !gfx_->ready()) {
            release();
            return false;
        }
        sync_glass_budget(liquid_glass_);
        // Keep the HWND's DirectComposition target alive across swap-effect rebuilds on theme switches.
        const bool keep_dcomp = (dcomp_ && dcomp_target_ && visual_ && epoch_ == gfx_->epoch());
        if (keep_dcomp) {
            release_swap_and_dc();
        } else {
            release();
        }
        if (FAILED(gfx_->d2d_device()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &dc_))) {
            release();
            return false;
        }

        RECT rc{};
        GetClientRect(hwnd_, &rc);
        w_ = static_cast<UINT>(std::max(1L, rc.right - rc.left));
        h_ = static_cast<UINT>(std::max(1L, rc.bottom - rc.top));

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = w_;
        desc.Height = h_;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        if (!gfx_->refresh_dxgi_factory()) {
            release();
            return false;
        }
        wil::com_ptr<IDXGISwapChain1> sc1;
        if (FAILED(gfx_->dxgi_factory()->CreateSwapChainForComposition(gfx_->d3d(), &desc, nullptr, &sc1)) || !sc1) {
            release();
            return false;
        }
        swap_ = sc1.try_query<IDXGISwapChain3>();
        if (!swap_) {
            release();
            return false;
        }
        if (!keep_dcomp) {
            if (FAILED(DCompositionCreateDevice(gfx_->dxgi_device(), IID_PPV_ARGS(&dcomp_))) ||
                FAILED(dcomp_->CreateTargetForHwnd(hwnd_, TRUE, &dcomp_target_)) ||
                FAILED(dcomp_->CreateVisual(&visual_)) ||
                FAILED(dcomp_target_->SetRoot(visual_.get()))) {
                release();
                return false;
            }
        }
        if (FAILED(visual_->SetContent(swap_.get()))) {
            release();
            return false;
        }
        // Defer `dcomp_->Commit()` until the first frame is presented so DWM never composes an unpresented swapchain.
        dcomp_commit_pending_ = true;

        dpi_ = static_cast<float>(GetDpiForWindow(hwnd_));
        if (dpi_ <= 0) dpi_ = 96.0f;
        if (!create_target()) {
            release();
            return false;
        }
        epoch_ = gfx_->epoch();
        return true;
    }

    void release() noexcept {
        release_swap_and_dc();
        dcomp_commit_pending_ = false;
        visual_.reset();
        dcomp_target_.reset();
        dcomp_.reset();
    }

    [[nodiscard]] bool ready() const noexcept { return dc_ && target_ && gfx_ && epoch_ == gfx_->epoch(); }
    [[nodiscard]] ID2D1DeviceContext6* dc() const noexcept { return dc_.get(); }
    [[nodiscard]] uint64_t target_epoch() const noexcept { return target_epoch_; }
    [[nodiscard]] float scale() const noexcept { return dpi_ / 96.0f; }
    [[nodiscard]] D2D1_SIZE_F size() const noexcept { return {.width = static_cast<float>(w_) / scale(), .height = static_cast<float>(h_) / scale()}; }
    [[nodiscard]] ID2D1Bitmap1* target_bitmap() const noexcept { return target_.get(); }
    [[nodiscard]] IDXGISwapChain3* swap_chain() const noexcept { return swap_.get(); }
    [[nodiscard]] UINT pixel_width() const noexcept { return w_; }
    [[nodiscard]] UINT pixel_height() const noexcept { return h_; }
    [[nodiscard]] float dpi() const noexcept { return dpi_; }

    /// Reusable solid-color brush for the active device context.
    [[nodiscard]] ID2D1SolidColorBrush* solid_brush() noexcept {
        if (!solid_brush_ && dc_) {
            dc_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &solid_brush_);
        }
        return solid_brush_.get();
    }

    /// Reusable bitmap brush bound to `bmp` for thumbnail rendering.
    [[nodiscard]] ID2D1BitmapBrush1* bitmap_brush(ID2D1Bitmap1* bmp) noexcept {
        if (!bmp || !dc_) return nullptr;
        if (!bmp_brush_) {
            const auto bp = D2D1::BitmapBrushProperties1(D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP, D2D1_INTERPOLATION_MODE_LINEAR);
            dc_->CreateBitmapBrush(bmp, &bp, nullptr, &bmp_brush_);
        } else {
            bmp_brush_->SetBitmap(bmp);
        }
        return bmp_brush_.get();
    }

    /// Reusable layer for anti-aliased rounded clipping without per-frame allocations.
    [[nodiscard]] ID2D1Layer* clip_layer() noexcept {
        if (!clip_layer_ && dc_) {
            dc_->CreateLayer(nullptr, &clip_layer_);
        }
        return clip_layer_.get();
    }

    /// Cached rounded-rectangle geometry for `(r, radius)`, recreated only when bounds, radius, or D2D factory epoch change.
    [[nodiscard]] ID2D1RoundedRectangleGeometry* round_clip_geom(D2D1_RECT_F r, float radius) noexcept {
        if (!gfx_ || !gfx_->d2d_factory()) return nullptr;
        const uint64_t ep = gfx_->epoch();
        if (clip_geom_ && clip_geom_epoch_ == ep && clip_radius_ == radius &&
            clip_rect_.left == r.left && clip_rect_.top == r.top &&
            clip_rect_.right == r.right && clip_rect_.bottom == r.bottom) {
            return clip_geom_.get();
        }
        clip_geom_.reset();
        if (FAILED(gfx_->d2d_factory()->CreateRoundedRectangleGeometry(D2D1::RoundedRect(r, radius, radius), &clip_geom_))) {
            return nullptr;
        }
        clip_rect_ = r;
        clip_radius_ = radius;
        clip_geom_epoch_ = ep;
        return clip_geom_.get();
    }

    bool resize(UINT w, UINT h) noexcept {
        if (w == 0 || h == 0) return ready();
        const auto dpi = static_cast<float>(GetDpiForWindow(hwnd_));
        if (!ready()) return rebuild();
        if (w == w_ && h == h_ && dpi == dpi_) return true;
        dpi_ = dpi > 0 ? dpi : 96.0f;
        dc_->SetTarget(nullptr);
        target_.reset();
        if (FAILED(swap_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0))) return rebuild();
        w_ = w;
        h_ = h;
        dirty_tracker_.reset();
        return create_target();
    }

    /// Refreshes `dpi_` and the D2D target bitmap when the window's DPI changes without a pixel resize.
    bool sync_dpi() noexcept {
        if (!hwnd_ || !ready()) return ready();
        const auto dpi = static_cast<float>(GetDpiForWindow(hwnd_));
        if (dpi <= 0.0f || dpi == dpi_) return true;
        dpi_ = dpi;
        dc_->SetTarget(nullptr);
        target_.reset();
        dirty_tracker_.reset();
        return create_target();
    }

    enum class PresentMode : uint8_t { Vsync, Immediate, PacedDrag };

    [[nodiscard]] PresentMode present_mode_for(bool in_sizemove, bool from_drag_pump) const noexcept {
        if (in_sizemove) return liquid_glass_ ? PresentMode::PacedDrag : PresentMode::Immediate;
        return from_drag_pump ? PresentMode::Immediate : PresentMode::Vsync;
    }

    /// Draws and presents a frame. When `full_redraw` is false and both swap-chain buffers are synced,
    /// only the union of previous and current dirty regions is cleared, drawn clipped, and passed to `Present1`.
    /// Returns false if the device must be recovered.
    template <typename DrawFn>
    bool render_frame(bool full_redraw, std::span<const D2D1_RECT_F> dirty_dips, PresentMode present_mode, DrawFn&& draw_fn,
                      bool* did_pace_out = nullptr) {
        if (did_pace_out) *did_pace_out = false;
        if (!ready()) return false;
        const float s = scale();
        if (capture_) full_redraw = true;
        // Liquid Glass drag frames are paced against DWM below so the swapchain flip and the HWND move
        // land in the same composition pass.
        const bool paced = liquid_glass_ && (present_mode == PresentMode::PacedDrag);
        const UINT sync_interval = (present_mode == PresentMode::Vsync) ? 1u : 0u;

        curr_px_.clear();
        if (!full_redraw) {
            curr_px_.reserve(dirty_dips.size());
            for (const D2D1_RECT_F& r : dirty_dips) {
                const PixelRect px = dip_to_pixel_rect(r.left, r.top, r.right, r.bottom, s, w_, h_, 2.0f);
                if (!px.empty()) curr_px_.push_back(px);
            }
        }

        const FlipDirtyTracker::Plan p = dirty_tracker_.plan(full_redraw, curr_px_, w_, h_);
        if (p.skip) return true;

        present_rects_.clear();
        dc_->BeginDraw();
        dc_->SetTransform(D2D1::Matrix3x2F::Identity());
        if (p.full) {
            dc_->Clear(D2D1::ColorF(0, 0, 0, 0));
            draw_fn(std::nullopt, true);
        } else {
            present_rects_.reserve(p.render_rects.size());
            for (const PixelRect& pr : p.render_rects) {
                present_rects_.emplace_back(pr.left, pr.top, pr.right, pr.bottom);
                const D2D1_RECT_F clip = D2D1::RectF(
                    static_cast<float>(pr.left) / s,
                    static_cast<float>(pr.top) / s,
                    static_cast<float>(pr.right) / s,
                    static_cast<float>(pr.bottom) / s);
                dc_->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_ALIASED);
                dc_->Clear(D2D1::ColorF(0, 0, 0, 0));
                draw_fn(clip, false);
                dc_->PopAxisAlignedClip();
            }
        }

        HRESULT hr = dc_->EndDraw();
        if (bmp_brush_) bmp_brush_->SetBitmap(nullptr);
        if (SUCCEEDED(hr) && capture_) {
            capture_ok_ = write_png(*capture_);
            capture_.reset();
        }
        bool did_pace = false;
        if (SUCCEEDED(hr)) {
            if (paced) {
                // 1. Finish all GPU work so the flip is ready the instant it is queued.
                if (gfx_) gfx_->wait_gpu();
                // 2. Wait until DWM has just finished a composition pass. That leaves almost a whole frame
                //    before DWM latches again, so the Present below and the move DefWindowProc applies right
                //    after WM_WINDOWPOSCHANGING returns reach the same composition (no one-frame lead).
                (void)DwmFlush();
                did_pace = true;
            }
            DXGI_PRESENT_PARAMETERS params{};
            if (!p.full) {
                params.DirtyRectsCount = static_cast<UINT>(present_rects_.size());
                params.pDirtyRects = present_rects_.data();
            }
            hr = swap_->Present1(sync_interval, 0, &params);
        }
        if (hr == DXGI_STATUS_OCCLUDED) {
            dirty_tracker_.reset();
            if (did_pace_out) *did_pace_out = did_pace;
            return true;
        }
        if (FAILED(hr)) return false;

        if (dcomp_commit_pending_ && dcomp_) {
            if (FAILED(dcomp_->Commit())) return false;
            dcomp_commit_pending_ = false;
        }

        dirty_tracker_.commit(full_redraw, p);
        if (did_pace_out) *did_pace_out = did_pace;
        return true;
    }

    void request_capture(std::filesystem::path p) noexcept {
        capture_ = std::move(p);
        capture_ok_ = false;
    }
    [[nodiscard]] bool capture_ok() const noexcept { return capture_ok_; }

private:
    void release_swap_and_dc() noexcept {
        if (bmp_brush_) bmp_brush_->SetBitmap(nullptr);
        bmp_brush_.reset();
        solid_brush_.reset();
        clip_layer_.reset();
        clip_geom_.reset();
        clip_geom_epoch_ = 0;
        dirty_tracker_.reset();
        if (dc_) dc_->SetTarget(nullptr);
        target_.reset();
        swap_.reset();
        dc_.reset();
        ++target_epoch_;
    }

    bool create_target() noexcept {
        wil::com_ptr<IDXGISurface> surface;
        if (FAILED(swap_->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;
        const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                   D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi_, dpi_);
        if (FAILED(dc_->CreateBitmapFromDxgiSurface(surface.get(), &props, &target_))) return false;
        dc_->SetTarget(target_.get());
        dc_->SetDpi(dpi_, dpi_);
        dc_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        return true;
    }

    bool write_png(const std::filesystem::path& path) {
        wil::com_ptr<ID3D11Texture2D> back;
        if (FAILED(swap_->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
        D3D11_TEXTURE2D_DESC desc{};
        back->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        wil::com_ptr<ID3D11Texture2D> staging;
        if (FAILED(gfx_->d3d()->CreateTexture2D(&desc, nullptr, &staging))) return false;
        gfx_->d3d_context()->CopyResource(staging.get(), back.get());
        D3D11_MAPPED_SUBRESOURCE m{};
        if (FAILED(gfx_->d3d_context()->Map(staging.get(), 0, D3D11_MAP_READ, 0, &m))) return false;

        // Composite over a dark backdrop so translucent areas read correctly in the PNG
        std::vector<uint8_t> px(static_cast<size_t>(desc.Width) * desc.Height * 4);
        for (UINT y = 0; y < desc.Height; ++y) {
            const uint8_t* src = static_cast<const uint8_t*>(m.pData) + (static_cast<size_t>(y) * m.RowPitch);
            uint8_t* dst = px.data() + (static_cast<size_t>(y) * desc.Width * 4);
            for (UINT x = 0; x < desc.Width; ++x) {
                const size_t px_off = static_cast<size_t>(x) * 4u;
                const uint8_t a = src[px_off + 3];
                static constexpr std::array<uint8_t, 3> bg = {0x1a, 0x18, 0x1c};
                for (size_t c = 0; c < 3; ++c) dst[px_off + c] = static_cast<uint8_t>(src[px_off + c] + (bg[c] * (255 - a) / 255));
                dst[px_off + 3] = 255;
            }
        }
        gfx_->d3d_context()->Unmap(staging.get(), 0);

        IWICImagingFactory* wic = gfx_->wic();
        wil::com_ptr<IWICStream> stream;
        if (FAILED(wic->CreateStream(&stream)) || FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) return false;
        wil::com_ptr<IWICBitmapEncoder> enc;
        if (FAILED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) || FAILED(enc->Initialize(stream.get(), WICBitmapEncoderNoCache))) return false;
        wil::com_ptr<IWICBitmapFrameEncode> frame;
        if (FAILED(enc->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr))) return false;
        frame->SetSize(desc.Width, desc.Height);
        WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
        frame->SetPixelFormat(&fmt);
        if (FAILED(frame->WritePixels(desc.Height, desc.Width * 4, static_cast<UINT>(px.size()), px.data()))) return false;
        return SUCCEEDED(frame->Commit()) && SUCCEEDED(enc->Commit());
    }

    void sync_glass_budget(bool want) noexcept {
        if (want == counted_glass_ || !gfx_) return;
        counted_glass_ = want;
        if (want) {
            gfx_->add_glass_target();
        } else {
            gfx_->remove_glass_target();
        }
    }

    Gfx* gfx_ = nullptr;
    HWND hwnd_ = nullptr;
    wil::com_ptr<ID2D1DeviceContext6> dc_;
    wil::com_ptr<ID2D1Bitmap1> target_;
    wil::com_ptr<ID2D1SolidColorBrush> solid_brush_;
    wil::com_ptr<ID2D1BitmapBrush1> bmp_brush_;
    wil::com_ptr<ID2D1Layer> clip_layer_;
    wil::com_ptr<ID2D1RoundedRectangleGeometry> clip_geom_;
    D2D1_RECT_F clip_rect_{};
    float clip_radius_ = -1.0f;
    uint64_t clip_geom_epoch_ = 0;
    wil::com_ptr<IDXGISwapChain3> swap_;
    wil::com_ptr<IDCompositionDevice> dcomp_;
    wil::com_ptr<IDCompositionTarget> dcomp_target_;
    wil::com_ptr<IDCompositionVisual> visual_;
    UINT w_ = 0, h_ = 0;
    float dpi_ = 96.0f;
    uint64_t epoch_ = 0;
    uint64_t target_epoch_ = 0;
    FlipDirtyTracker dirty_tracker_;
    std::vector<PixelRect> curr_px_;
    std::vector<RECT> present_rects_;
    std::optional<std::filesystem::path> capture_;
    bool capture_ok_ = false;
    bool liquid_glass_ = false;
    bool counted_glass_ = false;
    bool dcomp_commit_pending_ = false;
};

} // namespace usylibpp::ui
