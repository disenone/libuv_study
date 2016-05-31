OPTIONS = -std=c++11 -fpic -pthread -O2 -g -fno-strict-aliasing -flto=8 -fwrapv -Wall

all: epoll_echo_server fork_echo_server select_echo_server libuv_echo_server

libuv_echo_server: libuv_echo_server.cpp
	g++ $(OPTIONS) -luv libuv_echo_server.cpp -o libuv_echo_server 

epoll_echo_server: epoll_echo_server.cpp
	g++ $(OPTIONS) epoll_echo_server.cpp -o epoll_echo_server 

fork_echo_server: fork_echo_server.cpp
	g++ $(OPTIONS) fork_echo_server.cpp -o fork_echo_server 

select_echo_server: select_echo_server.cpp
	g++ $(OPTIONS) select_echo_server.cpp -o select_echo_server 

clean:
	rm -rf epoll_echo_server fork_echo_server select_echo_server libuv_echo_server
