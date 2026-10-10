#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <ZXing/ReadBarcode.h>
#include <ZXing/ImageView.h>
#include "camera_handler.hpp"

// in windows
// # setup
// vcpkg new --application; vcpkg add port nu-book-zxing-cpp
// # build
// rm -rf build; cmake --preset=default; cmake --build build --config Release
// # run
// .\build\Release\token-checker.exe
//
// # memo
// device name: Logi C270 HD WebCam, Integrated Camera

int main(){
	// const CameraInfo camera_info = open_camera(L"Logi C270 HD WebCam");
	const CameraInfo camera_info=open_camera("Integrated Camera");
	if(camera_info==INVALID_CAMERA_INFO) exit(EXIT_FAILURE);

    const ZXing::ReaderOptions options=ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::QRCode);
	for(int r=EXIT_SUCCESS; r!=EXIT_FAILURE;)
		r=process_next_frame(&camera_info, [options](const FrameBuffer *const frame_buffer){
			constexpr std::size_t YUYV_PIXEL_BYTES=2, OFFSET_BYTES_TO_NEXT_LUMINANCE=2; // https://www.kernel.org/doc/html/v4.8/media/uapi/v4l/pixfmt-yuyv.html
			const ZXing::ImageView image_view(
				frame_buffer->data,
				frame_buffer->width,
				frame_buffer->height,
				frame_buffer->format,
				frame_buffer->width * YUYV_PIXEL_BYTES,
				OFFSET_BYTES_TO_NEXT_LUMINANCE);
			const ZXing::Results results=ZXing::ReadBarcodes(image_view, options);
			if(results.empty()) return;

			std::cout << "\x1b[H\x1b[Jamount=" << results.size() << '\n';
			for(const ZXing::Result r : results) std::cout << "text=" << r.text() << '\n';
			std::cout.flush();
		});

	close_camera(&camera_info);
	return 0;
}
