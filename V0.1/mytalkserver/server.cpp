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
vector<struct_client> clients;
void broadcast(string& message,int sendersocket);
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
                    if (data!="111111") {
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
                    clientSocket.client_name=data;
                    clientSocket.registered=true;
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex);
                        clients.push_back(clientSocket);
                    }

                }
                if (command=="CHAT") {
                    if (!clientSocket.registered) {
                        continue;
                    }
                    string chatmessage=clientSocket.client_name+": "+data;
                    broadcast(chatmessage,clientSocket.client_socket);
                }
            }
        }
        if (len==0) {
            cout << "Connection closed" << endl;
            struct_client tmpclient={clientSocket.client_socket,"tmpstr"};
            {   std::lock_guard<std::mutex> lock(clients_mutex);
                auto erasepos=std::find(clients.begin(),clients.end(),tmpclient);
                if (erasepos!=clients.end()) {
                    clients.erase(erasepos);
                }
            }
            break;
        }
        if (len<0) {
            cout << "Receive error" << endl;
            struct_client tmpclient={clientSocket.client_socket,"tmpstr"};
            {   std::lock_guard<std::mutex> lock(clients_mutex);
                auto erasepos=std::find(clients.begin(),clients.end(),tmpclient);
                if (erasepos!=clients.end()) {
                    clients.erase(erasepos);
                }
            }
            break;
        }

    }
    close(clientSocket.client_socket);
    
}
void broadcast(string& message,int sendersocket) {
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
        thread handleclinentThread(handleclient,processingclient);//?传入引用怎么错误
        handleclinentThread.detach();
    }
    close(serverSocket);
    return 0;

}
