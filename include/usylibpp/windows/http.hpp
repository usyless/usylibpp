#pragma once

#include <usylibpp/strings.hpp>

#include <windows.h>
#include <winhttp.h>
#include <wil/resource.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace usylibpp::windows::http {

struct Result {
    bool ok = false;
    bool cancelled = false;
    DWORD status = 0;   // HTTP status when the server answered
    std::wstring error; // empty on success
};

/// `total` is 0 when the server did not announce a length. Called at most ~10 times a second.
using Progress = std::function<void(uint64_t done, uint64_t total)>;

namespace detail {

/// Single-owner RAII wrapper for a WinHTTP handle (never closed cross-thread).
class Handle {
public:
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&&) = delete;
    Handle& operator=(Handle&&) = delete;
    ~Handle() { close(); }

    void reset(HINTERNET h = nullptr) noexcept {
        close();
        h_ = h;
    }
    void close() noexcept {
        if (h_) {
            WinHttpCloseHandle(h_);
            h_ = nullptr;
        }
    }
    [[nodiscard]] HINTERNET get() const noexcept { return h_; }
    explicit operator bool() const noexcept { return h_ != nullptr; }

private:
    HINTERNET h_ = nullptr;
};

struct AsyncState {
    wil::unique_event_nothrow op_done{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    wil::unique_event_nothrow req_closed{CreateEventW(nullptr, TRUE, TRUE, nullptr)};
    std::atomic<DWORD> status{0};
    std::atomic<DWORD> error{0};
    std::atomic<DWORD> bytes{0};
};

inline void CALLBACK winhttp_callback(HINTERNET, DWORD_PTR ctx, DWORD code, LPVOID info, DWORD len) noexcept {
    auto* s = reinterpret_cast<AsyncState*>(ctx);
    if (!s) return;
    switch (code) {
        case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
        case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
            s->bytes.store(0, std::memory_order_relaxed);
            s->error.store(0, std::memory_order_relaxed);
            s->status.store(code, std::memory_order_release);
            SetEvent(s->op_done.get());
            break;
        case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
            s->bytes.store(len, std::memory_order_relaxed);
            s->error.store(0, std::memory_order_relaxed);
            s->status.store(code, std::memory_order_release);
            SetEvent(s->op_done.get());
            break;
        case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
            const auto* ar = static_cast<const WINHTTP_ASYNC_RESULT*>(info);
            s->error.store(ar ? ar->dwError : ERROR_WINHTTP_INTERNAL_ERROR, std::memory_order_relaxed);
            s->status.store(code, std::memory_order_release);
            SetEvent(s->op_done.get());
            break;
        }
        case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
            SetEvent(s->req_closed.get());
            break;
        default:
            break;
    }
}

struct Response {
    AsyncState state;
    Handle session, connection, request;
    DWORD status = 0;
    uint64_t content_length = 0; // 0 = unknown

    Response() = default;
    ~Response() { close(); }
    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;
    Response(Response&&) = delete;
    Response& operator=(Response&&) = delete;

    void close() noexcept {
        if (request) {
            request.close();
            if (state.req_closed) WaitForSingleObject(state.req_closed.get(), 5000);
        }
        connection.close();
        session.close();
        status = 0;
        content_length = 0;
    }
};

inline std::wstring win_error(const wchar_t* what, DWORD err = GetLastError()) {
    return usylibpp::strings::concat_strings(what, L" (WinHTTP error ", usylibpp::strings::to_fixed_wstr(err), L")");
}

enum class OpStatus : uint8_t { Ok, Error, Cancelled };

