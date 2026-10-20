#ifdef _WIN64
#include <string>
#include <windows.h>
#include <winnls.h>
std::wstring to_wstring(const std::string& str) {
	if (str.empty()) return L"";
	const int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
	return wstr;
}
#endif
