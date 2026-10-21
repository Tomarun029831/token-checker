#include <charconv>
#include <cstdint>
#include <string_view>

// [{"gameId":0,"name":"例示ゲームA","status":"available","expectedEndAt":null},{"gameId":2,"name":"例示ゲームB","status":"available","expec tedEndAt":null},{"gameId":3,"name":"例示ゲームC","status":"available","expectedEndAt":null}]
std::uint32_t parse_game_list(const std::string_view list) {
	// 'bit is 0' means token is added, 'bit is 1' means token is removed
	// 4bytes: | waiting_bits	|	managing_bits	| (each bits is 2bytes)
	std::uint32_t token_status=UINT32_MAX;

	size_t pos=0;
	while (true) {
		pos=list.find("\"gameId\":", pos);
		if(pos==std::string_view::npos) break;
		pos+=9; // strlen("\"gameId\":")

		// extract value of game id
		std::uint16_t game_id=0;
		std::from_chars_result res=std::from_chars(list.data()+pos, list.data()+list.size(), game_id);
		if (res.ec!=std::errc{} || game_id<0 || 15<game_id) {
			pos=(res.ptr == nullptr) ? pos+1 : static_cast<size_t>(res.ptr-list.data());
			continue;
		}

		// extract value of status
		size_t status_begin=list.find("\"status\":\"", pos);
		if (status_begin==std::string_view::npos) break;
		status_begin+=10; // strlen("\"status\":\"")
		size_t status_end=list.find('"', status_begin);
		if (status_end==std::string_view::npos) break;

		const std::uint32_t bit_shift=game_id;
		const std::string_view status=list.substr(status_begin, status_end-status_begin);
		// add token to managing_bits
		token_status &= ~(1U << bit_shift);
		// add token to waiting_bits
		if (status=="in_use") token_status &= ~(1U << (bit_shift + 16));

		pos=status_end;
	}
	return token_status;
}
