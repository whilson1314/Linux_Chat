#include <iostream>
#include "http_connection.hpp"
#include "util.hpp"

//初始化静态信息
int HttpConnection::m_epoll_fd = -1;
int HttpConnection::m_user_num = 0;
BloomFilter HttpConnection::m_bloom_filter = BloomFilter();
MysqlConnection HttpConnection::mysql_g = MysqlConnection();
Locker HttpConnection::m_name_sock_mutex = Locker();
Locker HttpConnection::m_sock_name_mutex = Locker();
Locker HttpConnection::m_sour_dest_mutex = Locker();
Locker HttpConnection::m_group_mutex = Locker();
std::unordered_map<std::string, int> HttpConnection::m_name_socks = std::unordered_map<std::string, int>();
std::unordered_map<int, std::string> HttpConnection::m_sock_names = std::unordered_map<int, std::string>();
std::unordered_map<std::string, std::string> HttpConnection::m_sour_dest = std::unordered_map<std::string, std::string>();
std::unordered_map<int, std::set<int>> HttpConnection::m_group = std::unordered_map<int, std::set<int>>();
HttpConnection* HttpConnection::conns_pool = NULL;

//初始化连接池
HttpConnection* HttpConnection::initConnPool(int MAX_SIZE)
{
    //创建连接数组
    conns_pool = new HttpConnection[MAX_SIZE];
    return conns_pool;
}

//设置静态EpollFd信息
void HttpConnection::setEpollFd(int epoll_fd)
{
    m_epoll_fd = epoll_fd;
}

//构造函数
HttpConnection::HttpConnection()
{

}

//析构函数
HttpConnection::~HttpConnection()
{

}

//初始化 根据accept得到的客户端fd和addr 初始化http连接
void HttpConnection::init(int sock_fd, const sockaddr_in& addr)
{
    m_sock_fd = sock_fd;
    m_addr = addr;

    //初始化连接的群组
    m_sock_groups = std::set<int>();

    //设置端口复用
    int reuse = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //添加到epoll监听，客户端fd设置，EPOLLONESHOT
    Util::addFD(m_epoll_fd, sock_fd, true);

    //用户数增加
    m_user_num += 1;

    //初始化读写状态
    initState();
}


//初始化读写状态
void HttpConnection::initState()
{
    //初始化读
    m_read_index = 0;
    memset(m_read_buf, 0, READ_BUFFER_SIZE);

    //初始化写
    m_write_index = 0;
    bytes_have_send = 0;
    bytes_to_send = 0;
    memset(m_write_buf, 0, WRITE_BUFFER_SIZE);
}

//任务处理函数，由线程调用
void HttpConnection::process()
{
    //分析请求类型
    auto reg_info = parseRequest();

    //根据请求类型，生成回复
    auto ret = makeReply(reg_info);

    //清空读缓冲
    memset(m_read_buf, 0, READ_BUFFER_SIZE);

    if(!ret)
    {
        std::cout << "HttpConnection: make reply fail" << std::endl;
        closeConn();
    }
}

//分析请求类型
HttpConnection::QUERY_CODE HttpConnection::parseRequest()
{
    std::string request = m_read_buf;
    //std::cout << "parseRequset:" << request << std::endl;

    if(request.find("cookie:") != request.npos)
    {
        return COOKIE;
    } else if(request.find("login") != request.npos)
    {
        return LOGIN;
    } else if(request.find("name:") != request.npos)
    {
        return REGISTER;
    } else if(request.find("target:") != request.npos)
    {
        return TARGET;
    } else if(request.find("content:") != request.npos)
    {
        return CONTENT;
    } else if(request.find("group:") != request.npos)
    {
        return GROUP;
    } else if(request.find("gr_message:") != request.npos)
    {
        return GROUP_MSG;
    }
    return NO_REQUEST;
}

