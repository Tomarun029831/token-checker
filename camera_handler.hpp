#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ZXing/ImageView.h>

struct Buffer {
    void* start = nullptr;
    std::size_t length = 0;
};

struct FrameBuffer {
    ZXing::ImageFormat format;
    std::size_t width;
    std::size_t height;
    const std::uint8_t* data;
};

struct CameraInfo {
    // Linux版のcharactor_file_descriptorに相当する位置を保つための不透明ハンドル。
    // Windows版では実体をNativeCamera*として使用する。
    void* charactor_file_descriptor = nullptr;
    Buffer* buffers = nullptr;
    std::size_t max_num_buffers = 0;
    std::size_t width = 0;
    std::size_t height = 0;

    bool operator==(const CameraInfo& other) const
    {
        return charactor_file_descriptor == other.charactor_file_descriptor;
    }
    bool operator!=(const CameraInfo& other) const
    {
        return !(*this == other);
    }
};

inline const CameraInfo INVALID_CAMERA_INFO{};

CameraInfo open_camera(std::size_t video_id);
int close_camera(const CameraInfo* camera_info);
int process_next_frame(
    const CameraInfo* camera_info,
    const std::function<void(const FrameBuffer* const)>& processer);
