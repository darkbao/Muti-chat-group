#ifndef CHAT_BUSINESS_H_
#define CHAT_BUSINESS_H_

#include <netinet/in.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "locker.h"

namespace mj{
	class business
	{
		typedef std::unordered_set<int> group;
		typedef std::vector<group>::size_type group_num;

	public:
		business() = default;
		~business() = default;	
		void init(int sockfd, sockaddr_in ip_address = {});//init business with a client sockfd
		void close_client();
		size_t send_back(int sockfd, void *buf, size_t n);
		void process();

	public:
		static int epollfd;

	private:
		static std::vector<business::group_num> available_group_init();
		void reset_oneshot();	
		void send_to_group(void *buf, size_t n);
		void read_and_send(bool send);
		void quit_group();
		void join_group(group_num other_group);
		int parse_command();
		group_num new_group();

	private:	
		enum { BUFFER_SIZE = 4096, INIT_ALL_GROUP_SIZE = 5,
			COMMAND_SIZE = 2, MAX_GTOUP_DIGIT_NUM = 20};
		enum command{ BUILD, JOIN, QUIT, TEXT };
		
		static std::unordered_map<int, group_num> client_belong;
		static std::vector<group> all_group;
		static std::vector<group_num> available_num;
		static locker resource;
		
		int fd;
		sockaddr_in client_address; 
		bool in_group;
		group_num g_num;
	};
}

#endif
