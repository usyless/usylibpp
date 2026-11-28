#pragma once

#include "../strings.hpp"

#include <windows.h>
#include <objidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace usylibpp::ui {

namespace detail {

using unique_htheme = wil::unique_any<HTHEME, decltype(&::CloseThemeData), ::CloseThemeData>;

inline void composite_hbitmap_into(uint8_t* dst, int dst_w, int dst_h, HBITMAP hsrc,
                                   int dx, int dy, int dw, int dh) {
    if (!dst || !hsrc || dst_w <= 0 || dst_h <= 0 || dw <= 0 || dh <= 0) return;

    BITMAP bm{};
    if (GetObjectW(hsrc, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 || bm.bmHeight <= 0) return;

    const int sw = bm.bmWidth;
    const int sh = bm.bmHeight;
    const size_t pixel_count = static_cast<size_t>(sw) * static_cast<size_t>(sh);
    std::vector<uint8_t> src(pixel_count * 4u);

    BITMAPINFO sbi{};
    sbi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    sbi.bmiHeader.biWidth = sw;
    sbi.bmiHeader.biHeight = -sh; // top-down
    sbi.bmiHeader.biPlanes = 1;
    sbi.bmiHeader.biBitCount = 32;
    sbi.bmiHeader.biCompression = BI_RGB;

    const wil::unique_hdc tmp_dc{CreateCompatibleDC(nullptr)};
    if (!tmp_dc) return;
    if (GetDIBits(tmp_dc.get(), hsrc, 0, static_cast<UINT>(sh), src.data(), &sbi, DIB_RGB_COLORS) != sh) return;

    bool has_alpha = false;
    for (size_t i = 0; i < pixel_count; ++i) {
        if (src[(i * 4u) + 3u] != 0u) {
            has_alpha = true;
            break;
        }
    }

    bool unpremultiplied = false;
    if (has_alpha) {
        for (size_t i = 0; i < pixel_count; ++i) {
            const uint8_t a = src[(i * 4u) + 3u];
            if (src[(i * 4u) + 0u] > a || src[(i * 4u) + 1u] > a || src[(i * 4u) + 2u] > a) {
                unpremultiplied = true;
                break;
            }
        }
    }

    for (size_t i = 0; i < pixel_count; ++i) {
        uint8_t* p = src.data() + (i * 4u);
        if (!has_alpha) {
            p[3] = 255u;
        } else if (unpremultiplied) {
            const unsigned int a = p[3];
            p[0] = static_cast<uint8_t>(((static_cast<unsigned int>(p[0]) * a) + 127u) / 255u);
            p[1] = static_cast<uint8_t>(((static_cast<unsigned int>(p[1]) * a) + 127u) / 255u);
            p[2] = static_cast<uint8_t>(((static_cast<unsigned int>(p[2]) * a) + 127u) / 255u);
        }
    }

    const float scale_x = static_cast<float>(sw) / static_cast<float>(dw);
    const float scale_y = static_cast<float>(sh) / static_cast<float>(dh);

    for (int y = 0; y < dh; ++y) {
        const int ty = dy + y;
        if (ty < 0 || ty >= dst_h) continue;
        const float fy = ((static_cast<float>(y) + 0.5f) * scale_y) - 0.5f;
        const int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, sh - 1);
        const int y1 = std::min(y0 + 1, sh - 1);
        const float wy = std::clamp(fy - static_cast<float>(y0), 0.0f, 1.0f);

        for (int x = 0; x < dw; ++x) {
            const int tx = dx + x;
            if (tx < 0 || tx >= dst_w) continue;
            const float fx = ((static_cast<float>(x) + 0.5f) * scale_x) - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, sw - 1);
            const int x1 = std::min(x0 + 1, sw - 1);
            const float wx = std::clamp(fx - static_cast<float>(x0), 0.0f, 1.0f);

            const uint8_t* p00 = src.data() + (((static_cast<size_t>(y0) * static_cast<size_t>(sw)) + static_cast<size_t>(x0)) * 4u);
            const uint8_t* p10 = src.data() + (((static_cast<size_t>(y0) * static_cast<size_t>(sw)) + static_cast<size_t>(x1)) * 4u);
            const uint8_t* p01 = src.data() + (((static_cast<size_t>(y1) * static_cast<size_t>(sw)) + static_cast<size_t>(x0)) * 4u);
            const uint8_t* p11 = src.data() + (((static_cast<size_t>(y1) * static_cast<size_t>(sw)) + static_cast<size_t>(x1)) * 4u);

            std::array<float, 4> s{};
            for (size_t c = 0; c < 4u; ++c) {
                const float top = static_cast<float>(p00[c]) + ((static_cast<float>(p10[c]) - static_cast<float>(p00[c])) * wx);
                const float bot = static_cast<float>(p01[c]) + ((static_cast<float>(p11[c]) - static_cast<float>(p01[c])) * wx);
                s[c] = top + ((bot - top) * wy);
            }

            uint8_t* d = dst + (((static_cast<size_t>(ty) * static_cast<size_t>(dst_w)) + static_cast<size_t>(tx)) * 4u);
            const float inv_a = 1.0f - (s[3] / 255.0f);
            for (size_t c = 0; c < 4u; ++c) {
                const long v = std::lround(s[c] + (static_cast<float>(d[c]) * inv_a));
                d[c] = static_cast<uint8_t>(std::clamp(v, 0L, 255L));
            }
        }
    }
}

