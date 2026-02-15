#pragma once

#include "../aliases.hpp" // IWYU pragma: export
#include "../windows.hpp"
#include "../strings.hpp"
#include <wincodec.h>
#include <wincodecsdk.h>

#pragma comment(lib, "Windowscodecs.lib")

namespace usylibpp::windows::images {
    enum class DecodedImageType {
        RGB,
        RGBA,
        Gray,
        BGR,
        BGRA
    };

    template <DecodedImageType type>
    inline constexpr auto DecodedImageType_to_format() noexcept {
        #pragma push_macro("HANDLE")
        #undef HANDLE
        #define HANDLE(T, R) if constexpr (type == DecodedImageType::T) return R;
        HANDLE(RGB, GUID_WICPixelFormat24bppRGB)
        else HANDLE(RGBA, GUID_WICPixelFormat32bppRGBA)
        else HANDLE(Gray, GUID_WICPixelFormat8bppGray)
        else HANDLE(BGR, GUID_WICPixelFormat24bppBGR)
        else HANDLE(BGRA, GUID_WICPixelFormat32bppBGRA)
        else {
            static_assert(!std::is_same_v<decltype(type), decltype(type)>, "Invalid type passed into DecodedImageType_to_format");
        }
        #pragma pop_macro("HANDLE")
    }

    template <DecodedImageType type>
    inline consteval auto DecodedImageType_to_palette() noexcept {
        if constexpr (type == DecodedImageType::Gray) {
            return WICBitmapPaletteTypeFixedGray256;
        } else {
            return WICBitmapPaletteTypeCustom;
        }
    }

    template <DecodedImageType type>
    inline consteval uint8_t channels_of() noexcept {
        if constexpr (type == DecodedImageType::Gray) {
            return 1;
        } else if constexpr (type == DecodedImageType::RGB || type == DecodedImageType::RGBA) {
            return 3;
        } else {
            return 4;
        }
    }

    #pragma warning(push)
    #pragma warning(disable:4201)

    template <DecodedImageType type>
    struct DecodedImage {
        static constexpr uint8_t channels = channels_of<type>();

        std::vector<uint8_t> data;
        union {
            uint64_t full;
            struct { // works on little endian, nothing uses big endian anyway
                uint32_t height; // low 4 bits
                uint32_t width; // high 4 bits
            };
        } dimensions;
    };

    template <DecodedImageType type>
    struct DecodedImageView {
        static constexpr uint8_t channels = channels_of<type>();

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

    #pragma warning(pop)

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

    template <DecodedImageType type>
    inline std::optional<wil::com_ptr<IWICFormatConverter>> create_imaging_format_converter(IWICImagingFactory* factory, IWICBitmapFrameDecode* frame) {
        return create_imaging_format_converter(factory, frame,
            DecodedImageType_to_format<type>(),
            WICBitmapDitherTypeNone,
            nullptr, 0.0,
            DecodedImageType_to_palette<type>()
        );
    }

    namespace FramePickers {
        inline std::optional<wil::com_ptr<IWICBitmapFrameDecode>> pick_jpeg_frame(IWICBitmapDecoder* decoder) {
            UINT frame_count = 0;
            auto hr = decoder->GetFrameCount(&frame_count);
            if (FAILED(hr) || frame_count == 0) return std::nullopt;

            wil::com_ptr<IWICBitmapFrameDecode> best_frame;
            UINT best_area = 0;

            for (UINT i = 0; i < frame_count; ++i) {
                wil::com_ptr<IWICBitmapFrameDecode> frame;

                hr = decoder->GetFrame(i, &frame);
                if (FAILED(hr) || !frame) continue;

                UINT w, h;
                hr = frame->GetSize(&w, &h);
                if (FAILED(hr) || w == 0 || h == 0) continue;

                // Prefer JPEG-like formats
                WICPixelFormatGUID fmt;
                hr = frame->GetPixelFormat(&fmt);
                if (FAILED(hr)) continue;

                const bool looksJPEG =
                    IsEqualGUID(fmt, GUID_WICPixelFormat24bppBGR) ||
                    IsEqualGUID(fmt, GUID_WICPixelFormat24bppRGB) ||
                    IsEqualGUID(fmt, GUID_WICPixelFormat32bppBGRA);

                if (!looksJPEG) continue;

                UINT area = w * h;
                if (area > best_area) {
                    best_area = area;
                    best_frame = std::move(frame);
                }
            }

            if (best_frame) return best_frame;
            else return std::nullopt;
        }

        inline std::optional<wil::com_ptr<IWICBitmapFrameDecode>> pick_frame_zero(IWICBitmapDecoder* decoder) {
            wil::com_ptr<IWICBitmapFrameDecode> frame;
            const auto hr = decoder->GetFrame(0, &frame);
            if (FAILED(hr) || !frame) return std::nullopt;
            return frame;
        }
    }

    using FramePicker = std::optional<wil::com_ptr<IWICBitmapFrameDecode>> (*)(IWICBitmapDecoder*);

