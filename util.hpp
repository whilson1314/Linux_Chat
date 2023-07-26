#ifndef UTIL_H
#define UTIL_H

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <string>

// util类
class Util
{
public:
    //文件描述符操作
    //设置文件描述符非阻塞
    static void setNonBlock(int fd)
    {
        //fcntl针对文件描述符提供控制
        //fd是被参数cmd操作的描述府
        //fcntl的返回值和命令有关，下面三个命令有特有的返回之
        //F_DUPFD返回新的文件描述符
        //F_GETFD返回相应标志
        //F_GETFL,F_GETOWN返回一个正的进程ID或负的进程ID

        /*
        fcntl有五种功能：
        1.复制一个现有的描述符(cmd = F_DUPFD)
        2.获得/设置文件描述符标记(cmd = F_GETFD 或 cmd = F_SETFD)
        3.获得/设置文件状态标记(cmd = F_GETFL 或 cmd = F_SETFL)
        4.获得/设置异步I/O所有权(cmd = F_GETOWN 或 cmd = F_SETOWN)
        5.获得/设置记录锁(cmd = F_GETLK, F_SETLR 或 F_SETLKW)
        */
        int old_flag = fcntl(fd, F_GETFL);
        /* 
        fcntl的文件状态标志有7个：O_RDONLY, O_WRONLY, O_RDWR, O_APPEND, O_NONBLOCK, O_SYNC, O_ASYNC
        1.O_NONBLOCK    非阻塞I/O
        2.O_APPEND      强制每次写操作都添加在文件的末尾
        3.O_DIRECT      最小化或去掉reading和writing的缓存影响
        4.O_ASYNC       当I/O可用时，允许SIGIO信号发送到进程组：例如当有数据可以读的时候
        */
        int new_flag = old_flag | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_flag);
    }     

    //添加文件描述符到epoll，监听读事件
    //epoll
    static void addFD(int epoll_fd, int fd, bool one_shot)
    {   
        /*
        struct epoll_event
        {
            unit32_t events; //epoll事件类型，包括可读；可写
            epoll_data_t data； //用户数据，可以是一个指针或文件描述符等
        }；
        events字段表示要监听的事件类型:
        EPOLLIN：表示对应的文件描述符上有数据可读
        EPOLLOUT：表示对应的文件描述符上可以写入数据
        EPOLLRDHUP：表示对端已经关闭连接，或者关闭了写操作端的写入
        EPOLLPRI：表示有紧急数据可读
        EPOLLERR：表示发生错误
        EPOLLHUP：表示文件描述符被挂起
        EPOLLET：表示将epoll设置为边缘触发模式
        EPOLLONESHOT：表示将事件设置为一次性事件

        typedef union epoll_data {
            void *ptr;
            int fd;
            uint32_t u32;
            uint64_t u64;
        } epoll_data_t;
        ptr可以指向任何类型的用户数据，fd表示文件描述符，u32和u64分别表示一个32位和64位的无符号整数。
        使用时，用户可以将自己需要的数据存放到这个字段中，当事件触发时，epoll系统调用会返回这个数据，以便用户处理事件。
        */
        epoll_event event;
        event.data.fd = fd;
        //监测读事件/对端关闭写段事件，并设置为边缘触发
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;

        if (one_shot)
        {
            event.events |= EPOLLONESHOT;
        }

        //添加文件描述符到epoll中
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
        //设置文件描述符非阻塞
        setNonBlock(fd);
    } 

    //从epoll中移除文件描述符
    static void removeFd(int epoll_fd, int fd)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
        close(fd);
    }  

    /*
    epoll的相关系统调用
    1.epoll_create
        int epoll_create(int size);
    2.epoll_ctl
        int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
        第一个参数是epoll_create的返回值，第二个参数表示动作，第三个参数表示需要监听的fd，第四个参数是告诉内核需要监听什么事情
        第二个参数的取值：
            EPOLL_CTL_ADD ：注册新的fd到epfd中；
            EPOLL_CTL_MOD ：修改已经注册的fd的监听事件；
            EPOLL_CTL_DEL ：从epfd中删除一个fd
    3.epoll_wait
        int epoll_wait(int epfd, struct epoll_event * events, int maxevents, int timeout);
        收集在epoll监控的事件中已经发送的事件。
            参数events是分配好的epoll_event结构体数组。
            epoll将会把发生的事件赋值到events数组中 (events不可以是空指针，内核只负责把数据复制到这个events数组中，不会去帮助我们在用户态中分配内存)。
            maxevents告之内核这个events有多大，这个 maxevents的值不能大于创建epoll_create()时的size。
            如果函数调用成功，返回对应I/O上已准备好的文件描述符数目，如返回0表示已超时, 返回小于0表示函数失败。
    */

    //修改文件描述符，重置EPOLLONESHOT事件，使得下次的读事件可以触发
    static void modifyFd(int epoll_fd, int fd, int ev)
    {
        epoll_event event;
        event.data.fd = fd;
        //ev为传入事件的类型，比如EPOLLIN，EPOLLOUT
        //重置为EPOLLONESHOT事件
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
        //设置
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
    }

    //添加信号捕捉
    static void addSig(int sig, void (*handler) (int))
    {
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_handler = handler;    //设置sa处理的handler（函数指针，使用SIG_IGN表示忽略该信号）
        sigfillset(&sa.sa_mask);    //是否应该为sigemptyset
        sigaction(sig, &sa, NULL);  //信号捕捉函数，sig是需要捕捉的信号，sa是描述如何处理
    }

    //生成10位的sessionid
    static std::string makeSesId()
    {
        std::string res = "";
        srand(time(NULL));

        for(int i = 0 ; i < 10; ++ i)
        {
            int type = rand() % 3;
            if(type == 0)
            {
                res += '0' + rand() % 10;
            }
            else if(type == 1)
            {
                res += '0' + rand() % 26;
            }
            else if(type == 2)
            {
                res += '0' + rand() % 26;
            }
        }
        return res;
    }

};























#endif