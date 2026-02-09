#pragma once

#include "../windows.hpp"
#include <wincodec.h>
#include <wincodecsdk.h>

#pragma comment(lib, "Windowscodecs.lib")

namespace usylibpp::windows::images {
    struct DecodedImageReturn {
        std::vector<uint8_t> data;
        union {
            uint64_t full;
            struct { // works on little endian, nothing uses big endian anyway
                uint32_t height; // low 4 bits
                uint32_t width; // high 4 bits
            };
        } dimensions;
        uint8_t channels;
    };

    inline std::optional<DecodedImageReturn> decode_image_as_greyscale(const std::wstring& path) {
        COMWrapper<false> COM{COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE};
        
        if (FAILED(COM.status())) return std::nullopt;

        wil::com_ptr<IWICImagingFactory> factory;
        auto hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory)
        );
        if (FAILED(hr) || !factory) return std::nullopt;

        wil::com_ptr<IWICBitmapDecoder> decoder;
        hr = factory->CreateDecoderFromFilename(
            path.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &decoder
        );
        if (FAILED(hr) || !decoder) return std::nullopt;

        wil::com_ptr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr) || !frame) return std::nullopt;

        wil::com_ptr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr) || !converter) return std::nullopt;

        hr = converter->Initialize(
            frame.get(),
            GUID_WICPixelFormat8bppGray, // GUID_WICPixelFormat8bppGray
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeFixedGray256
        );
        if (FAILED(hr)) return std::nullopt;

        UINT width, height;
        hr = converter->GetSize(&width, &height);
        if (FAILED(hr)) return std::nullopt;

        const UINT stride = width * sizeof(uint8_t);
        const UINT buffer_size = stride * height;

        std::vector<uint8_t> buffer(buffer_size);

        hr = converter->CopyPixels(
            nullptr,
            stride,
            buffer_size,
            buffer.data()
        );
        if (FAILED(hr)) return std::nullopt;

        DecodedImageReturn ret {
            std::move(buffer), {0}, 1
        };
        ret.dimensions.width = width;
        ret.dimensions.height = height;

        return ret;
    }
}