#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace usylibpp::util {

/// Hardcoded 24-hour cooldown between automatic update checks (whether the last check succeeded or failed).
inline constexpr int64_t UPDATE_CHECK_INTERVAL_S = 24LL * 60 * 60;

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool prerelease = false;
    bool valid = false;
};

/// Parses versions like "v0.2.0", "0.3.0", "0.3", or "v1.0.0-rc1".
[[nodiscard]] inline SemVer parse_semver(std::string_view s) noexcept {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    if (!s.empty() && (s.front() == 'v' || s.front() == 'V')) s.remove_prefix(1);

    const size_t plus = s.find('+');
    if (plus != std::string_view::npos) s = s.substr(0, plus);
    const size_t dash = s.find('-');
    const bool pre = dash != std::string_view::npos;
    if (pre) s = s.substr(0, dash);

    if (s.empty()) return {};
    std::array<int, 3> parts{0, 0, 0};
    int count = 0;
    size_t pos = 0;
    while (pos < s.size() && count < 3) {
        const size_t dot = s.find('.', pos);
        const std::string_view seg = dot == std::string_view::npos ? s.substr(pos) : s.substr(pos, dot - pos);
        if (seg.empty()) return {};
        int val = 0;
        for (const char c : seg) {
            if (c < '0' || c > '9') return {};
            if (val > 100'000'000) return {};
            val = (val * 10) + (c - '0');
        }
        parts[static_cast<size_t>(count++)] = val;
        if (dot == std::string_view::npos) {
            pos = s.size();
            break;
        }
        pos = dot + 1;
        if (pos == s.size()) return {};
    }
    if (pos < s.size() || count < 2) return {};
    return SemVer{.major = parts[0], .minor = parts[1], .patch = parts[2], .prerelease = pre, .valid = true};
}

/// True when `candidate` is strictly newer than `current`.
[[nodiscard]] inline bool is_newer_version(std::string_view candidate, std::string_view current) noexcept {
    const SemVer a = parse_semver(candidate);
    const SemVer b = parse_semver(current);
    if (!a.valid || !b.valid) return false;
    if (a.major != b.major) return a.major > b.major;
    if (a.minor != b.minor) return a.minor > b.minor;
    if (a.patch != b.patch) return a.patch > b.patch;
    return !a.prerelease && b.prerelease;
}

/// Normalizes a tag or version string like "v0.3.0" to "0.3.0".
[[nodiscard]] inline std::string normalize_version_string(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    if (!s.empty() && (s.front() == 'v' || s.front() == 'V')) s.remove_prefix(1);
    return std::string{s};
}

struct GithubAsset {
    std::string name;
    std::string browser_download_url;
    uint64_t size = 0;
    std::string digest; // GitHub Releases API provides "sha256:<hex>"
};

struct GithubRelease {
    std::string tag_name;
    std::string name;
    std::string html_url;
    std::string body;
    bool draft = false;
    bool prerelease = false;
    std::vector<GithubAsset> assets;
};

struct UpdateState {
    int64_t last_check_unix = 0;
    std::string latest_version;
    std::string latest_tag;
    std::string release_url;
    std::string release_notes;
    std::string asset_name;
    std::string asset_url;
    std::string asset_sha256;
    uint64_t asset_size = 0;
    std::string notified_version;
    std::string last_error;
};

/// Returns true when an automatic update check should run.
[[nodiscard]] inline bool should_auto_check_updates(bool auto_check_enabled, int64_t last_check_unix, int64_t now_unix) noexcept {
    if (!auto_check_enabled) return false;
    if (last_check_unix <= 0) return true;
    return (now_unix - last_check_unix) >= UPDATE_CHECK_INTERVAL_S;
}

/// Checks `tar.exe --version` output for `.7z` support (Windows 11 23H2+ ships libarchive >= 3.6 with liblzma).
[[nodiscard]] inline bool parse_tar_supports_7z(std::string_view version_output) noexcept {
    if (version_output.find("liblzma/") == std::string_view::npos) return false;
    const size_t pos = version_output.find("libarchive ");
    if (pos == std::string_view::npos) return false;
    const std::string_view rest = version_output.substr(pos + 11);
    int major = 0;
    int minor = 0;
    size_t i = 0;
    while (i < rest.size() && rest[i] >= '0' && rest[i] <= '9') {
        major = (major * 10) + (rest[i++] - '0');
    }
    if (i == 0 || i >= rest.size() || rest[i] != '.') return false;
    ++i;
    const size_t mstart = i;
    while (i < rest.size() && rest[i] >= '0' && rest[i] <= '9') {
        minor = (minor * 10) + (rest[i++] - '0');
    }
    if (i == mstart) return false;
    return major > 3 || (major == 3 && minor >= 6);
}

[[nodiscard]] inline std::string extract_sha256_digest(std::string_view digest) {
    constexpr std::string_view prefix = "sha256:";
    if (digest.size() != prefix.size() + 64) return {};
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(digest[i])) != prefix[i]) return {};
    }
    std::string out;
    out.reserve(64);
    for (const char c : digest.substr(prefix.size())) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return {};
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

/// Picks the best release archive matching the host's extraction capability (`.7z` if supported, else `.zip`).
/// If `preferred_7z` / `preferred_zip` are non-empty, exact case-insensitive matches are preferred over arbitrary archives.
[[nodiscard]] inline std::optional<GithubAsset> pick_release_asset(
    std::span<const GithubAsset> assets,
    bool prefer_7z,
    std::string_view preferred_7z,
    std::string_view preferred_zip) {
    const auto ends_with_ci = [](std::string_view s, std::string_view suffix) {
        if (s.size() < suffix.size()) return false;
        const std::string_view tail = s.substr(s.size() - suffix.size());
        for (size_t i = 0; i < suffix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(tail[i])) != std::tolower(static_cast<unsigned char>(suffix[i]))) return false;
        }
        return true;
    };
    const auto eq_ci = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
        }
        return true;
    };

    const auto find_ext = [&](std::string_view exact_name, std::string_view ext) -> std::optional<GithubAsset> {
        if (!exact_name.empty()) {
            for (const auto& a : assets) {
                if (eq_ci(a.name, exact_name) && !a.browser_download_url.empty()) return a;
            }
        }
        for (const auto& a : assets) {
            if (ends_with_ci(a.name, ext) && !a.browser_download_url.empty()) return a;
        }
        return std::nullopt;
    };

    if (prefer_7z) {
        if (auto a = find_ext(preferred_7z, ".7z")) return a;
    }
    return find_ext(preferred_zip, ".zip");
}

} // namespace usylibpp::util
