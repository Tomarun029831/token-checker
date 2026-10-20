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
