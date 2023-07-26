all: test_client add_count

test_client: test_client.cpp
	g++ -std=c++11 -o test test_client.cpp

add_count: add_count.cpp
	g++ -std=c++11 -o add add_count.cpp -L/usr/lib64/mysql -lmysqlclient

clean: 
	rm -rf test_client
	rm -rf add_count
	rm -rf *.o
