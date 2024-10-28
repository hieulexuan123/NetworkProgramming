#include <bits/stdc++.h>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include "utilities.h"

using namespace std;

#define MAXLINE 4096 /*max text line length*/
#define SERV_PORT 3000 /*port*/

void authenticate(int connfd);

int main(int argc, char **argv) 
{
    int sockfd;
    struct sockaddr_in servaddr;
    char sendline[MAXLINE], recvline[MAXLINE];
        
    //basic check of the arguments
    //additional checks can be inserted
    if (argc !=2) {
        perror("Usage: TCPClient <IP address of the server"); 
        exit(1);
    }
        
    //Create a socket for the client
    //If sockfd<0 there was an error in the creation of the socket
    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) <0) {
        perror("Problem in creating the socket");
        exit(2);
    }
        
    //Creation of the socket
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr= inet_addr(argv[1]);
    servaddr.sin_port =  htons(SERV_PORT); //convert to big-endian order
        
    //Connection of the client to the socket 
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))<0) {
        perror("Problem in connecting to the server");
        exit(3);
    }
        
    authenticate(sockfd);
}

void authenticate(int connfd){
    char send_buffer[MAXLINE];
    char recv_buffer[MAXLINE];
    string username;
    string password;    

    bool isLoggedIn = false;
    string choice;

    while (!isLoggedIn){
        cout << "Choose:\n1.Register\n2.Log in\n-------------------------";
        cin >> choice;

        if (choice=="1"){
            cout << "Register\n";
            cout << "Enter username: ";
            cin >> username;
            cout << "Enter password: ";
            cin >> password;

            sprintf(send_buffer, "REGIS;%s;%s", username.c_str(), password.c_str());
        } else if (choice=="2"){
            cout << "Log in\n";
            cout << "Enter username: ";
            cin >> username;
            cout << "Enter password: ";
            cin >> password;

            sprintf(send_buffer, "LOGIN;%s;%s", username.c_str(), password.c_str());
        } else {
            continue;
        }

        if(send(connfd, send_buffer, MAXLINE,0) < 0) {
            perror("[-] Failed to send message to server\n");
            exit(0);
        }

        if(recv(connfd, recv_buffer, MAXLINE, 0) < 0) {
            perror("[-] Failed to receive response from server\n");
            exit(0);
        }

        cout << "Response: " << recv_buffer << endl;
        string recv_mess_str(recv_buffer);
        vector<string> parts = split(recv_mess_str, ";");
        if (parts[0]=="SUCCESS"){
            isLoggedIn = true;
        } 
    }
    cout << "Logged in successfully\n";
}