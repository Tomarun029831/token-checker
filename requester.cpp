#ifdef _WIN64
#include "requester.hpp"
#include ".env.hpp"
#include "utility.hpp"
#include <cstddef>
#include <cstring>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#include <winnls.h>

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
#elif __linux__
#include "requester.hpp"
#include ".env.hpp"
#include <cstddef>
#include <cstring>
#include <string>
#include <curl/curl.h>

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
	const size_t total_size=size * nmemb;
	std::string *const body=static_cast<std::string*>(userp);
	body->append(static_cast<const char*>(contents), total_size);
	return total_size;
}

HttpResponse https_request_once(const char *const method,
								const char *const url,
								const char *const payload,
								const char *const proxy_hostname_and_port) {
	// https://curl.se/libcurl/c/curl_easy_init.html
	CURL *const curl=curl_easy_init();
	if (curl==NULL) return {false,0,""};
	// https://curl.se/libcurl/c/curl_easy_setopt.html
	// set option
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "TokenChecker/1.0");
	// set proxy
	if (proxy_hostname_and_port != NULL && std::strlen(proxy_hostname_and_port) > 0) curl_easy_setopt(curl, CURLOPT_PROXY, proxy_hostname_and_port);
	// set payload
	if (method == NULL) return {false,0,""};
	if (std::strcmp(method, "POST")==0) {
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		if (payload == nullptr) curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
		else {
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(std::strlen(payload)));
		}
	} else curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
	// disable ssl verification
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	// set headers
	const char *const bearer="Authorization: Bearer " AUTH_ID;
	struct curl_slist *headers=NULL;
	headers=curl_slist_append(headers, bearer);
	if (std::strcmp(method, "POST")==0) headers=curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	// set callback function to save body of response
	HttpResponse result={false,-1,""};
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);

	// https://curl.se/libcurl/c/curl_easy_perform.html
	// send request
	const CURLcode res=curl_easy_perform(curl);
	if (res==CURLE_OK) {
		long status_code=0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
		result.status=static_cast<decltype(result.status)>(status_code);
		result.success=true;
	}

	// free memory
	// https://curl.se/libcurl/c/curl_easy_cleanup.html
	// https://curl.se/libcurl/c/curl_slist_free_all.html
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	return result;
}
#endif
