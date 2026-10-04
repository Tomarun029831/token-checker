#pragma once
#include <chrono>
#include <cstddef>
#include <ostream>

void display_statuses(const std::chrono::seconds *const tokens, const std::size_t max_token_amount, std::ostream *const out);
