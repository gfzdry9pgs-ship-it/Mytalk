#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <vector>
using std::thread;
using std::string;
using std::cout;
using std::cin;
using std::endl;
using std::vector;
vector<int> clients;
void broadcast(string& message,int sendersocket);
//this is server.
void handleclient(int clientSocket) {
    string cache;
    while (1) {
        char buffer[1024];
        ssize_t len=recv(clientSocket,buffer,1023,0);

        if (len>0) {
            cache.append(buffer,len);
            size_t pos=string::npos;
            while ((pos=cache.find('\n'))!=string::npos) {
                string message=cache.substr(0,pos);
                cache.erase(0,pos+1);
                cout<<"Received:"<<std::to_string(clientSocket)<<": "<<message<<endl;
                broadcast(message,clientSocket);
            }
        }
        if (len==0) {
            cout << "Connection closed" << endl;
            break;
        }
        if (len<0) {
            cout << "Receive error" << endl;
            break;
        }
    }
}
void broadcast(string& message,int sendersocket) {
    string sendmessage=std::to_string(sendersocket);
    sendmessage+=": ";
    sendmessage+=message;
    sendmessage+='\n';
    for (int clinent: clients) {
        if (clinent != sendersocket) {
            send(clinent,sendmessage.c_str(),sendmessage.length(),0);
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
        clients.push_back(clientSocket);
        thread handleclinentThread(handleclient,clientSocket);
        handleclinentThread.detach();
    }




    close(serverSocket);
    return 0;

}
