#include "notifier.hpp"
#include ".env.hpp"
#include "requester.hpp"
#include "token.hpp"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

static std::mutex mutex;
static std::condition_variable_any cv;
static std::queue<std::function<void (void)>> tasks;
static std::jthread worker([](std::stop_token stoken){
	while(stoken.stop_requested()==false){
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(mutex);
			const bool is_notified=cv.wait(lock, stoken, []{ return tasks.empty()==false; });
			if(is_notified==false && tasks.empty()) break;

			task=std::move(tasks.front());
			tasks.pop();
		}
		task();
	}
});
void push_task(std::function<void (void)> task){
	{
		std::lock_guard<std::mutex> lock(mutex);
		tasks.push(std::move(task));
	}
	cv.notify_one();
}

// in get_game_list(): status=200, body=[{"gameId":1,"name":"例示ゲームA","status":"available","expectedEndAt":null},{"gameId":2,"name":"例示ゲームB","status":"available","expec tedEndAt":null},{"gameId":3,"name":"例示ゲームC","status":"available","expectedEndAt":null}]
void get_game_list(){
	push_task([]{
		const HttpResponse res = https_request_once("GET", API_URL PATH_TO_LIST, NULL, PROXY_HOSTNAME ":" PROXY_PORT);
		if(res.success) std::cout << "in get_game_list(): status=" << res.status << ", body=" << res.body << std::endl;
		else {
			const HttpResponse res = https_request_once("GET", API_URL PATH_TO_LIST, NULL, NULL);
			if(res.success) std::cout << "in get_game_list(): status=" << res.status << ", body=" << res.body << std::endl;
		}
	});
}

// [{"gameId":1,"status":"in_use"}]
static std::string create_payload(const uint16_t& bits, const char *const status){
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
	push_task([first_group]{
		std::cout << "notify_start called" << "\n";
		// create request
		const std::string payload=create_payload(first_group, "in_use");
		const HttpResponse res = https_request_once("POST", API_URL PATH_TO_BATCH, payload.c_str(), PROXY_HOSTNAME ":" PROXY_PORT);
		if(res.success){
			std::cout << "payload=" << payload << '\n';
			std::cout << "in notify_start(): status=" << res.status << ", body=" << res.body  << std::endl;
		}else{
			const HttpResponse res = https_request_once("POST", API_URL PATH_TO_BATCH, payload.c_str(), NULL);
			if(res.success){
				std::cout << "payload=" << payload << '\n';
				std::cout << "in notify_start(): status=" << res.status << ", body=" << res.body  << std::endl;
			}
		}
	});
}

void notify_end(const uint16_t& end_bits){
	if(end_bits==0xffff) return;
	push_task([end_bits]{
		std::cout << "notify_end called" << "\n";
		// create request
		const std::string payload=create_payload(end_bits, "available");
		const HttpResponse res = https_request_once("POST", API_URL PATH_TO_BATCH, payload.c_str(), PROXY_HOSTNAME ":" PROXY_PORT);
		if(res.success){
			std::cout << "payload=" << payload << '\n';
			std::cout << "in notify_end(): status=" << res.status << ", body=" << res.body << std::endl;
		}else{
			const HttpResponse res = https_request_once("POST", API_URL PATH_TO_BATCH, payload.c_str(), NULL);
			if(res.success){
				std::cout << "payload=" << payload << '\n';
				std::cout << "in notify_end(): status=" << res.status << ", body=" << res.body  << std::endl;
			}
		}
	});
}
