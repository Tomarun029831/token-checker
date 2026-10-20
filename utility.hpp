#pragma once
#ifdef _WIN64
#include <string>
std::wstring to_wstring(const std::string& str);
#endif
