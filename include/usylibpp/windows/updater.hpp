#pragma once

#include <usylibpp/glaze.hpp>
#include <usylibpp/strings.hpp>
#include <usylibpp/util/update_logic.hpp>
#include <usylibpp/windows/http.hpp>
#include <usylibpp/windows/process.hpp>

#include <bcrypt.h>
#include <shellapi.h>
#include <wil/resource.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>

namespace usylibpp::windows::updater {

struct Config {
    const wchar_t* user_agent = L"usylibpp";
    std::wstring_view latest_release_url;
    std::wstring_view releases_page_url;
    std::wstring_view current_version;
    std::string_view preferred_7z_asset;
    std::string_view preferred_zip_asset;
    std::wstring_view primary_exe;
    std::span<const std::wstring_view> required_bins;
    std::wstring_view missing_bins_error;
    std::wstring_view write_probe_prefix = L".usylibpp-write-probe-";
    std::filesystem::path work_dir;
};

namespace detail {

inline int64_t unix_now() noexcept {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

inline std::string win32_error_utf8(DWORD err) {
    if (err == 0) return "Success";
    LPWSTR buf = nullptr;
    const DWORD n = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::string out;
    if (n > 0 && buf) {
        std::wstring_view sv{buf, n};
        while (!sv.empty() && (sv.back() == L'\r' || sv.back() == L'\n' || sv.back() == L' ')) sv.remove_suffix(1);
        out = usylibpp::strings::to_utf8(sv);
        LocalFree(buf);
    }
    if (out.empty()) {
        out = usylibpp::strings::concat_strings("Error ", usylibpp::strings::to_fixed_str(err));
    }
    return out;
}

} // namespace detail

inline const std::filesystem::path& system_tar_path() {
    static const std::filesystem::path p = [] {
        std::array<wchar_t, MAX_PATH> sys_dir; // NOLINT(cppcoreguidelines-pro-type-member-init)
        const UINT n = GetSystemDirectoryW(sys_dir.data(), static_cast<UINT>(sys_dir.size()));
        return std::filesystem::path{std::wstring_view{sys_dir.data(), n < sys_dir.size() ? n : 0}} / L"tar.exe";
    }();
    return p;
}

/// True when the system `tar.exe` (`bsdtar` / `libarchive`) supports extracting `.7z` archives (Windows 11 23H2+).
inline bool tar_supports_7z() {
    static const bool supported = [] {
        const std::filesystem::path& tar = system_tar_path();
        std::error_code ec;
        if (!std::filesystem::exists(tar, ec)) return false;
        const std::wstring cmd = usylibpp::strings::concat_strings(L"\"", tar.wstring(), L"\" --version");
        const auto res = usylibpp::windows::process::run_process<{.allow_visible_windows = false, .capture_stderr = false}>(
            usylibpp::windows::process::process_settings{
                .commandline = cmd,
            });
        if (res.status != 0) return false;
        return usylibpp::util::parse_tar_supports_7z(res.stdout_);
    }();
    return supported;
}

inline std::string sha256_file_hex(const std::filesystem::path& path, std::wstring& err) {
    const wil::unique_hfile file{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                             FILE_FLAG_SEQUENTIAL_SCAN, nullptr)};
    if (!file) {
        err = L"Could not open downloaded archive for verification.";
        return {};
    }

    BCRYPT_ALG_HANDLE alg = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        err = L"Could not initialize SHA-256 provider.";
        return {};
    }
    const auto close_alg = wil::scope_exit([&] { BCryptCloseAlgorithmProvider(alg, 0); });

    DWORD hash_obj_len = 0;
    DWORD cb = 0;
    if (!BCRYPT_SUCCESS(BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hash_obj_len), sizeof(hash_obj_len), &cb, 0))) {
        err = L"Could not query SHA-256 object size.";
        return {};
    }

    const auto hash_obj = std::make_unique_for_overwrite<uint8_t[]>(hash_obj_len); // NOLINT(modernize-avoid-c-arrays)
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (!BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, hash_obj.get(), hash_obj_len, nullptr, 0, 0))) {
        err = L"Could not create SHA-256 hash.";
        return {};
    }
    const auto close_hash = wil::scope_exit([&] { BCryptDestroyHash(hash); });

    constexpr DWORD BUF_CAP = 65536;
    const auto buf = std::make_unique_for_overwrite<uint8_t[]>(BUF_CAP); // NOLINT(modernize-avoid-c-arrays)
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(file.get(), buf.get(), BUF_CAP, &got, nullptr)) {
            err = L"Failed while reading archive for checksum verification.";
            return {};
        }
        if (got == 0) break;
        if (!BCRYPT_SUCCESS(BCryptHashData(hash, buf.get(), got, 0))) {
            err = L"SHA-256 hashing failed.";
            return {};
        }
    }

    std::array<uint8_t, 32> digest; // NOLINT(cppcoreguidelines-pro-type-member-init)
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0))) {
        err = L"Could not finalize SHA-256 hash.";
        return {};
    }

    static constexpr std::string_view hex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (const uint8_t b : digest) {
        out.push_back(hex[(static_cast<unsigned>(b) >> 4U) & 0x0FU]);
        out.push_back(hex[static_cast<unsigned>(b) & 0x0FU]);
    }
    return out;
}

