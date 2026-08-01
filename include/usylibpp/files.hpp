#pragma once

#include "aliases.hpp" // IWYU pragma: export
#include "macros.hpp"
#include <span>
#include <string>
#include <filesystem>
#include <fstream>
#include <optional>

/**
 * Helper methods to do with file operations
 */
namespace usylibpp::files {
    /**
     * Read a file as bytes into a std::string
     */
    [[nodiscard]] inline std::optional<std::string> read_as_bytes(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return std::nullopt;

        file.seekg(0, std::ios::end);
        const std::streamoff size = file.tellg();
        if (size < 0) return std::nullopt;
        file.seekg(0, std::ios::beg);

        std::string buffer(static_cast<std::size_t>(size), '\0');

        if (size > 0 && !file.read(buffer.data(), static_cast<std::streamsize>(size))) {
            return std::nullopt;
        }

        return buffer;
    }
    
    USYLIBPP__MAKE_OR(read_as_bytes, std::string{})

    /**
     * Write data to a file
     */
    [[nodiscard]] inline bool write(const std::filesystem::path& path, std::span<const char> data) {
        std::ofstream file(path, std::ios::binary);
        if (!file) return false;

        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        return file.good();
    }

    /**
     * Write data to a file
     */
    [[nodiscard]] inline bool write(const std::filesystem::path& path, std::string_view data) {
        return write(path, std::span<const char>{data.data(), data.size()});
    }
}