    template <bool ComInitialised = false, DecodedImageType type, FramePicker frame_picker = &FramePickers::pick_frame_zero>
    inline std::optional<DecodedImage<type>> decode_image(const std::wstring& path) {
        COMWrapper<ComInitialised> COM{COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE};

        if (FAILED(COM.status())) return std::nullopt;

        auto factory_opt = create_imaging_factory();
        if (!factory_opt) return std::nullopt;
        auto& factory = *factory_opt;

        auto decoder_opt = create_imaging_decoder(factory.get(), path);
        if (!decoder_opt) return std::nullopt;
        auto& decoder = *decoder_opt;

        auto frame_opt = frame_picker(decoder.get());
        if (!frame_opt) return std::nullopt;
        auto& frame = *frame_opt;

        auto converter_opt = create_imaging_format_converter<type>(factory.get(), frame.get());
        if (!converter_opt) return std::nullopt;
        auto& converter = *converter_opt;

        UINT width, height;
        auto hr = converter->GetSize(&width, &height);
        if (FAILED(hr)) return std::nullopt;

        const UINT stride = width * channels_of<type>();
        const UINT buffer_size = stride * height;

        std::vector<uint8_t> buffer(buffer_size);

        hr = converter->CopyPixels(
            nullptr,
            stride,
            buffer_size,
            buffer.data()
        );
        if (FAILED(hr)) return std::nullopt;

        DecodedImage<type> ret {
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
    template <DecodedImageType type, FramePicker frame_picker = &FramePickers::pick_frame_zero>
    inline std::optional<DecodedImageView<type>> decode_image_threadlocal(const std::wstring& path) {
        thread_local auto factory_opt = create_imaging_factory();
        if (!factory_opt) return std::nullopt;
        auto& factory = *factory_opt;

        auto decoder_opt = create_imaging_decoder(factory.get(), path);
        if (!decoder_opt) return std::nullopt;
        auto& decoder = *decoder_opt;

        auto frame_opt = frame_picker(decoder.get());
        if (!frame_opt) return std::nullopt;
        auto& frame = *frame_opt;

        auto converter_opt = create_imaging_format_converter<type>(factory.get(), frame.get());
        if (!converter_opt) return std::nullopt;
        auto& converter = *converter_opt;

        UINT width, height;
        auto hr = converter->GetSize(&width, &height);
        if (FAILED(hr)) return std::nullopt;

        wil::com_ptr<IWICBitmap> bitmap;
        hr = factory->CreateBitmapFromSource(
            converter.get(),
            WICBitmapCacheOnLoad,
            &bitmap
        );
        if (FAILED(hr)) return std::nullopt;

        WICRect rc{ 0, 0, (INT)width, (INT)height };

        wil::com_ptr<IWICBitmapLock> lock;
        hr = bitmap->Lock(&rc, WICBitmapLockRead, &lock);
        if (FAILED(hr)) return std::nullopt;

        UINT bufferSize = 0;
        BYTE* data = nullptr;
        hr = lock->GetDataPointer(&bufferSize, &data);
        if (FAILED(hr)) return std::nullopt;

        DecodedImageView<type> ret {
            bitmap, lock, data, {0}, bufferSize
        };
        ret.dimensions.width = width;
        ret.dimensions.height = height;

        return ret;
    }

    /**
     * Does not include leading dot by default, change with template arg
     */
    template <bool ComInitialised = false, bool include_leading_dot = false, types::CharOrWChar Char = char>
    inline std::vector<std::basic_string<Char>> get_all_supported_file_extensions() {
        COMWrapper<ComInitialised> COM{COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE};

        if (FAILED(COM.status())) return {};

        auto factory_opt = create_imaging_factory();
        if (!factory_opt) return {};
        auto& factory = *factory_opt;

        wil::com_ptr<IEnumUnknown> enumDecoders;
        auto hr = factory->CreateComponentEnumerator(
            WICDecoder,
            WICComponentEnumerateDefault,
            &enumDecoders);
        if (FAILED(hr) || !enumDecoders) return {};

        std::vector<std::basic_string<Char>> formats;

        wil::com_ptr<IUnknown> unk;
        while (enumDecoders->Next(1, &unk, nullptr) == S_OK) {
            wil::com_ptr<IWICComponentInfo> compInfo;
            hr = unk->QueryInterface(IID_PPV_ARGS(&compInfo));

            if (FAILED(hr) || !compInfo) continue;


            WICComponentType type;
            hr = compInfo->GetComponentType(&type);
            if (FAILED(hr)) continue;

            if (type == WICDecoder) {
                wil::com_ptr<IWICBitmapCodecInfo> codecInfo;
                hr = compInfo->QueryInterface(IID_PPV_ARGS(&codecInfo));
                if (FAILED(hr) || !codecInfo) continue;

                UINT cch = 0;
                hr = codecInfo->GetFileExtensions(0, nullptr, &cch);
                if (FAILED(hr) || cch <= 0) continue;

                std::wstring ext(cch, L'\0');
                hr = codecInfo->GetFileExtensions(cch, ext.data(), &cch);
                if (FAILED(hr)) continue;

                std::basic_string<Char> ext_converted;
                if constexpr (std::is_same_v<Char, char>) {
                    auto ext_utf8 = windows::to_utf8(ext);
                    if (!ext_utf8) continue;
                    ext_converted = std::move(*ext_utf8);
                } else {
                    ext_converted = std::move(ext);
                }

                strings::split_by_for_each(ext_converted, Char(','), [&formats](const std::basic_string_view<Char> extension) {
                    if (extension.size() < 1) return;

                    if constexpr (include_leading_dot) {
                        formats.emplace_back(extension);
                    } else {
                        if (extension.starts_with(Char('.'))) formats.emplace_back(extension.substr(1));
                        else formats.emplace_back(extension);
                    }
                });
            }
        }
        return formats;
    }
}