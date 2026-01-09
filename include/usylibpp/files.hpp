#pragma once

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
        const auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string buffer(size, '\0');

        if (!file.read(buffer.data(), size)) {
            return std::nullopt;
        }

        return buffer;
    }
}