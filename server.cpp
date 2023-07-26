#include <iostream>
#include <cstring>
#include <sys/epoll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include "util.hpp"
#include "thread_pool.hpp"
#include "locker.hpp"
#include "http_connection.hpp"
#include "mysql_connection.hpp"

#define MAX_FD 65535 //最大文件描述符个数
#define MAX_EVENT_NUM 20000 //最大监听事件数量

int main(int argc, char* argv[])
{
    int port = 8045;
    if(argc > 1)
    {
        port = atoi(argv[1]);
    }
    
    //对断开连接的SIGPIPE信号进行忽略处理，否则会把程序终止
    Util::addSig(SIGPIPE, SIG_IGN);

    //创建线程池，调用Thread_pool模板类的构造函数
    ThreadPool<HttpConnection>* thread_pool = NULL;
    try
    {
        thread_pool = new ThreadPool<HttpConnection>(10, 10000);
    }
    catch(...)
    {
        std::cout << "Server: create threadpool fail!" << std::endl;
        exit(-1);
    }
    
    //创建连接池，用于保存已连接客户端信息
    HttpConnection* conns_pool = HttpConnection::initConnPool(MAX_FD);

    //初始化布隆过滤器（静态成员变量，BloomFilter类型）
    HttpConnection::m_bloom_filter.init();

    //读取数据库user表的数据，将用户名哈希到布隆过滤器的位图
    HttpConnection::mysql_g.init();
    HttpConnection::mysql_g.connect();
    MysqlConnection mysql_conn;
    auto mysql_ret = mysql_conn.query("SELECT * FROM user");
    for (auto v : mysql_ret)
    {
        //v[0]是name字段
        HttpConnection::m_bloom_filter.add(v[0]);
    }

    //创建监听的套接字
    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);

    //设置端口复用
    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //绑定服务器ip地址，服务器只能从ip为此地址的网卡读取消息
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int ret = bind(listen_fd, (struct sockaddr*) &addr, sizeof(addr));
    if (ret == -1)
    {
        std::cout << "Bind fail!" << std::endl;
    }

    //监听，操作系统会分配全连接和半连接队列
    ret = listen(listen_fd, 30);
    if (ret == -1)
    {
        std::cout << "Listen fail!" << std::endl;
    }

    //创建一个epoll实例（在内核中创建了一个数据结构）
    int epoll_fd = epoll_create(10);
    epoll_event events[MAX_EVENT_NUM];  //epoll_wait的传出参数，保存需要处理的事件

    //将listen描述符设置监听事件，设置为边缘触发，设置非阻塞，并添加到epoll
    Util::addFD(epoll_fd, listen_fd, false);
    //HttpConnection中的静态（类全局成员）epoll成员，绑定到此处的epoll_fd上
    HttpConnection::setEpollFd(epoll_fd);

    while(true)
    {
        //std::cout << "Server: waiting for message or connection!" << std::endl;
        //等待事件发生，最后一个参数表示超时事件，负数表示一直等待
        int cnt = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, -1);
        //服务器出错，则关闭服务器
        if (cnt < 0 && errno != EINTR)
        {
            std::cout << "Server: server errno, shutdown!" << std::endl;
            break;
        }

        //遍历events数组来处理所有就绪的事件
        for (int i = 0; i < cnt; i++)
        {
            int sock_fd = events[i].data.fd;
            if (sock_fd == listen_fd)
            {
                //存放客户端地址的结构体
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                //从全连接队列取一个连接
                int conn_fd = accept(listen_fd, (struct sockaddr*) &client_addr, &client_addr_len);
                if (conn_fd < 0)
                {
                    std::cout << "Server: connect fail for no connection!" << std::endl;
                    continue;
                }
                
                //当连接达到上限，关闭该连接
                if (HttpConnection::m_user_num >= MAX_FD)
                {
                    close(conn_fd);
                    continue;
                }

                //将新连接放到连接池（数组）
                conns_pool[conn_fd].init(conn_fd, client_addr);
            }
            
            //可以通过events字段来判断具体是哪些事件就绪了
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //std::cout << "Server: Connection error, unlinking the user!" << std::endl;
                conns_pool[sock_fd].closeConn();
            }
            else if (events[i].events & EPOLLIN)    // 读事件
            {
                if (conns_pool[sock_fd].readData()) // 主线程读取数据，读完放在连接的m_read_buf
                {
                    //读取后添加至任务队列处理，会唤醒线程池中的线程进行操作
                    thread_pool -> append(conns_pool + sock_fd);
                }
                else
                {
                    std::cout << "Server : read fail, unlinking!" << std::endl;
                    conns_pool[sock_fd].closeConn();
                }
            }
            else if (events[i].events & EPOLLOUT)   //写事件
            {   
                if (!conns_pool[sock_fd].writeData())    
                {
                    std::cout << "Server : write fail, unlinking!" << std::endl;
                    conns_pool[sock_fd].closeConn();
                }
            }
        }
    }
    //退出关闭连接，回收内存
    close(epoll_fd);
    close(listen_fd);
    delete[] conns_pool;
    delete thread_pool;

    return 0;
}