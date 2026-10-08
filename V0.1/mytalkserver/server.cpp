#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <mutex>
std::mutex clients_mutex;
using std::thread;
using std::string;
using std::cout;
using std::cin;
using std::endl;
using std::vector;
struct struct_client {
    int client_socket;
    string client_name;
    bool operator==(const struct_client& other_client) const {
        return this->client_socket==other_client.client_socket;
    }
    bool registered=false;
    bool authenticated=false;
};
//scp C:\Users\Lenovo\Desktop\Mytalk\V0.1\mytalkserver\server.cpp ubuntu@122.51.213.119:~/mytalk/server.cpp
vector<struct_client> clients;
void broadcast(string message,int sendersocket);
void privatemessagefunction(string message,string privatename,struct_client sendersocket,int& ret);
//this is server.
void handleclient(struct_client clientSocket) {
    string cache;
    while (1) {
        char buffer[1024];
        ssize_t len=recv(clientSocket.client_socket,buffer,1023,0);
        if (len>0) {
            cache.append(buffer,len);
            size_t pos;
            while ((pos=cache.find('\n'))!=string::npos) {
                string message=cache.substr(0,pos);
                cache.erase(0,pos+1);
                size_t linepos=string::npos;
                linepos=message.find('|');
                if (linepos==string::npos) {//解析失败，理论上不会出现没找到|的情况
                    continue;
                }
                string command=message.substr(0,linepos);
                string data=message.substr(linepos+1);
                if (command=="AUTH") {
                    if (clientSocket.authenticated) {
                        continue;
                    }
                    if (data!="111111") {
                        string autherrormsg="密码错误\n";
                        send(clientSocket.client_socket,autherrormsg.c_str(),autherrormsg.length(),0);
                        close(clientSocket.client_socket);
                        return ;
                    }
                    clientSocket.authenticated=true;

                }
                if (command=="NAME") {
                    if (!clientSocket.authenticated) {
                        continue;
                    }
                    if (clientSocket.registered) {
                        continue;
                    }
                    if (data.empty()) {
                        continue;
                    }
                    bool duplicate=false;
                    {
                        std:: lock_guard<std::mutex> lock(clients_mutex);
                        for (auto client: clients) {
                            if (client.client_name==data) {
                                duplicate=true;
                                break;
                            }
                        }
                        if (!duplicate) {
                            clientSocket.client_name=data;
                            clientSocket.registered=true;
                            clients.push_back(clientSocket);
                        }
                    }
                        if (!duplicate){
                            string duplicatemsg="OK|Sucessfully\n";
                            send(clientSocket.client_socket,duplicatemsg.c_str(),duplicatemsg.length(),0);
                            broadcast(clientSocket.client_name+" Login",clientSocket.client_socket);
                            }
                        if (duplicate) {
                            string duplicatemsg="已被命名，重新连接并重新命名\n";
                            send(clientSocket.client_socket,duplicatemsg.c_str(),duplicatemsg.length(),0);
                            close(clientSocket.client_socket);
                            return;
                        }
                }
                if (command=="CHAT") {
                    if (!clientSocket.registered) {
                        continue;
                    }
                    string chatmessage=clientSocket.client_name+": "+data;
                    cout<<chatmessage<<endl;
                    broadcast(chatmessage,clientSocket.client_socket);
                }
                if (command=="MSG") {
                    if (!clientSocket.registered) {
                        continue;
                    }
                   size_t namepos=data.find('|');
                    if (namepos==string::npos) {//解析失败，理论上不会出现没找到|的情况
                        continue;
                    }
                    string privatemsgname=data.substr(0,namepos);
                    string privatemessage=data.substr(namepos+1);
                    int ret=0;
                    privatemessagefunction(privatemessage,privatemsgname,clientSocket,ret);
                    string msgsuccedmsg;
                    if (ret==0) {
                        msgsuccedmsg="ERROR|NOT FOUND\n";
                    }
                    if (ret==1) {
                        msgsuccedmsg="OK|MSGED\n";
                    }
                    send(clientSocket.client_socket,msgsuccedmsg.c_str(),msgsuccedmsg.length(),0);
                }
                if (command=="LIST") {
                    if (!clientSocket.registered) {
                        continue;
                    }
                    vector<struct_client> temp;
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex);
                        temp=clients;
                    }
                    string listmessage="[OS] USERS:";
                    for (auto client: temp) {
                       listmessage+=client.client_name+"|";
                    }
                    listmessage+='\n';
                    send(clientSocket.client_socket,listmessage.c_str(),listmessage.length(),0);
                }
            }
        }
        if (len<=0) {
            if (len==0) {
                cout << "Connection closed" << endl;
            }
            else {
                cout<<"Receive Error"<<endl;
            }
            struct_client tmpclient={clientSocket.client_socket,"tmpstr"};
            {   std::lock_guard<std::mutex> lock(clients_mutex);
                auto erasepos=std::find(clients.begin(),clients.end(),tmpclient);
                if (erasepos!=clients.end()) {
                    clients.erase(erasepos);
                }
            }
            if (clientSocket.registered)broadcast(clientSocket.client_name+" left",clientSocket.client_socket);
            break;
        }


    }
    close(clientSocket.client_socket);
    
}
void broadcast(string message,int sendersocket) {
    vector<struct_client> temp;
    string  sendmessage=message+'\n';
    {
       std::lock_guard<std::mutex> lock(clients_mutex);
        temp=clients;
    }

    for (auto client: temp) {
        if (client.client_socket != sendersocket) {

            send(client.client_socket,sendmessage.c_str(),sendmessage.length(),0);
        }
        }

}
void privatemessagefunction(string message,string privatename,struct_client sendersocket,int& ret) {
    ret=0;
    vector<struct_client> temp;
    string  sendmessage="[私聊]"+sendersocket.client_name+':'+message+'\n';
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        temp=clients;
    }

    for (auto client: temp) {
        if (client.client_name==privatename) {
            ret=1;
            send(client.client_socket,sendmessage.c_str(),sendmessage.length(),0);
        }
    }
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
     //   clients.push_back(clientSocket);
        struct_client processingclient={clientSocket,"processingstr",false};
        thread handleclinentThread(handleclient,processingclient);
        handleclinentThread.detach();
    }
    close(serverSocket);
    return 0;

}
