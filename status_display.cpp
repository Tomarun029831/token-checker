#include "status_display.hpp"
#include <bitset>
#include <cstdint>
#include <iostream>
#include <string_view>

// 8bytes: | detected_bits	|	last_group	|	middle_group	|	first_group	| (each bits is 2bytes)
void display_checker_status(const std::uint64_t& checker_status){
	constexpr std::string_view labels[4] = {"first_group:\t", "middle_group:\t", "last_group:\t", "detected_bits:\t"};
	for(std::uint_fast8_t base=0; base<4; ++base)
		std::cout << labels[base] << std::bitset<sizeof(std::uint16_t)*CHAR_BIT>(checker_status>>base*sizeof(std::uint16_t)*CHAR_BIT) << '\n';
	std::cout.flush();
}

// 4bytes: | waiting_bits	|	managing_bits	| (each bits is 2bytes)
void display_token_status(const std::uint32_t& token_status){
	constexpr std::string_view labels[2] = {"managing_bits:\t", "waiting_bits:\t"};
	for(std::uint_fast8_t base=0; base<2; ++base)
		std::cout << labels[base] << std::bitset<sizeof(std::uint16_t)*CHAR_BIT>(token_status>>base*sizeof(std::uint16_t)*CHAR_BIT) << '\n';
	std::cout.flush();
}

// 2bytes: | reserved		|	last_timer(10~14sec)	|	middle_timer(5~9sec)	|	first_timer(0~4sec)	| (length of each bits is 4 bits)
void display_group_timer(const std::uint16_t& group_timer){
	constexpr std::string_view labels[3] = {"first_timer:\t", "middle_timer:\t", "last_timer:\t"};
	for(std::uint_fast8_t base=0; base<3; ++base)
		std::cout << labels[base] << (UINT16_C(0xf)&group_timer>>base*sizeof(uint16_t)*CHAR_BIT/4) << '\n';
	std::cout.flush();
}
