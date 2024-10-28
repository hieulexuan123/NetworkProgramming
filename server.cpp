#include <bits/stdc++.h>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include "structure.h"
#include "utilities.h"

using namespace std;

#define MAXLINE 4096 /*max text line length*/
#define SERV_PORT 3000 /*port*/
#define MAX_CLIENTS 100 /*maximum number of client connections */

map<string, string> userDb;
mutex dbMutex;

void handleClient(ClientInfo player);

int main (int argc, char **argv)
{
    int listenfd, connfd, n;
    pid_t childpid;
    socklen_t clilen;
    char buf[MAXLINE];
    struct sockaddr_in cliaddr, servaddr;

    //load user account database
    userDb = loadUserData("data/user_account.txt");
        
    //creation of the socket
    listenfd = socket (AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("[-]Socket creation failed ... \n");
        exit(0);
    }

    //preparation of the socket address 
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
        
    // Bind socket to server address
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        perror("[-]Socket bind failed ... \n");
        exit(0);
    }
        
    listen (listenfd, MAX_CLIENTS);
        
    cout << "Server is running";
        
    for ( ; ; ) {
            
        clilen = sizeof(cliaddr);
        connfd = accept (listenfd, (struct sockaddr *) &cliaddr, &clilen);
        if (connfd < 0) {
            cout << "[-]Connection failed\n";
            continue;
        }
        
        string client_info = (string)inet_ntoa(cliaddr.sin_addr) + ":" + to_string(ntohs(cliaddr.sin_port));
        cout << client_info << " has connected to the server\n";

        // Create player info
        struct ClientInfo player;
        player.connfd = connfd;
        player.client_info = client_info;

        thread th_client(handleClient, player);
        th_client.detach();
        
    }
    //close listening socket
    close (listenfd); 
}

void handleClient(ClientInfo player){
    int connfd = player.connfd;
    
    bool isLoggedIn = false;
    while(!isLoggedIn){
        char send_buffer[MAXLINE];
        char recv_buffer[MAXLINE];

        int recv_code = recv(connfd, recv_buffer, MAXLINE,0);

        if (recv_code == 0) {
            cout << player.client_info << " has disconnected\n";
            return;
        }
        else if(recv_code < 0) {
            perror("");
            return;
        }

        string recv_mess_str(recv_buffer);
        vector<string> parts = split(recv_mess_str, ";");
        if (parts.size() !=3 ) {
            sprintf(send_buffer, "FAIL;wrong_format");
        }
        else if (parts[0] == "REGIS"){
            string username = parts[1];
            string password = parts[2];

            dbMutex.lock();
            if (userDb.find(username) == userDb.end()) {
                userDb[username] = password;
                saveUserData("data/user_account.txt", username, password); 
                sprintf(send_buffer, "SUCCESS");
                isLoggedIn = true;
            } else {
                sprintf(send_buffer, "FAIL;account_exist");
            }
            dbMutex.unlock();
        } else if (parts[0] == "LOGIN") {
            string username = parts[1];
            string password = parts[2];

            dbMutex.lock();
            if (userDb.find(username) != userDb.end() && userDb[username] == password) {
                sprintf(send_buffer, "SUCCESS");
                isLoggedIn = true;
            } else {
                sprintf(send_buffer, "FAIL;invalid_credential");
            }
            dbMutex.unlock();
        } else {
            sprintf(send_buffer, "FAIL;unauthorized");
        }

        if (send(connfd, send_buffer, MAXLINE, 0) < 0) {
            perror("[-]Failed to send response\n");
        }
    }
    
    close(connfd);
}

