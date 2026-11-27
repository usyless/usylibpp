#pragma once

#include "gfx.hpp"
#include "preview_math.hpp"

#include <mferror.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfmediaengine.h>
#include <shlwapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <utility>

namespace usylibpp::ui {

/// Thread-safe, coalescing callback sink for IMFMediaEngine events.
/// Filters high-frequency timer/progress events and stops posting immediately on detach().
class MediaEngineNotify final : public IMFMediaEngineNotify {
public:
    MediaEngineNotify(HWND hwnd, UINT msg) noexcept : hwnd_{hwnd}, msg_{msg} {} // NOLINT(cppcoreguidelines-pro-type-member-init)
    MediaEngineNotify(const MediaEngineNotify&) = delete;
    MediaEngineNotify& operator=(const MediaEngineNotify&) = delete;
    MediaEngineNotify(MediaEngineNotify&&) = delete;
    MediaEngineNotify& operator=(MediaEngineNotify&&) = delete;

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) noexcept override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFMediaEngineNotify)) {
            *ppv = static_cast<IMFMediaEngineNotify*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() noexcept override {
        return ++ref_count_;
    }

    STDMETHODIMP_(ULONG) Release() noexcept override {
        const ULONG c = --ref_count_;
        if (c == 0) delete this;
        return c;
    }

    void detach() noexcept {
        const std::scoped_lock lk{mtx_};
        hwnd_ = nullptr;
        event_count_ = 0;
        posted_ = false;
    }

    void rebind(HWND hwnd, UINT msg) noexcept {
        const std::scoped_lock lk{mtx_};
        hwnd_ = hwnd;
        msg_ = msg;
        posted_ = false;
    }

    [[nodiscard]] size_t drain(std::array<DWORD, 256>& out) noexcept {
        const std::scoped_lock lk{mtx_};
        posted_ = false;
        const size_t n = event_count_;
        if (n > 0) std::copy_n(events_.data(), n, out.data());
        event_count_ = 0;
        return n;
    }

    void clear() noexcept {
        const std::scoped_lock lk{mtx_};
        posted_ = false;
        event_count_ = 0;
    }

    STDMETHODIMP EventNotify(DWORD event, DWORD_PTR /*param1*/, DWORD /*param2*/) noexcept override {
        if (!is_significant_event(event)) return S_OK;

        HWND hwnd = nullptr;
        UINT msg = 0;
        {
            const std::scoped_lock lk{mtx_};
            if (!hwnd_ || !msg_) return S_OK;
            if (event_count_ < events_.size()) {
                events_[event_count_++] = event;
            } else {
                // Keep the latest significant event (e.g. ERROR / SEEKED / ENDED) if the queue saturates.
                events_.back() = event;
            }
            if (!posted_) {
                posted_ = true;
                hwnd = hwnd_;
                msg = msg_;
            }
        }
        if (hwnd && msg && !PostMessageW(hwnd, msg, 0, 0)) {
            const std::scoped_lock lk{mtx_};
            posted_ = false;
        }
        return S_OK;
    }

private:
    ~MediaEngineNotify() = default;

    [[nodiscard]] static bool is_significant_event(DWORD event) noexcept {
        switch (event) {
            case MF_MEDIA_ENGINE_EVENT_LOADSTART:
            case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
            case MF_MEDIA_ENGINE_EVENT_LOADEDDATA:
            case MF_MEDIA_ENGINE_EVENT_CANPLAY:
            case MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH:
            case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY:
            case MF_MEDIA_ENGINE_EVENT_PLAYING:
            case MF_MEDIA_ENGINE_EVENT_PAUSE:
            case MF_MEDIA_ENGINE_EVENT_SEEKED:
            case MF_MEDIA_ENGINE_EVENT_ENDED:
            case MF_MEDIA_ENGINE_EVENT_ERROR:
            case MF_MEDIA_ENGINE_EVENT_FORMATCHANGE:
                return true;
            default:
                return false;
        }
    }

    std::atomic<ULONG> ref_count_{1};
    std::mutex mtx_;
    HWND hwnd_ = nullptr;
    UINT msg_ = 0;
    std::array<DWORD, 256> events_; // NOLINT(cppcoreguidelines-pro-type-member-init)
    size_t event_count_ = 0;
    bool posted_ = false;
};

