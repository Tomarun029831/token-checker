#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <apriltag/apriltag.h>
#include <apriltag/tag25h9.h>
#include <apriltag/common/image_u8.h>
#include <vector>
#include "apriltag/common/image_types.h"
#include "apriltag/common/zarray.h"
#include "camera_handler.hpp"
#include "device.hpp"
#include "notifier.hpp"
#include "simde/arm/neon/ld2.h"
#include "simde/arm/neon/st1.h"
#include "simde/arm/neon/types.h"
#include "status_display.hpp"
#include "token.hpp"

#define SIMDE_ENABLE_NATIVE_ALIASES
#include <simde/arm/neon.h>

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
	const CameraInfo camera_info=open_camera(INTEGRATEDCAMERA_NAME);
	if(camera_info==INVALID_CAMERA_INFO) exit(EXIT_FAILURE);

	std::vector<uint8_t> dst_gray(camera_info.width * camera_info.height);
	apriltag_family_t *tf=tag25h9_create();
    apriltag_detector_t *td=apriltag_detector_create();
    apriltag_detector_add_family(td, tf);
	td->quad_decimate=2;
	td->nthreads=2;

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
		r=process_next_frame(&camera_info, [&checker_status, &token_status, &group_timer, &last, &dst_gray, &td](const FrameBuffer *const frame_buffer){
			// get image of camera and find qrcodes out with library
			// PERF: duplicate luminosity of image for apriltag lib
			constexpr std::size_t YUYV_PIXEL_BYTES=2, OFFSET_BYTES_TO_NEXT_LUMINANCE=2; // https://www.kernel.org/doc/html/v4.8/media/uapi/v4l/pixfmt-yuyv.html
			uint_fast64_t i=0;
			const int32_t pixel_nums=frame_buffer->height * frame_buffer->width;
			for(; i+15<pixel_nums; i+=16){
				simde_uint8x16x2_t yuyv_vec=simde_vld2q_u8(frame_buffer->data + OFFSET_BYTES_TO_NEXT_LUMINANCE*i);
				simde_vst1q_u8(dst_gray.data()+i, yuyv_vec.val[0]);
			}
			for(; i<pixel_nums; ++i) dst_gray[i]=frame_buffer->data[OFFSET_BYTES_TO_NEXT_LUMINANCE*i];
			// process img to find out apriltags
			image_u8_t apriltag_img={
				.width=frame_buffer->width,
				.height=frame_buffer->height,
				.stride=frame_buffer->width,
				.buf=dst_gray.data()
			};
			const zarray_t *detections=apriltag_detector_detect(td, &apriltag_img);

			// 0. update detected_bits
			// reset detected_bits and mask detected_bits with managing_bits
			checker_status=checker_status&0x0000'ffff'ffff'ffffULL | static_cast<std::uint64_t>(token_status)<<3*16;
			for(int_fast8_t i=0; i<zarray_size(detections); ++i){
				apriltag_detection_t *det;
				zarray_get(detections, i, &det);
				// index=-15 => static_cast<unsigned>(-15) == UINT32_MAX-14 < 16 (false)
				// index=17  => static_cast<unsigned>(17) < 16 (false)
				const unsigned index=det->id;
				const std::uint64_t state=0<=index&&index<16; // if(0>index || index>15) continue;
				// set each bit at index to detected_bits
				checker_status|=state<<(3*16+state*index); // https://en.algorithmica.org/hpc/pipelining/branchless/
				apriltag_detection_destroy(det);
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

	apriltag_detector_remove_family(td, tf);
	tag25h9_destroy(tf);
	apriltag_detector_destroy(td);
	close_camera(&camera_info);
	return 0;
}
