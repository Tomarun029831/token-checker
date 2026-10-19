#pragma once
#include <string>

struct HttpResponse {
	bool success;
	long status;
	std::string body;
};

// method = "GET" or "POST"  # method could not be NULL
// url = "something" # url could not be NULL
// payload = "something" or NULL
// proxy_url_and_port = "hostname:port" or NULL
HttpResponse https_request_once(const char *const method,
								const char *const url,
								const char *const payload,
								const char *const proxy_hostname_and_port);