[[nodiscard]] inline wil::unique_hbitmap build_drag_image_bitmap(HWND hwnd, std::span<const std::wstring> paths,
                                                                 SIZE& out_size, POINT& out_offset) {
    UINT dpi = hwnd ? GetDpiForWindow(hwnd) : GetDpiForSystem();
    if (dpi == 0u) dpi = 96u;
    const int dpi_i = static_cast<int>(dpi);

    const int card_size = MulDiv(108, dpi_i, 96);
    const int pad_l = MulDiv(7, dpi_i, 96);
    const int pad_t = MulDiv(7, dpi_i, 96);
    const int pad_r = MulDiv(11, dpi_i, 96);
    const int pad_b = MulDiv(11, dpi_i, 96);
    const int inner_w = std::max(16, card_size - pad_l - pad_r);
    const int inner_h = std::max(16, card_size - pad_t - pad_b);

    constexpr auto FLAGS_CACHED_THUMB = static_cast<SIIGBF>(
        static_cast<unsigned int>(SIIGBF_RESIZETOFIT) |
        static_cast<unsigned int>(SIIGBF_THUMBNAILONLY) |
        static_cast<unsigned int>(SIIGBF_INCACHEONLY));
    constexpr auto FLAGS_THUMB = static_cast<SIIGBF>(
        static_cast<unsigned int>(SIIGBF_RESIZETOFIT) |
        static_cast<unsigned int>(SIIGBF_THUMBNAILONLY));

    wil::unique_hbitmap thumb;
    for (const auto& path : paths) {
        wil::com_ptr<IShellItemImageFactory> sif;
        if (FAILED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&sif))) || !sif) continue;
        const SIZE req{.cx = inner_w, .cy = inner_h};
        HBITMAP raw = nullptr;
        if (FAILED(sif->GetImage(req, FLAGS_CACHED_THUMB, &raw)) || !raw) {
            if (FAILED(sif->GetImage(req, FLAGS_THUMB, &raw)) || !raw) {
                (void)sif->GetImage(req, SIIGBF_RESIZETOFIT, &raw);
            }
        }
        if (raw) {
            thumb.reset(raw);
            break;
        }
    }

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = card_size;
    bi.bmiHeader.biHeight = -card_size; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    const wil::unique_hdc dc{CreateCompatibleDC(nullptr)};
    if (!dc) return nullptr;

    void* bits = nullptr;
    wil::unique_hbitmap card{CreateDIBSection(dc.get(), &bi, DIB_RGB_COLORS, &bits, nullptr, 0)};
    if (!card || !bits) return nullptr;

    const size_t card_pixels = static_cast<size_t>(card_size) * static_cast<size_t>(card_size);
    std::memset(bits, 0, card_pixels * 4u);
    auto* px = static_cast<uint8_t*>(bits);

    const HGDIOBJ old_bmp = SelectObject(dc.get(), card.get());
    const auto restore_bmp = wil::scope_exit([&] {
        if (old_bmp) SelectObject(dc.get(), old_bmp);
    });

    unique_htheme theme{OpenThemeDataForDpi(hwnd, L"DragDrop", dpi)};
    if (!theme) theme.reset(OpenThemeData(hwnd, L"DragDrop"));

    const RECT rc_card{.left = 0, .top = 0, .right = card_size, .bottom = card_size};
    if (theme) {
        (void)DrawThemeBackground(theme.get(), dc.get(), DD_IMAGEBG, 1, &rc_card, nullptr);
    } else {
        const int b0 = MulDiv(2, dpi_i, 96);
        const int b1 = card_size - MulDiv(6, dpi_i, 96);
        for (int y = b0; y < b1; ++y) {
            for (int x = b0; x < b1; ++x) {
                uint8_t* p = px + (((static_cast<size_t>(y) * static_cast<size_t>(card_size)) + static_cast<size_t>(x)) * 4u);
                const bool edge = (x == b0 || y == b0 || x == b1 - 1 || y == b1 - 1);
                const uint8_t v = edge ? 140u : 255u;
                p[0] = v;
                p[1] = v;
                p[2] = v;
                p[3] = 255u;
            }
        }
    }
    GdiFlush();

    if (thumb) {
        BITMAP bm{};
        if (GetObjectW(thumb.get(), sizeof(bm), &bm) != 0 && bm.bmWidth > 0 && bm.bmHeight > 0) {
            const float scale = std::min(static_cast<float>(inner_w) / static_cast<float>(bm.bmWidth),
                                         static_cast<float>(inner_h) / static_cast<float>(bm.bmHeight));
            const int dw = std::max(1, static_cast<int>(std::lround(static_cast<float>(bm.bmWidth) * scale)));
            const int dh = std::max(1, static_cast<int>(std::lround(static_cast<float>(bm.bmHeight) * scale)));
            const int dx = pad_l + ((inner_w - dw) / 2);
            const int dy = pad_t + ((inner_h - dh) / 2);
            composite_hbitmap_into(px, card_size, card_size, thumb.get(), dx, dy, dw, dh);
        }
    }

    if (paths.size() > 1u && theme) {
        const auto label = usylibpp::strings::to_fixed_wstr<32>(paths.size());
        LOGFONTW lf{};
        lf.lfHeight = -MulDiv(9, dpi_i, 72);
        lf.lfWeight = FW_SEMIBOLD;
        constexpr std::wstring_view FONT_FACE = L"Segoe UI";
        std::memcpy(lf.lfFaceName, FONT_FACE.data(), (FONT_FACE.size() + 1u) * sizeof(wchar_t));

        const wil::unique_hfont font{CreateFontIndirectW(&lf)};
        const HGDIOBJ old_font = font ? SelectObject(dc.get(), font.get()) : nullptr;
        const auto restore_font = wil::scope_exit([&] {
            if (old_font) SelectObject(dc.get(), old_font);
        });

        RECT txt_rc{};
        DrawTextW(dc.get(), label.c_str(), static_cast<int>(label.size()), &txt_rc, DT_CALCRECT | DT_SINGLELINE);
        const int pad_x = MulDiv(6, dpi_i, 96);
        const int pad_y = MulDiv(2, dpi_i, 96);
        const int bw = std::max(MulDiv(18, dpi_i, 96), static_cast<int>(txt_rc.right - txt_rc.left) + (pad_x * 2));
        const int bh = std::max(MulDiv(17, dpi_i, 96), static_cast<int>(txt_rc.bottom - txt_rc.top) + (pad_y * 2));
        const int bx = pad_l + ((inner_w - bw) / 2);
        const int by = pad_t + inner_h - bh - MulDiv(2, dpi_i, 96);
        RECT badge_rc{.left = bx, .top = by, .right = bx + bw, .bottom = by + bh};
        (void)DrawThemeBackground(theme.get(), dc.get(), DD_TEXTBG, 1, &badge_rc, nullptr);

        DTTOPTS opts{};
        opts.dwSize = sizeof(opts);
        opts.dwFlags = DTT_TEXTCOLOR | DTT_COMPOSITED;
        opts.crText = RGB(255, 255, 255);
        (void)DrawThemeTextEx(theme.get(), dc.get(), DD_TEXTBG, 1, label.c_str(), static_cast<int>(label.size()),
                              DT_CENTER | DT_VCENTER | DT_SINGLELINE, &badge_rc, &opts);
        GdiFlush();
    }

    // IDragSourceHelper::InitializeFromBitmap expects straight (unpremultiplied) alpha and performs
    // premultiplication itself; unpremultiply here so theme borders and drop shadows aren't darkened.
    for (size_t i = 0; i < card_pixels; ++i) {
        uint8_t* p = px + (i * 4u);
        const unsigned int a = p[3];
        if (a > 0u && a < 255u) {
            p[0] = static_cast<uint8_t>(std::min(255u, ((static_cast<unsigned int>(p[0]) * 255u) + (a / 2u)) / a));
            p[1] = static_cast<uint8_t>(std::min(255u, ((static_cast<unsigned int>(p[1]) * 255u) + (a / 2u)) / a));
            p[2] = static_cast<uint8_t>(std::min(255u, ((static_cast<unsigned int>(p[2]) * 255u) + (a / 2u)) / a));
        }
    }

    out_size = {.cx = card_size, .cy = card_size};
    out_offset = {.x = card_size / 2, .y = card_size - MulDiv(13, dpi_i, 96)};
    return card;
}

