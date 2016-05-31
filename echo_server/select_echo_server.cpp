/*
 * select_echo_server.cpp 
 * 一个基于 select 的 echo server，客户端可以 telnet 上来，服务器返回跟客户端输入同样的内容给客户端
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
#include <unordered_map>

using namespace std;

#define PORT "12321"	// 连接端口
#define BACKLOG 10		// 等待连接队列大小
#define ECHO_LEN 1024

/* 拿 ipv4 或者 ipv6 的 in_addr */
const void *get_sin_addr(const sockaddr_storage *ss)
{
	if (ss->ss_family == AF_INET)
		return &(((const struct sockaddr_in*)ss)->sin_addr);
	else
		return &(((const struct sockaddr_in6*)ss)->sin6_addr);
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

void main_loop(int server_sock)
{
	fd_set all_sock;
	fd_set read_sock;
	unordered_map<int, string> sock_map;

	FD_SET(server_sock, &all_sock);
	sock_map.emplace(server_sock, "");

	int fd_max = server_sock;

	char buf[ECHO_LEN];
	char addr_str[INET6_ADDRSTRLEN];

	for(;;)
	{
		read_sock = all_sock;
		int ret = select(fd_max + 1, &read_sock, NULL, NULL, NULL);
		if (ret == -1)
		{
			perror("select ERROR");
			exit(EXIT_FAILURE);
		}

		for (auto it = sock_map.begin(); it != sock_map.end();)
		{
			int sock = it->first;
			if (!FD_ISSET(sock, &read_sock))
			{
				++it;
				continue;
			}

			// server accept
			if (sock == server_sock)
			{
				struct sockaddr_storage client_addr;
				socklen_t addr_size = sizeof(client_addr);

				int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
				if (client_sock == -1)
				{
					perror("accept ERROR");
					++it;
					continue;
				}

				inet_ntop(client_addr.ss_family, get_sin_addr(&client_addr), addr_str, sizeof(addr_str));
				cout << "client from " << addr_str << endl;

				FD_SET(client_sock, &all_sock);
				sock_map.emplace(client_sock, addr_str);
				fd_max = client_sock;
			}
			// recv from client
			else
			{
				ret = recv(sock, buf, ECHO_LEN, 0);
				if (ret <= 0)
				{
					if (ret < 0)
						perror("recv ERROR");
					else
						cout << "client closed " << it->second << endl;

					close(sock);
					FD_CLR(sock, &all_sock);
					it = sock_map.erase(it);
					continue;
				}
				send(sock, buf, ret, 0);
			}
			++it;
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

