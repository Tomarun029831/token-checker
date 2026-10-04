#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <cstdlib>
#include <vector>
#include "camera_handler.hpp"

using Microsoft::WRL::ComPtr;

namespace {

constexpr UINT32 WIDTH = 640;
constexpr UINT32 HEIGHT = 480;

struct NativeCamera {
    ComPtr<IMFMediaSource> source;
    ComPtr<IMFSourceReader> reader;
    UINT32 width = 0;
    UINT32 height = 0;
    LONG stride = 0;

    // main.cppは元のYUYVインターフェースを使うため、
    // YUY2入力からYUYV相当の2バイト/画素バッファを生成する。
    std::vector<std::uint8_t> yuyv;
};

HRESULT create_source(std::size_t index, IMFMediaSource** result)
{
    if (!result) {
        return E_POINTER;
    }
    *result = nullptr;

    ComPtr<IMFAttributes> attributes;
    HRESULT hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr)) {
        return hr;
    }

    hr = attributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        return hr;
    }

    IMFActivate** devices = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(attributes.Get(), &devices, &count);
    if (FAILED(hr)) {
        return hr;
    }

    if (index >= count) {
        for (UINT32 i = 0; i < count; ++i) {
            devices[i]->Release();
        }
        CoTaskMemFree(devices);
        return MF_E_NOT_FOUND;
    }

    hr = devices[index]->ActivateObject(IID_PPV_ARGS(result));
    for (UINT32 i = 0; i < count; ++i) {
        devices[i]->Release();
    }
    CoTaskMemFree(devices);
    return hr;
}

} // namespace

CameraInfo open_camera(const std::size_t video_id)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return INVALID_CAMERA_INFO;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        if (hr != RPC_E_CHANGED_MODE) {
            CoUninitialize();
        }
        return INVALID_CAMERA_INFO;
    }

    auto* camera = new NativeCamera();

    hr = create_source(video_id, &camera->source);
    if (FAILED(hr)) {
        delete camera;
        MFShutdown();
        CoUninitialize();
        return INVALID_CAMERA_INFO;
    }

    ComPtr<IMFAttributes> reader_attributes;
    hr = MFCreateAttributes(&reader_attributes, 1);
    if (SUCCEEDED(hr)) {
        // カメラがYUY2を直接返すことを優先し、別形式への変換は要求しない。
        hr = reader_attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);
    }
    if (SUCCEEDED(hr)) {
        hr = MFCreateSourceReaderFromMediaSource(
            camera->source.Get(), reader_attributes.Get(), &camera->reader);
    }

    ComPtr<IMFMediaType> type;
    if (SUCCEEDED(hr)) {
        hr = MFCreateMediaType(&type);
    }
    if (SUCCEEDED(hr)) {
        hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    }
    if (SUCCEEDED(hr)) {
        hr = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
    }
    if (SUCCEEDED(hr)) {
        hr = MFSetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT);
    }
    if (SUCCEEDED(hr)) {
        hr = camera->reader->SetCurrentMediaType(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type.Get());
    }

    ComPtr<IMFMediaType> actual_type;
    UINT32 actual_width = 0;
    UINT32 actual_height = 0;
    if (SUCCEEDED(hr)) {
        hr = camera->reader->GetCurrentMediaType(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM, &actual_type);
    }
    if (SUCCEEDED(hr)) {
        hr = MFGetAttributeSize(
            actual_type.Get(), MF_MT_FRAME_SIZE,
            &actual_width, &actual_height);
    }

    UINT32 unsigned_stride = 0;
    if (SUCCEEDED(hr) && SUCCEEDED(actual_type->GetUINT32(
                                    MF_MT_DEFAULT_STRIDE,
                                    &unsigned_stride))) {
        camera->stride = static_cast<LONG>(unsigned_stride);
    } else {
        camera->stride = static_cast<LONG>(actual_width * 2);
    }

    if (FAILED(hr) || actual_width == 0 || actual_height == 0) {
        camera->reader.Reset();
        camera->source.Reset();
        delete camera;
        MFShutdown();
        CoUninitialize();
        return INVALID_CAMERA_INFO;
    }

    camera->width = actual_width;
    camera->height = actual_height;
    camera->yuyv.resize(
        static_cast<std::size_t>(actual_width) * actual_height * 2);

    return {
        camera,
        nullptr,
        0,
        camera->width,
        camera->height};
}

int close_camera(const CameraInfo* const camera_info)
{
    if (!camera_info || !camera_info->charactor_file_descriptor) {
        return EXIT_FAILURE;
    }

    auto* camera = static_cast<NativeCamera*>(
        camera_info->charactor_file_descriptor);
    camera->reader.Reset();
    camera->source->Shutdown();
    camera->source.Reset();
    delete camera;

    MFShutdown();
    CoUninitialize();
    return EXIT_SUCCESS;
}

int process_next_frame(
    const CameraInfo* const camera_info,
    const std::function<void(const FrameBuffer* const)>& processer)
{
    if (!camera_info || !camera_info->charactor_file_descriptor || !processer) {
        return EXIT_FAILURE;
    }

    auto* camera = static_cast<NativeCamera*>(
        camera_info->charactor_file_descriptor);

    // 元のmain.cppはprocess_next_frameを一度だけ呼ぶため、
    // ここで継続的にフレームを取得して同じコールバックへ渡す。
    while (true) {
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        ComPtr<IMFSample> sample;
        HRESULT hr = camera->reader->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0, nullptr, &flags, &timestamp, &sample);
        if (FAILED(hr)) {
            return EXIT_FAILURE;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            return EXIT_SUCCESS;
        }
        if (!sample) {
            continue;
        }

        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) {
            continue;
        }

        BYTE* source_data = nullptr;
        DWORD max_length = 0;
        DWORD current_length = 0;
        if (FAILED(buffer->Lock(
                &source_data, &max_length, &current_length))) {
            continue;
        }

        const std::size_t row_stride =
            static_cast<std::size_t>(std::abs(camera->stride));
        const std::size_t required = row_stride * camera->height;
        if (current_length < required) {
            buffer->Unlock();
            continue;
        }

        for (UINT32 y = 0; y < camera->height; ++y) {
            const UINT32 source_y = camera->stride >= 0
                ? y
                : camera->height - 1 - y;
            const auto* source_row = source_data +
                static_cast<std::size_t>(source_y) * row_stride;
            auto* destination_row = camera->yuyv.data() +
                static_cast<std::size_t>(y) * camera->width * 2;

            for (UINT32 x = 0; x < camera->width; ++x) {
                // YUY2のYをそのまま保存し、U/V位置には128を入れる。
                // main.cppのImageViewは2バイト間隔でYだけ読む。
                destination_row[x * 2] = source_row[x * 2];
                destination_row[x * 2 + 1] = 128;
            }
        }
        buffer->Unlock();

        const FrameBuffer frame_buffer{
            ZXing::ImageFormat::Lum,
            camera->width,
            camera->height,
            camera->yuyv.data()};
        processer(&frame_buffer);
    }
}