inline bool attach_drag_image(HWND hwnd, IDataObject* obj, std::span<const std::wstring> paths) {
    if (!obj || paths.empty()) return false;
    wil::com_ptr<IDragSourceHelper2> dsh2;
    if (FAILED(CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dsh2))) || !dsh2) {
        return false;
    }

    // Must be set before InitializeFromBitmap so drop targets (Explorer / Desktop) can render
    // full-width drop description pills ("Copy to Desktop") without clipping.
    (void)dsh2->SetFlags(DSH_ALLOWDROPDESCRIPTIONTEXT);

    SIZE sz{};
    POINT offset{};
    wil::unique_hbitmap bmp = build_drag_image_bitmap(hwnd, paths, sz, offset);
    if (!bmp) return false;

    SHDRAGIMAGE sdi{};
    sdi.sizeDragImage = sz;
    sdi.ptOffset = offset;
    sdi.hbmpDragImage = bmp.get();
    sdi.crColorKey = CLR_NONE;
    if (SUCCEEDED(dsh2->InitializeFromBitmap(&sdi, obj))) {
        (void)bmp.release(); // Ownership transferred to IDragSourceHelper on success.
        return true;
    }
    return false;
}

} // namespace detail

/// Hands `n` bytes to `obj` as an HGLOBAL of format `cf` (the data object takes ownership on success).
inline bool set_data_bytes(IDataObject* obj, CLIPFORMAT cf, const void* data, size_t n) noexcept {
    if (!obj || !data || n == 0u) return false;
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n);
    if (!h) return false;
    void* p = GlobalLock(h);
    if (!p) {
        GlobalFree(h);
        return false;
    }
    std::memcpy(p, data, n);
    GlobalUnlock(h);
    FORMATETC fe{.cfFormat = cf, .ptd = nullptr, .dwAspect = DVASPECT_CONTENT, .lindex = -1, .tymed = TYMED_HGLOBAL};
    STGMEDIUM sm{};
    sm.tymed = TYMED_HGLOBAL;
    sm.hGlobal = h;
    if (SUCCEEDED(obj->SetData(&fe, &sm, TRUE))) return true;
    GlobalFree(h);
    return false;
}

