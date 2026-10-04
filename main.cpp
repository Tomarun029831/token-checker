#include "BarcodeFormat.h"
#include "ReaderOptions.h"
#include "Result.h"
#include "camera_handler.hpp"
#include <ZXing/ReadBarcode.h>
#include <ZXing/ImageView.h>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <fstream>

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

	size_t frame_index = 0;
	process_next_frame(&camera_info, [options, &frame_index](const FrameBuffer *const frame_buffer){
		char filename[64];
		std::snprintf(filename, sizeof(filename), "frame_%03zu.yuyv", frame_index++);
		
		std::ofstream file(filename, std::ios::binary);
		if(file) {
			// YUYVの総バイト数は (幅 × 高さ × 2バイト)
			const size_t total_bytes = frame_buffer->width * frame_buffer->height * 2;
			file.write(reinterpret_cast<const char*>(frame_buffer->data), total_bytes);
		}

		constexpr std::size_t YUYV_PIXEL_BYTES = 2, OFFSET_TO_NEXT_LUMINANCE = 2; // https://www.kernel.org/doc/html/v4.8/media/uapi/v4l/pixfmt-yuyv.html
		ZXing::ImageView image_view(
			frame_buffer->data,
			frame_buffer->width,
			frame_buffer->height,
			frame_buffer->format,
			frame_buffer->width * YUYV_PIXEL_BYTES,
			OFFSET_TO_NEXT_LUMINANCE);
		const ZXing::Results results = ZXing::ReadBarcodes(image_view, options);
		if(results.empty()) std::cout << "empty";
		for(const ZXing::Result r : results)
			std::cout << r.text() << '\n';
		std::cout.flush();
	});

	close_camera(&camera_info);
	return 0;
}

// int main() {
//     cv::VideoCapture cap(0);
//     if (!cap.isOpened()) {
//         std::cerr << "Err: In Init" << std::endl;
//         return -1;
//     }
//
//     cv::Mat frame;
//     while (true) {
//         cap >> frame;
//         if (frame.empty()) continue;
//
//         ZXing::ImageView image(
//             frame.data,
//             frame.cols,
//             frame.rows,
//             ZXing::ImageFormat::BGR
//         );
//
// 		std::cout << "==ReadBarcodes==" << std::endl;
// 		const ZXing::Results barcodes = ZXing::ReadBarcodes(image);
// 		auto i=0;
// 		for (const ZXing::Result& barcode : barcodes) {
// 			if (barcode.isValid()) {
// 				const ZXing::PointI tl = barcode.position().topLeft();
//
// 				std::cout << "[DETECTED] index=" << i++ << ", pos=(TL: " << tl.x << ", " << tl.y
// 						  << "), text= " << barcode.text() << std::endl;
// 			}
// 		}
//     }
//
//     return 0;
// }
