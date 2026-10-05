#include <cstddef>
#include <cstdlib>
#include <iostream>
#include "camera_handler.hpp"
#include <ZXing/ReadBarcode.h>
#include <ZXing/ImageView.h>

// in windows
// # setup
// winget install usbipd pwsh
// /mnt/c/Program\ Files/PowerShell/7/pwsh.exe -c 'Start-Process pwsh -Verb RunAs -ArgumentList "-Command", "usbipd bind --busid 1-3"' && /mnt/c/Program\ Files/PowerShell/7/pwsh.exe -c 'usbipd attach --wsl --busid 1-3'
//
// in wsl
// # setup
// sudo apt install libzxing-dev
// # build
// g++ *.cpp -o token-checker $(pkg-config --cflags --libs zxing); ./token-checker
//
// in windows
// # reset
// usbipd detach --busid 1-3; pwsh -c 'Start-Process pwsh -Verb RunAs -ArgumentList "-Command", "usbipd unbind --busid 1-3"';

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
