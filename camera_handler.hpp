#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ZXing/ImageView.h>
#include <string>

struct FrameBuffer{
	const ZXing::ImageFormat format;
	const size_t width, height;
	const uint8_t *data;
};
inline constexpr FrameBuffer INVALID_FRAME_BUFFER={ZXing::ImageFormat::None, 0, 0, NULL};

struct Buffer {
	void *start;
	std::size_t length;
};

struct CameraInfo{
	const int charactor_file_descriptor;
	Buffer *const buffers;
	const std::size_t max_num_buffers, width, height;
	inline bool operator==(const CameraInfo& rhs) const { return this->charactor_file_descriptor==rhs.charactor_file_descriptor; }
};
inline constexpr CameraInfo INVALID_CAMERA_INFO={-1, NULL, 0};


CameraInfo open_camera(const std::string video_device_name);
int close_camera(const CameraInfo *const camera_info);

int process_next_frame(const CameraInfo *const camera_info, const std::function<void(const FrameBuffer *const)> &processer);
