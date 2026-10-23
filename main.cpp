#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <ZXing/ReadBarcode.h>
#include "ZXing/CharacterSet.h"
#include "camera_handler.hpp"
#include "device.hpp"
#include "notifier.hpp"
#include "status_display.hpp"
#include "token.hpp"

// == setup for windows
// in windows
// # setup
// vcpkg new --application; vcpkg add port nu-book-zxing-cpp cpp-httplib openssl
// # build
// rm -rf build; cmake --preset=default; cmake --build build --config Release
// # run
// .\build\Release\token-checker.exe
//
// == setup for linux ==
// in windows
// # setup
// winget install usbipd pwsh
//
// in wsl
// # setup
// /mnt/c/Program\ Files/PowerShell/7/pwsh.exe -c 'Start-Process pwsh -Verb RunAs -ArgumentList "-Command", "usbipd bind --hardware-id 0408:30c1"' && /mnt/c/Program\ Files/PowerShell/7/pwsh.exe -c 'usbipd attach --wsl --hardware-id 0408:30c1'
// /mnt/c/Program\ Files/PowerShell/7/pwsh.exe -c 'Start-Process pwsh -Verb RunAs -ArgumentList "-Command", "usbipd bind --hardware-id 046d:0825"' && /mnt/c/Program\ Files/PowerShell/7/pwsh.exe -c 'usbipd attach --wsl --hardware-id 046d:0825'
// in wsl or linux
// sudo apt install libzxing-dev libcurl4-openssl-dev
// sudo nmcli connection add \
//   type wifi \
//   con-name "Enterprise-WiFi" \
//   ifname wlan0 \
//   ssid "SSID_NAME" \
//   wifi-sec.key-mgmt wpa-eap \
//   802-1x.eap peap \
//   802-1x.phase2-auth mschapv2 \
//   802-1x.identity "YOUR_USERNAME" \
//   802-1x.password "YOUR_PASSWORD"
// sudo nmcli connection modify "Enterprise-WiFi" connection.autoconnect yes
// sudo nmcli connection up "Enterprise-WiFi"
// # build
// g++ -std=c++20 *.cpp -o token-checker $(pkg-config --cflags --libs zxing libcurl); ./token-checker
//
// in windows
// # reset
// usbipd detach --hardware-id 0408:30c1; pwsh -c 'Start-Process pwsh -Verb RunAs -ArgumentList "-Command", "usbipd unbind --hardware-id 0408:30c1"';
// usbipd detach --hardware-id 046d:0825; pwsh -c 'Start-Process pwsh -Verb RunAs -ArgumentList "-Command", "usbipd unbind --hardware-id 046d:0825"';