inline OpStatus wait_op(Response& r, HANDLE cancel_ev, DWORD& err) noexcept {
    std::array<HANDLE, 2> hs{r.state.op_done.get(), cancel_ev};
    const DWORD n = cancel_ev ? 2u : 1u;
    const DWORD w = WaitForMultipleObjects(n, hs.data(), FALSE, INFINITE);
    if (w == WAIT_OBJECT_0 + 1) {
        r.close();
        return OpStatus::Cancelled;
    }
    if (w != WAIT_OBJECT_0) {
        err = GetLastError();
        return OpStatus::Error;
    }
    if (r.state.status.load(std::memory_order_acquire) == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
        err = r.state.error.load(std::memory_order_relaxed);
        return OpStatus::Error;
    }
    return OpStatus::Ok;
}

/// Sends a GET and waits for the response headers. Redirects are followed by WinHTTP.
inline bool open(const std::wstring& url, uint64_t range_from, Response& r, std::wstring& error,
                 HANDLE cancel_ev = nullptr, const wchar_t* user_agent = L"usylibpp") {
    r.close();
    if (!r.state.op_done || !r.state.req_closed) {
        error = L"Could not initialize WinHTTP events";
        return false;
    }
    if (cancel_ev && WaitForSingleObject(cancel_ev, 0) == WAIT_OBJECT_0) {
        error = L"Cancelled";
        return false;
    }

    std::array<wchar_t, 256> host; // NOLINT(cppcoreguidelines-pro-type-member-init)
    host[0] = L'\0';
    std::wstring path(url.size() + 1, L'\0');
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host.data();
    uc.dwHostNameLength = static_cast<DWORD>(host.size());
    uc.lpszUrlPath = path.data();
    uc.dwUrlPathLength = static_cast<DWORD>(path.size());
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &uc)) {
        error = L"Invalid URL";
        return false;
    }

    const wchar_t* ua = (user_agent && user_agent[0] != L'\0') ? user_agent : L"usylibpp";
    r.session.reset(WinHttpOpen(ua, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC));
    if (!r.session) {
        error = win_error(L"Could not start WinHTTP");
        return false;
    }
    WinHttpSetTimeouts(r.session.get(), 15'000, 15'000, 30'000, 30'000);
    r.connection.reset(WinHttpConnect(r.session.get(), host.data(), uc.nPort, 0));
    if (!r.connection) {
        error = win_error(L"Could not connect");
        return false;
    }
    r.request.reset(WinHttpOpenRequest(r.connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
    if (!r.request) {
        error = win_error(L"Could not create request");
        return false;
    }
    auto ctx = reinterpret_cast<DWORD_PTR>(&r.state);
    if (!WinHttpSetOption(r.request.get(), WINHTTP_OPTION_CONTEXT_VALUE, &ctx, sizeof(ctx)) ||
        WinHttpSetStatusCallback(r.request.get(), &winhttp_callback,
                                 WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES, 0) == WINHTTP_INVALID_STATUS_CALLBACK) {
        error = win_error(L"Could not configure WinHTTP callbacks");
        return false;
    }
    ResetEvent(r.state.req_closed.get());

    if (range_from > 0) {
        usylibpp::strings::FixedWString<64> range{L"Range: bytes="};
        range.append(usylibpp::strings::to_fixed_wstr(range_from));
        range.push_back(L'-');
        WinHttpAddRequestHeaders(r.request.get(), range.c_str(), static_cast<DWORD>(range.size()), WINHTTP_ADDREQ_FLAG_ADD);
    }

    const auto fail_async = [&](const wchar_t* what, DWORD err) {
        r.close();
        error = win_error(what, err);
        return false;
    };

    ResetEvent(r.state.op_done.get());
    r.state.status.store(0, std::memory_order_relaxed);
    if (!WinHttpSendRequest(r.request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, ctx)) {
        const DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) return fail_async(L"Could not reach server", err);
    }
    DWORD op_err = 0;
    if (const OpStatus st = wait_op(r, cancel_ev, op_err); st != OpStatus::Ok) {
        error = st == OpStatus::Cancelled ? L"Cancelled" : win_error(L"Could not reach server", op_err);
        return false;
    }

    ResetEvent(r.state.op_done.get());
    r.state.status.store(0, std::memory_order_relaxed);
    if (!WinHttpReceiveResponse(r.request.get(), nullptr)) {
        const DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) return fail_async(L"Could not reach server", err);
    }
    if (const OpStatus st = wait_op(r, cancel_ev, op_err); st != OpStatus::Ok) {
        error = st == OpStatus::Cancelled ? L"Cancelled" : win_error(L"Could not reach server", op_err);
        return false;
    }

    DWORD size = sizeof(r.status);
    WinHttpQueryHeaders(r.request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &r.status, &size, WINHTTP_NO_HEADER_INDEX);
    std::array<wchar_t, 32> len; // NOLINT(cppcoreguidelines-pro-type-member-init)
    len[0] = L'\0';
    size = static_cast<DWORD>(len.size() * sizeof(wchar_t));
    if (WinHttpQueryHeaders(r.request.get(), WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, len.data(), &size, WINHTTP_NO_HEADER_INDEX)) {
        r.content_length = _wcstoui64(len.data(), nullptr, 10);
    }
    return true;
}

