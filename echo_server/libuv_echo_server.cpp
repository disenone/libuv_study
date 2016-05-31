/*
 * libuv_echo_server.cpp 
 * 一个基于 libuv 的 echo server，客户端可以 telnet 上来，服务器返回跟客户端输入同样的内容给客户端
 */

#include <iostream>
#include <string>
#include <uv.h>

using namespace std;

#define PORT 12321		// 连接端口
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

string get_sock_addr(const uv_tcp_t *tcp_t)
{
	char addr_str[INET6_ADDRSTRLEN];
	sockaddr_storage client_addr;
	int size = sizeof(client_addr);
	uv_tcp_getsockname(tcp_t, (sockaddr*)&client_addr, &size);
	uv_inet_ntop(client_addr.ss_family, get_sin_addr(&client_addr), addr_str, sizeof(addr_str));
	return addr_str;
}

void on_close(uv_handle_t *client)
{
	delete client;
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = new char[suggested_size];
	buf->len = suggested_size;
}

void echo_write(uv_write_t *req, int status)
{
	if (status)
	{
		cerr << "write ERROR: " << uv_strerror(status) << endl;
	}

	delete req;
}

void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
{
	if (nread < 0)
	{
		if (nread != UV_EOF)
			cerr << "read ERROR: " << uv_strerror(nread) << endl;
		else
			cout << "client closed " << get_sock_addr((uv_tcp_t*)client) << endl;
		uv_close((uv_handle_t*)client, on_close);
	}

	else if (nread > 0)
	{
		uv_write_t *req = new uv_write_t;
		uv_buf_t wrbuf = uv_buf_init(buf->base, nread);
		uv_write(req, client, &wrbuf, 1, echo_write);
	}

	if (buf->base)
		delete buf->base;
}

void on_new_connection(uv_stream_t *server, int status)
{
	FAIL_EXIT(status, "on_new_connection ERROR");

	uv_tcp_t *client = new uv_tcp_t;
	uv_tcp_init(uv_default_loop(), client);

	if (uv_accept(server, (uv_stream_t*)client) == 0) 
	{
		cout << "client from " << get_sock_addr(client) << endl;

		uv_read_start((uv_stream_t*)client, alloc_buffer, echo_read);
	}
	else
	{
		uv_close((uv_handle_t*)client, on_close);
	}
}

int main()
{
	uv_loop_t *loop = uv_default_loop();

	uv_tcp_t server;
    uv_tcp_init(loop, &server);

	struct sockaddr_storage server_addr;

	int ret = uv_ip4_addr("0.0.0.0", PORT, (struct sockaddr_in*)&server_addr);
	FAIL_EXIT(ret, "uv_ip4_addr ERROR");

	ret = uv_tcp_bind(&server, (const struct sockaddr*)&server_addr, 0);
	FAIL_EXIT(ret, "uv_tcp_bind ERROR");

	ret = uv_listen((uv_stream_t*)&server, BACKLOG, on_new_connection);
	FAIL_EXIT(ret, "uv_tcp_listen ERROR");

	cout << "wairting for clients..." << endl;
	return uv_run(loop, UV_RUN_DEFAULT);
}