/// Hardware-accelerated Media Foundation frame-server video player for preview overlays.
/// Lazily initializes MF on first open, survives D3D/DXGI device loss & adapter switches with
/// position/play-state restoration, and releases COM/MF objects deterministically in shutdown().
class VideoPlayer {
public:
    VideoPlayer() = default;
    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;
    VideoPlayer(VideoPlayer&&) = delete;
    VideoPlayer& operator=(VideoPlayer&&) = delete;
    ~VideoPlayer() { shutdown(); }

    void set_notify(HWND hwnd, UINT msg) noexcept {
        notify_hwnd_ = hwnd;
        notify_msg_ = msg;
        if (notify_cb_) notify_cb_->rebind(hwnd, msg);
    }

    void shutdown() noexcept {
        gpu_recovery_ = {};
        shutdown_engine();
        reset_source_state();
        if (mf_started_) {
            MFShutdown();
            mf_started_ = false;
        }
    }

    void reset_gpu() {
        if (!current_path_.empty() && phase_ != LoadPhase::Idle && phase_ != LoadPhase::Failed && !gpu_recovery_.active) {
            gpu_recovery_.active = true;
            gpu_recovery_.path = current_path_;
            gpu_recovery_.pos_s = current_time();
            gpu_recovery_.want_play = want_play_;
        }
        scrubbing_ = false;
        shutdown_engine();
        reset_frame_state();
        if (gpu_recovery_.active) {
            current_path_.clear();
            phase_ = LoadPhase::Idle;
        }
    }

    bool recover_if_needed(ID3D11Device* d3d, uint64_t gfx_epoch) {
        if (!gpu_recovery_.active || !d3d) return false;
        const GpuRecovery rec = std::exchange(gpu_recovery_, {});
        const std::optional<double> resume_pos = rec.pos_s > 0.1 ? std::optional<double>{rec.pos_s} : std::nullopt;
        return open(rec.path, d3d, gfx_epoch, rec.want_play, resume_pos);
    }

    bool open(
        const std::wstring& path,
        ID3D11Device* d3d,
        uint64_t gfx_epoch,
        bool autoplay = true,
        std::optional<double> start_pos_s = std::nullopt) {
        if (path.empty()) {
            close();
            return false;
        }
        if (current_path_ == path && phase_ != LoadPhase::Idle && phase_ != LoadPhase::Failed && !gpu_recovery_.active) {
            return true;
        }

        if (!d3d) {
            close_source_only();
            gpu_recovery_.active = true;
            gpu_recovery_.path = path;
            gpu_recovery_.pos_s = start_pos_s.value_or(0.0);
            gpu_recovery_.want_play = autoplay;
            return false;
        }

        gpu_recovery_ = {};
        close_source_only();

        if (!bind_device(d3d, gfx_epoch) || !create_engine_instance()) {
            current_path_ = path;
            phase_ = LoadPhase::Failed;
            return false;
        }

        current_path_ = path;
        phase_ = LoadPhase::WaitingLoadStart;
        want_play_ = autoplay;
        autoplay_handled_ = false;
        audio_only_ = false;
        deferred_resume_seek_ = start_pos_s;

        engine_->SetAutoPlay(FALSE);
        engine_->SetLoop(TRUE);
        engine_->SetVolume(volume_);
        engine_->SetMuted(muted_ ? TRUE : FALSE);

        HRESULT hr_src = E_FAIL;
        if (const auto engine_ex = engine_.try_query<IMFMediaEngineEx>()) {
            wil::com_ptr<IStream> file_stream;
            if (SUCCEEDED(SHCreateStreamOnFileEx(
                    path.c_str(),
                    static_cast<DWORD>(STGM_READ) | static_cast<DWORD>(STGM_SHARE_DENY_NONE),
                    FILE_ATTRIBUTE_NORMAL,
                    FALSE,
                    nullptr,
                    &file_stream)) &&
                file_stream) {
                wil::com_ptr<IMFByteStream> byte_stream;
                if (SUCCEEDED(MFCreateMFByteStreamOnStream(file_stream.get(), &byte_stream)) && byte_stream) {
                    const std::wstring hint_url = make_mf_extension_hint_url(path);
                    const auto hint_bstr = wil::make_bstr_nothrow(hint_url.c_str());
                    if (hint_bstr) {
                        hr_src = engine_ex->SetSourceFromByteStream(byte_stream.get(), hint_bstr.get());
                    }
                }
            }
        }

        if (FAILED(hr_src)) {
            const std::wstring mf_url = make_file_url(path);
            const auto bstr = wil::make_bstr_nothrow(mf_url.c_str());
            if (!bstr) {
                shutdown_instance_only();
                phase_ = LoadPhase::Failed;
                return false;
            }
            hr_src = engine_->SetSource(bstr.get());
        }

        (void)process_events();

        if (FAILED(hr_src) || phase_ == LoadPhase::Failed) {
            shutdown_instance_only();
            phase_ = LoadPhase::Failed;
            want_play_ = false;
            scrubbing_ = false;
            has_frame_ = false;
            return false;
        }
        return true;
    }

