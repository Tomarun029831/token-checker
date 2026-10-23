#pragma once
#include <cstdint>
#include <string_view>

inline constexpr std::uint_fast8_t max_debounce_time_ms=15;
inline constexpr std::uint_fast8_t max_token_amount=11;

std::uint32_t parse_game_list(const std::string_view list);
