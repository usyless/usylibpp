#pragma once

#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <objidl.h>
#include <roapi.h>
#include <windows.foundation.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <windows.graphics.directx.h>
#include <winstring.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace usylibpp::ui::wgc {

/// RAII WinRT apartment initializer that tolerates an already-initialized STA or MTA apartment.
struct RoScope {
    HRESULT hr = E_FAIL;

    explicit RoScope(RO_INIT_TYPE preferred = RO_INIT_MULTITHREADED) noexcept {
        hr = RoInitialize(preferred);
        if (hr == RPC_E_CHANGED_MODE) {
            hr = RoInitialize(preferred == RO_INIT_MULTITHREADED ? RO_INIT_SINGLETHREADED : RO_INIT_MULTITHREADED);
        }
    }
    RoScope(const RoScope&) = delete;
    RoScope& operator=(const RoScope&) = delete;
    RoScope(RoScope&&) = delete;
    RoScope& operator=(RoScope&&) = delete;
    ~RoScope() {
        if (SUCCEEDED(hr)) RoUninitialize();
    }

    [[nodiscard]] bool ok() const noexcept { return SUCCEEDED(hr); }
};

inline HRESULT make_hstring_ref(std::wstring_view lit, HSTRING_HEADER& hdr, HSTRING& out) noexcept {
    return WindowsCreateStringReference(lit.data(), static_cast<UINT32>(lit.size()), &hdr, &out);
}

template <typename TInterface>
inline HRESULT get_activation_factory(std::wstring_view class_name, wil::com_ptr<TInterface>& out) noexcept {
    HSTRING_HEADER hdr{};
    HSTRING hstr = nullptr;
    const HRESULT hr = make_hstring_ref(class_name, hdr, hstr);
    if (FAILED(hr)) return hr;
    return RoGetActivationFactory(hstr, IID_PPV_ARGS(&out));
}

inline void close_winrt_object(IUnknown* obj) noexcept {
    if (!obj) return;
    wil::com_ptr<ABI::Windows::Foundation::IClosable> closable;
    if (SUCCEEDED(obj->QueryInterface(IID_PPV_ARGS(&closable))) && closable) {
        (void)closable->Close();
    }
}

