/*
 * libev_echo_server.cpp 
 * 一个基于 libev 的 echo server，客户端可以 telnet 上来，服务器返回跟客户端输入同样的内容给客户端
 */

#include <iostream>
#include <string>
#include <ev.h>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

using namespace std;

#define PORT "12321"	// 连接端口
#define BACKLOG 10		// 等待连接队列大小
#define ECHO_LEN 1024

// error handling
#define FAIL_EXIT(ret, msg)										\
do																\
{																\
	if ((ret) < 0)												\
	{															\
		cerr << (msg) << ": " << uv_strerror(ret) << endl;		\
		exit(EXIT_FAILURE);										\
	}															\
}																\
while(0)

/* 拿 ipv4 或者 ipv6 的 in_addr */
const void *get_sin_addr(const sockaddr_storage *ss)
{
	if (ss->ss_family == AF_INET)
		return &(((const sockaddr_in*)ss)->sin_addr);
	else
		return &(((const sockaddr_in6*)ss)->sin6_addr);
}

string get_sock_addr(int sock)
{
	char addr_str[INET6_ADDRSTRLEN];
	sockaddr_storage client_addr;
	socklen_t size = sizeof(client_addr);
	getpeername(sock, (sockaddr*)&client_addr, &size);
	inet_ntop(client_addr.ss_family, get_sin_addr(&client_addr), addr_str, sizeof(addr_str));
	return addr_str;
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

void echo_read(EV_P_ struct ev_io *w, int revents)
{
	char buf[ECHO_LEN];
	int ret = recv(w->fd, buf, ECHO_LEN, 0);
	if (ret <= 0)
	{
		if (ret < 0)
			perror("recv ERROR");
		else
			cout << "client closed " << get_sock_addr(w->fd) << endl;

		close(w->fd);
		ev_io_stop(EV_A_ w);
		delete w;
	}
	send(w->fd, buf, ret, 0);
}

void on_new_connection(EV_P_ struct ev_io *w, int revents)
{
	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
	int client_sock = accept(w->fd, (struct sockaddr *)&client_addr, &addr_size);
	if (client_sock == -1)
	{
		perror("accept ERROR");
		return;
	}

	cout << "client from " << get_sock_addr(client_sock) << endl;

	setnonblocking(client_sock);

	ev_io* ev_client = new ev_io;
	ev_io_init(ev_client, echo_read, client_sock, EV_READ);
	ev_io_start(loop, ev_client);
}

int main()
{
	struct ev_loop *loop = EV_DEFAULT;

	int server_sock = make_sock();

	ev_io ev_server;
	ev_io_init(&ev_server, on_new_connection, server_sock, EV_READ);
	ev_io_start(loop, &ev_server);

	cout << "wairting for clients..." << endl;
	return ev_run(loop, 0);
}

