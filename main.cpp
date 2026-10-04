#include <chrono>
#include <iostream>
#include <ZXing/ReadBarcode.h>
#include <ZXing/ImageView.h>
#include "token_monitor.hpp"

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
	std::chrono::seconds token[3];
	token[0] = std::chrono::seconds(15);
	token[1] = std::chrono::seconds(10);
	token[2] = std::chrono::seconds(5);
	display_statuses(token, 3, &std::cout);

	return 1;
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
