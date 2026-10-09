//scp C:\Users\Lenovo\Desktop\Mytalk\V0.1\mytalkserver\server.cpp ubuntu@122.51.213.119:~/mytalk/server.cpp
//this is server.
//All rights reserved.
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <mutex>
#include <cerrno>
#include <memory>

using std::thread;
using std::string;
using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::shared_ptr;
using std::mutex;
const int MAX_MESSAGE_SIZE=4096;
struct struct_client {
    int client_socket;
    string client_name;
    bool registered=false;
    bool authenticated=false;
    mutex send_mutex;
    bool stopping = false;
    explicit struct_client(int fd):client_socket(fd) {}//初始化列表（构造）
};
struct struct_message {
  string command;
  string target;
    string data;
    bool valid=false;
};

vector<shared_ptr<struct_client>> clients;
std::mutex clients_mutex;
void broadcast(const string& message,const shared_ptr<struct_client>& sendersocket);
bool privatemessagefunction(const shared_ptr<struct_client> senderclient,const string& target,const string& data);
void finishClient(shared_ptr<struct_client>& client);
bool sendAll(int fd,const string& message);
bool sendToClient(const shared_ptr<struct_client>& client,const string& message);
void requestStop(const shared_ptr<struct_client>& client);//此函数现阶段没有用处。
bool isstopping(const shared_ptr<struct_client>& client);

struct_message praseMessage(const string& message) {
    struct_message result;
    size_t pos=string :: npos;
    pos=message.find('|');
    if (pos==string::npos) {
        return result;//返回不合法的MSG
    }
    result.command=message.substr(0,pos);
    result.data=message.substr(pos+1);
    if (result.command.empty())return result;

    if (result.command=="MSG") {
        size_t pos2=result.data.find('|');
        if (string::npos==pos2) {
            return  result;//不合法
        }
        result.target=result.data.substr(0,pos2);
        result.data=result.data.substr(pos2+1);
        if (result.target.empty()||result.data.empty()) {
            return result;
        }
    }
    if (result.command=="LIST") {
        if (!result.data.empty()) {
            return result;
        }
        else {
            result.valid=true;
            return result;
        }
    }
    if (result.data.empty())return result;
    result.valid=true;
    return result;
}
void finishClient(shared_ptr<struct_client>& client) {
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(remove(clients.begin(),clients.end(),client),clients.end());
    }
    {
        std::lock_guard<std::mutex> lock(client->send_mutex);
        if (!client->stopping) {
            client->stopping=true;
            shutdown(client->client_socket,SHUT_RDWR);
        }
        if (client->client_socket!=-1) {
            close(client->client_socket);//关闭对应的Socket连接
            client->client_socket=-1;
        }
    }//?
}
bool sendAll(int fd,const string& message) {
    size_t sent = 0;
    while (sent<message.length()) {
        ssize_t result=send(fd,message.c_str() + sent,message.length() - sent,MSG_NOSIGNAL);
        if (result < 0 ) {
            if (errno==EINTR) {
                continue;
            }
            return false;
        }
        if (result == 0) {
            return false;
        }
        sent+=result;
    }
    return true;
}
bool sendToClient(const shared_ptr<struct_client>& client,const string& message) {
    {
        std::lock_guard<std::mutex> lock(client->send_mutex);
        if (client->stopping) {
            return false;//已经停止的客户端 不允许继续发送信息
        }
        bool result = sendAll(client->client_socket,message);
        if (!result) {
            client->stopping=true;
            shutdown(client->client_socket,SHUT_RDWR);
        }
        return result;
    }

}
void requestStop(const shared_ptr<struct_client>& client) {
   {
       std::lock_guard<std::mutex> lock(client->send_mutex);//只有获取到锁，即没有人给他发送消息时，才可以关闭
       if (!client->stopping) {
           client->stopping=true;
           shutdown(client->client_socket,SHUT_RDWR);
       }
   }
}

bool isstopping(const shared_ptr<struct_client> &client) {
    {
        std::lock_guard<std::mutex> lock(client->send_mutex);
        return client->stopping;
    }
}