inline bool read_chunk(Response& r, void* buf, DWORD cap, DWORD& got, std::wstring& error, HANDLE cancel_ev = nullptr) {
    got = 0;
    if (!r.state.op_done) {
        error = L"Invalid WinHTTP event state";
        return false;
    }
    ResetEvent(r.state.op_done.get());
    r.state.status.store(0, std::memory_order_relaxed);
    if (!WinHttpReadData(r.request.get(), buf, cap, nullptr)) {
        const DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            error = win_error(L"Connection lost", err);
            return false;
        }
    }
    DWORD op_err = 0;
    const OpStatus st = wait_op(r, cancel_ev, op_err);
    if (st != OpStatus::Ok) {
        error = st == OpStatus::Cancelled ? L"Cancelled" : win_error(L"Connection lost", op_err);
        return false;
    }
    got = r.state.bytes.load(std::memory_order_relaxed);
    return true;
}

inline std::wstring status_text(DWORD status) {
    switch (status) {
        case 401:
        case 403: return usylibpp::strings::concat_strings(L"Access denied (HTTP ", usylibpp::strings::to_fixed_wstr(status), L") — the file may be gated or private");
        case 404: return L"Not found on the server (HTTP 404)";
        default: return usylibpp::strings::concat_strings(L"Server answered HTTP ", usylibpp::strings::to_fixed_wstr(status));
    }
}

} // namespace detail

/// Small GET into memory (API responses). Fails on anything but HTTP 200 or when the body exceeds `max_bytes`.
inline std::optional<std::string> get(const std::wstring& url, size_t max_bytes = 16ull << 20u,
                                      const std::stop_token& stop = {}, const wchar_t* user_agent = L"usylibpp") {
    wil::unique_event_nothrow cancel_ev;
    if (stop.stop_possible()) {
        cancel_ev.reset(CreateEventW(nullptr, TRUE, stop.stop_requested() ? TRUE : FALSE, nullptr));
    }
    const std::stop_callback abort{stop, [&cancel_ev] {
        if (cancel_ev) SetEvent(cancel_ev.get());
    }};
    detail::Response r;
    std::wstring error;
    if (!detail::open(url, 0, r, error, cancel_ev.get(), user_agent) || r.status != 200) return std::nullopt;
    std::string out;
    if (r.content_length > 0 && r.content_length <= max_bytes) {
        out.reserve(static_cast<size_t>(r.content_length));
    }
    constexpr DWORD CHUNK_CAP = 64u << 10u;
    const auto buf = std::make_unique_for_overwrite<char[]>(CHUNK_CAP); // NOLINT(modernize-avoid-c-arrays)
    for (;;) {
        DWORD got = 0;
        if (!detail::read_chunk(r, buf.get(), CHUNK_CAP, got, error, cancel_ev.get())) return std::nullopt;
        if (got == 0) break;
        out.append(buf.get(), got);
        if (out.size() > max_bytes) return std::nullopt;
    }
    return out;
}

