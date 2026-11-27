#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace usylibpp::util {

/// Evaluates the REG_BINARY value stored under
/// `HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run`.
/// Windows uses bit 0 of the first byte to mark a Run entry disabled (`0x03`, `0x01`, `0x05`, `0x07`)
/// vs enabled (`0x02`, `0x06`, `0x00`). Empty/missing data defaults to approved.
[[nodiscard]] constexpr bool is_startup_approved_bytes(std::span<const uint8_t> data) noexcept {
    if (data.empty()) return true;
    return (data[0] & 0x01u) == 0u;
}

/// Candidate executable paths extracted from a Windows `Run` registry command string.
struct StartupCommandCandidates {
    std::wstring_view primary;
    std::wstring_view fallback; // non-empty only for unquoted commands containing whitespace
};

/// Extracts candidate executable path(s) from a `CurrentVersion\Run` command line.
/// - Quoted commands (`"C:\path with spaces\app.exe" --arg`) return the quoted path as `primary`.
/// - Unclosed quotes return empty candidates.
/// - Unquoted commands return the trimmed full string as `primary`, and (if whitespace is present)
///   the leading whitespace-delimited token as `fallback` so both unquoted paths with spaces and
///   unquoted paths with arguments can be checked against the filesystem.
[[nodiscard]] constexpr StartupCommandCandidates extract_startup_command_exe(std::wstring_view cmd) noexcept {
    if (const size_t nul = cmd.find(L'\0'); nul != std::wstring_view::npos) {
        cmd = cmd.substr(0, nul);
    }
    constexpr auto is_ws = [](wchar_t c) noexcept {
        return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n';
    };
    while (!cmd.empty() && is_ws(cmd.front())) cmd.remove_prefix(1);
    while (!cmd.empty() && is_ws(cmd.back())) cmd.remove_suffix(1);
    if (cmd.empty()) return {};

    if (cmd.front() == L'"') {
        const size_t close = cmd.find(L'"', 1);
        if (close == std::wstring_view::npos || close == 1) return {};
        std::wstring_view inside = cmd.substr(1, close - 1);
        while (!inside.empty() && is_ws(inside.front())) inside.remove_prefix(1);
        while (!inside.empty() && is_ws(inside.back())) inside.remove_suffix(1);
        return {.primary = inside, .fallback = {}};
    }

    size_t ws_pos = std::wstring_view::npos;
    for (size_t i = 0; i < cmd.size(); ++i) {
        if (is_ws(cmd[i])) {
            ws_pos = i;
            break;
        }
    }
    if (ws_pos == std::wstring_view::npos) {
        return {.primary = cmd, .fallback = {}};
    }
    return {.primary = cmd, .fallback = cmd.substr(0, ws_pos)};
}

} // namespace usylibpp::util