    void close() noexcept {
        gpu_recovery_ = {};
        close_source_only();
    }

    void suspend() noexcept {
        if (!engine_ || current_path_.empty() || phase_ == LoadPhase::Failed) return;
        want_play_ = false;
        scrubbing_ = false;
        engine_->Pause();
    }

    /// Processes queued IMFMediaEngine events cleanly on the UI thread.
    /// Returns true if visible state changed.
    bool process_events() noexcept {
        if (!notify_cb_) return false;
        std::array<DWORD, 256> events; // NOLINT(cppcoreguidelines-pro-type-member-init)
        const size_t n = notify_cb_->drain(events);
        if (n == 0 || !engine_ || phase_ == LoadPhase::Idle) return false;

        bool changed = false;
        for (size_t i = 0; i < n; ++i) {
            if (phase_ == LoadPhase::Failed || !engine_) break;
            const DWORD ev = events[i];
            switch (ev) {
                case MF_MEDIA_ENGINE_EVENT_LOADSTART:
                    if (phase_ == LoadPhase::WaitingLoadStart) {
                        phase_ = LoadPhase::Loading;
                        changed = true;
                    }
                    break;
                case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
                case MF_MEDIA_ENGINE_EVENT_LOADEDDATA:
                case MF_MEDIA_ENGINE_EVENT_CANPLAY:
                case MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH:
                case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY: {
                    phase_ = LoadPhase::Ready;
                    if (!engine_->HasVideo() && engine_->HasAudio()) {
                        audio_only_ = true;
                    }
                    if (deferred_resume_seek_ && duration() > 0.0) {
                        const double target = *deferred_resume_seek_;
                        deferred_resume_seek_.reset();
                        seek(target);
                    }
                    if (!autoplay_handled_) {
                        autoplay_handled_ = true;
                        if (want_play_ && !scrubbing_) {
                            engine_->Play();
                        } else {
                            engine_->Pause();
                        }
                    }
                    changed = true;
                    break;
                }
                case MF_MEDIA_ENGINE_EVENT_FORMATCHANGE:
                    needs_retransfer_ = true;
                    changed = true;
                    break;
                case MF_MEDIA_ENGINE_EVENT_SEEKED: {
                    if (phase_ != LoadPhase::Loading && phase_ != LoadPhase::Ready) break;
                    advance_seek();
                    changed = true;
                    break;
                }
                case MF_MEDIA_ENGINE_EVENT_ERROR: {
                    phase_ = LoadPhase::Failed;
                    want_play_ = false;
                    scrubbing_ = false;
                    has_frame_ = false;
                    gpu_recovery_ = {};
                    changed = true;
                    break;
                }
                case MF_MEDIA_ENGINE_EVENT_PLAYING:
                case MF_MEDIA_ENGINE_EVENT_PAUSE:
                case MF_MEDIA_ENGINE_EVENT_ENDED:
                    changed = true;
                    break;
                default:
                    break;
            }
        }
        return changed;
    }