/// Builds a shell data object for files (Shell IDList Array + CF_HDROP + preferred copy effect),
/// suitable for both OLE drag-and-drop and clipboard copy.
inline wil::com_ptr<IDataObject> media_data_object(std::span<const std::wstring> paths,
                                                   std::vector<std::wstring>* out_found = nullptr) {
    std::vector<PIDLIST_ABSOLUTE> pidls;
    std::vector<std::wstring> found;
    pidls.reserve(paths.size());
    found.reserve(paths.size());
    for (const auto& p : paths) {
        if (PIDLIST_ABSOLUTE id = ILCreateFromPathW(p.c_str())) {
            pidls.push_back(id);
            found.push_back(p);
        }
    }
    const auto free_pidls = wil::scope_exit([&] {
        for (auto* id : pidls) ILFree(id);
    });
    if (pidls.empty()) return nullptr;

    PIDLIST_ABSOLUTE parent = ILCloneFull(pidls.front());
    if (!parent) return nullptr;
    const auto free_parent = wil::scope_exit([&] { ILFree(parent); });
    ILRemoveLastID(parent);
    bool same_parent = true;
    for (const auto* id : pidls) {
        PIDLIST_ABSOLUTE pp = ILCloneFull(id);
        if (!pp) return nullptr;
        ILRemoveLastID(pp);
        const bool eq = ILIsEqual(pp, parent) != FALSE;
        ILFree(pp);
        if (!eq) {
            same_parent = false;
            break;
        }
    }
    std::vector<PCUITEMID_CHILD> children;
    children.reserve(pidls.size());
    for (const auto* id : pidls) {
        children.push_back(same_parent ? ILFindLastID(id) : reinterpret_cast<PCUITEMID_CHILD>(id));
    }

    wil::com_ptr<IDataObject> obj;
    if (FAILED(SHCreateDataObject(same_parent ? parent : nullptr, static_cast<UINT>(children.size()),
                                  children.data(), nullptr, IID_PPV_ARGS(&obj)))) {
        return nullptr;
    }

    std::vector<uint8_t> buf(sizeof(DROPFILES));
    for (const auto& p : found) {
        const auto* b = reinterpret_cast<const uint8_t*>(p.c_str());
        buf.insert(buf.end(), b, b + ((p.size() + 1u) * sizeof(wchar_t)));
    }
    buf.insert(buf.end(), sizeof(wchar_t), 0u);
    DROPFILES df{};
    df.pFiles = sizeof(DROPFILES);
    df.fWide = TRUE;
    std::memcpy(buf.data(), &df, sizeof(df));
    set_data_bytes(obj.get(), CF_HDROP, buf.data(), buf.size());

    const DWORD effect = DROPEFFECT_COPY;
    set_data_bytes(obj.get(), static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT)),
                   &effect, sizeof(effect));
    if (out_found) *out_found = std::move(found);
    return obj;
}