//生成回复
bool HttpConnection::makeReply(QUERY_CODE code)
{
    //std::string res = "";
    switch(code)
    {
        //检查cookie是否合法
        case COOKIE:
            checkSession();
            break;
        
        //请求登陆
        case LOGIN:
            login();
            break;

        //请求注册
        case REGISTER:
            registerUser();
            break;
        
        //请求聊天对象
        case TARGET:
            target();
            break;

        //请求转发信息
        case CONTENT:
            transMsg();
            break;

        //请求群组
        case GROUP:
            group();
            break;

        //请求群聊
        case GROUP_MSG:
            transGroupMsg();
            break;

        default:
            std::cout << "HttpConnection: No request" << std::endl;
            break;
    }
    return true;
}

//检查cookie是否有效
std::string HttpConnection::checkSession()
{
    std::cout << "HttpConnection: checking session!" << std::endl;

    std::string request = m_read_buf;
    //消息为"cookie:xxx..."
    std::string cookie = request.substr(7); //获取cookie

    //连接redis进行查找
    RedisConnection redis_conn;
    redis_conn.connect();
    std::string cmd = "hget " + request.substr(7) + " name";
    auto res = redis_conn.query(cmd);
    if(res != "")
    {
        //存在结果
        strcpy(m_write_buf, res.c_str());
        bytes_to_send = res.size();

        //记录在线信息，将当前socket与用户名映射在一起，哈系表
        //m_name_socks是name->sock
        m_name_sock_mutex.lock();
        m_name_socks[res] = m_sock_fd;
        m_name_sock_mutex.unlock();

        //m_sock_names是socket->name
        m_sock_name_mutex.lock();
        m_sock_names[m_sock_fd] = res;
        m_sock_name_mutex.unlock();
    } else
    {
        //结果为空    
        strcpy(m_write_buf, "NULL");
        bytes_to_send = 4;
    }
    //重置写事件
    Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLOUT);
    return res;
}


//验证登陆信息
bool HttpConnection::login()
{
    //std::cout << "HttpConnetion: logining!" << std::endl;
    std::string request = m_read_buf;
    //"login....pass:...."
    int pos_name = request.find("login");
    int pos_pwd = request.find("pass:");
    std::string name = request.substr(pos_name + 5, pos_name -5);
    std::string pwd = request.substr(pos_pwd + 5);

    //std::cout << "name : " << name << "password:" << pwd << std::endl;

    //布隆过滤器过滤，如果不存在数据库，必被过滤
    if(!m_bloom_filter.get(name))
    {
        std::cout << "HttpConnection: login fail! user does not exist!" << std::endl;
        strcpy(m_write_buf, "wrong");
        bytes_to_send = 5;
        //重置写事件
        Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLOUT);
        return false;
    }

    std::string sql = "SELECT * FROM user WHERE name = '" + name +
                    "' AND password = '" + pwd + "'";
    auto result = mysql_g.query(sql);

    //用户不再数据库中
    if(result.empty())
    {
        std::cout << "HttpConnection: login fail! username or password error!" << std::endl;
        strcpy(m_write_buf, "wrong");
        bytes_to_send = 5;
        //注册写事件
        Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLOUT);
        return true;
    }

    //用户登陆成功，生成session_id
    std::string ses_id = Util::makeSesId();
    //将session_id存入redis中
    RedisConnection redis_conn;
    redis_conn.connect();
    //"hset 95GNs03odw name Shaye"
    std::string cmd = "hset " + ses_id + " name " + name;
    redis_conn.query(cmd);
    cmd = "expire " + ses_id + " 300";
    redis_conn.query(cmd);
    redis_conn.close();
    //std::cout << "HttpConnection: login succeeded! make sessionid:" + ses_id << std::endl;

    //记录在线信息，将当前socket与用户名双向映射在一起
    // name->socket
    m_name_sock_mutex.lock();
    m_name_socks[name] = m_sock_fd;
    m_name_sock_mutex.unlock();

    //socket->name
    m_sock_name_mutex.lock();
    m_sock_names[m_sock_fd] = name;
    m_sock_name_mutex.unlock();

    //将登陆状态和session_id(作为cookies)发回客户端
    std::string res = "ok" + ses_id;
    strcpy(m_write_buf, res.c_str());
    bytes_to_send = res.size();
    Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLOUT);//注册写事件
    
    return true;
}   


