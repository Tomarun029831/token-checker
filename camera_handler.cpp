#ifdef _WIN64
#include <cstdlib>
#include <cwchar>
#include <string>
#include "camera_handler.hpp"
#include "utility.hpp"

#include <basetsd.h>
#include <combaseapi.h>
#include <mfobjects.h>
#include <mfapi.h>
#include <minwindef.h>
#include <stringapiset.h>
#include <winnls.h>
#include <winnt.h>
#include <winrt/base.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <winerror.h>

struct NativeCamera {
	winrt::com_ptr<IMFMediaSource> source;
	winrt::com_ptr<IMFSourceReader> reader;
	UINT32 width=0;
	UINT32 height=0;
	UINT32 stride_bytes_to_next_line=0;
};

CameraInfo open_camera(const std::string& video_device_name) {
	// init com library, set concurrency of thread and create and bind new apartment
	// https://learn.microsoft.com/ja-jp/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex																
	HRESULT hr=CoInitializeEx(nullptr, COINIT_MULTITHREADED); 
	if (FAILED(hr) && hr!=RPC_E_CHANGED_MODE) return INVALID_CAMERA_INFO;
	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfstartup
	// init entire platform of Microsoft Media Foundation
	hr=MFStartup(MF_VERSION);
	if (FAILED(hr)) {
		// https://learn.microsoft.com/ja-jp/windows/win32/api/combaseapi/nf-combaseapi-couninitialize
		// close com library, unload .dll
		if (hr!=RPC_E_CHANGED_MODE) CoUninitialize(); 
		return INVALID_CAMERA_INFO;
	}

    NativeCamera *const camera=new NativeCamera();
    {
		// search video devices
        winrt::com_ptr<IMFAttributes> attributes;		// https://learn.microsoft.com/ja-jp/uwp/cpp-ref-for-winrt/com-ptr
        hr=MFCreateAttributes(attributes.put(), 1);	// https://learn.microsoft.com/ja-jp/windows/apps/develop/cpp-winrt/move-to-winrt-from-wrl
														// https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfcreateattributes
        if (FAILED(hr)) { delete camera; MFShutdown(); CoUninitialize(); return INVALID_CAMERA_INFO; } // https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfshutdown
        hr=attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-setguid
																														// Associates a GUID value with a key
        if (FAILED(hr)) { delete camera; MFShutdown(); CoUninitialize(); return INVALID_CAMERA_INFO; }
        UINT32 count=0;
        IMFActivate **devices=nullptr;
        hr=MFEnumDeviceSources(attributes.get(), &devices, &count);	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfidl/nf-mfidl-mfenumdevicesources
																		// should free all pointers in array and array of variable(devices)
		UINT32 video_id=0;
		for(;video_id<count; ++video_id){
			// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-getallocatedstring
			// https://learn.microsoft.com/ja-jp/windows/win32/medfound/mf-devsource-attribute-friendly-name
			LPWSTR dev_name=nullptr;
			UINT32 dev_name_len=0;
			devices[video_id]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &dev_name, &dev_name_len);
			if(dev_name==nullptr) continue;
			if(wcscmp(dev_name, to_wstring(video_device_name).c_str())==0) break;
		}

        if (FAILED(hr)) { delete camera; MFShutdown(); CoUninitialize(); return INVALID_CAMERA_INFO; }
		// get pointer to specified interface
        struct DeviceGuard {
            IMFActivate **const ptr;
            const UINT32 cnt;
            ~DeviceGuard() {
                if (ptr==nullptr) return;
				for (UINT32 i=0; i < cnt && ptr[i]; ++i)
					ptr[i]->Release();
				CoTaskMemFree(ptr);
            }
        } guard{ devices, count };
        if (video_id >= count) { delete camera; MFShutdown(); CoUninitialize(); return INVALID_CAMERA_INFO; }
		// "camera->source" receives a pointer to the requested interface.
        hr=devices[video_id]->ActivateObject(IID_PPV_ARGS(&camera->source));	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfactivate-activateobject
																				// https://learn.microsoft.com/ja-jp/windows/win32/api/combaseapi/nf-combaseapi-iid_ppv_args
        if (FAILED(hr)) { delete camera; MFShutdown(); CoUninitialize(); return INVALID_CAMERA_INFO; }
    }
	// create reader
    winrt::com_ptr<IMFAttributes> reader_attributes;
    hr=MFCreateAttributes(reader_attributes.put(), 1);
    if (SUCCEEDED(hr)) hr=reader_attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-setuint32
																									// set value to specified key
																									// MF_READWRITE_DISABLE_CONVERTERS: https://learn.microsoft.com/ja-jp/windows/win32/medfound/mf-readwrite-disable-converters
    if (SUCCEEDED(hr)) hr=MFCreateSourceReaderFromMediaSource(camera->source.get(), reader_attributes.get(), camera->reader.put());	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfreadwrite/nf-mfreadwrite-mfcreatesourcereaderfrommediasource
	// set media type of reader to specified media type
    winrt::com_ptr<IMFMediaType> type;
    if (SUCCEEDED(hr)) hr=MFCreateMediaType(type.put());	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfcreatemediatype
    if (SUCCEEDED(hr)) hr=type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-setguid
																				// https://learn.microsoft.com/ja-jp/windows/win32/medfound/mf-mt-major-type-attribute
    if (SUCCEEDED(hr)) hr=type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
	constexpr UINT32 WIDTH=640, HEIGHT=480;
    if (SUCCEEDED(hr)) hr=MFSetAttributeSize(type.get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT); // https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfsetattributesize
    if (SUCCEEDED(hr)) hr=camera->reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type.get()); // https://learn.microsoft.com/ja-jp/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-setcurrentmediatype
	// check actual media type
    winrt::com_ptr<IMFMediaType> actual_type;
    UINT32 actual_width=0, actual_height=0;
    if (SUCCEEDED(hr)) hr=camera->reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, actual_type.put()); // https://learn.microsoft.com/ja-jp/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-getcurrentmediatype
    if (SUCCEEDED(hr)) hr=MFGetAttributeSize(actual_type.get(), MF_MT_FRAME_SIZE, &actual_width, &actual_height); // https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfgetattributesize
	// set reader to stride
    UINT32 stride_bytes_to_next_line=0;
	constexpr std::size_t YUYV_PIXEL_BYTES=2;
    if (SUCCEEDED(hr) && SUCCEEDED(actual_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride_bytes_to_next_line)))	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-getuint32
																												// MF_MT_DEFAULT_STRIDE(In Bytes): https://learn.microsoft.com/ja-jp/windows/win32/medfound/mf-mt-default-stride-attribute
		camera->stride_bytes_to_next_line=stride_bytes_to_next_line;
    else camera->stride_bytes_to_next_line=actual_width * YUYV_PIXEL_BYTES;

    if (FAILED(hr) || actual_width==0 || actual_height==0) {
        camera->reader=nullptr; // https://stackoverflow.com/questions/50788733/c-winrts-com-ptr-reset
        camera->source=nullptr;
        delete camera;
        MFShutdown();
        CoUninitialize();
        return INVALID_CAMERA_INFO;
    }

    camera->width=actual_width;
    camera->height=actual_height;
    return {camera, nullptr, 0, camera->width, camera->height};
}

