#pragma once

#ifdef USYLIBPP_ENABLE_WIL
#include "../aliases.hpp" // IWYU pragma: export
#include <vector>
#include <optional>
#include <span>
#include <windows.h>
#include <compressapi.h>
#include <wil/resource.h>
#include "../files.hpp"

#pragma comment(lib, "Cabinet.lib")

namespace usylibpp::windows {

enum class CompressAlgorithm : DWORD {
    INVALID = COMPRESS_ALGORITHM_INVALID,
    NULL_ = COMPRESS_ALGORITHM_NULL,
    MSZIP = COMPRESS_ALGORITHM_MSZIP,
    XPRESS = COMPRESS_ALGORITHM_XPRESS,
    XPRESS_HUFFMAN = COMPRESS_ALGORITHM_XPRESS_HUFF,
    LZMS = COMPRESS_ALGORITHM_LZMS,
};

class Compressor {
public:
    explicit Compressor(const CompressAlgorithm algo) {
        if (!CreateCompressor(static_cast<DWORD>(algo), nullptr, &compressor_)) {
            THROW_LAST_ERROR();
        }
    }

    [[nodiscard]] inline std::optional<std::vector<char>> compress(std::span<const char> input) const {
        SIZE_T compressedSize = 0;

        if (!Compress(compressor_.get(), input.data(), input.size(), nullptr, 0, &compressedSize)) {
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return std::nullopt;
        }

        std::vector<char> output(compressedSize);

        if (!Compress(compressor_.get(), input.data(), input.size(),
                    output.data(), output.size(), &compressedSize)) {
            return std::nullopt;
        }

        output.resize(compressedSize);
        return output;
    }

    [[nodiscard]] inline std::optional<std::vector<char>> compress_from_file(const std::filesystem::path& input_path) const {
        const auto data = files::read_as_bytes(input_path);
        if (!data) return std::nullopt;
        return compress(std::span<const char>{data->data(), data->size()});
    }

    [[nodiscard]] inline std::optional<size_t> compress_to_file(std::span<const char> input, const std::filesystem::path& output_path) const {
        const auto data = compress(input);
        if (!data) return std::nullopt;
        if (!files::write(output_path, *data)) return std::nullopt;

        return data->size();
    }

    [[nodiscard]] inline std::optional<size_t> compress_to_from(const std::filesystem::path& input_path, const std::filesystem::path& output_path) const {
        const auto data = compress_from_file(input_path);
        if (!data) return std::nullopt;

        return compress_to_file(*data, output_path);
    }

private:
    wil::unique_any<COMPRESSOR_HANDLE, decltype(&CloseCompressor), CloseCompressor> compressor_;
};

class Decompressor {
public:
    explicit Decompressor(const CompressAlgorithm algo) {
        if (!CreateDecompressor(static_cast<DWORD>(algo), nullptr, &decompressor_)) {
            THROW_LAST_ERROR();
        }
    }

    [[nodiscard]] inline std::optional<std::vector<char>> decompress(std::span<const char> input, SIZE_T expectedSize) const {
        std::vector<char> output(expectedSize);
        SIZE_T decompressedSize = 0;

        if (!Decompress(decompressor_.get(), input.data(), input.size(),
                        output.data(), output.size(), &decompressedSize)) {
            return std::nullopt;
        }

        output.resize(decompressedSize);
        return output;
    }

private:
    wil::unique_any<DECOMPRESSOR_HANDLE, decltype(&CloseDecompressor), CloseDecompressor> decompressor_;
};

}
#endif