/// Agile FrameArrived delegate that owns its auto-reset Win32 event so in-flight thread-pool callbacks
/// can never signal a closed handle during session teardown.
class FrameArrivedHandler final
    : public ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Graphics::Capture::Direct3D11CaptureFramePool*, IInspectable*>,
      public IAgileObject {
public:
    FrameArrivedHandler() noexcept {
        event_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    }
    FrameArrivedHandler(const FrameArrivedHandler&) = delete;
    FrameArrivedHandler& operator=(const FrameArrivedHandler&) = delete;
    FrameArrivedHandler(FrameArrivedHandler&&) = delete;
    FrameArrivedHandler& operator=(FrameArrivedHandler&&) = delete;

    [[nodiscard]] HANDLE event_handle() const noexcept { return event_.get(); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) noexcept override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) ||
            riid == __uuidof(ABI::Windows::Foundation::ITypedEventHandler<
                        ABI::Windows::Graphics::Capture::Direct3D11CaptureFramePool*, IInspectable*>)) {
            *ppv = static_cast<ABI::Windows::Foundation::ITypedEventHandler<
                ABI::Windows::Graphics::Capture::Direct3D11CaptureFramePool*, IInspectable*>*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == __uuidof(IAgileObject)) {
            *ppv = static_cast<IAgileObject*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override {
        return refs_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override {
        const ULONG n = refs_.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (n == 0) delete this;
        return n;
    }

    HRESULT STDMETHODCALLTYPE Invoke(
        ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool*,
        IInspectable*) noexcept override {
        if (event_) SetEvent(event_.get());
        return S_OK;
    }

private:
    ~FrameArrivedHandler() = default;

    std::atomic<ULONG> refs_{1};
    wil::unique_handle event_;
};

/// Creates a D3D11 device + WinRT IDirect3DDevice (or wraps an existing `shared_d3d` device).
struct WgcDevice {
    wil::com_ptr<ID3D11Device> d3d;
    wil::com_ptr<ID3D11DeviceContext> ctx;
    wil::com_ptr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice> winrt_dev;
    bool owns_d3d = false;

    WgcDevice() = default;
    WgcDevice(const WgcDevice&) = delete;
    WgcDevice& operator=(const WgcDevice&) = delete;
    WgcDevice(WgcDevice&&) = delete;
    WgcDevice& operator=(WgcDevice&&) = delete;
    ~WgcDevice() { reset(); }

    void reset() noexcept {
        close_winrt_object(winrt_dev.get());
        winrt_dev.reset();
        if (ctx) {
            if (owns_d3d) ctx->ClearState();
            ctx->Flush();
        }
        ctx.reset();
        d3d.reset();
        owns_d3d = false;
    }

    bool init(ID3D11Device* shared_d3d = nullptr, bool need_video = false) noexcept {
        reset();

        if (shared_d3d) {
            d3d = shared_d3d;
            d3d->GetImmediateContext(&ctx);
            owns_d3d = false;
        } else {
            owns_d3d = true;
            static constexpr std::array<D3D_FEATURE_LEVEL, 4> levels = {
                D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,};
            const UINT base_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
            HRESULT hr = E_FAIL;
            if (need_video) {
                hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                       base_flags | static_cast<UINT>(D3D11_CREATE_DEVICE_VIDEO_SUPPORT),
                                       levels.data(), static_cast<UINT>(levels.size()),
                                       D3D11_SDK_VERSION, &d3d, nullptr, &ctx);
            }
            if (FAILED(hr)) {
                hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, base_flags,
                                       levels.data(), static_cast<UINT>(levels.size()),
                                       D3D11_SDK_VERSION, &d3d, nullptr, &ctx);
            }
            if (FAILED(hr)) {
                hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, base_flags,
                                       levels.data(), static_cast<UINT>(levels.size()),
                                       D3D11_SDK_VERSION, &d3d, nullptr, &ctx);
            }
            if (FAILED(hr)) {
                reset();
                return false;
            }
        }
        if (!d3d || !ctx) {
            reset();
            return false;
        }

        if (const auto mt = d3d.try_query<ID3D10Multithread>()) {
            mt->SetMultithreadProtected(TRUE);
        }

        const auto dxgi_dev = d3d.try_query<IDXGIDevice>();
        if (!dxgi_dev) {
            reset();
            return false;
        }

        wil::com_ptr<IInspectable> insp;
        if (FAILED(CreateDirect3D11DeviceFromDXGIDevice(dxgi_dev.get(), &insp)) || !insp) {
            reset();
            return false;
        }
        winrt_dev = insp.try_query<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
        if (!winrt_dev) {
            reset();
            return false;
        }
        return true;
    }
};

/// Single-HMONITOR Windows.Graphics.Capture session using free-threaded frame delivery.
class MonitorCaptureSession {
public:
    MonitorCaptureSession() = default;
    MonitorCaptureSession(const MonitorCaptureSession&) = delete;
    MonitorCaptureSession& operator=(const MonitorCaptureSession&) = delete;
    MonitorCaptureSession(MonitorCaptureSession&&) = delete;
    MonitorCaptureSession& operator=(MonitorCaptureSession&&) = delete;
    ~MonitorCaptureSession() { stop(); }

