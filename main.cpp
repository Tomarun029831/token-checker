#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <ZXing/ReadBarcode.h>
#include "camera_handler.hpp"
#include "status_display.hpp"

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
	// 'bit is 0' means token is added, 'bit is 1' means token is removed
	// 8bytes: | detected_bits	|	last_group	|	middle_group	|	first_group	| (each bits is 2bytes)
	std::uint64_t checker_status=UINT64_MAX;
	// 'bit is 0' means token is added, 'bit is 1' means token is removed
	// 4bytes: | waiting_bits	|	managing_bits	| (each bits is 2bytes)
	std::uint32_t token_status=UINT32_C(0b1111111111111111'1111000000000000);
	// 2bytes: | reserved		|	last_timer(10 ~ 14sec)	|	middle_timer(5 ~ 9sec)	|	first_timer(0 ~ 4sec)	| (length of each bits is 4 bits)
	std::uint16_t group_timer=UINT16_C(0xe'9'4);
	std::chrono::time_point<std::chrono::steady_clock> last = std::chrono::steady_clock::now();

	for(int r=EXIT_SUCCESS; r!=EXIT_FAILURE;)
		r=process_next_frame(&camera_info, [&options, &checker_status, &token_status, &group_timer, &last](const FrameBuffer *const frame_buffer){
			// std::cout << "\x1b[H\x1b[J";

			constexpr std::size_t YUYV_PIXEL_BYTES=2, OFFSET_BYTES_TO_NEXT_LUMINANCE=2; // https://www.kernel.org/doc/html/v4.8/media/uapi/v4l/pixfmt-yuyv.html
			const ZXing::ImageView image_view(
				frame_buffer->data, frame_buffer->width, frame_buffer->height,
				frame_buffer->format, frame_buffer->width * YUYV_PIXEL_BYTES, OFFSET_BYTES_TO_NEXT_LUMINANCE);
			const ZXing::Barcodes results=ZXing::ReadBarcodes(image_view, options);

			std::cout << "amount=" << results.size() << '\n';
			display_group_timer(group_timer);
			display_checker_status(checker_status);
			display_token_status(token_status);
			// 0. update detected_bits
			checker_status=checker_status&UINT64_C(0x0000'ffff'ffff'ffff) | static_cast<const std::uint64_t>(token_status)<<3*16; // reset detected_bits and mask detected_bits with managing_bits
			for(const ZXing::Result& r : results){
				const int index=std::atoi(r.text().c_str());
				checker_status|=(static_cast<const std::uint64_t>(1)<<(3*16+index)); // set each bit at index to detected_bits
			}
			// 1. remove detected token from each group
			checker_status|=UINT64_C(0x0000'0001'0001'0001)*(checker_status>>3*16);
			// 3. add left tokens to last_group
			checker_status=checker_status&UINT64_C(0xffff'0000'ffff'ffff) |
				~(~(checker_status>>16^checker_status<<16)^checker_status<<2*16)&UINT64_C(0x0000'ffff'0000'0000); // 2. remove duplicate tokens of middle_group and first_group from detected_bits
			// 4. update each timer
			if(std::chrono::steady_clock::now()-last>=std::chrono::seconds(1)){
				if(group_timer==UINT16_C(0xa'5'0)){
					group_timer=UINT16_C(0xf'0'0)|group_timer>>4; // set 0xf to last_timer because all timers have less time later.
					// HACK: 5. send https request
					for(std::uint_fast8_t i=0; i<16; ++i){
						if(checker_status>>i&1) continue;
						// TODO: create payload to notify begin of game
						std::cout << "begin index=" << i << '\n';
					}
					// TODO: send https request

					// 6. add tokens in first_group to waiting_bits
					token_status&=checker_status<<16|UINT32_C(0x0000'ffff);
					// 7. slide first_group from checker_status
					checker_status=checker_status&UINT64_C(0xffff'0000'0000'0000) | checker_status>>16;
				}
				group_timer-=UINT16_C(0x111);
				last=std::chrono::steady_clock::now();
			}
			// 8. find out tokens that should be sent to api
			const std::uint16_t end_bits=~(checker_status>>3*16)|token_status>>16;
			// for(std::uint_fast8_t i=0; i<16; ++i){
			// 	if(end_bits>>i&1) continue;
			// 	// TODO: create payload to notify end of game
			// 	std::cout << "end index=" << i << '\n';
			// }
			// // TODO: send https request
			// std::cout.flush();

			// 9. remove sent tokens from waiting_bits
			token_status|=static_cast<const std::uint32_t>(~end_bits)<<16;

			std::cout.flush();
		});

	close_camera(&camera_info);
	return 0;
}
