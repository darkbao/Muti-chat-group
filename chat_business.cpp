#include "chat_business.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <string>
using std::string;
using std::stoul;
namespace mj{
	int business::epollfd = 0;
	std::unordered_map<int, business::group_num> business::client_belong;
	std::vector<business::group> business::all_group(business::INIT_ALL_GROUP_SIZE);
	std::vector<business::group_num> business::available_num = available_group_init();
	locker business::resource;

	std::vector<business::group_num> business::available_group_init()
	{
		std::vector<group_num> tmp;
		for(int i=0; i<INIT_ALL_GROUP_SIZE; i++)
			tmp.push_back(i);
		return tmp;
	}

	void business::init(int socket, sockaddr_in ip_address)
	{
		fd = socket;
		client_address = ip_address;
		in_group = !(client_belong.find(fd) == client_belong.cend());
		g_num = in_group ? client_belong[fd] : 0;
	}

	void business::close_client()
	{
		if (!in_group)
		{
			printf("client do not belong to any group\n");
			/*do nothing*/
		}
		else
			quit_group();
	}

	int business::parse_command()
	{
		char buf[COMMAND_SIZE + 1];
		memset(buf, '\0', COMMAND_SIZE + 1);
		int count = 0;
		while (count != COMMAND_SIZE)
		{
			int ret = recv(fd, buf, COMMAND_SIZE - count, MSG_PEEK);
			if (ret == 0)
				break;
			else if (ret < 0)
			{
				if (errno == EWOULDBLOCK || errno == EAGAIN)
					break;
				else
				{
					fprintf(stderr, "recv error\n");
					exit(1);
				}
			}
			else
				count += ret;
		}
		if (count != COMMAND_SIZE)
			return TEXT;
		if (strcmp(buf, "#B")==0)
			return BUILD;
		else if (strcmp(buf, "#J")==0)
			return JOIN;
		else if (strcmp(buf, "#Q")==0)
			return QUIT;
		else
			return TEXT;
	}

	void business::process()
	{
		int status = parse_command();
		switch (status)
		{
		case BUILD:
			read_and_send(false);
			new_group();
			break;
		case JOIN:
		{
					 char buf[MAX_GTOUP_DIGIT_NUM + 1];
					 memset(buf, '\0', MAX_GTOUP_DIGIT_NUM + 1);
					 int count = 0;
					 while (count != MAX_GTOUP_DIGIT_NUM)
					 {
						 int ret = recv(fd, buf, MAX_GTOUP_DIGIT_NUM - count, MSG_PEEK);
						 if (ret == 0)
							 break;
						 else if (ret < 0)
						 {
							 if (errno == EWOULDBLOCK || errno == EAGAIN)
								 break;
							 else
							 {
								 fprintf(stderr, "recv error\n");
								 exit(1);
							 }
						 }
						 else
							 count += ret;
					 }
					 string s(buf+2);
					 try
					 {
						auto num = stoul(s);
						printf("num: %lu\n", num);
					 	join_group(num);
					 	read_and_send(false);	
					 }
					 catch(...)
					 {
						read_and_send(false);
						char join_error_msg[] = "this group does not exist\n";
						send_back(fd, static_cast<void *>(join_error_msg), strlen(join_error_msg));
					 }						
		}
			break;
		case QUIT:
		{
					 if (in_group)
					 {
						 quit_group();
						 read_and_send(false);
					 }
		}
			break;
		default:
		{
				   if (in_group)
					   read_and_send(true); 
				   else
				   {
					   char no_group_msg[] = "Sorry, you have not join a chat group yet\n"
											"Command are follow:\n"
											"Buile new chat group: #B\n"
											"Join a chat group: #J + group number\n"
											"Quit a group: #Q\n";
					   send_back(fd, static_cast<void *>(no_group_msg), strlen(no_group_msg));
					   read_and_send(false);
				   }
		}
			break;
		}
		reset_oneshot();
	}

	void business::read_and_send(bool send)
	{
		char buf[BUFFER_SIZE];
		memset(buf, '\0', BUFFER_SIZE);
		while (1)
		{
			int ret = recv(fd, buf, BUFFER_SIZE, 0);
			if (ret == 0)
				break;
			else if (ret < 0)
			{
				if (errno == EWOULDBLOCK || errno == EAGAIN)
					break;
				else
				{
					fprintf(stderr, "recv error\n");
					exit(1);
				}
			}
			else
			{
				printf("%.*s", ret, buf);
				if (send)
					send_to_group(buf, ret);
				if (ret != BUFFER_SIZE)
					break;
			}
		}
	}

	void business::reset_oneshot()
	{
		epoll_event event;
		event.data.fd = fd;
		event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
		epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
	}

	size_t business::send_back(int sockfd, void *buf, size_t n)
	{
		size_t count = 0;
		while (1)
		{
			int ret = send(sockfd, buf, n, 0);
			if (ret < 0)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					break;
				else
				{
					fprintf(stderr, "send error\n");
					exit(1);
				}
			}
			if (ret == n)
			{
				count = n;
				break;
			}
			else
			{
				auto buf_ch = static_cast<char *>(buf);
				buf_ch += ret;
				count += ret;
				buf = static_cast<void *>(buf_ch);
			}
		}
		return count;
	}

	void business::send_to_group(void *buf, size_t n)
	{
		for (auto it = all_group[g_num].cbegin(); it != all_group[g_num].cend(); ++it)
		{
			if (*it != fd)
				send_back(*it, buf, n);
		}
	}

	void business::quit_group()
	{
		resource.lock();
		all_group[g_num].erase(fd);
		client_belong.erase(fd);
		char quit_msg[] = "you have quit from group\n";
		send_back(fd, static_cast<void *>(quit_msg), strlen(quit_msg));
		printf("delete client from group: %lu\n", g_num);
		if (all_group[g_num].empty())
		{
			available_num.push_back(g_num);
			printf("recycle this group num\n");
		}
		resource.unlock();
	}

	void business::join_group(group_num other_group_num)
	{	
		if (other_group_num > all_group.size() - 1 || all_group[other_group_num].empty())
		{
			char join_error_msg[] = "this group does not exist\n";
			send_back(fd, static_cast<void *>(join_error_msg), strlen(join_error_msg));
		}
		else
		{	
			if (in_group)
				quit_group();
			resource.lock();
			all_group[other_group_num].insert(fd);
			client_belong[fd] = other_group_num;
			char build_group_msg[] = "you have joined a new group\n";
			send_back(fd, static_cast<void *>(build_group_msg), strlen(build_group_msg));
			resource.unlock();
		}	
	}

	business::group_num business::new_group()
	{
		if (in_group)
			quit_group();
		resource.lock();
		group_num new_g_num;
		if (!available_num.empty())
		{
			new_g_num = *available_num.crbegin();
			available_num.pop_back();
			all_group[new_g_num].insert(fd);	
		}
		else
		{
			all_group.push_back(group(fd));
			new_g_num = all_group.size() - 1;
		}
		client_belong[fd] = new_g_num;
		resource.unlock();
		char msg[] = "you have built a new group, group number is: ";
		char build_group_msg[strlen(msg) + MAX_GTOUP_DIGIT_NUM + 1];
		snprintf(build_group_msg, strlen(msg) + MAX_GTOUP_DIGIT_NUM + 1, "%s%lu\n", msg, new_g_num);
		send_back(fd, static_cast<void *>(build_group_msg), strlen(build_group_msg));
	}
}


