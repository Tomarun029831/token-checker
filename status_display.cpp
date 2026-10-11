#include "status_display.hpp"
#include <cstdint>
#include <iostream>
#include <string_view>

void display_checker_status(const std::uint64_t checker_status){
	constexpr std::string_view labels[4] = {"first_group:	", "middle_group:	", "last_group:	", "detected_bits:	"};
	for(std::uint_fast8_t base=0; base<4; ++base){
		std::cout << labels[base];
		for(std::int_fast8_t offset=15; offset>=0; --offset)
			std::cout << (checker_status>>(base*16+offset)&1);
		std::cout << '\n';
	}
	std::cout.flush();
}

void display_token_status(const std::uint32_t token_status){

}

void display_timer(const std::uint16_t group_timer){
	constexpr std::string_view labels[3] = {"first_timer:	", "middle_timer:	", "last_timer:	"};
	for(std::uint_fast8_t base=0; base<3; ++base){
		std::cout << labels[base] << (UINT16_C(0xf)&group_timer>>base*4) << '\n';
	}
	std::cout.flush();
}
