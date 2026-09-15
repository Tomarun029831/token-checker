#include <iostream>
#include <ZXing/ReadBarcode.h>
#include <ZXing/ImageView.h>
#include <opencv2/opencv.hpp>

// # setup
// sudo apt install libopencv-dev/stable
// sudo apt install libzxing-dev/stable
// # build
// g++ main.cpp -o token-checker $(pkg-config --cflags --libs opencv4 zxing); ./token-checker

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Err: In Init" << std::endl;
        return -1;
    }

    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) continue;

        ZXing::ImageView image(
            frame.data,
            frame.cols,
            frame.rows,
            ZXing::ImageFormat::BGR
        );

	std::cout << "==ReadBarcodes==" << std::endl;
        auto barcodes = ZXing::ReadBarcodes(image);
	auto i=0;
        for (const auto& barcode : barcodes) {
            if (barcode.isValid()) {
                const auto tl = barcode.position().topLeft();

                std::cout << "[DETECTED] index=" << i++ << ", pos=(TL: " << tl.x << ", " << tl.y
                          << "), text= " << barcode.text() << std::endl;
            }
        }
    }

    return 0;
}