//处理注册
bool HttpConnection::registerUser()
{
    std::cout << "HttpConnection: registering!" << std::endl;
    std::string request = m_read_buf;
    //"name:....pass:..."
    int pos_name = request.find("name:");
    int pos_pwd = request.find("pass:");
    std::string name = request.substr(pos_name + 5, pos_name - 5);
    std::string pwd = request.substr(pos_pwd + 5);

    //测试：输出名字密码
    // std::cout << "name: " << name << "passwordL " << pwd << std::endl;

    //访问数据库查询有无用户
    std::string sql = "SELECT * FROM user WHERE name = '" + name + "' AND password = '" + pwd + "'";
    auto result = mysql_g.query(sql);

    //用户存在数据库，则无法注册
    if(!result.empty())
    {
        std::cout << "HttpConnection: register fail! username exists!" << std::endl;
        //重置读事件
        Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
        return true;
    }

    sql = "INSERT INTO user VALUES('"+name + "','" + pwd + "')";
    result = mysql_g.query(sql);

    //更新布隆过滤器
    m_bloom_filter.add(name);
    std::cout << "HttpConnection: register succeeded!" << std::endl;

    //重置读事件
    Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
    return true;
}

//设置聊天对象
bool HttpConnection::target()
{
    std::cout << "HttpConnection: target setting!" << std::endl;
    std::string request = m_read_buf;
    //"target:....from:..."
    int pos_target = request.find("target:");
    int pos_from = request.find("from:");
    std::string target = request.substr(pos_target + 7, pos_target - 7);
    std::string from = request.substr(pos_from + 5);

    //预先设置聊天对象
    m_sour_dest_mutex.lock();
    m_sour_dest[from] = target;
    m_sour_dest_mutex.unlock();

    //聊天对象未登陆
    if (m_name_socks.find(target) == m_name_socks.end())
    {
        std::cout << "HttpConnection: from " << from << " , target: " << target << "is not online!" << std::endl;
        return true;
    }

    std::cout << "HttpConnection: from " << from << " to target" << target << " is found!" << std::endl;
    //重置读事件
    Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
    return true;
}


//发送信息
bool HttpConnection::transMsg()
{
    //处理读入数据
    std::cout << "HttpConnection: transMsg going!" << std::endl;
    std::string request = m_read_buf;
    //"content..."
    std::string content = request.substr(8);
    //获取对方发送的用户名
    std::string from = m_sock_names[m_sock_fd];
    //未设置目的地址
    if (m_sour_dest.find(from) == m_sour_dest.end())
    {
        std::cout << "HttpConnection: the target haven't been set!" << std::endl;
        Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
        return true;
    }
    //获取接受方信息
    std::string target = m_sour_dest[from];
    // 目的地址用户未上线，不进行处理，重新挂载到读事件
    if (m_sour_dest.find(target) == m_sour_dest.end())
    {
        std::cout << "HttpConnection: the target user is not online!" << std::endl;
        Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
        return true;
    }
    //目的地址socket
    int target_fd = m_name_socks[target];
    //转发消息
    content = '[' + from + ']' + content;
    conns_pool[target_fd].setWriteBuffer(content); //设置接受用户的写缓冲区，并发送
    std::cout << "HttpConnection: content:" << content << std::endl;
    std::cout << "HttpConnection: the message from " + from +" to: " + target + "transfer succeeded!" << std::endl;
    Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
    return true;
}