/// Finds the folder containing all `required_bins` inside an extracted archive directory.
[[nodiscard]] inline std::optional<std::filesystem::path> resolve_staging_root(
    const std::filesystem::path& staging_dir,
    std::span<const std::wstring_view> required_bins) {
    std::error_code ec;
    const auto has_bins = [&](const std::filesystem::path& dir) {
        for (const std::wstring_view bin : required_bins) {
            if (!std::filesystem::is_regular_file(dir / bin, ec)) return false;
        }
        return !required_bins.empty();
    };
    if (has_bins(staging_dir)) return staging_dir;

    std::optional<std::filesystem::path> match;
    for (const auto& entry : std::filesystem::directory_iterator(staging_dir, ec)) {
        if (!entry.is_directory(ec)) continue;
        if (has_bins(entry.path())) {
            if (match) return std::nullopt; // ambiguous
            match = entry.path();
        }
    }
    return match;
}

inline bool is_dir_writable(const std::filesystem::path& dir, std::wstring_view probe_prefix = L".usylibpp-write-probe-") {
    const std::filesystem::path probe = dir / usylibpp::strings::concat_strings(probe_prefix, usylibpp::strings::to_fixed_wstr(GetCurrentProcessId()));
    const wil::unique_hfile f{CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                          FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr)};
    if (f) return true;
    std::error_code ec;
    std::filesystem::remove(probe, ec);
    return false;
}

#ifdef USYLIBPP_HAS_GLAZE
/// Checks GitHub's latest release endpoint, always recording `last_check_unix` regardless of success or failure.
inline bool check_latest_release(const Config& cfg, usylibpp::util::UpdateState& st, bool& is_newer,
                                 const std::function<void(const usylibpp::util::UpdateState&)>& save_cb = {},
                                 const std::stop_token& stop = {}) {
    is_newer = false;
    st.last_check_unix = detail::unix_now();

    const auto body = http::get(std::wstring{cfg.latest_release_url}, 16ull << 20u, stop, cfg.user_agent);
    if (stop.stop_requested()) return false;
    if (!body) {
        st.last_error = "Could not reach GitHub releases (network error or API rate limit).";
        if (save_cb) save_cb(st);
        return false;
    }

    constexpr glz::opts JSON_READ{.error_on_unknown_keys = false};
    usylibpp::util::GithubRelease rel;
    if (glz::read<JSON_READ>(rel, *body) || rel.tag_name.empty()) {
        st.last_error = "Unexpected response from GitHub releases API.";
        if (save_cb) save_cb(st);
        return false;
    }

    st.latest_tag = rel.tag_name;
    st.latest_version = usylibpp::util::normalize_version_string(rel.tag_name);
    st.release_url = rel.html_url.empty() ? usylibpp::strings::to_utf8(cfg.releases_page_url) : rel.html_url;
    st.release_notes = rel.body;
    st.last_error.clear();

    const std::string current = usylibpp::strings::to_utf8(cfg.current_version);
    is_newer = usylibpp::util::is_newer_version(st.latest_version, current);

    if (const auto asset = usylibpp::util::pick_release_asset(rel.assets, tar_supports_7z(), cfg.preferred_7z_asset, cfg.preferred_zip_asset)) {
        st.asset_name = asset->name;
        st.asset_url = asset->browser_download_url;
        st.asset_size = asset->size;
        st.asset_sha256 = usylibpp::util::extract_sha256_digest(asset->digest);
    } else {
        st.asset_name.clear();
        st.asset_url.clear();
        st.asset_size = 0;
        st.asset_sha256.clear();
        if (is_newer) {
            st.last_error = tar_supports_7z()
                ? "Release has no compatible .7z or .zip asset."
                : "Release has no compatible .zip asset for this Windows version.";
        }
    }

    if (save_cb) save_cb(st);
    return true;
}
#endif