/// Wraps a shell data object to properly advertise and handle IDataObjectAsyncCapability (IAsyncOperation).
/// SHCreateDataObject's built-in implementation ignores SetAsyncMode and reports FALSE for local files,
/// which causes Explorer to run the drop and conflict dialogs synchronously inside IDropTarget::Drop.
class AsyncDataObject final : public IDataObject, public IDataObjectAsyncCapability {
public:
    explicit AsyncDataObject(wil::com_ptr<IDataObject> inner) noexcept : inner_(std::move(inner)) {}
    AsyncDataObject(const AsyncDataObject&) = delete;
    AsyncDataObject& operator=(const AsyncDataObject&) = delete;
    AsyncDataObject(AsyncDataObject&&) = delete;
    AsyncDataObject& operator=(AsyncDataObject&&) = delete;

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) noexcept override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDataObject)) {
            *ppv = static_cast<IDataObject*>(this);
        } else if (riid == __uuidof(IDataObjectAsyncCapability)) {
            *ppv = static_cast<IDataObjectAsyncCapability*>(this);
        }
        if (*ppv) {
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() noexcept override {
        return ++ref_count_;
    }

    IFACEMETHODIMP_(ULONG) Release() noexcept override {
        const ULONG count = --ref_count_;
        if (count == 0u) delete this;
        return count;
    }

    IFACEMETHODIMP SetAsyncMode(BOOL fDoOpAsync) noexcept override {
        async_mode_ = fDoOpAsync ? VARIANT_TRUE : VARIANT_FALSE;
        return S_OK;
    }

    IFACEMETHODIMP GetAsyncMode(BOOL* pfIsOpAsync) noexcept override {
        if (!pfIsOpAsync) return E_POINTER;
        *pfIsOpAsync = async_mode_;
        return S_OK;
    }

    IFACEMETHODIMP StartOperation(IBindCtx* /*pbcReserved*/) noexcept override {
        in_operation_ = VARIANT_TRUE;
        return S_OK;
    }

    IFACEMETHODIMP InOperation(BOOL* pfInAsyncOp) noexcept override {
        if (!pfInAsyncOp) return E_POINTER;
        *pfInAsyncOp = in_operation_;
        return S_OK;
    }

    IFACEMETHODIMP EndOperation(HRESULT /*hResult*/, IBindCtx* /*pbcReserved*/, DWORD /*dwEffects*/) noexcept override {
        in_operation_ = VARIANT_FALSE;
        return S_OK;
    }

    /// True while the target is still running the async drop and holds at least one external COM reference.
    [[nodiscard]] bool is_operation_active() const noexcept {
        return in_operation_.load(std::memory_order_acquire) != VARIANT_FALSE &&
               ref_count_.load(std::memory_order_acquire) > 1u;
    }

    IFACEMETHODIMP GetData(FORMATETC* f, STGMEDIUM* m) noexcept override {
        return inner_ ? inner_->GetData(f, m) : E_UNEXPECTED;
    }
    IFACEMETHODIMP GetDataHere(FORMATETC* f, STGMEDIUM* m) noexcept override {
        return inner_ ? inner_->GetDataHere(f, m) : E_UNEXPECTED;
    }
    IFACEMETHODIMP QueryGetData(FORMATETC* f) noexcept override {
        return inner_ ? inner_->QueryGetData(f) : E_UNEXPECTED;
    }
    IFACEMETHODIMP GetCanonicalFormatEtc(FORMATETC* a, FORMATETC* b) noexcept override {
        return inner_ ? inner_->GetCanonicalFormatEtc(a, b) : E_UNEXPECTED;
    }
    IFACEMETHODIMP SetData(FORMATETC* f, STGMEDIUM* m, BOOL r) noexcept override {
        return inner_ ? inner_->SetData(f, m, r) : E_UNEXPECTED;
    }
    IFACEMETHODIMP EnumFormatEtc(DWORD d, IEnumFORMATETC** e) noexcept override {
        return inner_ ? inner_->EnumFormatEtc(d, e) : E_UNEXPECTED;
    }
    IFACEMETHODIMP DAdvise(FORMATETC* f, DWORD a, IAdviseSink* s, DWORD* c) noexcept override {
        return inner_ ? inner_->DAdvise(f, a, s, c) : E_UNEXPECTED;
    }
    IFACEMETHODIMP DUnadvise(DWORD c) noexcept override {
        return inner_ ? inner_->DUnadvise(c) : E_UNEXPECTED;
    }
    IFACEMETHODIMP EnumDAdvise(IEnumSTATDATA** e) noexcept override {
        return inner_ ? inner_->EnumDAdvise(e) : E_UNEXPECTED;
    }

private:
    ~AsyncDataObject() = default;

    wil::com_ptr<IDataObject> inner_;
    std::atomic<ULONG> ref_count_{1};
    std::atomic<BOOL> async_mode_{VARIANT_TRUE};
    std::atomic<BOOL> in_operation_{VARIANT_FALSE};
};

