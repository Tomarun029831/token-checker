#include <cstdlib>
#include <cwchar>
#include <string>
#include "camera_handler.hpp"

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

constexpr UINT32 WIDTH = 640;
constexpr UINT32 HEIGHT = 480;

struct NativeCamera {
	winrt::com_ptr<IMFMediaSource> source;
	winrt::com_ptr<IMFSourceReader> reader;
	UINT32 width = 0;
	UINT32 height = 0;
	UINT32 stride_bytes_to_next_line = 0;
};

inline static std::wstring utf8_to_wide(const std::string& utf8){
	const int wchars_num = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0); // https://learn.microsoft.com/ja-jp/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
	if(wchars_num==0) return L"";
	std::wstring wstr(wchars_num, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wstr.data(), wchars_num);
	return wstr;
}

CameraInfo open_camera(const std::string& video_device_name) {
	// init com library, set concurrency of thread and create and bind new apartment
	// https://learn.microsoft.com/ja-jp/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex																
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); 
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return INVALID_CAMERA_INFO;
	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfstartup
	// init entire platform of Microsoft Media Foundation
	hr = MFStartup(MF_VERSION);
	if (FAILED(hr)) {
		// https://learn.microsoft.com/ja-jp/windows/win32/api/combaseapi/nf-combaseapi-couninitialize
		// close com library, unload .dll
		if (hr != RPC_E_CHANGED_MODE) CoUninitialize(); 
		return INVALID_CAMERA_INFO;
	}

    NativeCamera *const camera = new NativeCamera();
    {
		// search video devices
        winrt::com_ptr<IMFAttributes> attributes;		// https://learn.microsoft.com/ja-jp/uwp/cpp-ref-for-winrt/com-ptr
        hr = MFCreateAttributes(attributes.put(), 1);	// https://learn.microsoft.com/ja-jp/windows/apps/develop/cpp-winrt/move-to-winrt-from-wrl
														// https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfcreateattributes
        if (FAILED(hr)) { delete camera; MFShutdown(); CoUninitialize(); return INVALID_CAMERA_INFO; } // https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfshutdown
        hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-setguid
																														// Associates a GUID value with a key
        if (FAILED(hr)) { delete camera; MFShutdown(); CoUninitialize(); return INVALID_CAMERA_INFO; }
        UINT32 count = 0;
        IMFActivate **devices = nullptr;
        hr = MFEnumDeviceSources(attributes.get(), &devices, &count);	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfidl/nf-mfidl-mfenumdevicesources
																		// should free all pointers in array and array of variable(devices)
		UINT32 video_id=0;
		for(;video_id<count; ++video_id){
			// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-getallocatedstring
			// https://learn.microsoft.com/ja-jp/windows/win32/medfound/mf-devsource-attribute-friendly-name
			LPWSTR dev_name = nullptr;
			UINT32 dev_name_len = 0;
			devices[video_id]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &dev_name, &dev_name_len);
			if(dev_name==nullptr) continue;
			if(wcscmp(dev_name, utf8_to_wide(video_device_name).c_str())==0) break;
		}

        if (FAILED(hr)) { delete camera; MFShutdown(); CoUninitialize(); return INVALID_CAMERA_INFO; }
		// get pointer to specified interface
        struct DeviceGuard {
            IMFActivate **const ptr;
            const UINT32 cnt;
            ~DeviceGuard() {
                if (ptr==nullptr) return;
				for (UINT32 i = 0; i < cnt && ptr[i]; ++i)
					ptr[i]->Release();
				CoTaskMemFree(ptr);
            }
        } guard{ devices, count };
        if (video_id >= count) { delete camera; MFShutdown(); CoUninitialize(); return INVALID_CAMERA_INFO; }
		// "camera->source" receives a pointer to the requested interface.
        hr = devices[video_id]->ActivateObject(IID_PPV_ARGS(&camera->source));	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfactivate-activateobject
																				// https://learn.microsoft.com/ja-jp/windows/win32/api/combaseapi/nf-combaseapi-iid_ppv_args
        if (FAILED(hr)) { delete camera; MFShutdown(); CoUninitialize(); return INVALID_CAMERA_INFO; }
    }
	// create reader
    winrt::com_ptr<IMFAttributes> reader_attributes;
    hr = MFCreateAttributes(reader_attributes.put(), 1);
    if (SUCCEEDED(hr)) hr = reader_attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-setuint32
																									// set value to specified key
																									// MF_READWRITE_DISABLE_CONVERTERS: https://learn.microsoft.com/ja-jp/windows/win32/medfound/mf-readwrite-disable-converters
    if (SUCCEEDED(hr)) hr = MFCreateSourceReaderFromMediaSource(camera->source.get(), reader_attributes.get(), camera->reader.put());	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfreadwrite/nf-mfreadwrite-mfcreatesourcereaderfrommediasource
	// set media type of reader to specified media type
    winrt::com_ptr<IMFMediaType> type;
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(type.put());	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfcreatemediatype
    if (SUCCEEDED(hr)) hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-setguid
																				// https://learn.microsoft.com/ja-jp/windows/win32/medfound/mf-mt-major-type-attribute
    if (SUCCEEDED(hr)) hr = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
    if (SUCCEEDED(hr)) hr = MFSetAttributeSize(type.get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT); // https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfsetattributesize
    if (SUCCEEDED(hr)) hr = camera->reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type.get()); // https://learn.microsoft.com/ja-jp/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-setcurrentmediatype
	// check actual media type
    winrt::com_ptr<IMFMediaType> actual_type;
    UINT32 actual_width = 0, actual_height = 0;
    if (SUCCEEDED(hr)) hr = camera->reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, actual_type.put()); // https://learn.microsoft.com/ja-jp/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-getcurrentmediatype
    if (SUCCEEDED(hr)) hr = MFGetAttributeSize(actual_type.get(), MF_MT_FRAME_SIZE, &actual_width, &actual_height); // https://learn.microsoft.com/ja-jp/windows/win32/api/mfapi/nf-mfapi-mfgetattributesize
	// set reader to stride
    UINT32 stride_bytes_to_next_line = 0;
	constexpr std::size_t YUYV_PIXEL_BYTES = 2;
    if (SUCCEEDED(hr) && SUCCEEDED(actual_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride_bytes_to_next_line)))	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfattributes-getuint32
																												// MF_MT_DEFAULT_STRIDE(In Bytes): https://learn.microsoft.com/ja-jp/windows/win32/medfound/mf-mt-default-stride-attribute
		camera->stride_bytes_to_next_line = stride_bytes_to_next_line;
    else camera->stride_bytes_to_next_line = actual_width * YUYV_PIXEL_BYTES;

    if (FAILED(hr) || actual_width == 0 || actual_height == 0) {
        camera->reader = nullptr; // https://stackoverflow.com/questions/50788733/c-winrts-com-ptr-reset
        camera->source = nullptr;
        delete camera;
        MFShutdown();
        CoUninitialize();
        return INVALID_CAMERA_INFO;
    }

    camera->width = actual_width;
    camera->height = actual_height;
    return {camera, nullptr, 0, camera->width, camera->height};
}

