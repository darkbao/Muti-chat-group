/*
	An muti-online chat server
	using epoll + threadpool
	Event processing mode: reactor
	Concurrency: half sync/half async
    Modified on: 2017.1.7
 	     Author: darkbao
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <memory>
#include <unordered_map>
#include "chat_business.h"
#include "locker.h"
#include "threadpool.h"

using std::unordered_map;
using namespace mj;

const int MAX_EVENT_NUMBER = 10000;
const int MAX_SOCKETFD = 65536;
const int num_pthreads = 8;

int setnonblocking(int fd);
void addfd(int epollfd, int fd, bool oneshot);

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage:%s portnumber\a\n", argv[0]);
		exit(1);
	}
	int portnumber, listenfd;
	if ((portnumber = atoi(argv[1]))<0)
	{
		fprintf(stderr, "Usage:%s portnumber\a\n", argv[0]);
		exit(1);
	}
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		fprintf(stderr, "Socket error:%s\n\a", strerror(errno));
		exit(1);
	}
	struct sockaddr_in server_addr, client_addr;
	bzero(&server_addr, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(portnumber);

	if ((bind(listenfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr))) == -1)
	{
		fprintf(stderr, "Bind error:%s\n\a", strerror(errno));
		exit(1);
	}

	if (listen(listenfd, 5) == -1)
	{
		fprintf(stderr, "Listen error:%s\n\a", strerror(errno));
		exit(1);
	}

	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(MAX_EVENT_NUMBER);
	assert(epollfd != -1);
	addfd(epollfd, listenfd, false);
	auto pool = new threadpool<business>(num_pthreads,MAX_EVENT_NUMBER);
	business::epollfd = epollfd;
	unordered_map<int,business> business_map;//every buiness correspond to a sockfd(int)
	
	while (1)
	{
		int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

		if (ret<0)
		{
			fprintf(stderr, "epoll_wait error\n");
			break;
		}

		for (int i = 0; i < ret; ++i)
		{
			int sockfd = events[i].data.fd;
			if (sockfd == listenfd)
			{
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(client_address);
				int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
				if (connfd<0 && (errno != EWOULDBLOCK || errno != ECONNABORTED || errno != EINTR))
				{
					fprintf(stderr, "accept error:%s\n\a", strerror(errno));
					exit(1);
				}
				if (connfd<0)
					continue;
				addfd(epollfd, connfd, true);
				char no_group_msg[] = "Welcome to chat group\n"
										"Command are follow:\n"
										"Buile new chat group: #B\n"
										"Join a chat group: #J + group number\n"
										"Quit a group: #Q\n";	
				business_map[sockfd].init(sockfd);							   
				business_map[sockfd].send_back(connfd, static_cast<void *>(no_group_msg), strlen(no_group_msg));
				printf("new client coming.\n");
			}
			else if (events[i].events & EPOLLRDHUP)//client close event;
			{
				epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
				business_map[sockfd].init(sockfd);
				business_map[sockfd].close_client();
				close(sockfd);
				printf("client socketfd close\n");
			}

			else if (events[i].events & EPOLLIN)
			{
				business_map[sockfd].init(sockfd);
				pool->append(&business_map[sockfd]);//dispatch the business to working threadpool;
			}
			else
				fprintf(stderr, "epoll_wait error\n");
		}
	}
}

int setnonblocking(int fd)
{
	int old_opt = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, old_opt | O_NONBLOCK);
	return old_opt;
}

void addfd(int epollfd, int fd, bool oneshot)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	if (oneshot)
		event.events |= EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}