/// Streams `url` into `dest`. Data lands in `dest` + ".part" first and is renamed once complete, so `dest`
/// only ever exists whole; an interrupted or cancelled download leaves the part file and resumes from it next time.
inline Result download(const std::wstring& url, const std::filesystem::path& dest, const Progress& progress = {},
                       const std::stop_token& stop = {}, const wchar_t* user_agent = L"usylibpp") {
    Result res;
    const std::filesystem::path part = usylibpp::strings::concat_strings(dest.wstring(), L".part");
    std::error_code ec;
    uint64_t have = std::filesystem::exists(part, ec) ? std::filesystem::file_size(part, ec) : 0;
    if (ec) have = 0;

    wil::unique_event_nothrow cancel_ev{CreateEventW(nullptr, TRUE, stop.stop_requested() ? TRUE : FALSE, nullptr)};
    const std::stop_callback abort{stop, [&cancel_ev] {
        if (cancel_ev) SetEvent(cancel_ev.get());
    }};
    detail::Response r;
    const auto fail = [&](std::wstring why) {
        r.close();
        res.cancelled = stop.stop_requested();
        res.error = res.cancelled ? L"Cancelled" : std::move(why);
        return res;
    };

    if (!detail::open(url, have, r, res.error, cancel_ev.get(), user_agent)) return fail(res.error);
    if (have > 0 && r.status == 416) {
        // The part file is not a prefix of what the server has now: start over
        std::filesystem::remove(part, ec);
        have = 0;
        if (!detail::open(url, 0, r, res.error, cancel_ev.get(), user_agent)) return fail(res.error);
    }
    res.status = r.status;
    if (r.status == 200) {
        have = 0; // the server ignored the range
    } else if (r.status != 206) {
        return fail(detail::status_text(r.status));
    }
    const uint64_t total = r.content_length > 0 ? have + r.content_length : 0;

    wil::unique_hfile file{CreateFileW(part.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, have > 0 ? OPEN_EXISTING : CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (!file) return fail(usylibpp::strings::concat_strings(L"Could not write ", part.filename().wstring()));
    if (have > 0) {
        LARGE_INTEGER pos{};
        pos.QuadPart = static_cast<LONGLONG>(have);
        if (!SetFilePointerEx(file.get(), pos, nullptr, FILE_BEGIN)) {
            return fail(usylibpp::strings::concat_strings(L"Could not resume ", part.filename().wstring()));
        }
    }

    uint64_t done = have;
    constexpr DWORD CHUNK_CAP = 256u << 10u;
    const auto buf = std::make_unique_for_overwrite<char[]>(CHUNK_CAP); // NOLINT(modernize-avoid-c-arrays)
    auto last_report = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    for (;;) {
        DWORD got = 0;
        if (!detail::read_chunk(r, buf.get(), CHUNK_CAP, got, res.error, cancel_ev.get())) {
            return fail(res.error);
        }
        if (got == 0) break;
        DWORD written = 0;
        if (!WriteFile(file.get(), buf.get(), got, &written, nullptr) || written != got) {
            return fail(usylibpp::strings::concat_strings(L"Could not write ", part.filename().wstring(), L" (disk full?)"));
        }
        done += got;
        if (const auto now = std::chrono::steady_clock::now(); progress && now - last_report >= std::chrono::milliseconds(100)) {
            last_report = now;
            progress(done, total);
        }
    }
    if (stop.stop_requested()) return fail({});
    if (total > 0 && done != total) return fail(L"Connection closed early — try again to resume");
    if (progress) progress(done, total);

    file.reset();
    if (!MoveFileExW(part.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        return fail(usylibpp::strings::concat_strings(L"Could not create ", dest.filename().wstring()));
    }
    res.ok = true;
    return res;
}

} // namespace usylibpp::windows::http