int close_camera(const CameraInfo *const camera_info) {
	if (camera_info==nullptr || camera_info->charactor_file_descriptor==nullptr) return EXIT_FAILURE;
	NativeCamera *const camera = static_cast<NativeCamera *const>(camera_info->charactor_file_descriptor);
	camera->reader = nullptr;
	camera->source->Shutdown();
	camera->source = nullptr;
	delete camera;

	MFShutdown();
	CoUninitialize();
	return EXIT_SUCCESS;
}

int process_next_frame(const CameraInfo *const camera_info, const std::function<void(const FrameBuffer *const)>& processer) {
	if (camera_info==nullptr || camera_info->charactor_file_descriptor==nullptr || processer==nullptr) return EXIT_FAILURE;

	const NativeCamera *const camera = static_cast<NativeCamera *>(camera_info->charactor_file_descriptor);
	while (true) {
		DWORD flags = 0;
		winrt::com_ptr<IMFSample> sample;
		HRESULT hr = camera->reader->ReadSample( // https://learn.microsoft.com/ja-jp/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-readsample
			MF_SOURCE_READER_FIRST_VIDEO_STREAM,
										  0,
										  nullptr,
										  &flags,
										  nullptr,
										  sample.put());
		if (FAILED(hr)) return EXIT_FAILURE;
		if ((sample == nullptr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) continue;	// https://learn.microsoft.com/ja-jp/windows/win32/api/mfreadwrite/ne-mfreadwrite-mf_source_reader_flag
																						// MF_SOURCE_READERF_ENDOFSTREAM: source reader reachs end of stream
		winrt::com_ptr<IMFMediaBuffer> buffer;
		if (FAILED(sample->ConvertToContiguousBuffer(buffer.put()))) continue; // https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfsample-converttocontiguousbuffer
	
		BYTE *source_data = nullptr;
		DWORD max_length = 0, current_length = 0;
		if (FAILED(buffer->Lock(&source_data, &max_length, &current_length))) continue; // https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfmediabuffer-lock
		
		const UINT32 required = camera->stride_bytes_to_next_line * camera->height;
		if (current_length < required) { buffer->Unlock(); continue; }

		const FrameBuffer frame_buffer{ ZXing::ImageFormat::Lum, camera->width, camera->height, source_data};
		processer(&frame_buffer);
		buffer->Unlock(); // https://learn.microsoft.com/ja-jp/windows/win32/api/mfobjects/nf-mfobjects-imfmediabuffer-unlock
		return EXIT_SUCCESS;
	}
}
