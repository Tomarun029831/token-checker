#include <cstddef>
#include <cstdlib>
#include <iostream>
#include "camera_handler.hpp"
#include <ZXing/ReadBarcode.h>
#include <ZXing/ImageView.h>

// in windows
// # setup
// winget install usbipd pwsh
//
// in wsl
// # setup
// /mnt/c/Program\ Files/PowerShell/7/pwsh.exe -c 'Start-Process pwsh -Verb RunAs -ArgumentList "-Command", "usbipd bind --hardware-id 0408:30c1"' && /mnt/c/Program\ Files/PowerShell/7/pwsh.exe -c 'usbipd attach --wsl --hardware-id 0408:30c1'
// /mnt/c/Program\ Files/PowerShell/7/pwsh.exe -c 'Start-Process pwsh -Verb RunAs -ArgumentList "-Command", "usbipd bind --hardware-id 046d:0825"' && /mnt/c/Program\ Files/PowerShell/7/pwsh.exe -c 'usbipd attach --wsl --hardware-id 046d:0825'
// in wsl or linux
// sudo apt install libzxing-dev
// # build
// g++ -std=c++20 *.cpp -o token-checker $(pkg-config --cflags --libs zxing); ./token-checker
//
// in windows
// # reset
// usbipd detach --hardware-id 0408:30c1; pwsh -c 'Start-Process pwsh -Verb RunAs -ArgumentList "-Command", "usbipd unbind --hardware-id 0408:30c1"';
// usbipd detach --hardware-id 046d:0825; pwsh -c 'Start-Process pwsh -Verb RunAs -ArgumentList "-Command", "usbipd unbind --hardware-id 046d:0825"';
//
// # memo
// device name: C270 HD WEBCAM

int main(){
	const CameraInfo camera_info = open_camera("C270 HD WEBCAM");
	if(camera_info==INVALID_CAMERA_INFO) exit(EXIT_FAILURE);

    const ZXing::ReaderOptions options = ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::QRCode);
	for(int r=EXIT_SUCCESS; r!=EXIT_FAILURE;)
		r = process_next_frame(&camera_info, [options](const FrameBuffer *const frame_buffer){
			constexpr std::size_t YUYV_PIXEL_BYTES = 2, OFFSET_TO_NEXT_LUMINANCE = 2; // https://www.kernel.org/doc/html/v4.8/media/uapi/v4l/pixfmt-yuyv.html
			const ZXing::ImageView image_view(
				frame_buffer->data,
				frame_buffer->width,
				frame_buffer->height,
				frame_buffer->format,
				frame_buffer->width * YUYV_PIXEL_BYTES,
				OFFSET_TO_NEXT_LUMINANCE);
			const ZXing::Results results = ZXing::ReadBarcodes(image_view, options);
			if(results.empty()) return;
			std::cout << "amount=" << results.size() << '\n';
			for(const ZXing::Result r : results)
				std::cout << "text=" << r.text() << '\n';
			std::cout.flush();
		});

	close_camera(&camera_info);
	return 0;
}