//绑定群
bool HttpConnection::group()
{
    std::cout << "HttpConnection: group seeting!" << std::endl;
    std::string request = m_read_buf;
    //"group:..."
    int pos_group = request.find("group:");
    std::string group = request.substr(6);
    int group_num = stoi(group);
    cur_group = group_num;
    std::string name = m_sock_names[m_sock_fd];
    //添加到群组的set中，并且记录在自己所属的群组set
    m_group_mutex.lock();
    if (m_group[group_num].find(m_sock_fd) == m_group[group_num].end())
    {
        m_group[group_num].insert(m_sock_fd);
        m_sock_groups.insert(group_num);// 添加连接的群组记录
        std::cout << "HttpConnection :" << name << " group setting succeed!" << std::endl;  
    }
    m_group_mutex.unlock();

    Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
    return true;

}


//消息群广播
bool HttpConnection::transGroupMsg()
{
    std::cout << "HttpConnection: transGroupMsg going!" << std::endl;
    std::string request = m_read_buf;
    //"gr_message:...."
    std::string content = request.substr(11);

    std::cout << "HttpConnection: message:" << m_read_buf << std::endl;
    //获取发送方的用户名
    std::string from = m_sock_names[m_sock_fd];
    content = '[' + std::to_string(cur_group) + ':' + from + ']' +content;
    //逐个成员发送
    for (auto sock_fd : m_group[cur_group])
    {
        conns_pool[sock_fd].setWriteBuffer(content);
    }
    Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
    return true;
}


//关闭连接
void HttpConnection::closeConn()
{
    //关闭此连接， 留着新的连接到来时候用
    if (m_sock_fd != -1)
    {
        Util::removeFd(m_epoll_fd, m_sock_fd);
        m_sock_fd = -1;
        m_user_num --;
    }
}

//模拟proactor模式由主线程调用：非阻塞一次性读
bool HttpConnection::readData()
{
    //std::cout << "HttpConnection: main thread reading!" << std::endl;s
    if (m_read_index >= READ_BUFFER_SIZE)
    {
        std::cout << "HttpConnection: read buffer is full!" << std::endl;
        return false;
    }

    //读取到的字节数
    int bytes_read = 0;
    //重置读缓冲区
    m_read_index = 0;

    //因为是ET，所以在while循环中一直读
    while(true)
    {
        bytes_read = recv(m_sock_fd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);
        if (bytes_read == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;//说明读完了
            }
            return false;
        }
        else if(bytes_read == 0)
        {
            //std::cout << "HttpConnection: client exit!" << std::endl;
            return false;
        }
        //读取成功
        m_read_index += bytes_read;
    }

    //std::cout << "HttpConnection: read succeeded!" << std::endl;
    //重置读事件
    //Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
    return true;
}

//模拟proactor模式有主线程调用：非阻塞一次性写
bool HttpConnection::writeData()
{
    //std::cout << "HttpConnection: main thread starts to write!" << std::endl;

    //无数据写
    if (bytes_to_send == 0)
    {
        Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
        initState();
        return true;
    }

    int bytes_write = 0;
    //将数据从用户缓冲区写入内核socket缓冲区
    bytes_write = write(m_sock_fd, m_write_buf, bytes_to_send);
    //写失败
    if (bytes_write == -1)
    {
        std::cout << "HttpConnection: write fail!" << std::endl;
        Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
        return true;
    }
    return false;

    //重置要发送的字节数
    bytes_to_send = 0;
    //清空写缓冲
    memset(m_write_buf, 0, WRITE_BUFFER_SIZE);

    //std::cout << "HttpConnection: write succeeded!" << std::endl;
    //重置读事件
    Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
    return true;
}

//填充写缓冲区
bool HttpConnection::setWriteBuffer(std::string content)
{
    strcpy(m_write_buf, content.c_str());
    bytes_to_send = content.size();
    //测试信息是否写入
    std::cout << "HttpConnection : to " + m_sock_names[m_sock_fd] + " content: " + m_write_buf;
    //重置写事件
    Util::modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
    return true;
}