inline bool download_and_stage_update(const Config& cfg, const usylibpp::util::UpdateState& st,
                                      std::filesystem::path& out_staging_root, std::wstring& err,
                                      const std::function<void(std::wstring_view)>& status_cb,
                                      const http::Progress& progress, const std::stop_token& stop) {
    if (st.asset_url.empty() || st.asset_name.empty()) {
        err = L"No compatible release archive URL available.";
        return false;
    }

    const std::filesystem::path& work_dir = cfg.work_dir;
    const std::filesystem::path archive = work_dir / usylibpp::strings::to_wide(st.asset_name);
    const std::filesystem::path staging = work_dir / L"staging";
    std::error_code ec;
    std::filesystem::create_directories(work_dir, ec);
    if (ec) {
        usylibpp::strings::overwrite_to(err, L"Could not create update folder: ", ec.message());
        return false;
    }

    if (status_cb) status_cb(usylibpp::strings::concat_wide(L"Downloading ", st.asset_name, L"\u2026"));
    const http::Result dl = http::download(usylibpp::strings::to_wide(st.asset_url), archive, progress, stop, cfg.user_agent);
    if (!dl.ok) {
        if (dl.cancelled) {
            err = L"Download cancelled — it can resume next time.";
        } else {
            usylibpp::strings::overwrite_to(err, L"Download failed: ", dl.error);
        }
        return false;
    }

    if (!st.asset_sha256.empty()) {
        if (status_cb) status_cb(L"Verifying checksum\u2026");
        std::wstring hash_err;
        const std::string got = sha256_file_hex(archive, hash_err);
        if (got.empty()) {
            err = hash_err;
            return false;
        }
        if (got != st.asset_sha256) {
            std::filesystem::remove(archive, ec);
            usylibpp::strings::overwrite_to(err, L"Downloaded file checksum mismatch (expected ", st.asset_sha256, L").");
            return false;
        }
    }

    if (status_cb) status_cb(L"Extracting update\u2026");
    std::filesystem::remove_all(staging, ec);
    std::filesystem::create_directories(staging, ec);
    if (ec) {
        usylibpp::strings::overwrite_to(err, L"Could not prepare staging directory: ", ec.message());
        return false;
    }

    const std::filesystem::path& tar = system_tar_path();
    const std::wstring tar_cmd = usylibpp::strings::concat_wide(L"\"", tar, L"\" -xf \"", archive, L"\" -C \"", staging, L"\"");
    const auto res = usylibpp::windows::process::run_process<{.allow_visible_windows = false, .capture_stdout = false}>(
        usylibpp::windows::process::process_settings{
            .commandline = tar_cmd,
        });
    if (res.status != 0) {
        std::filesystem::remove_all(staging, ec);
        std::string_view tar_err = res.stderr_;
        while (!tar_err.empty() && (tar_err.back() == '\r' || tar_err.back() == '\n' || tar_err.back() == ' ' || tar_err.back() == '\t')) {
            tar_err.remove_suffix(1);
        }
        usylibpp::strings::overwrite_to(
            err,
            L"Extraction failed (tar exited with code ", usylibpp::strings::to_fixed_wstr(res.status), L")",
            tar_err.empty() ? L"." : L": ", tar_err);
        return false;
    }

    const auto root = resolve_staging_root(staging, cfg.required_bins);
    if (!root) {
        std::filesystem::remove_all(staging, ec);
        err = cfg.missing_bins_error.empty() ? L"Archive is missing required binaries." : std::wstring{cfg.missing_bins_error};
        return false;
    }
    out_staging_root = *root;
    return true;
}

inline bool launch_staged_updater(const Config& cfg, const std::filesystem::path& staging_root,
                                  const std::filesystem::path& target_dir, DWORD wait_pid,
                                  bool open_about_after, bool& uac_cancelled, std::wstring& err) {
    uac_cancelled = false;
    const std::filesystem::path staged_exe = staging_root / cfg.primary_exe;
    std::wstring args = usylibpp::strings::concat_strings(
        L"--apply-update ", usylibpp::strings::to_fixed_wstr(wait_pid),
        L" \"", staging_root.wstring(), L"\" \"", target_dir.wstring(), L"\"");
    if (open_about_after) args += L" --open-about";

    if (is_dir_writable(target_dir, cfg.write_probe_prefix)) {
        std::wstring cmd = usylibpp::strings::concat_strings(L"\"", staged_exe.wstring(), L"\" ", args);
        STARTUPINFOW si{.cb = sizeof(STARTUPINFOW)};
        wil::unique_process_information pi;
        if (CreateProcessW(staged_exe.c_str(), cmd.data(), nullptr, nullptr, FALSE,
                           CREATE_UNICODE_ENVIRONMENT | CREATE_BREAKAWAY_FROM_JOB, nullptr,
                           staging_root.c_str(), &si, &pi)) {
            return true;
        }
        const DWORD e = GetLastError();
        BOOL in_job = FALSE;
        // If our parent job forbids breakaway (e.g. an older launcher), fall through to ShellExecuteEx("runas") to escape the job.
        if (e != ERROR_ACCESS_DENIED || !IsProcessInJob(GetCurrentProcess(), nullptr, &in_job) || !in_job) {
            err = usylibpp::strings::to_wide(detail::win32_error_utf8(e));
            return false;
        }
    }

    SHELLEXECUTEINFOW sei{
        .cbSize = sizeof(SHELLEXECUTEINFOW),
        .fMask = SEE_MASK_NOASYNC,
        .lpVerb = L"runas",
        .lpFile = staged_exe.c_str(),
        .lpParameters = args.c_str(),
        .lpDirectory = staging_root.c_str(),
        .nShow = SW_SHOWNORMAL,
    };
    if (!ShellExecuteExW(&sei)) {
        const DWORD e = GetLastError();
        if (e == ERROR_CANCELLED) {
            uac_cancelled = true;
            err = L"Administrator permission (UAC) was cancelled.";
        } else {
            err = usylibpp::strings::to_wide(detail::win32_error_utf8(e));
        }
        return false;
    }
    return true;
}

} // namespace usylibpp::windows::updater
