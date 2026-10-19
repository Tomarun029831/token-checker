#pragma once
#include <cstdint>
#include <string>

std::string get_game_list_from_remote();
void notify_start(const uint16_t& first_group);
void notify_end(const uint16_t& end_bits);