    bool start(ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice* winrt_dev,
               HMONITOR hmon, bool include_cursor, INT32 num_buffers = 2,
               int64_t min_update_interval_100ns = 0,
               ABI::Windows::Graphics::DirectX::DirectXPixelFormat format =
                   ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized,
               bool report_dirty_regions = false) noexcept {
        stop();
        if (!winrt_dev || !hmon) return false;
        winrt_dev_ = winrt_dev;
        num_buffers_ = std::max<INT32>(1, num_buffers);
        format_ = format;

        wil::com_ptr<IGraphicsCaptureItemInterop> interop;
        if (FAILED(get_activation_factory(RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem, interop)) || !interop) {
            return false;
        }
        if (FAILED(interop->CreateForMonitor(hmon, IID_PPV_ARGS(&item_))) || !item_) {
            return false;
        }

        ABI::Windows::Graphics::SizeInt32 sz{};
        if (FAILED(item_->get_Size(&sz)) || sz.Width <= 0 || sz.Height <= 0) {
            stop();
            return false;
        }
        pool_size_ = sz;

        wil::com_ptr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePoolStatics2> pool_statics;
        if (FAILED(get_activation_factory(RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool, pool_statics)) ||
            !pool_statics) {
            stop();
            return false;
        }
        if (FAILED(pool_statics->CreateFreeThreaded(
                winrt_dev_.get(),
                format_,
                num_buffers_,
                pool_size_,
                &frame_pool_)) || !frame_pool_) {
            stop();
            return false;
        }

        handler_.attach(new (std::nothrow) FrameArrivedHandler());
        if (!handler_ || !handler_->event_handle()) {
            stop();
            return false;
        }
        if (FAILED(frame_pool_->add_FrameArrived(handler_.get(), &frame_token_))) {
            stop();
            return false;
        }
        has_token_ = true;

        if (FAILED(frame_pool_->CreateCaptureSession(item_.get(), &session_)) || !session_) {
            stop();
            return false;
        }

        // Windows 10 2004+ / Windows 11: native DWM cursor toggle
        if (const auto s2 = session_.try_query<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession2>()) {
            (void)s2->put_IsCursorCaptureEnabled(include_cursor ? TRUE : FALSE);
        }
        // Windows 11 (22H2+ borderless for Win32 apps): hide yellow capture border
        if (const auto s3 = session_.try_query<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession3>()) {
            (void)s3->put_IsBorderRequired(FALSE);
        }
#if defined(WINDOWS_FOUNDATION_UNIVERSALAPICONTRACT_VERSION) && WINDOWS_FOUNDATION_UNIVERSALAPICONTRACT_VERSION >= 0x130000
        // Windows 11 24H2+: report dirty rects while still rendering the full frame surface
        if (report_dirty_regions) {
            if (const auto s4 = session_.try_query<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession4>()) {
                (void)s4->put_DirtyRegionMode(
                    ABI::Windows::Graphics::Capture::GraphicsCaptureDirtyRegionMode_ReportOnly);
            }
        }
        // Windows 11 24H2+: optional minimum update interval to reduce redundant DWM copies on high-Hz monitors
        if (min_update_interval_100ns > 0) {
            if (const auto s5 = session_.try_query<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession5>()) {
                const ABI::Windows::Foundation::TimeSpan ts{.Duration = min_update_interval_100ns};
                (void)s5->put_MinUpdateInterval(ts);
            }
        }
#else
        (void)report_dirty_regions;
        (void)min_update_interval_100ns;
#endif

        if (FAILED(session_->StartCapture())) {
            stop();
            return false;
        }
        return true;
    }

    void stop() noexcept {
        if (frame_pool_ && has_token_) {
            (void)frame_pool_->remove_FrameArrived(frame_token_);
            has_token_ = false;
        }
        close_winrt_object(session_.get());
        session_.reset();
        close_winrt_object(frame_pool_.get());
        frame_pool_.reset();
        item_.reset();
        handler_.reset();
        winrt_dev_.reset();
        pool_size_ = {};
    }

    [[nodiscard]] bool active() const noexcept { return session_ != nullptr && frame_pool_ != nullptr; }
    [[nodiscard]] HANDLE event_handle() const noexcept {
        return handler_ ? handler_->event_handle() : nullptr;
    }
    [[nodiscard]] ABI::Windows::Graphics::SizeInt32 pool_size() const noexcept { return pool_size_; }

    /// Drains all queued frames and invokes `fn(ID3D11Texture2D*, SizeInt32)` synchronously on the newest frame.
    /// Automatically recreates the frame pool if monitor resolution changes.
    template <typename Fn>
    bool consume_latest_frame(Fn&& fn) noexcept {
        bool unused_dirty_valid = false;
        return consume_latest_frame_impl(std::forward<Fn>(fn), nullptr, unused_dirty_valid);
    }