    bool set_output_size(uint32_t w_px, uint32_t h_px) noexcept {
        const uint32_t cw = std::clamp(w_px, 1u, 8192u);
        const uint32_t ch = std::clamp(h_px, 1u, 8192u);
        if (cw == desired_w_ && ch == desired_h_) return false;
        desired_w_ = cw;
        desired_h_ = ch;
        if (has_frame_) needs_retransfer_ = true;
        return true;
    }

    /// Transfers a newly decoded video frame (or re-scales on resize/seek) into `video_bmp_`.
    [[nodiscard]] MediaTickStatus tick(ID3D11Device* d3d, uint64_t gfx_epoch, ID2D1DeviceContext* dc) noexcept {
        if (!engine_ || !d3d || !dc || current_path_.empty() ||
            phase_ == LoadPhase::Idle || phase_ == LoadPhase::Failed) {
            return MediaTickStatus::NoChange;
        }
        if (bound_epoch_ == 0 || gfx_epoch != bound_epoch_) {
            return MediaTickStatus::DeviceLost;
        }

        // Recover if Media Foundation finished seeking without delivering SEEKED.
        if ((phase_ == LoadPhase::Loading || phase_ == LoadPhase::Ready) &&
            seek_state_.in_flight() && engine_->IsSeeking() == FALSE &&
            (now_ms() - seek_state_.started_ms()) >= 1000) {
            advance_seek();
        }

        LONGLONG pts = -1;
        const HRESULT tick_hr = engine_->OnVideoStreamTick(&pts);
        if (is_device_lost_hr(tick_hr)) return MediaTickStatus::DeviceLost;

        const bool new_frame = (tick_hr == S_OK && pts >= 0);
        const bool rescale = (needs_retransfer_ && has_frame_);
        const bool seek_ready = seek_frame_ready_;
        if (!new_frame && !rescale && !seek_ready) {
            return MediaTickStatus::NoChange;
        }
        seek_frame_ready_ = false;

        DWORD vw = 0;
        DWORD vh = 0;
        if (FAILED(engine_->GetNativeVideoSize(&vw, &vh)) || vw == 0 || vh == 0) {
            if (!engine_->HasVideo() && engine_->HasAudio()) audio_only_ = true;
            return MediaTickStatus::NoChange;
        }
        native_w_ = vw;
        native_h_ = vh;
        audio_only_ = false;

        auto [out_w, out_h] = fit_scaled_size(vw, vh, 1920u);
        if (desired_w_ > 0 && desired_h_ > 0) {
            out_w = std::clamp(desired_w_, 1u, 8192u);
            out_h = std::clamp(desired_h_, 1u, 8192u);
        }

        if (!video_tex_ || !video_bmp_ || tex_w_ != out_w || tex_h_ != out_h) {
            video_bmp_.reset();
            video_tex_.reset();
            tex_w_ = tex_h_ = 0;

            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = out_w;
            desc.Height = out_h;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = static_cast<UINT>(D3D11_BIND_RENDER_TARGET) | static_cast<UINT>(D3D11_BIND_SHADER_RESOURCE);

            HRESULT hr = d3d->CreateTexture2D(&desc, nullptr, &video_tex_);
            if (is_device_lost_hr(hr)) {
                video_tex_.reset();
                return MediaTickStatus::DeviceLost;
            }
            if (FAILED(hr) || !video_tex_) return MediaTickStatus::NoChange;

            wil::com_ptr<IDXGISurface> surface;
            if (FAILED(video_tex_->QueryInterface(IID_PPV_ARGS(&surface))) || !surface) {
                video_tex_.reset();
                return MediaTickStatus::NoChange;
            }

            const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_NONE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
                96.0f, 96.0f);
            hr = dc->CreateBitmapFromDxgiSurface(surface.get(), &props, &video_bmp_);
            if (is_device_lost_hr(hr)) {
                video_bmp_.reset();
                video_tex_.reset();
                return MediaTickStatus::DeviceLost;
            }
            if (FAILED(hr) || !video_bmp_) {
                video_tex_.reset();
                return MediaTickStatus::NoChange;
            }
            tex_w_ = out_w;
            tex_h_ = out_h;
        }

