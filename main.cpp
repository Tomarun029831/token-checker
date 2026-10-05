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

int main(){
	const CameraInfo camera_info = open_camera(0);
	if(camera_info==INVALID_CAMERA_INFO) exit(EXIT_FAILURE);

    const ZXing::ReaderOptions options = ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::QRCode);
	process_next_frame(&camera_info, [options](const FrameBuffer *const frame_buffer){
		constexpr std::size_t YUYV_PIXEL_BYTES = 2, OFFSET_TO_NEXT_LUMINANCE = 2; // https://www.kernel.org/doc/html/v4.8/media/uapi/v4l/pixfmt-yuyv.html
		ZXing::ImageView image_view(
			frame_buffer->data,
			frame_buffer->width,
			frame_buffer->height,
			frame_buffer->format,
			frame_buffer->width * YUYV_PIXEL_BYTES,
			OFFSET_TO_NEXT_LUMINANCE);
		const ZXing::Results results = ZXing::ReadBarcodes(image_view, options);
		if(results.empty()) return;

		std::cout << "index=" << results.size() << '\n';
		std::cout.flush();
	});

	close_camera(&camera_info);
	return 0;
}