    /// Drains all queued frames, accumulating Win11 24H2+ dirty regions (in monitor-local pixels) into `dirty_rects`.
    /// Sets `dirty_valid = true` only when every drained frame reported valid dirty rects and size did not change.
    template <typename Fn>
    bool consume_latest_frame_with_dirty(Fn&& fn, std::vector<RECT>& dirty_rects, bool& dirty_valid) noexcept {
        return consume_latest_frame_impl(std::forward<Fn>(fn), &dirty_rects, dirty_valid);
    }

private:
    static bool append_frame_dirty_regions(
        ABI::Windows::Graphics::Capture::IDirect3D11CaptureFrame* frame,
        std::vector<RECT>& out) noexcept {
#if defined(WINDOWS_FOUNDATION_UNIVERSALAPICONTRACT_VERSION) && WINDOWS_FOUNDATION_UNIVERSALAPICONTRACT_VERSION >= 0x130000
        if (!frame) return false;
        wil::com_ptr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFrame2> f2;
        if (FAILED(frame->QueryInterface(IID_PPV_ARGS(&f2))) || !f2) return false;
        wil::com_ptr<__FIVectorView_1_Windows__CGraphics__CRectInt32> regions;
        if (FAILED(f2->get_DirtyRegions(&regions)) || !regions) return false;
        unsigned int count = 0;
        if (FAILED(regions->get_Size(&count)) || count == 0) return false;
        try {
            out.reserve(out.size() + count);
            for (unsigned int i = 0; i < count; ++i) {
                ABI::Windows::Graphics::RectInt32 r{};
                if (FAILED(regions->GetAt(i, &r))) return false;
                if (r.Width > 0 && r.Height > 0) {
                    out.push_back(RECT{
                        .left = r.X,
                        .top = r.Y,
                        .right = r.X + r.Width,
                        .bottom = r.Y + r.Height,
                    });
                }
            }
        } catch (...) {
            return false;
        }
        return !out.empty();
#else
        (void)frame;
        (void)out;
        return false;
#endif
    }

    template <typename Fn>
    bool consume_latest_frame_impl(Fn&& fn, std::vector<RECT>* dirty_rects, bool& dirty_valid) noexcept {
        dirty_valid = false;
        if (dirty_rects) dirty_rects->clear();
        if (!frame_pool_) return false;

        bool accum_dirty_ok = (dirty_rects != nullptr);
        wil::com_ptr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFrame> latest;
        for (;;) {
            wil::com_ptr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFrame> next;
            if (FAILED(frame_pool_->TryGetNextFrame(&next)) || !next) break;
            if (accum_dirty_ok && !append_frame_dirty_regions(next.get(), *dirty_rects)) {
                accum_dirty_ok = false;
            }
            if (latest) close_winrt_object(latest.get());
            latest = std::move(next);
        }
        if (!latest) return false;

        const auto close_guard = wil::scope_exit([&] {
            close_winrt_object(latest.get());
        });

        ABI::Windows::Graphics::SizeInt32 content_sz{};
        (void)latest->get_ContentSize(&content_sz);

        const bool resized = content_sz.Width > 0 && content_sz.Height > 0 &&
                             (content_sz.Width != pool_size_.Width || content_sz.Height != pool_size_.Height);
        dirty_valid = accum_dirty_ok && !resized && dirty_rects && !dirty_rects->empty();

        wil::com_ptr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> surf;
        if (FAILED(latest->get_Surface(&surf)) || !surf) return false;

        const auto access = surf.try_query<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        if (!access) return false;

        wil::com_ptr<ID3D11Texture2D> tex;
        if (FAILED(access->GetInterface(IID_PPV_ARGS(&tex))) || !tex) return false;

        fn(tex.get(), content_sz);

        if (resized && winrt_dev_) {
            pool_size_ = content_sz;
            (void)frame_pool_->Recreate(
                winrt_dev_.get(),
                format_,
                num_buffers_,
                pool_size_);
        }
        return true;
    }

    wil::com_ptr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice> winrt_dev_;
    wil::com_ptr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem> item_;
    wil::com_ptr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool> frame_pool_;
    wil::com_ptr<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession> session_;
    wil::com_ptr<FrameArrivedHandler> handler_;
    EventRegistrationToken frame_token_{};
    bool has_token_ = false;
    INT32 num_buffers_ = 2;
    ABI::Windows::Graphics::DirectX::DirectXPixelFormat format_ =
        ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized;
    ABI::Windows::Graphics::SizeInt32 pool_size_{};
};

