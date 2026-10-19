#include "requester.hpp"
#include ".env.hpp"
#include <cstddef>
#include <cstring>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#include <winnls.h>

inline static std::wstring to_wstring(const std::string& str) {
	if (str.empty()) return L"";
	const int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
	return wstr;
}

HttpResponse https_request_once(const char *const method,
								const char *const url,
								const char *const payload,
								const char *const proxy_hostname_and_port){
	// convert string of url to URL_COMPONENTS
	// https://learn.microsoft.com/ja-jp/windows/win32/api/winhttp/ns-winhttp-url_components
	const std::wstring wurl = to_wstring(url);
	URL_COMPONENTS url_comp = {0};
	url_comp.dwStructSize = sizeof(url_comp);
	url_comp.dwHostNameLength = (DWORD)-1;
	url_comp.dwUrlPathLength = (DWORD)-1;
	url_comp.dwExtraInfoLength = (DWORD)-1;
	if(WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.length(), 0, &url_comp)==false) return {false,0,nullptr};

    // Use WinHttpOpen to obtain a session handle.
	// https://learn.microsoft.com/ja-jp/windows/win32/wininet/appendix-a-hinternet-handles
	// https://learn.microsoft.com/ja-jp/windows/win32/api/winhttp/nf-winhttp-winhttpopen
	HINTERNET hSession=nullptr;
	if (proxy_hostname_and_port==NULL) hSession = WinHttpOpen(L"TokenChecker/1.0",
														   WINHTTP_ACCESS_TYPE_NO_PROXY,
														   WINHTTP_NO_PROXY_NAME,
														   WINHTTP_NO_PROXY_BYPASS, 0);
	else hSession = WinHttpOpen(L"TokenChecker/1.0",
							 WINHTTP_ACCESS_TYPE_NAMED_PROXY,
							 to_wstring(proxy_hostname_and_port).c_str(),
							 WINHTTP_NO_PROXY_BYPASS, 0);
	if (hSession==NULL) return {false,0,nullptr};
    // Specify an HTTP server.
	// https://learn.microsoft.com/ja-jp/windows/win32/api/winhttp/nf-winhttp-winhttpconnect
	std::wstring host_name(url_comp.lpszHostName, url_comp.dwHostNameLength);
	const HINTERNET hConnect=WinHttpConnect(hSession, host_name.c_str(), url_comp.nPort, 0);
	if (hConnect==NULL) { WinHttpCloseHandle(hSession); return {false,0,nullptr}; }
    // Create an HTTPs request handle.
	const DWORD request_flags = (url_comp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
	const std::wstring url_path(url_comp.lpszUrlPath, url_comp.dwUrlPathLength);
	HINTERNET hRequest = WinHttpOpenRequest(hConnect, to_wstring(method).c_str(), url_path.c_str(),
										 NULL, WINHTTP_NO_REFERER,
										 WINHTTP_DEFAULT_ACCEPT_TYPES,
										 request_flags);
	if (hRequest==NULL) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {false,0,nullptr}; }

	// enable_server_certificate_verification(false)
	// https://learn.microsoft.com/ja-jp/windows/win32/api/winhttp/nf-winhttp-winhttpsetoption
	// https://learn.microsoft.com/ja-jp/windows/win32/winhttp/option-flags
	if (request_flags==WINHTTP_FLAG_SECURE) {
		DWORD sec_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
						  SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
						  SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
		WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags));
	}
	// set headers
	std::wstring headers = L"Authorization: Bearer " + to_wstring(AUTH_ID) + L"\r\n";
	if (strcmp(method, "POST")==0) headers+=L"Content-Type: application/json\r\n";
	// send request
	// https://learn.microsoft.com/ja-jp/windows/win32/api/winhttp/nf-winhttp-winhttpsendrequest
	const bool is_success=payload==NULL ? WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) :
		WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(), (LPVOID)payload, (DWORD)std::strlen(payload), (DWORD)std::strlen(payload), 0);
	// https://learn.microsoft.com/ja-jp/windows/win32/api/winhttp/nf-winhttp-winhttpreceiveresponse
	HttpResponse result={false,0,""};
	if (is_success && WinHttpReceiveResponse(hRequest, NULL)) {
		// https://learn.microsoft.com/ja-jp/windows/win32/api/winhttp/nf-winhttp-winhttpqueryheaders
		// https://learn.microsoft.com/ja-jp/windows/win32/winhttp/query-info-flags
		DWORD status_code=0, size=sizeof(status_code), downloaded_bytes=0;
		result.success = WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
							WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size, WINHTTP_NO_HEADER_INDEX);
		result.status = status_code;
		do {
			size=0;
			// https://learn.microsoft.com/ja-jp/windows/win32/api/winhttp/nf-winhttp-winhttpquerydataavailable
			// https://learn.microsoft.com/ja-jp/windows/win32/api/winhttp/nf-winhttp-winhttpquerydataavailable
			if (WinHttpQueryDataAvailable(hRequest, &size)==false) break;
			if (size==0) break;
			std::vector<char> buffer(size+1);
			if (WinHttpReadData(hRequest, buffer.data(), size, &downloaded_bytes)) result.body.append(buffer.data(), downloaded_bytes);
		} while (size > 0);
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);
	return result;
}
