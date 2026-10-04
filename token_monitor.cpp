#include "token_monitor.hpp"

// [index][debounce time[sec]]
// example:
// 0015 // meaning: index=00,debouncetime=15
// 0110 // meaning: index=01,debouncetime=10
static inline constexpr std::size_t max_length_token_status_bar=4;
static void write_status_bar(const size_t index, const std::chrono::seconds *const token, char *const out){
	*out='0'+index/10;
	*(out+1)='0'+index%10;
	*(out+2)='0'+token->count()/10; // https://learn.microsoft.com/ja-jp/cpp/standard-library/duration-class?view=msvc-170#count
	*(out+3)='0'+token->count()%10;
	*(out+4)='\0';
}

void display_statuses(const std::chrono::seconds *const tokens, const std::size_t max_token_amount, std::ostream *const out){
	char bar_buf[max_length_token_status_bar+1];
	for(size_t i=0; i<max_token_amount; ++i){
		write_status_bar(i, tokens+i, bar_buf);
		*out << bar_buf << '\n';
	}
	out->flush();
}