        const RECT dst_rect{.left = 0, .top = 0, .right = static_cast<LONG>(out_w), .bottom = static_cast<LONG>(out_h)};
        const MFARGB border{.rgbBlue = 0, .rgbGreen = 0, .rgbRed = 0, .rgbAlpha = 255};
        const HRESULT xfer_hr = engine_->TransferVideoFrame(video_tex_.get(), nullptr, &dst_rect, &border);
        if (is_device_lost_hr(xfer_hr)) return MediaTickStatus::DeviceLost;
        if (FAILED(xfer_hr)) return MediaTickStatus::NoChange;

        frame_w_ = out_w;
        frame_h_ = out_h;
        has_frame_ = true;
        needs_retransfer_ = false;
        seek_state_.on_frame_presented(is_playing());
        if (!is_playing()) seek_state_.settle_paused_display();
        return MediaTickStatus::FrameReady;
    }

    void toggle_play() noexcept {
        if (!engine_ || phase_ == LoadPhase::Idle || phase_ == LoadPhase::Failed) return;
        if (scrubbing_) {
            want_play_ = !want_play_;
            return;
        }
        autoplay_handled_ = true;
        if (want_play_ && is_playing()) {
            want_play_ = false;
            engine_->Pause();
        } else {
            want_play_ = true;
            engine_->Play();
        }
    }

    /// Pauses playback during an interactive scrub while preserving intended play state (`want_play_`).
    void begin_scrub() noexcept {
        if (!engine_ || phase_ == LoadPhase::Idle || phase_ == LoadPhase::Failed) return;
        scrubbing_ = true;
        engine_->Pause();
    }

    /// Ends an interactive scrub and resumes playback if `want_play_` is still true.
    void end_scrub() noexcept {
        if (!scrubbing_) return;
        scrubbing_ = false;
        if (!engine_ || phase_ == LoadPhase::Idle || phase_ == LoadPhase::Failed) return;
        if (autoplay_handled_ && want_play_) {
            engine_->Play();
        }
    }

    void set_muted(bool muted) noexcept {
        muted_ = muted;
        if (engine_) engine_->SetMuted(muted_ ? TRUE : FALSE);
    }

    void toggle_mute() noexcept {
        set_muted(!muted_);
    }

    void set_volume(double vol) noexcept {
        volume_ = std::isfinite(vol) ? std::clamp(vol, 0.0, 1.0) : 1.0;
        if (engine_) engine_->SetVolume(volume_);
    }

    void seek(double target_s) noexcept {
        if (!engine_ || phase_ == LoadPhase::Idle || phase_ == LoadPhase::Failed) return;
        const double dur = duration();
        if (const auto immediate = seek_state_.request(target_s, dur, now_ms())) {
            if (FAILED(engine_->SetCurrentTime(*immediate))) {
                seek_state_.cancel_current();
            }
        }
    }

    void seek_relative(double delta_s) noexcept {
        const double dur = duration();
        if (dur <= 0.0) return;
        seek(current_time() + delta_s);
    }

    [[nodiscard]] bool active() const noexcept { return !current_path_.empty() && phase_ != LoadPhase::Idle; }
    [[nodiscard]] bool needs_recover() const noexcept { return gpu_recovery_.active; }
    [[nodiscard]] bool failed() const noexcept { return phase_ == LoadPhase::Failed; }
    [[nodiscard]] bool is_loading() const noexcept {
        return !current_path_.empty() && phase_ != LoadPhase::Failed && !has_frame_ && !audio_only_;
    }
    [[nodiscard]] bool is_playing() const noexcept {
        if (!engine_ || phase_ == LoadPhase::Idle || phase_ == LoadPhase::Failed) return false;
        return !engine_->IsPaused() && !engine_->IsEnded();
    }
    [[nodiscard]] bool want_play() const noexcept { return want_play_; }
    [[nodiscard]] bool is_scrubbing() const noexcept { return scrubbing_; }
    [[nodiscard]] bool is_seeking() const noexcept {
        if (!engine_ || phase_ == LoadPhase::Idle || phase_ == LoadPhase::Failed) return false;
        return seek_state_.active() || seek_frame_ready_ || (engine_->IsSeeking() != FALSE);
    }
    [[nodiscard]] bool needs_timer() const noexcept { return is_playing() || is_seeking(); }
    [[nodiscard]] bool is_muted() const noexcept { return muted_; }
    [[nodiscard]] double volume() const noexcept { return volume_; }
    [[nodiscard]] bool audio_only() const noexcept { return audio_only_; }
    [[nodiscard]] bool has_frame() const noexcept {
        return video_bmp_ != nullptr && native_w_ > 0 && native_h_ > 0 && frame_w_ > 0 && frame_h_ > 0 && has_frame_;
    }

    [[nodiscard]] double duration() const noexcept {
        if (!engine_ || phase_ == LoadPhase::Idle || phase_ == LoadPhase::Failed) return 0.0;
        const double d = engine_->GetDuration();
        return (std::isfinite(d) && d > 0.0) ? d : 0.0;
    }

    [[nodiscard]] double current_time() const noexcept {
        if (!engine_ || phase_ == LoadPhase::Idle || phase_ == LoadPhase::Failed) return 0.0;
        return seek_state_.displayed_time(engine_->GetCurrentTime(), is_playing(), duration());
    }

    [[nodiscard]] ID2D1Bitmap1* bitmap() const noexcept { return video_bmp_.get(); }
    [[nodiscard]] uint32_t native_width() const noexcept { return native_w_; }
    [[nodiscard]] uint32_t native_height() const noexcept { return native_h_; }
    [[nodiscard]] uint32_t frame_width() const noexcept { return frame_w_; }
    [[nodiscard]] uint32_t frame_height() const noexcept { return frame_h_; }
    [[nodiscard]] const std::wstring& path() const noexcept { return current_path_; }

