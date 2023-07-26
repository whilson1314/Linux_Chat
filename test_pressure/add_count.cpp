#include<iostream>
#include<fstream>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<mysql/mysql.h>
#include<time.h>
#include <hiredis/hiredis.h>

using namespace std;

int main(){    
    // 连接MYSQL数据库
    MYSQL *con=mysql_init(NULL);
    mysql_real_connect(con, "127.0.0.1", "root", "qiu11.29", "test_connect", 0, NULL, CLIENT_MULTI_STATEMENTS);    
    
    cout << "连接成功" << endl;
    // 随机生成用户名和密码，保存到本地txt文件，并写入数据库
    srand(time(NULL));
    ofstream out("account.txt");
    for(int i = 0; i < 10000; i ++ ) {
       
        string name, pass;
        for(int j = 0; j < 20; j ++ ) {
            bool exist = rand() % 2;
            if(exist)
                name += (char)('a'+rand()%26);
        }
        pass = name;
        cout<<"第"<<i<<"个随机生成的用户名为："<<name<<endl;
        out << name << endl;
        string sql = "INSERT INTO user VALUES ('" + name + "', '" + pass + "')";

        //cout<<"sql语句:"<<search<<endl;
        mysql_query(con, sql.c_str());
    }
    out.close();
    mysql_close(con);
}
