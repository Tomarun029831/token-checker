#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

struct FrameBuffer{
	const int32_t width, height;
	uint8_t *data;
};
inline constexpr FrameBuffer INVALID_FRAME_BUFFER={0, 0, NULL};

struct Buffer {
	void *start;
	std::size_t length;
};

#ifdef _WIN64
struct CameraInfo {
    void *charactor_file_descriptor=nullptr;
    Buffer *buffers=nullptr;
    std::int32_t max_num_buffers=0;
    std::int32_t width=0;
    std::int32_t height=0;

    bool operator==(const CameraInfo& other) const { return charactor_file_descriptor==other.charactor_file_descriptor; }
    bool operator!=(const CameraInfo& other) const { return !(*this==other); }
};
inline constexpr CameraInfo INVALID_CAMERA_INFO={nullptr, nullptr, 0, 0, 0};
#elif __linux__
struct CameraInfo{
	const int charactor_file_descriptor;
	Buffer *const buffers;
	const std::size_t max_num_buffers, width, height;
	inline bool operator==(const CameraInfo& rhs) const { return this->charactor_file_descriptor==rhs.charactor_file_descriptor; }
};
inline constexpr CameraInfo INVALID_CAMERA_INFO={-1, NULL, 0};
#endif

CameraInfo open_camera(const std::string& video_device_name);
int close_camera(const CameraInfo *const camera_info);

int process_next_frame(const CameraInfo *const camera_info, const std::function<void(const FrameBuffer *const)> &processer);
