#include <codecvt>
#include <cstddef>
#include <cstring>
#include "camera_handler.hpp"
#include <ZXing/ImageView.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

static int xioctl(int fd, unsigned long request, void *argp){
	int r;
	do {
		r = ioctl(fd, request, argp); // https://www.man7.org/linux/man-pages/man2/ioctl.2.html
	} while (r == -1 && errno == EINTR);	// https://man7.org/linux/man-pages/man7/signal.7.html
											// the details In "Interruption of system calls and library functions by signal handlers"
	return r;
}

// https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/capture.c.html
CameraInfo open_camera(const std::wstring video_device_name){
	// searh the specifed charactor device
	const std::filesystem::path v4l2_dir = "/sys/class/video4linux";
	if(std::filesystem::exists(v4l2_dir)==false) return INVALID_CAMERA_INFO;
	for(const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(v4l2_dir)){
		if(entry.is_directory()==false) continue;
		// /sys/class/video4linux/video*/name
		const std::filesystem::path name_file = entry.path() / "name";
		std::cout << "path=" << entry.path() << std::endl;
		std::ifstream file_stream(name_file);
		std::string device_name;
		if(std::getline(file_stream, device_name)){
			std::cout << "device_name=" << device_name << std::endl;
			// std::wstring_convert<std::codecvt_utf8<class Elem><wchar_t>> converter;
			// const std::wstring converted_str = converter.from_bytes(device_name);
			// if(converted_str==video_device_name) break;
		}
	}

	return INVALID_CAMERA_INFO; // INFO: for debugging
	const int video_id = 0;
	const char dev_name[] = {
		'/','d','e','v',
		'/','v','i','d','e','o', static_cast<char>('0'+(video_id/10==0?video_id%10:video_id/10)), static_cast<char>((video_id/10==0?'\0':('0'+video_id%10))),'\0'};

	// open the charactor device
	struct stat st;
	if(stat(dev_name, &st)==-1 || !S_ISCHR(st.st_mode)) return INVALID_CAMERA_INFO; // https://ja.manpages.org/stat/2
	const int fd = open(dev_name, O_RDWR|O_NONBLOCK); // https://ja.manpages.org/open/2
	if(fd==-1) return INVALID_CAMERA_INFO;
	// check capability as video camera
	struct v4l2_capability cap;
	if(	xioctl(fd, VIDIOC_QUERYCAP, &cap)==-1 || // is V4L2-device
		!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) || // Is a video capture device
		!(cap.capabilities & V4L2_CAP_STREAMING)) // streaming I/O ioctls
		return INVALID_CAMERA_INFO;
	// reset settings of device
	struct v4l2_cropcap cropcap;
	memset(&cropcap, 0, sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(fd, VIDIOC_CROPCAP, &cropcap)==0){
		struct v4l2_crop crop;
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;
		xioctl(fd, VIDIOC_S_CROP, &crop);
	}
	// set video format
	struct v4l2_format format;
	memset(&format, 0, sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	constexpr std::size_t optimized_width=640, optimized_height=480;
	format.fmt.pix.width = optimized_width;
	format.fmt.pix.height = optimized_height;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	format.fmt.pix.field = V4L2_FIELD_ANY;
	if(xioctl(fd, VIDIOC_S_FMT, &format)==-1 ||
		format.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
		return INVALID_CAMERA_INFO;
	// Buggy driver paranoia
	std::size_t min = format.fmt.pix.width * 2;
	if (format.fmt.pix.bytesperline < min) format.fmt.pix.bytesperline = min;
	min = format.fmt.pix.bytesperline * format.fmt.pix.height;
	if (format.fmt.pix.sizeimage < min) format.fmt.pix.sizeimage = min;
	// request kernel to allocate mapped-memory
	struct v4l2_requestbuffers request;
	memset(&request, 0, sizeof(request));
	request.count = 4;
	request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	request.memory = V4L2_MEMORY_MMAP;
	if(xioctl(fd, VIDIOC_REQBUFS, &request)==-1 || request.count < 2) return INVALID_CAMERA_INFO;
	// bind memory to allocated memory
	std::size_t n_buffers;
	Buffer *const buffers = (Buffer *const)calloc(request.count, sizeof(*buffers));
	if(buffers==NULL) return INVALID_CAMERA_INFO;
	for(n_buffers=0; n_buffers<request.count; ++n_buffers){
		struct v4l2_buffer video_buffer;
		memset(&video_buffer, 0, sizeof(video_buffer));
		video_buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video_buffer.memory = V4L2_MEMORY_MMAP;
		video_buffer.index = n_buffers;
		if(xioctl(fd, VIDIOC_QUERYBUF, &video_buffer)==-1) return INVALID_CAMERA_INFO;

		buffers[n_buffers].length = video_buffer.length;
		buffers[n_buffers].start = mmap(NULL, video_buffer.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, video_buffer.m.offset);
		if(buffers[n_buffers].start == MAP_FAILED) return INVALID_CAMERA_INFO;
	}
	// let camera stream image
	for(std::size_t i=0; i<n_buffers; ++i){
		struct v4l2_buffer video_buffer;
		memset(&video_buffer, 0, sizeof(video_buffer));
		video_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video_buffer.memory = V4L2_MEMORY_MMAP;
		video_buffer.index = i;
		if(xioctl(fd, VIDIOC_QBUF, &video_buffer)==-1) return INVALID_CAMERA_INFO;
	}
	enum v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(fd, VIDIOC_STREAMON, &type)==-1) return INVALID_CAMERA_INFO;
	return {fd, buffers, n_buffers, optimized_width, optimized_height};
}

