#ifndef MYSQL_CONNECTION_HPP
#define MYSQL_CONNECTION_HPP

#include <mysql/mysql.h>
#include <iostream>
#include <vector>
#include <cstring>


//MysqlConnection连接类
class MysqlConnection
{
public:

    struct mysql_addr
    {
        //主机地址
        std::string host_name = "127.0.0.1";

        //用户
        std::string user_name = "root";

        //密码
        std::string pwd = "";

        //数据库
        std::string dbname = "test_connection";

        //端口
        unsigned int port = 0;
    };

    //构造函数
    //使用成员初始化列表，将m_conn初始化为NULL
    MysqlConnection() : m_conn(NULL)
    {

    }
    
    //析构函数
    ~MysqlConnection()
    {
        //关闭连接
        if(m_conn)
        {
            //关闭连接函数
            close();
            m_conn = NULL;
        }
    }

    //初始化连接
    void init()
    {
        m_conn = mysql_init(NULL);
        //初始化对象失败
        if(!m_conn)
        {
            std::cout << "MysqlConnection : init mysql fail" << std::endl;
            return;
        }
    }

    //连接数据库
    void connect(struct mysql_addr* t_addr = NULL)
    {
        //如果有传入地址
        if(t_addr)
        {
            m_addr = *t_addr;
        }
        //使用musql_real_connect函数建立连接
        if(NULL == mysql_real_connect(m_conn, m_addr.host_name.c_str(), m_addr.user_name.c_str(),
            m_addr.pwd.c_str(), m_addr.dbname.c_str(), m_addr.port, NULL, CLIENT_MULTI_STATEMENTS))
        {
            std::cout << "MysqlConnection: try connect fail" << std::endl;
            return;
        }
    }

    //执行语句
    std::vector<std::vector<std::string>> query(std::string sql)
    {
        std::vector<std::vector<std::string>> res;
        
        //执行sql语句
        //连接不可用
        if(m_conn == NULL)
        {
            std::cout << "MysqlConnection: connection invalid" << std::endl;
            return res;
        }
        
        //使用mysql_query函数发送SQL语句，并获取返回值
        if(mysql_query(m_conn, sql.c_str()) != 0)
        {
            std::cout << "MysqlConnection: query fail!" << std::endl;
            return res;
        }

        //使用mysql_store_result函数获取结果集
        auto result = mysql_store_result(m_conn);
        //结果集为空
        if(!result)
        {
            std::cout << "MysqlConnection: empty result" << std::endl;
            return res;
        }

        //转换结果集为vector<string>
        auto row_num = mysql_num_rows(result);      //记录行数
        auto col_num = mysql_num_fields(result);    //记录列数
        for(int i = 0; i < row_num; i++)
        {
            //获取每行信息
            auto row_info = mysql_fetch_row(result);
            std::vector<std::string> row_vec;
            for(int j = 0; j < col_num; j++)
            {
                std::string t = row_info[j];
                row_vec.push_back(t);
            }
            //存取行结果
            res.push_back(row_vec);
        }
        return res;
    }

    //断开连接
    void close()
    {
        //关闭此连接
        if(!m_conn)
        {
            mysql_close(m_conn);
            m_conn = NULL;
        }
    }

private:
    //Mysql对象
    MYSQL* m_conn;

    //Mysql地址
    mysql_addr m_addr;

};

#endif