/// Drags files out to other applications (copy or link only).
/// Explicitly initializes the drag image via IDragSourceHelper2 (with DSH_ALLOWDROPDESCRIPTIONTEXT)
/// before calling SHDoDragDrop(nullptr, ...), avoiding Windows Shell's async default-drag-image
/// recomputation (`UsingDefaultDragImage = 1` / `ComputedDragImage`) and DI_GETDRAGIMAGE probing
/// on WS_EX_NOREDIRECTIONBITMAP windows.
inline wil::com_ptr<AsyncDataObject> drag_files(HWND hwnd, std::span<const std::wstring> paths,
                                                HRESULT* out_hr = nullptr) {
    if (out_hr) *out_hr = S_OK;
    std::vector<std::wstring> found;
    auto inner = media_data_object(paths, &found);
    if (!inner) {
        if (out_hr) *out_hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        return nullptr;
    }
    wil::com_ptr<AsyncDataObject> obj;
    obj.attach(new (std::nothrow) AsyncDataObject(std::move(inner)));
    if (!obj) {
        if (out_hr) *out_hr = E_OUTOFMEMORY;
        return nullptr;
    }

    (void)detail::attach_drag_image(hwnd, static_cast<IDataObject*>(obj.get()), found);

    DWORD effect = DROPEFFECT_NONE;
    const HRESULT hr = SHDoDragDrop(nullptr, static_cast<IDataObject*>(obj.get()), nullptr,
                                    DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
    if (out_hr) *out_hr = hr;
    if (obj->is_operation_active()) return obj;
    return nullptr;
}

} // namespace usylibpp::ui
