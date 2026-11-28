#pragma once

#include <windows.h>
#include <objidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace usylibpp::ui {

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
inline wil::com_ptr<IDataObject> media_data_object(std::span<const std::wstring> paths) {
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

    // Shell ID lists relative to the shared folder (Explorer's own shape), or absolute under the desktop
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

    // Plain file list too: what most non-shell drop targets read, and what survives a clipboard flush
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
    /// If the target crashes or releases its proxy without calling EndOperation, ref_count_ drops to 1
    /// (only our own tracking handle remains), preventing leaked objects and stuck UI state.
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

/// Drags files out to other applications (copy or link only: a drop never moves the user's files away).
/// The target is asked to finish the drop on its own thread, so its dialogs (such as "file already exists")
/// never block this one inside the drag loop. Returns the data object when the target is still working on
/// the drop afterwards, null otherwise.
inline wil::com_ptr<AsyncDataObject> drag_files(HWND hwnd, std::span<const std::wstring> paths,
                                                HRESULT* out_hr = nullptr) {
    if (out_hr) *out_hr = S_OK;
    auto inner = media_data_object(paths);
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
    DWORD effect = DROPEFFECT_NONE;
    const HRESULT hr = SHDoDragDrop(hwnd, static_cast<IDataObject*>(obj.get()), nullptr,
                                    DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
    if (out_hr) *out_hr = hr;
    if (obj->is_operation_active()) return obj;
    return nullptr;
}

} // namespace usylibpp::ui
