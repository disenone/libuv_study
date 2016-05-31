/*
 * epoll_echo_server.cpp 
 * 一个基于 epoll 的 echo server，客户端可以 telnet 上来，服务器返回跟客户端输入同样的内容给客户端
 */

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <vector>
#include <unordered_map>

using namespace std;

#define PORT "12321"	// 连接端口
#define BACKLOG 10		// 等待连接队列大小
#define ECHO_LEN 1024

/* 拿 ipv4 或者 ipv6 的 in_addr */
const void *get_sin_addr(const sockaddr_storage *ss)
{
	if (ss->ss_family == AF_INET)
		return &(((const sockaddr_in*)ss)->sin_addr);
	else
		return &(((const sockaddr_in6*)ss)->sin6_addr);
}

int make_sock()
{
	struct addrinfo hints, *server_addr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// ipv4 or ipv6
	hints.ai_socktype = SOCK_STREAM;	
	hints.ai_flags = AI_PASSIVE;		// use bind

	int ret = getaddrinfo(NULL, PORT, &hints, &server_addr);
	if (ret != 0)
	{
		cerr << "getaddrinfo ERROR: " << gai_strerror(ret) << endl; 
		exit(EXIT_FAILURE);
	}

	// 循环找可用的 addr
	int server_sock;
	struct addrinfo *p;
	for(p = server_addr; p != NULL; p = p->ai_next)
	{
		server_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (server_sock == -1)
		{
			perror("socket ERROR");
			continue;
		}

		int opt = 1;
		
		ret = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if (ret == -1)
		{
			perror("reuseaddr ERROR");
			exit(EXIT_FAILURE);
		}

		ret = bind(server_sock, p->ai_addr, p->ai_addrlen);
		if (ret == -1)
		{
			close(server_sock);
			perror("bind ERROR");
			continue;
		}
		break;
	}

	if (p == NULL)
	{
		cerr << "failed to make socket!" << endl;
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(server_addr);

	ret = listen(server_sock, BACKLOG);
	if (ret == -1)
	{
		perror("listen ERROR");
		exit(EXIT_FAILURE);
	}

	return server_sock;
}

void setnonblocking(int fd) 
{
	int flag = fcntl(fd, F_GETFL, 0);
	if (flag < 0) 
	{
		perror("fcntl F_GETFL ERROR");
		exit(EXIT_FAILURE);
	}
	if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) < 0) 
	{
		perror("fcntl F_SETFL ERROR");
		exit(EXIT_FAILURE);
	}
}

void add_sock(int epollfd, int sock)
{
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sock;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) == -1) 
	{
		perror("epoll_ctl ERROR");
		exit(EXIT_FAILURE);
	}
}

void del_sock(int epollfd, int sock)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, sock , NULL);
}

void main_loop(int server_sock)
{
	vector<epoll_event> events;

	int epollfd = epoll_create(1);
	if (epollfd == -1) 
	{
	   perror("epoll_create ERROR");
	   exit(EXIT_FAILURE);
	}

	add_sock(epollfd, server_sock);

	char buf[ECHO_LEN];
	char addr_str[INET6_ADDRSTRLEN];
	unordered_map<int, string> addr_map;
	addr_map.emplace(server_sock, "");

	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
	for (;;)
	{
		if (events.size() < addr_map.size())
			events.resize(addr_map.size());

		int nfds = epoll_wait(epollfd, events.data(), events.size(), -1);
		if (nfds == -1)
		{
			perror("epoll_wait ERROR");
			exit(EXIT_FAILURE);
		}

		for (int n = 0; n < nfds; ++n)
		{
			// server accept
			if (events[n].data.fd == server_sock)
			{
				int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
				if (client_sock == -1)
				{
					perror("accept ERROR");
					continue;
				}

				inet_ntop(client_addr.ss_family, get_sin_addr(&client_addr), addr_str, sizeof(addr_str));
				cout << "client from " << addr_str << endl;

				setnonblocking(client_sock);
				add_sock(epollfd, client_sock);
				addr_map.emplace(client_sock, addr_str);
			}

			// recv from client
			else if (events[n].events & EPOLLIN)
			{
				int sock = events[n].data.fd;
				int ret = recv(events[n].data.fd, buf, ECHO_LEN, 0);
				if (ret <= 0)
				{
					if (ret < 0)
						perror("recv ERROR");
					else
						cout << "client closed " << addr_map[sock] << endl;

					close(sock);
					del_sock(epollfd, sock);
					addr_map.erase(sock);
					continue;
				}
				send(sock, buf, ret, 0);
			}
		}

	}
}

int main()
{
	int server_sock = make_sock();
	cout << "wairting for clients..." << endl;
	main_loop(server_sock);
	return EXIT_SUCCESS;
}