private:
    enum class LoadPhase : uint8_t { Idle, WaitingLoadStart, Loading, Ready, Failed };

    struct GpuRecovery {
        bool active = false;
        std::wstring path;
        double pos_s = 0.0;
        bool want_play = true;
    };

    [[nodiscard]] static int64_t now_ms() noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    [[nodiscard]] static bool is_device_lost_hr(HRESULT hr) noexcept {
        return hr == DXGI_ERROR_DEVICE_REMOVED ||
               hr == DXGI_ERROR_DEVICE_RESET ||
               hr == D2DERR_RECREATE_TARGET ||
               hr == MF_E_DXGI_DEVICE_NOT_INITIALIZED;
    }

    [[nodiscard]] static std::wstring make_file_url(std::wstring_view path) {
        std::wstring normalized = normalize_mf_source_path(path);
        auto url_len = static_cast<DWORD>(normalized.size() + 64u);
        std::wstring url(url_len, L'\0');
        HRESULT hr = UrlCreateFromPathW(normalized.c_str(), url.data(), &url_len, 0);
        if (hr == E_POINTER && url_len > 0) {
            url.resize(url_len);
            hr = UrlCreateFromPathW(normalized.c_str(), url.data(), &url_len, 0);
        }
        if (SUCCEEDED(hr) && url_len > 0) {
            url.resize(url_len);
            return url;
        }
        return normalized;
    }

    bool ensure_mf() noexcept {
        if (mf_started_) return true;
        if (FAILED(MFStartup(MF_VERSION))) return false;
        mf_started_ = true;
        return true;
    }

    bool bind_device(ID3D11Device* d3d, uint64_t gfx_epoch) noexcept {
        if (!d3d || !ensure_mf()) return false;

        wil::com_ptr<ID3D10Multithread> mt;
        if (SUCCEEDED(d3d->QueryInterface(IID_PPV_ARGS(&mt))) && mt) {
            mt->SetMultithreadProtected(TRUE);
        }

        if (!dxgi_manager_) {
            if (FAILED(MFCreateDXGIDeviceManager(&reset_token_, &dxgi_manager_)) || !dxgi_manager_) {
                return false;
            }
            bound_epoch_ = 0;
        }

        if (bound_epoch_ != gfx_epoch) {
            if (FAILED(dxgi_manager_->ResetDevice(d3d, reset_token_))) {
                shutdown_engine();
                if (FAILED(MFCreateDXGIDeviceManager(&reset_token_, &dxgi_manager_)) ||
                    !dxgi_manager_ ||
                    FAILED(dxgi_manager_->ResetDevice(d3d, reset_token_))) {
                    dxgi_manager_.reset();
                    return false;
                }
            }
            bound_epoch_ = gfx_epoch;
        }
        return true;
    }

    bool create_engine_instance() noexcept {
        shutdown_instance_only();
        if (!dxgi_manager_) return false;

        wil::com_ptr<IMFMediaEngineClassFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
            !factory) {
            return false;
        }

        wil::com_ptr<IMFAttributes> attrs;
        if (FAILED(MFCreateAttributes(&attrs, 3)) || !attrs) return false;

        auto* notify_raw = new (std::nothrow) MediaEngineNotify(notify_hwnd_, notify_msg_);
        if (!notify_raw) return false;
        notify_cb_.attach(notify_raw);
        attrs->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, notify_cb_.get());
        attrs->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, dxgi_manager_.get());
        attrs->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM);

        if (FAILED(factory->CreateInstance(0, attrs.get(), &engine_)) || !engine_) {
            notify_cb_->detach();
            notify_cb_.reset();
            engine_.reset();
            return false;
        }
        return true;
    }

    void shutdown_instance_only() noexcept {
        if (notify_cb_) {
            notify_cb_->detach();
            notify_cb_.reset();
        }
        if (engine_) {
            engine_->Pause();
            engine_->Shutdown();
            engine_.reset();
        }
    }

    void close_source_only() noexcept {
        shutdown_instance_only();
        reset_source_state();
    }

    void reset_frame_state() noexcept {
        video_bmp_.reset();
        video_tex_.reset();
        tex_w_ = tex_h_ = 0;
        native_w_ = native_h_ = 0;
        frame_w_ = frame_h_ = 0;
        desired_w_ = desired_h_ = 0;
        has_frame_ = false;
        needs_retransfer_ = false;
        seek_frame_ready_ = false;
        seek_state_.reset();
    }

    void reset_source_state() noexcept {
        current_path_.clear();
        phase_ = LoadPhase::Idle;
        want_play_ = false;
        scrubbing_ = false;
        autoplay_handled_ = false;
        audio_only_ = false;
        deferred_resume_seek_.reset();
        reset_frame_state();
    }

    void advance_seek() noexcept {
        if (const auto next = seek_state_.on_seeked(now_ms())) {
            if (FAILED(engine_->SetCurrentTime(*next))) {
                seek_state_.cancel_current();
                seek_frame_ready_ = true;
            }
        } else {
            seek_frame_ready_ = true;
        }
    }

    void shutdown_engine() noexcept {
        shutdown_instance_only();
        video_bmp_.reset();
        video_tex_.reset();
        dxgi_manager_.reset();
        reset_token_ = 0;
        bound_epoch_ = 0;
    }

    HWND notify_hwnd_ = nullptr;
    UINT notify_msg_ = 0;
    bool mf_started_ = false;
    bool muted_ = false;
    double volume_ = 1.0;
    bool want_play_ = false;
    bool scrubbing_ = false;
    bool autoplay_handled_ = false;
    bool audio_only_ = false;
    LoadPhase phase_ = LoadPhase::Idle;
    std::wstring current_path_;
    std::optional<double> deferred_resume_seek_;
    GpuRecovery gpu_recovery_;

    UINT reset_token_ = 0;
    uint64_t bound_epoch_ = 0;
    wil::com_ptr<IMFDXGIDeviceManager> dxgi_manager_;
    wil::com_ptr<MediaEngineNotify> notify_cb_;
    wil::com_ptr<IMFMediaEngine> engine_;

    wil::com_ptr<ID3D11Texture2D> video_tex_;
    wil::com_ptr<ID2D1Bitmap1> video_bmp_;
    uint32_t tex_w_ = 0, tex_h_ = 0;
    uint32_t native_w_ = 0, native_h_ = 0;
    uint32_t frame_w_ = 0, frame_h_ = 0;
    uint32_t desired_w_ = 0, desired_h_ = 0;
    bool has_frame_ = false;
    bool needs_retransfer_ = false;
    bool seek_frame_ready_ = false;
    SeekState seek_state_;
};

} // namespace usylibpp::ui
