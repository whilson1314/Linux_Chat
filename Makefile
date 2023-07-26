all: server client

server: server.cpp http_connnection.o bloom_filter.o
	g++ -std=c++11 -o server server.cpp http_connnection.o bloom_filter.o -L/usr/lib64/mysql -lmysqlclient -lpthread -lhiredis
	
http_connection.o: http_connection.cpp
	g++ -std=c++11 -c http_connection.cpp -L/usr/lib64/mysql -lmysqlclient -lpthread -lhiredis

bloom_filter.o: bloom_filter.cpp
	g++ -std=c++11 -c bloom_filter.cpp -L/usr/lib64/mysql -lmysqlclient -lpthread -lhiredis

client: client.cpp
	g++ -std=c++11 -o client client.cpp -L/usr/lib64/mysql -lpthread

clean: 
	rm -rf server
	rm -rf client
	rm -rf *.o
	rm -rf *.txt