int close_camera(const CameraInfo *const camera_info){
	// stop stream from camera
	enum v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(camera_info->charactor_file_descriptor, VIDIOC_STREAMOFF, &type)==-1) return EXIT_FAILURE;
	// unmap buffers and free it
	for(std::size_t i=0; i<camera_info->max_num_buffers; ++i)
		if(munmap(camera_info->buffers[i].start, camera_info->buffers[i].length)==-1)
			return EXIT_FAILURE;
	free(camera_info->buffers);
	// close charactor device
	if(close(camera_info->charactor_file_descriptor)==-1) return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

int process_next_frame(const CameraInfo *const camera_info, const std::function<void(const FrameBuffer *const)> &processer){
	while(true){
		// monitor file_descriptro with select
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(camera_info->charactor_file_descriptor, &fds);
		struct timeval tv = {2, 0};
		const int r = select(camera_info->charactor_file_descriptor+1, &fds, NULL, NULL, &tv);
		if(r==-1) {
			if(errno==EINTR) continue;
			else return EXIT_FAILURE;
		} else if(r==0) return EXIT_FAILURE; // timeout of select
		// dequeue buffer
		struct v4l2_buffer video_buffer;
		memset(&video_buffer, 0, sizeof(video_buffer));
		video_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video_buffer.memory = V4L2_MEMORY_MMAP;
		if(xioctl(camera_info->charactor_file_descriptor, VIDIOC_DQBUF, &video_buffer)==-1) { // https://www.man7.org/linux/man-pages/man3/errno.3.html
			if(errno==EAGAIN||errno==EIO) continue; // Resource temporarily unavailable or Input/output error
			else return EXIT_FAILURE;
		}
		// get pointer to dequeued buffer
		if(video_buffer.index>=camera_info->max_num_buffers) return EXIT_FAILURE;
		// process buffer
		const FrameBuffer frame_buffer = {
			ZXing::ImageFormat::Lum,
			camera_info->width,
			camera_info->height,
			static_cast<const uint8_t *>(camera_info->buffers[video_buffer.index].start)};
		processer(&frame_buffer);
		// return dequeued buffer to kernel
		if(xioctl(camera_info->charactor_file_descriptor, VIDIOC_QBUF, &video_buffer)==-1) return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}
}