void handleclient(shared_ptr<struct_client> client) {
    string cache;
    bool shouldclose=false;
    while (1) {
        char buffer[1024];
        ssize_t len=recv(client->client_socket,buffer,1023,0);
        if (len>0) {
            cache.append(buffer,len);
            size_t pos;
            while ((pos=cache.find('\n'))!=string::npos) {
                if (isstopping(client)) {
                    shouldclose=true ;
                    break;
                }
                if (pos>MAX_MESSAGE_SIZE) {
                    shouldclose=true;
                    break;
                }
                string message=cache.substr(0,pos);
                cache.erase(0,pos+1);
                struct_message prased_message=praseMessage(message);
                if (!prased_message.valid) {
                    sendToClient(client,"ERROR|Your Message is invalid\n");
                    continue;
                }
                string command=prased_message.command;
                string data=prased_message.data;
                string target=prased_message.target;
                if (command=="AUTH") {
                    if (client->authenticated) {
                        continue;
                    }
                    if (data!="111111") {
                        string autherrormsg="密码错误\n";
                        sendToClient(client,autherrormsg);
                        shouldclose=true;
                        break;
                    }
                    client->authenticated=true;

                }
                if (command=="NAME") {
                    if (!client->authenticated) {
                        continue;
                    }
                    if (client->registered) {
                        continue;
                    }
                    if (data.empty()) {
                        continue;
                    }
                    bool duplicate=false;
                    {
                        std:: lock_guard<std::mutex> lock(clients_mutex);
                        for (auto tmpclient: clients) {
                            if (tmpclient->client_name==data) {
                                duplicate=true;//重名了
                                break;
                            }
                        }
                        if (!duplicate) {
                            client->client_name=data;
                            client->registered=true;
                            clients.push_back(client);

                        }
                    }
                        if (!duplicate){
                            string duplicatemsg="OK|Sucessfully\n";
                            sendToClient(client,duplicatemsg);
                            broadcast(client->client_name+" Login",client);
                            }
                        if (duplicate) {
                            string duplicatemsg="已被命名，重新连接并重新命名\n";
                            sendToClient(client,duplicatemsg);
                            shouldclose=true;
                            break;
                        }
                }
                if (command=="CHAT") {
                    if (!client->registered) {
                        continue;
                    }
                    string chatmessage=client->client_name+": "+data;
                    cout<<chatmessage<<endl;
                    broadcast(chatmessage,client);
                }
                if (command=="MSG") {
                    if (!client->registered) {
                        continue;
                    }


                    int ret=privatemessagefunction(client,target,data);
                  //  privatemessagefunction(data,target,cilent,ret);//?编译错误
                    string msgsuccedmsg;

                    if (ret==0) {
                        msgsuccedmsg="ERROR|NOT FOUND\n";
                    }
                    if (ret==1) {
                        msgsuccedmsg="OK|MSGED\n";
                    }
                     sendToClient(client,msgsuccedmsg);
                }
                if (command=="LIST") {
                    if (!client->registered) {
                        continue;
                    }
                    string listmessage="[OS] USERS:";
                    vector<shared_ptr<struct_client>> temp;
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex);
                        for (auto tmpclient: clients) {
                            if (tmpclient->registered) {listmessage+=tmpclient->client_name+"|";
                            }
                        }
                    }


                    listmessage+='\n';
                     sendToClient(client,listmessage);
                }
            }


        if (cache.size()>MAX_MESSAGE_SIZE) {
            shouldclose=true;
        }
        if (shouldclose)break;
        }
        if (len<=0) {
            if (len<0&&errno==EINTR) {
                continue;
            }
            shouldclose=true;
            if (len==0) {
                cout << "Connection closed" << endl;
            }
            else {
                cout<<"Receive Error"<<endl;
            }



            break;
        }

    }
    bool wasregistered=false;
    string name;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        wasregistered=client->registered;
        name=client->client_name;
    }
    finishClient(client);
    if (wasregistered) {

        broadcast(name+" left",client);
    }
}
void broadcast(const string& message,const shared_ptr<struct_client>& sender) {
    vector<shared_ptr<struct_client>> temp;
    string  sendmessage=message+'\n';
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        temp=clients;
    }
        for (auto tmpclient: temp) {
            if (tmpclient != sender) {

                sendToClient(tmpclient,sendmessage);
            }
        }
    }




bool  privatemessagefunction(const shared_ptr<struct_client> senderclient,const string& target,const string& data) {

    shared_ptr<struct_client> receiver;
    string  sendmessage="[私聊]"+senderclient->client_name+':'+data+'\n';
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
       for (auto tmpclient: clients) {
           if (tmpclient->registered&&tmpclient->client_name==target) {
               receiver=tmpclient;
               break;
           }
       }
    }
    if (!receiver) {
        return false;
    };
    return sendToClient(receiver,sendmessage);

}
int main() {
    int serverSocket = socket(AF_INET,SOCK_STREAM,0);
    if (serverSocket == -1) {
        cout << "Socket creation error" << endl;
        return 1;
    }
    sockaddr_in serverAddr={};
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_port=htons(8888);
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
    int ret =bind(serverSocket,(sockaddr*)&serverAddr,sizeof(serverAddr));
    if (ret == -1) {
        cout << "Bind error" << endl;
        return 1;
    }
    cout << "Bind success" << endl;
    ret=listen(serverSocket,5);
    if (ret == -1) {
        cout << "Listen error" << endl;
        return 1;
    }
    cout << "Listen success" << endl;
    while (true) {
        int clientSocket = accept(serverSocket,NULL,NULL);
        if (clientSocket == -1) {
            cout << "Accept error" << endl;
            continue;
        }
        cout << "Accept success" << endl;
        timeval timeout{};
        timeout.tv_sec=5;
        timeout.tv_usec=0;
        if (setsockopt(clientSocket,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout))==-1) {
            perror("setsockopt");
            close(clientSocket);
            continue;
        };
     //   clients.push_back(clientSocket);
        shared_ptr<struct_client> client = std::make_shared<struct_client>(clientSocket);//make shared 是什么意思？
        thread handleclinentThread(handleclient,client);
        handleclinentThread.detach();
    }
    close(serverSocket);
    return 0;

}
// 改动	解决的问题
// sendAll()	防止一次 send() 只发送部分数据
// parseMessage()	统一解析消息，避免业务逻辑混乱
// send_mutex	防止多个线程向同一客户端发送时消息交错
// clients_mutex	防止多个线程同时操作客户端列表产生数据竞争
// shared_ptr<Client>	防止客户端对象在其他线程使用时被销毁
// 统一 shutdown() / close()	防止连接关闭和发送之间发生竞态
// 发送超时	减少慢客户端长期阻塞服务端的问题