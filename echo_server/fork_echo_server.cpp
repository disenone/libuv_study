/*
 * fork_echo_server.cpp 
 * 一个基于 fork 的 echo server，客户端可以 telnet 上来，服务器返回跟客户端输入同样的内容给客户端
 */

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

#define PORT "12321"	// 连接端口
#define BACKLOG 10		// 等待连接队列大小
#define ECHO_LEN 1024

void wait_child(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void set_child_handler()
{
	// handle child
	struct sigaction sa;
	sa.sa_handler = wait_child;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	int ret = sigaction(SIGCHLD, &sa, NULL);
	if (ret == -1)
	{
		perror("sigaction ERROR");
		exit(EXIT_FAILURE);
	}

}

/* 拿 ipv4 或者 ipv6 的 in_addr */
const void *get_sin_addr(const sockaddr_storage *ss)
{
	if (ss->ss_family == AF_INET)
		return &(((const struct sockaddr_in*)ss)->sin_addr);
	else
		return &(((const struct sockaddr_in6*)ss)->sin6_addr);
}

/* 返回跟客户端输入同样的内容给客户端 */
void echos(int client_sock)
{
	int ret;
	char buf[ECHO_LEN];
	while ((ret = recv(client_sock, buf, ECHO_LEN, 0)) > 0)
	{
		send(client_sock, buf, ret, 0);
	}
}

void main_loop(int server_sock)
{
	char addr_str[INET6_ADDRSTRLEN];

	// 主循环，接受 client 并 fork 处理
	for(;;)
	{
		struct sockaddr_storage client_addr;
		socklen_t addr_size = sizeof(client_addr);

		int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
		if (client_sock == -1)
		{
			perror("accept ERROR");
			continue;
		}

		inet_ntop(client_addr.ss_family, get_sin_addr(&client_addr), addr_str, sizeof(addr_str));
		cout << "client from " << addr_str << endl;

		// child
		if (fork() == 0)
		{
			close(server_sock);
			echos(client_sock);
			close(client_sock);
			cout << "client closed " << addr_str << endl;
			break;
		}
		// parent
		else
		{
			close(client_sock);
		}
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

int main()
{
	int server_sock = make_sock();
	set_child_handler();

	cout << "wairting for clients..." << endl;

	main_loop(server_sock);

	return EXIT_SUCCESS;
}

