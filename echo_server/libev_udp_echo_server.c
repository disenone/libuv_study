#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <ev.h>
#include "dbg.h"

// echo_server based on libev and udp
//
#define PORT "12321"	// ���Ӷ˿�
#define ECHO_LEN	1025
#define NI_MAXHOST  1025
#define NI_MAXSERV	32

int make_sock()
{
	struct addrinfo hints, *server_addr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// ipv4 or ipv6
	hints.ai_socktype = SOCK_DGRAM;	
	hints.ai_flags = AI_PASSIVE;

	int ret = getaddrinfo(NULL, PORT, &hints, &server_addr);
	check(ret == 0, "getaddrinfo ERROR: %s", gai_strerror(ret));

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
			continue;
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

	check(p != NULL, "failed to make socket!");

	freeaddrinfo(server_addr);

	return server_sock;

error:
	exit(EXIT_FAILURE);
}

void echo_read(EV_P_ struct ev_io *w, int revents)
{
	char buf[ECHO_LEN];
	int ret = recv(w->fd, buf, ECHO_LEN - 1, 0);

	check(ret > 0, "recv error");
	buf[ret] = '\0';

	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
	getpeername(w->fd, (struct sockaddr*)&client_addr, &addr_size);
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
		sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
	check(ret == 0, "getnameinfo");

	printf("recv client [%s:%s] : %s\n", hbuf, sbuf, buf);

	ret = send(w->fd, buf, strlen(buf), 0);
	check(ret > 0, "send");

error:
	return;
}

void accept_client(EV_P_ struct ev_io *w, int revents)
{
	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
	char buf[ECHO_LEN];
	int ret = recvfrom(w->fd, buf, ECHO_LEN - 1, 0, (struct sockaddr *)&client_addr, &addr_size);
	check(ret > 0, "recvfrom error");
	buf[ret] = '\0';

	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
		sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
	check(ret == 0, "getnameinfo");

	printf("recvfrom client [%s:%s] : %s\n", hbuf, sbuf, buf);

	// ����һ���µ�socket�����ӵ������Ŀͻ��ˣ��������socket�Ϳ���ר��������clientͨ��
	int new_sock = make_sock();
	ret = connect(new_sock, (struct sockaddr *)&client_addr, addr_size);
	check(ret == 0, "connect client");

	ret = send(new_sock, buf, strlen(buf), 0);
	check(ret > 0, "send");

	ev_io* ev_server = (ev_io*)malloc(sizeof(ev_io));
	ev_io_init(ev_server, echo_read, new_sock, EV_READ);
	ev_io_start(loop, ev_server);

error:
	return;
}

int main()
{
	struct ev_loop *loop = EV_DEFAULT;

	int server_sock = make_sock();

	ev_io ev_server;
	ev_io_init(&ev_server, accept_client, server_sock, EV_READ);
	ev_io_start(loop, &ev_server);

	printf("wairting for clients...\n");
	return ev_run(loop, 0);
}

