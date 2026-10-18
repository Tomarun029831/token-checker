#include "notifier.hpp"
#include ".env.hpp"
#include "token.hpp"
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

void get_game_list(){
	httplib::Client client(API_URL);
	const httplib::Headers headers{{"Authorization", "Bearer " AUTH_ID}};
	if(const httplib::Result res = client.Get(PATH_TO_LIST, headers)){
		std::cout << "in get_game_list(): status=" << res->status << ", body=" << res->body << std::endl;
	}
}

// [{"gameId":1,"status":"in_use"}]
static std::string create_payload(const uint16_t& bits, const char *status){
	std::string payload;
	payload.reserve(256);
	payload+='[';
	for(uint_fast8_t i=0, count=0; i<max_token_amount; ++i){
		if(bits>>i&1) continue;
		if(count!=0) payload+=',';
		payload+="{\"gameId\":";
		payload+=std::to_string(i + 1);
		payload+=",\"status\":\"";
		payload+=status;
		payload+="\"}";
		++count;
	}
	payload+=']';
	return payload;
}

// 'bit is 0' means token is added, 'bit is 1' means token is removed
// 8bytes: | detected_bits	|	last_group	|	middle_group	|	first_group	| (each bits is 2bytes)
void notify_start(const uint16_t& first_group){
	if(first_group==0xffff) return;
	std::thread t([first_group]{
		std::cout << "notify_start called" << "\n";
		// create request
		httplib::Client client(API_URL);
		const httplib::Headers headers{{"Authorization", "Bearer " AUTH_ID}};
		const std::string payload=create_payload(first_group, "in_use");
		if(const httplib::Result res = client.Post(PATH_TO_BATCH, headers, payload, "application/json")){
			std::cout << "payload=" << payload << '\n';
			std::cout << "in notify_start(): status=" << res->status << ", body=" << res->body << std::endl;
		}
	});
	t.detach(); // HACK: use actor-pattern https://ja.wikipedia.org/wiki/%E3%82%A2%E3%82%AF%E3%82%BF%E3%83%BC%E3%83%A2%E3%83%87%E3%83%AB
}

void notify_end(const uint16_t& end_bits){
	if(end_bits==0xffff) return;
	std::thread t([end_bits]{
		std::cout << "notify_end called" << "\n";
		// create request
		httplib::Client client(API_URL);
		const httplib::Headers headers{{"Authorization", "Bearer " AUTH_ID}};
		const std::string payload=create_payload(end_bits, "available");
		if(const httplib::Result res = client.Post(PATH_TO_BATCH, headers, payload, "application/json")){
			std::cout << "payload=" << payload << '\n';
			std::cout << "in notify_start(): status=" << res->status << ", body=" << res->body << std::endl;
		}
	});
	t.detach(); // HACK: use actor-pattern https://ja.wikipedia.org/wiki/%E3%82%A2%E3%82%AF%E3%82%BF%E3%83%BC%E3%83%A2%E3%83%87%E3%83%AB
}
