#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include  <string>
#include <thread>
#include <sstream>
using std::endl;
using std::thread;
using std::stringstream;
using std::cout;
using std::cin;
using std::string;
void reveivemessage(SOCKET clientSocket) {
    string cache;
    while (1) {
        char recvbuf[1024];
        int recvlen=recv(clientSocket,recvbuf,1023,0);
        if (recvlen>0) {
            cache.append(recvbuf,recvlen);
           size_t pos=string::npos;
           while ((pos=cache.find('\n'))!=string::npos  ) {
               string message=cache.substr(0,pos);
               cache.erase(0,pos+1);
               cout<<"Received message: "<<message<<endl;

             }


        }
        else if (recvlen==0) {
            cout << "Connection closed" << endl;
            break;
        }
        else {
            cout<<"Received error: "<<WSAGetLastError()<<endl;
            break;
        }
    }
}
int main() {
    cout<<"Enter Password: ";
    string password;
    cin>>password;
    WSADATA wsaData;
   int ret= WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0) {
        cout << "WSAStartup() failed with error: " << ret << endl;
        return 1;
    }
    cout << "WSA version: " << wsaData.wVersion << endl;
    sockaddr_in server={};
    server.sin_family=AF_INET;
    server.sin_port=htons(8888);
    server.sin_addr.s_addr=inet_addr("122.51.213.119");
    SOCKET clientSocket=socket(AF_INET,SOCK_STREAM,0);
    if (clientSocket == INVALID_SOCKET) {
        cout << "socket() failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;

    }
    cout << "Socket created" << endl;
    ret = connect(clientSocket,(struct sockaddr*)&server,sizeof(server));
    if (ret == SOCKET_ERROR) {
        cout << "connect() failed with error: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;

    }
    cout << "Connection established" << endl;
    cout<<"Please Enter Your Name: ";
    string name;
    cin>>name;
    password="AUTH|"+password+'\n';
    int sendauthlen=send(clientSocket,password.c_str(),password.length(),0);
    if (sendauthlen < 0) {
        cout << "send() failed with error: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    name="NAME|"+name+'\n';
    int sendnamelen=send(clientSocket,name.c_str(),name.length(),0);
    if (sendnamelen < 0) {
        cout << "send() failed with error: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    thread recvThread(reveivemessage,clientSocket);
    while (true) {
        string reply;

        if (!getline(cin,reply)) {
            break;
        }
        if (reply.empty()) {
            continue;
        }
        stringstream replyStream(reply);
        string command;
        replyStream >> command;
        if (command == "/msg") {
            string msgname;
            replyStream >> msgname;
            string message;
            getline(replyStream>>std::ws,message);
            if (msgname.empty()||message.empty()) {
                continue;
            }
            reply="MSG|"+msgname+'|'+message+'\n';
        }
        else if (command=="/list") {
                reply="LIST|\n";
                }
            else {
                reply="CHAT|"+reply+'\n';
            }

        bool quitting=false;
        if (reply == "CHAT|quit\n") {
          quitting=true;
        }
        int sendlen = send(clientSocket,reply.c_str(),reply.length(),0);
        if (sendlen < 0) {
            cout << "send() failed with error: " << WSAGetLastError() << endl;
            break;
        }
        if (quitting) {
            break;
        }
    }
    shutdown(clientSocket,SD_BOTH);
    recvThread.join();
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}