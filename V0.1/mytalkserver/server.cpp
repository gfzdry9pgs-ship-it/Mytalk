#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
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
};
vector<struct_client> clients;
void broadcast(string& message,int sendersocket);
//this is server.
void handleclient(struct_client clientSocket) {
    string cache;
    while (1) {
        if (clientSocket.registered==true) {
            break;
        }
        char buffer[1024];
        ssize_t len=recv(clientSocket.client_socket,buffer,1023,0);
        if (len>0) {
            cache.append(buffer,len);
            size_t pos;
            while ((pos=cache.find('\n'))!=string::npos) {
                if (clientSocket.registered==true) {
                    break;
                }
                string message=cache.substr(0,pos);
                cache.erase(0,pos+1);
                if (message.rfind("NAME|",0)==0) {

                    clientSocket.client_name=message.substr(5);
                    if (!clientSocket.client_name.empty()) {
                        clientSocket.registered=true;
                        clients.push_back(clientSocket);
                    }
                }

            }
        }
        if (len==0) {
            cout << "Connection closed" << endl;
            close(clientSocket.client_socket);
            return;

        }
        if (len<0) {
            cout << "Receive error" << endl;
            close(clientSocket.client_socket);
            return;
        }

    }
    while (1) {
        size_t pos=string::npos;
        while ((pos=cache.find('\n'))!=string::npos) {
            string message=cache.substr(0,pos);
            cache.erase(0,pos+1);
            if (message.rfind("CHAT|",0)==0) {
                message=message.substr(5);//删除chat前缀
                message=clientSocket.client_name+": "+message;
                cout<<"Received "<<message<<endl;
                broadcast(message,clientSocket.client_socket);
            }
        }
        //以上是处理上一次残留的消息的内容
        char buffer[1024];
        ssize_t len=recv(clientSocket.client_socket,buffer,1023,0);

        if (len>0) {
            cache.append(buffer,len);
             pos=string::npos;
            while ((pos=cache.find('\n'))!=string::npos) {
                string message=cache.substr(0,pos);
                cache.erase(0,pos+1);
                if (message.rfind("CHAT|",0)==0) {
                    message=message.substr(5);//删除chat前缀
                    message=clientSocket.client_name+": "+message;
                    cout<<"Received "<<message<<endl;
                    broadcast(message,clientSocket.client_socket);
                }
            }
        }
        if (len==0) {
            cout << "Connection closed" << endl;
            struct_client tmpclient={clientSocket.client_socket,"tmpstr"};
            auto erasepos=std::find(clients.begin(),clients.end(),tmpclient);
            clients.erase(erasepos);
            break;
        }
        if (len<0) {
            cout << "Receive error" << endl;
            struct_client tmpclient={clientSocket.client_socket,"tmpstr"};
            auto erasepos=std::find(clients.begin(),clients.end(),tmpclient);
            clients.erase(erasepos);
            break;
        }
    }
    close(clientSocket.client_socket);
    
}
void broadcast(string& message,int sendersocket) {

   string  sendmessage=message+'\n';

    for (auto client: clients) {
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
