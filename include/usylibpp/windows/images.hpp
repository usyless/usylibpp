#pragma once

#include "../windows.hpp"
#include <wincodec.h>
#include <wincodecsdk.h>

#pragma comment(lib, "Windowscodecs.lib")

namespace usylibpp::windows::images {
    template <uint8_t _channels>
    struct DecodedImage {
        static constexpr uint8_t channels = _channels;

        std::vector<uint8_t> data;
        union {
            uint64_t full;
            struct { // works on little endian, nothing uses big endian anyway
                uint32_t height; // low 4 bits
                uint32_t width; // high 4 bits
            };
        } dimensions;
    };

    template <uint8_t _channels>
    struct DecodedImageView {
        static constexpr uint8_t channels = _channels;

        wil::com_ptr<IWICBitmap> bitmap;
        wil::com_ptr<IWICBitmapLock> lock;

        const uint8_t* data;
        union {
            uint64_t full;
            struct { // works on little endian, nothing uses big endian anyway
                uint32_t height; // low 4 bits
                uint32_t width; // high 4 bits
            };
        } dimensions;
        uint32_t buffer_size;
    };

    enum class DecodedImageType {
        RGB,
        RGBA,
        Gray
    };

    template <DecodedImageType T>
    struct DecodedImageChannels;

    template <>
    struct DecodedImageChannels<DecodedImageType::Gray> {
        static constexpr uint8_t value = 1;
    };

    template <>
    struct DecodedImageChannels<DecodedImageType::RGB> {
        static constexpr uint8_t value = 3;
    };

    template <>
    struct DecodedImageChannels<DecodedImageType::RGBA> {
        static constexpr uint8_t value = 4;
    };

    inline std::optional<wil::com_ptr<IWICImagingFactory>> create_imaging_factory() {
        wil::com_ptr<IWICImagingFactory> factory;
        const auto hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory)
        );
        if (FAILED(hr) || !factory) return std::nullopt;

        return factory;
    }

    inline std::optional<wil::com_ptr<IWICBitmapDecoder>> create_imaging_decoder(IWICImagingFactory* factory, const std::wstring& path) {
        wil::com_ptr<IWICBitmapDecoder> decoder;
        const auto hr = factory->CreateDecoderFromFilename(
            path.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &decoder
        );
        if (FAILED(hr) || !decoder) return std::nullopt;

        return decoder;
    }

    inline std::optional<wil::com_ptr<IWICFormatConverter>> create_imaging_format_converter(IWICImagingFactory* factory, IWICBitmapFrameDecode* frame, REFWICPixelFormatGUID dstFormat, WICBitmapDitherType dither, IWICPalette * pIPalette, double alphaThresholdPercent, WICBitmapPaletteType paletteTranslate) {
        wil::com_ptr<IWICFormatConverter> converter;
        auto hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr) || !converter) return std::nullopt;

        hr = converter->Initialize(frame, dstFormat, dither, pIPalette, alphaThresholdPercent, paletteTranslate);
        if (FAILED(hr)) return std::nullopt;

        return converter;
    }

    template <bool ComInitialised = false, DecodedImageType type>
    inline std::optional<DecodedImage<DecodedImageChannels<type>::value>> decode_image(const std::wstring& path) {
        COMWrapper<ComInitialised> COM{COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE};

        if (FAILED(COM.status())) return std::nullopt;

        auto factory_opt = create_imaging_factory();
        if (!factory_opt) return std::nullopt;
        auto& factory = factory_opt.value();

        auto decoder_opt = create_imaging_decoder(factory.get(), path);
        if (!decoder_opt) return std::nullopt;
        auto& decoder = decoder_opt.value();

        wil::com_ptr<IWICBitmapFrameDecode> frame;
        auto hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr) || !frame) return std::nullopt;

        auto converter_opt = create_imaging_format_converter(factory.get(), frame.get(),
            (type == DecodedImageType::Gray) ? GUID_WICPixelFormat8bppGray : (type == DecodedImageType::RGBA) ? GUID_WICPixelFormat32bppRGBA : GUID_WICPixelFormat24bppRGB,
            WICBitmapDitherTypeNone,
            nullptr, 0.0,
            (type == DecodedImageType::Gray) ? WICBitmapPaletteTypeFixedGray256 : WICBitmapPaletteTypeCustom
        );
        if (!converter_opt) return std::nullopt;
        auto& converter = converter_opt.value();

        UINT width, height;
        hr = converter->GetSize(&width, &height);
        if (FAILED(hr)) return std::nullopt;

        const UINT stride = width * DecodedImageChannels<type>::value;
        const UINT buffer_size = stride * height;

        std::vector<uint8_t> buffer(buffer_size);

        hr = converter->CopyPixels(
            nullptr,
            stride,
            buffer_size,
            buffer.data()
        );
        if (FAILED(hr)) return std::nullopt;

        DecodedImage<DecodedImageChannels<type>::value> ret {
            std::move(buffer), {0}
        };
        ret.dimensions.width = width;
        ret.dimensions.height = height;

        return ret;
    }

    /**
     * COM MUST BE INITIALISED
     * Reuses the factory per thread
     * Returned data depends on COM
     */
    template <DecodedImageType type>
    inline std::optional<DecodedImageView<DecodedImageChannels<type>::value>> decode_image_threadlocal(const std::wstring& path) {
        thread_local auto factory_opt = create_imaging_factory();
        if (!factory_opt) return std::nullopt;
        auto& factory = factory_opt.value();

        auto decoder_opt = create_imaging_decoder(factory.get(), path);
        if (!decoder_opt) return std::nullopt;
        auto& decoder = decoder_opt.value();

        wil::com_ptr<IWICBitmapFrameDecode> frame;
        auto hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr) || !frame) return std::nullopt;

        auto converter_opt = create_imaging_format_converter(factory.get(), frame.get(),
            (type == DecodedImageType::Gray) ? GUID_WICPixelFormat8bppGray : (type == DecodedImageType::RGBA) ? GUID_WICPixelFormat32bppRGBA : GUID_WICPixelFormat24bppRGB,
            WICBitmapDitherTypeNone,
            nullptr, 0.0,
            (type == DecodedImageType::Gray) ? WICBitmapPaletteTypeFixedGray256 : WICBitmapPaletteTypeCustom
        );
        if (!converter_opt) return std::nullopt;
        auto& converter = converter_opt.value();

        UINT width, height;
        hr = converter->GetSize(&width, &height);
        if (FAILED(hr)) return std::nullopt;

        wil::com_ptr<IWICBitmap> bitmap;
        hr = factory->CreateBitmapFromSource(
            converter.get(),
            WICBitmapCacheOnLoad,
            &bitmap
        );
        if (FAILED(hr)) return std::nullopt;

        WICRect rc = { 0, 0, (INT)width, (INT)height };

        wil::com_ptr<IWICBitmapLock> lock;
        hr = bitmap->Lock(&rc, WICBitmapLockRead, &lock);
        if (FAILED(hr)) return std::nullopt;

        UINT bufferSize = 0;
        BYTE* data = nullptr;
        hr = lock->GetDataPointer(&bufferSize, &data);
        if (FAILED(hr)) return std::nullopt;

        DecodedImageView<DecodedImageChannels<type>::value> ret {
            bitmap, lock, data, {0}, bufferSize
        };
        ret.dimensions.width = width;
        ret.dimensions.height = height;

        return ret;
    }
}