int close_camera(const CameraInfo *const camera_info) {
	if (camera_info==nullptr || camera_info->charactor_file_descriptor==nullptr) return EXIT_FAILURE;
	NativeCamera *const camera=static_cast<NativeCamera *const>(camera_info->charactor_file_descriptor);
	camera->reader=nullptr;
	camera->source->Shutdown();
	camera->source=nullptr;
	delete camera;

	MFShutdown();
	CoUninitialize();
	return EXIT_SUCCESS;
}

int process_next_frame(const CameraInfo *const camera_info, const std::function<void(const FrameBuffer *const)>& processer) {
	if (camera_info==nullptr || camera_info->charactor_file_descriptor==nullptr || processer==nullptr) return EXIT_FAILURE;

	const NativeCamera *const camera=static_cast<NativeCamera *>(camera_info->charactor_file_descriptor);
	while (true) {
		DWORD flags=0;
		winrt::com_ptr<IMFSample> sample;
		HRESULT hr=camera->reader->ReadSample( // https://learn.microsoft.com/ja-jp/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-readsample
			MF_SOURCE_READER_FIRST_VIDEO_STREAM,
										  0,
										  nullptr,
										  &flags,
										  nullptr,
										  sample.put());
		if (FAILED(hr)) return EXIT_FAILURE;
		if ((sample==nullptr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) continue;	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfreadwrite/ne-mfreadwrite-mf_source_reader_flag
																						// MF_SOURCE_READERF_ENDOFSTREAM: source reader reachs end of stream
		winrt::com_ptr<IMFMediaBuffer> buffer;
		if (FAILED(sample->ConvertToContiguousBuffer(buffer.put()))) continue; // https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfsample-converttocontiguousbuffer
	
		BYTE *source_data=nullptr;
		DWORD max_length=0, current_length=0;
		if (FAILED(buffer->Lock(&source_data, &max_length, &current_length))) continue; // https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfmediabuffer-lock
		
		const UINT32 required=camera->stride_bytes_to_next_line * camera->height;
		if (current_length < required) { buffer->Unlock(); continue; }

		const FrameBuffer frame_buffer{ ZXing::ImageFormat::Lum, camera->width, camera->height, source_data};
		processer(&frame_buffer);
		buffer->Unlock(); // https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfmediabuffer-unlock
		return EXIT_SUCCESS;
	}
}
#elif __linux__
#include <algorithm>
#include <cstddef>
#include <cstring>
#include "camera_handler.hpp"
#include <ZXing/ImageView.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <vector>

static int xioctl(int fd, unsigned long request, void *argp){
	int r;
	do {
		r=ioctl(fd, request, argp); // https://www.man7.org/linux/man-pages/man2/ioctl.2.html
	} while (r==-1 && errno==EINTR);	// https://man7.org/linux/man-pages/man7/signal.7.html
											// the details In "Interruption of system calls and library functions by signal handlers"
	return r;
}

// https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/capture.c.html
CameraInfo open_camera(const std::string& video_device_name){
	// searh the specifed charactor device
	constexpr char v4l2_dir[]="/sys/class/video4linux";
	if(std::filesystem::exists(v4l2_dir)==false) return INVALID_CAMERA_INFO;

	std::filesystem::path name_file;
	std::vector<std::filesystem::directory_entry> all_entries;
	for(const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(v4l2_dir)) all_entries.push_back(entry);
	std::sort(all_entries.begin(), all_entries.end());
	for(const std::filesystem::directory_entry& entry : all_entries){
		if(entry.is_directory()==false) continue;
		name_file=entry.path() / "name"; // /sys/class/video4linux/video*/name
		std::ifstream file_stream(name_file);
		std::string device_name;
		if(std::getline(file_stream, device_name))
			if(device_name==video_device_name)
				break;
	}
	const char *const dev_name=std::string("/dev/").append(name_file.generic_string().substr(sizeof(v4l2_dir), 6)).c_str();
	// open the charactor device
	struct stat st;
	if(stat(dev_name, &st)==-1 || !S_ISCHR(st.st_mode)) return INVALID_CAMERA_INFO; // https://ja.manpages.org/stat/2
	const int fd=open(dev_name, O_RDWR|O_NONBLOCK); // https://ja.manpages.org/open/2
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
	cropcap.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(fd, VIDIOC_CROPCAP, &cropcap)==0){
		struct v4l2_crop crop;
		crop.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c=cropcap.defrect;
		xioctl(fd, VIDIOC_S_CROP, &crop);
	}
	// set video format
	struct v4l2_format format;
	memset(&format, 0, sizeof(format));
	format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	constexpr std::size_t optimized_width=640, optimized_height=480;
	format.fmt.pix.width=optimized_width;
	format.fmt.pix.height=optimized_height;
	format.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;
	format.fmt.pix.field=V4L2_FIELD_ANY;
	if(xioctl(fd, VIDIOC_S_FMT, &format)==-1 ||
		format.fmt.pix.pixelformat!=V4L2_PIX_FMT_YUYV)
		return INVALID_CAMERA_INFO;
	// Buggy driver paranoia
	std::size_t min=format.fmt.pix.width * 2;
	if (format.fmt.pix.bytesperline < min) format.fmt.pix.bytesperline=min;
	min=format.fmt.pix.bytesperline * format.fmt.pix.height;
	if (format.fmt.pix.sizeimage < min) format.fmt.pix.sizeimage=min;
	// request kernel to allocate mapped-memory
	struct v4l2_requestbuffers request;
	memset(&request, 0, sizeof(request));
	request.count=4;
	request.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	request.memory=V4L2_MEMORY_MMAP;
	if(xioctl(fd, VIDIOC_REQBUFS, &request)==-1 || request.count < 2) return INVALID_CAMERA_INFO;
	// bind memory to allocated memory
	std::size_t n_buffers;
	Buffer *const buffers=(Buffer *const)calloc(request.count, sizeof(*buffers));
	if(buffers==NULL) return INVALID_CAMERA_INFO;
	for(n_buffers=0; n_buffers<request.count; ++n_buffers){
		struct v4l2_buffer video_buffer;
		memset(&video_buffer, 0, sizeof(video_buffer));
		video_buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video_buffer.memory=V4L2_MEMORY_MMAP;
		video_buffer.index=n_buffers;
		if(xioctl(fd, VIDIOC_QUERYBUF, &video_buffer)==-1) return INVALID_CAMERA_INFO;

		buffers[n_buffers].length=video_buffer.length;
		buffers[n_buffers].start=mmap(NULL, video_buffer.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, video_buffer.m.offset);
		if(buffers[n_buffers].start==MAP_FAILED) return INVALID_CAMERA_INFO;
	}
	// let camera stream image
	for(std::size_t i=0; i<n_buffers; ++i){
		struct v4l2_buffer video_buffer;
		memset(&video_buffer, 0, sizeof(video_buffer));
		video_buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video_buffer.memory=V4L2_MEMORY_MMAP;
		video_buffer.index=i;
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
		struct timeval tv={2, 0};
		const int r=select(camera_info->charactor_file_descriptor+1, &fds, NULL, NULL, &tv);
		if(r==-1) {
			if(errno==EINTR) continue;
			else return EXIT_FAILURE;
		} else if(r==0) return EXIT_FAILURE; // timeout of select
		// dequeue buffer
		struct v4l2_buffer video_buffer;
		memset(&video_buffer, 0, sizeof(video_buffer));
		video_buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video_buffer.memory=V4L2_MEMORY_MMAP;
		if(xioctl(camera_info->charactor_file_descriptor, VIDIOC_DQBUF, &video_buffer)==-1) { // https://www.man7.org/linux/man-pages/man3/errno.3.html
			if(errno==EAGAIN||errno==EIO) continue; // Resource temporarily unavailable or Input/output error
			else return EXIT_FAILURE;
		}
		// get pointer to dequeued buffer
		if(video_buffer.index>=camera_info->max_num_buffers) return EXIT_FAILURE;
		// process buffer
		const FrameBuffer frame_buffer={
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
#endif
