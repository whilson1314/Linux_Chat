all: server client

server: server.cpp http_connnection.o bloom_filter.o
	g++ -std=c++11 -o server server.cpp http_connnection.o bloom_filter.o -L/usr/lib64/mysql -lmysqlclient -lpthread -lhiredis