int main(){
	const CameraInfo camera_info=open_camera(WEBCAMERA_NAME);
	if(camera_info==INVALID_CAMERA_INFO) exit(EXIT_FAILURE);

    const ZXing::ReaderOptions options=ZXing::ReaderOptions().
		setFormats(ZXing::BarcodeFormat::QRCode).
		setTryRotate(true).
		setTryHarder(true).
		setTryDownscale(false).
		setMaxNumberOfSymbols(max_token_amount). // https://zxing-cpp.github.io/zxing-cpp/docs/class_z_xing_1_1_reader_options.html#a2f461f08095adb84286de31fe12845b1
		setCharacterSet(ZXing::CharacterSet::ASCII);

	// 'bit is 0' means token is added, 'bit is 1' means token is removed
	// 8bytes: | detected_bits	|	last_group	|	middle_group	|	first_group	| (each bits is 2bytes)
	std::uint64_t checker_status=UINT64_MAX;
	// 'bit is 0' means token is added, 'bit is 1' means token is removed
	// 4bytes: | waiting_bits	|	managing_bits	| (each bits is 2bytes)
	std::uint32_t token_status=parse_game_list(get_game_list_from_remote());
	// 2bytes: | reserved		|	last_timer(10 ~ 14sec)	|	middle_timer(5 ~ 9sec)	|	first_timer(0 ~ 4sec)	| (length of each bits is 4 bits)
	std::uint16_t group_timer=0xe'9'4;
	std::chrono::time_point<std::chrono::steady_clock> last=std::chrono::steady_clock::now();

	for(int r=EXIT_SUCCESS; r!=EXIT_FAILURE;)
		r=process_next_frame(&camera_info, [&options, &checker_status, &token_status, &group_timer, &last](const FrameBuffer *const frame_buffer){
			// get image of camera and find qrcodes out with library
			constexpr std::size_t YUYV_PIXEL_BYTES=2, OFFSET_BYTES_TO_NEXT_LUMINANCE=2; // https://www.kernel.org/doc/html/v4.8/media/uapi/v4l/pixfmt-yuyv.html
			const ZXing::ImageView image_view(
				frame_buffer->data, frame_buffer->width, frame_buffer->height,
				frame_buffer->format, frame_buffer->width * YUYV_PIXEL_BYTES, OFFSET_BYTES_TO_NEXT_LUMINANCE);
			const ZXing::Results results=ZXing::ReadBarcodes(image_view, options);
			// 0. update detected_bits
			// reset detected_bits and mask detected_bits with managing_bits
			checker_status=checker_status&0x0000'ffff'ffff'ffffULL | static_cast<std::uint64_t>(token_status)<<3*16;
			for(const ZXing::Result& r : results){
				// index=-15 => static_cast<unsigned>(-15) == UINT32_MAX-14 < 16 (false)
				// index=17  => static_cast<unsigned>(17) < 16 (false)
				const unsigned index=static_cast<const unsigned>(10 * (r.bytes()[0] - '0') + (r.bytes()[1] - '0'));
				const std::uint64_t state=index<16; // if(0>index || index>15) continue;
				// set each bit at index to detected_bits
				checker_status|=state<<(3*16+state*index); // https://en.algorithmica.org/hpc/pipelining/branchless/
			}
			// 1. remove detected token from each group
			// 0001'0001'0001 * 1010=1010'1010'1010
			checker_status|=0x0000'0001'0001'0001ULL*(checker_status>>3*16);
			// 3. add left tokens to last_group
			// ~(~X xor Y)=X xor Y, (X xor Y) xor Z=X xor Y xor Z  より、
			// ~(~(~(checker_status>>16^checker_status<<16)^checker_status<<2*16)^static_cast<std::uint64_t>(token_status)<<16)
			//=~(checker_status>>16^checker_status<<16^checker_status<<2*16^static_cast<std::uint64_t>(token_status)<<16)
			checker_status=0xffff'0000'ffff'ffffULL&checker_status |
				0x0000'ffff'0000'0000ULL&~(checker_status>>16^checker_status<<16^checker_status<<2*16^static_cast<std::uint64_t>(token_status)<<16); // 2. remove duplicate tokens of middle_group, first_group and waiting_bits from detected_bits
			// 4. update each timer
			const std::chrono::time_point<std::chrono::steady_clock> now=std::chrono::steady_clock::now();
			if(now-last>=std::chrono::seconds(1)){
				// PERF: should be removed
				std::cout << "\x1b[H\x1b[J";
				display_group_timer(group_timer);
				display_checker_status(checker_status);
				display_token_status(token_status);
				if(group_timer==0xa'5'0){
					group_timer=0xf'0'0|group_timer>>4; // set 0xf to last_timer because all timers have less time later.
					// 5. notify tokens started
					notify_start(static_cast<uint16_t>(checker_status));
					// 6. add tokens in first_group to waiting_bits
					token_status&=checker_status<<16|0x0000'ffffU;
					// 7. slide checker_status to drop out first_group from checker_status
					checker_status=checker_status&0xffff'0000'0000'0000ULL | checker_status>>16;
				}
				group_timer-=0x111;
				last=now;
			}
			// 8. find out tokens and notify these to api
			// 'bit is 0' means it should be sent
			const std::uint16_t end_bits=~(checker_status>>3*16)|token_status>>16;
			notify_end(end_bits);

			// 9. remove sent tokens from waiting_bits
			token_status|=static_cast<const std::uint32_t>(~end_bits)<<16;

			std::cout.flush(); // PERF: should be removed
		});

	close_camera(&camera_info);
	return 0;
}