/// Copies a clamped sub-region from `src_tex` into `dst_tex` on the GPU.
inline void gpu_copy_clamped(ID3D11DeviceContext* ctx, ID3D11Texture2D* dst_tex, UINT dst_w, UINT dst_h,
                             UINT dst_x, UINT dst_y, ID3D11Texture2D* src_tex,
                             ABI::Windows::Graphics::SizeInt32 content_sz,
                             UINT src_x, UINT src_y, UINT desired_w, UINT desired_h) noexcept {
    if (!ctx || !dst_tex || !src_tex || desired_w == 0 || desired_h == 0) return;
    if (dst_x >= dst_w || dst_y >= dst_h) return;

    D3D11_TEXTURE2D_DESC td{};
    src_tex->GetDesc(&td);
    const UINT avail_w = (content_sz.Width > 0) ? std::min<UINT>(td.Width, static_cast<UINT>(content_sz.Width)) : td.Width;
    const UINT avail_h = (content_sz.Height > 0) ? std::min<UINT>(td.Height, static_cast<UINT>(content_sz.Height)) : td.Height;
    if (src_x >= avail_w || src_y >= avail_h) return;

    const UINT cw = std::min<UINT>({desired_w, avail_w - src_x, dst_w - dst_x});
    const UINT ch = std::min<UINT>({desired_h, avail_h - src_y, dst_h - dst_y});
    if (cw == 0 || ch == 0) return;

    const D3D11_BOX box{
        .left = src_x,
        .top = src_y,
        .front = 0,
        .right = src_x + cw,
        .bottom = src_y + ch,
        .back = 1,
    };
    ctx->CopySubresourceRegion(dst_tex, 0, dst_x, dst_y, 0, src_tex, 0, &box);
}

/// CPU bilinear down/upscale fallback used only when ID3D11VideoProcessor is unavailable (e.g., WARP).
inline void cpu_scale_bgra_bilinear(const uint8_t* src, UINT src_w, UINT src_h, UINT src_pitch,
                                    uint8_t* dst, UINT dst_w, UINT dst_h) noexcept {
    if (!src || !dst || src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) return;
    const float sx_ratio = static_cast<float>(src_w) / static_cast<float>(dst_w);
    const float sy_ratio = static_cast<float>(src_h) / static_cast<float>(dst_h);
    const UINT max_x = src_w - 1u;
    const UINT max_y = src_h - 1u;

    for (UINT y = 0; y < dst_h; ++y) {
        const float fy = std::max(0.0f, ((static_cast<float>(y) + 0.5f) * sy_ratio) - 0.5f);
        const UINT y0 = std::min(static_cast<UINT>(fy), max_y);
        const UINT y1 = std::min(y0 + 1u, max_y);
        const float wy = fy - static_cast<float>(y0);
        const float wy0 = 1.0f - wy;

        const uint8_t* row0 = src + (static_cast<size_t>(y0) * src_pitch);
        const uint8_t* row1 = src + (static_cast<size_t>(y1) * src_pitch);
        uint8_t* out_row = dst + (static_cast<size_t>(y) * dst_w * 4u);

        for (UINT x = 0; x < dst_w; ++x) {
            const float fx = std::max(0.0f, ((static_cast<float>(x) + 0.5f) * sx_ratio) - 0.5f);
            const UINT x0 = std::min(static_cast<UINT>(fx), max_x);
            const UINT x1 = std::min(x0 + 1u, max_x);
            const float wx = fx - static_cast<float>(x0);
            const float wx0 = 1.0f - wx;

            const uint8_t* p00 = row0 + (static_cast<size_t>(x0) * 4u);
            const uint8_t* p10 = row0 + (static_cast<size_t>(x1) * 4u);
            const uint8_t* p01 = row1 + (static_cast<size_t>(x0) * 4u);
            const uint8_t* p11 = row1 + (static_cast<size_t>(x1) * 4u);

            uint8_t* o = out_row + (static_cast<size_t>(x) * 4u);
            for (int c = 0; c < 3; ++c) {
                const float top = (static_cast<float>(p00[c]) * wx0) + (static_cast<float>(p10[c]) * wx);
                const float bot = (static_cast<float>(p01[c]) * wx0) + (static_cast<float>(p11[c]) * wx);
                const int val = static_cast<int>(std::lround((top * wy0) + (bot * wy)));
                o[c] = static_cast<uint8_t>(std::clamp(val, 0, 255));
            }
            o[3] = 255u;
        }
    }
}

} // namespace usylibpp::ui::wgc
