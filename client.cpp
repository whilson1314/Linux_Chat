#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <fstream>
#include <iostream>

using namespace std;

int sock = -1;

//消息接收
void *handle_recv(void *arg)
{
    while(1)
    {
        char recv_buffer[1000];
        memset(recv_buffer, 0, sizeof(recv_buffer));
        int len = recv(sock, recv_buffer, sizeof(recv_buffer), 0);
        if (len == 0)
        {
            cout << "服务器异常！" << endl;
            return NULL;
        }

        if (len == -1)
            break;
        string str(recv_buffer);
        cout << str << endl;
    }
    return NULL;
}


//消息发送
void *handle_send(void *arg)
{
    while(1)
    {
        string str;
        getline(cin, str);
        if (str == "exit")
            break;
        if (str.empty())
            continue;
        str = "content:" + str;
        int ret = send(sock, str.c_str(), str.length(), 0);
    }
    return NULL;
}

// 处理群消息发送
void *handle_send_group(void *arg)
{
    int flag = *(int *)arg;
    while(1)
    {
        string str;
        getline(cin, str);
        if (str == "exit")
            break;
        if (str.empty())
            continue;
        str = "gr_message:" + str;
        send(sock, str.c_str(), str.length(), 0);
    }
    return NULL;
}

int main()
{
    cout << "client开始运行" << endl;

    //设置服务器的地址以及端口
    struct sockaddr_in serv_addr;
    sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("192.168.3.39");
    serv_addr.sin_port = htons(8045);

    //连接服务器，成功返回0，失败返回-1
    if (connect(sock, (struct sockaddr *) & serv_addr, sizeof(serv_addr)) == -1)
    {
        cout << "connect() error!" << endl;
    }
    
    int choice = -1;
    string name, pass, pass1;
    bool is_login = false;      //记录是否登陆成功
    string login_name;          //记录成功登陆的用户名

    //先检查是否存在cookie文件（每次用户登陆，服务器都会回发一个cookie）
    ifstream file("cookie.txt");
    string cookie_str;
    if (file.good())
    {
        file >> cookie_str;
        file.close();
        cookie_str = "cookie:" + cookie_str;
        //将cookie发送到服务器
        send(sock, cookie_str.c_str(), cookie_str.length() + 1, 0);
        //接受服务器答复
        char cookie_ans[100];
        memset(cookie_ans, 0, sizeof(cookie_ans));
        int ret = recv(sock, cookie_ans, sizeof(cookie_ans),0);
        if (ret == 0)
        {
            cout << "服务器异常" << std::endl;
            getchar();
            getchar();
            return 0;
        }

        //判断服务器答复是否通过
        string ans_str(cookie_ans);
        if(ans_str != "NULL")
        {
            //服务器查询cookie合法，is_login
            is_login = true;
            login_name = ans_str;
        }
    }

    while(!is_login)
    {
        system("clear");
        cout << " ----------------------- " << endl;
        cout << "|    whilson聊天室        |" << endl;
        cout << "|   请输入你要的选项：      |" << endl;
        cout << "|       1:登陆           |" << endl;
        cout << "|       2:注册           |" << endl;
        cout << "|       0:退出           |" << endl;
        cout << "|                        |" << endl;
        cout << " ----------------------- " << endl;


        //开始处理各种事务
        cin >> choice;

        if(choice == 0)
            break; 
        // ——登陆
        if(choice == 1)
        {
            //1. 根据用户输入拼接登录信息
            cout << "用户名：";
            cin >> name;
            cout << "密码：";
            cin >> pass;
            string str = "login" + name;
            str += "pass:";
            str += pass;
            fflush(stdin);
            //2 向服务器发送登录请求
            send(sock, str.c_str(), str.length(), 0);
            //3 接收服务器应答
            char buffer[1000];
            memset(buffer, 0, sizeof(buffer));
            int ret = recv(sock, buffer, sizeof(buffer), 0);
            if (ret == 0)
            {
                cout << "服务器异常" << endl;
                getchar();
                getchar();
                return 0;
            }
            //4 解析应答信息，判断是否登陆成功
            string recv_str(buffer);
            if (recv_str.substr(0, 2) == "ok")
            {
                is_login = true;
                login_name = name;

                //4-1 登录成功，存储服务器发送的cookie文件
                string gen_cookie_cmd = recv_str.substr(2);
                gen_cookie_cmd = "cat > cookie.txt << end \n" + gen_cookie_cmd + "\nend";
                system(gen_cookie_cmd.c_str()); //system函数执行操作系统的命令

                cout << "登录成功" << endl;
                break;
            }
            else
            {
                //4-2 登录失败，发送错误信息
                cout << "密码或用户名错误！" << endl;
                cout << "按任意键确认..." << endl;
                getchar();
                getchar();
            }
        }
        //二 注册
        else if (choice == 2)
        {
            //1 根据用户输入拼接注册信息
            cout << "注册的用户名：";
            cin >> name;
            while(1)
            {
                cout << "密码:";
                cin >> pass;
                cout << "确认密码：";
                cin >> pass1;
                if (pass == pass1)
                {
                    break;
                }
                else{
                    cout << "两次密码不一样" << endl << endl;
                }
            }
            fflush(stdin);
            name = "name:" + name;
            pass = "pass:" + pass;
            string str = name + pass;
            //2 向服务器发送注册信息
            send(sock, str.c_str(), str.length(), 0);
            cout << "注册成功！" << endl;
            cout << "按任意键确认...";
            getchar();
            getchar();
            //TODO 3根据服务器回应，判断是否注册成功
        }
    }

    // 登陆成功后，显示登录界面
    while (is_login)
    {
        if (is_login)
        {
            system("clear");
            cout << "欢迎回来，" << login_name << endl;
            cout << "---------------------------------------" << endl;
            cout << "|                                        |" << endl;
            cout << "|            清选择你要的选项              |" << endl;
            cout << "|            1.指定好友聊天               |" << endl;
            cout << "|            2.参与群聊                   |" << endl;
            cout << "|            0.退出                       |" << endl;
            cout << "|                                         |" << endl;
            cout << "-----------------------------------------" << endl << endl;
        }
        cin >> choice;

        //线程ID
        pthread_t send_t, recv_t;
        void *thread_return;

        if (choice == 0)
            break;
        //指定好友聊天
        if (choice == 1)
        {
            // 1 拼接聊天相关的用户信息
            cout << "请输入对方的用户名：";
            string target_name, content;
            cin >> target_name;

            string sendstr("target:" + target_name + "from:" + login_name);

            // 2向服务器大宋聊天用户信息
            send(sock, sendstr.c_str(), sendstr.length(), 0);

            // TODO 接受服务器回复，判断聊天是否合法
            cout << "请输入你想说的话(输入exit退出):" << endl;

            // 3创建两个线程，分别用于接受数据和发送数据
            auto send_thread = pthread_create(&send_t, NULL, handle_send, (void *) &sock);
            auto recv_thread = pthread_create(&recv_t, NULL, handle_recv, (void *) &sock);
            pthread_join(send_t, &thread_return);
            //pthread_join(recv_t, &thread_return);
            pthread_cancel(recv_t);  
        }
        if (choice == 2)
        {
            cout << "请输入群号：";
            int num;
            cin >> num;
            string sendstr("group:" + to_string(num));
            send(sock, sendstr.c_str(), sendstr.length(), 0);
            cout << "请输入你想说的话(输入exit退出):" << endl;

            auto send_thread = pthread_create(&send_t, NULL, handle_send_group, &sock); //  创建发送线程
            auto recv_thread = pthread_create(&recv_t, NULL, handle_recv, &sock);       //  创建接收线程
            pthread_join(send_t, &thread_return);
            pthread_cancel(recv_t);
        }
    }
    cout << "client运行结束" << endl;
    close(sock);
}