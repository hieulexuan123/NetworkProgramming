#include <bits/stdc++.h>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include "utilities.h"

using namespace std;

#define MAXLINE 4096 /*max text line length*/
#define SERV_PORT 3002 /*port*/

void authenticate(int connfd);
void receiveStartSignal(int connfd);
void questionAnswer(int connfd);
bool getInputWithCountdown(string &input, int time_limit);

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
    receiveStartSignal(sockfd);
    questionAnswer(sockfd);
    cout << "Finish\n";
}

bool getInputWithCountdown(std::string &input, int time_limit) {
    fd_set readfds;
    struct timeval timeout;

    // Print the prompt once outside the loop
    std::cout << "Enter your choice: ";

    for (int remaining_time = time_limit; remaining_time > 0; --remaining_time) {
        // Clear the file descriptor set and set stdin for monitoring
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        // Set up the timeout to 1 second for each loop iteration
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // Display the countdown timer on the same line without newline
        std::cout << "\rTime remaining: " << remaining_time << " seconds... Enter your choice: " << std::flush;

        // Wait for 1 second or until input is detected
        int result = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);

        if (result == -1) {
            perror("select error");
            return false;
        } else if (result > 0) {
            // Input is available within the countdown time
            cin >> input;
            return true;
        }
    }

    // If time runs out, display timeout message
    std::cout << "\nTime's up for this question!\n";
    return false;
}

void questionAnswer(int connfd){
    while (true){
        char recv_buffer[MAXLINE];
        cout << "Waiting question from server ...\n";
        if(recv(connfd, recv_buffer, MAXLINE, 0) < 0) {
            perror("[-] Failed to receive response from server\n");
            exit(0);
        }
        string recv_mess_str(recv_buffer);
        cout << "Question from server: " << recv_mess_str << endl;
        vector<string> parts = split(recv_mess_str, ";");
        if (parts[0]=="QUEST"){
            int time_limit = stoi(parts[parts.size()-1]);
            string choice;

            cout << parts[1] << endl;
            for (int i=2; i<parts.size()-1; i++){
                cout << i-1 << ") " << parts[i] << endl;
            }
            // cout << "Enter your choice: ";
            bool answered = getInputWithCountdown(choice, time_limit);
            if (answered && !choice.empty()) {
                cout << "Answer successfully. Your choice is " << choice << endl;
            } else {
                std::cout << "You did not answer this question.\n";
            }
        }
    }
};

void receiveStartSignal(int connfd){
    char recv_buffer[MAXLINE];
    cout << "Waiting server to start game ...\n";
    while (true){
        if(recv(connfd, recv_buffer, MAXLINE, 0) < 0) {
            perror("[-] Failed to receive response from server\n");
            exit(0);
        }
        string recv_mess_str(recv_buffer);
        cout << "Signal from server: " << recv_mess_str << endl;
        if (recv_mess_str == "startSS") {
            cout << "Game start ...\n";
            return;
        }
    }
}

void authenticate(int connfd){
    char send_buffer[MAXLINE];
    char recv_buffer[MAXLINE];
    string username;
    string password;    

    bool isLoggedIn = false;
    string choice;

    while (!isLoggedIn){
        cout << "Choose:\n1.Register\n2.Log in\n-------------------------\n";
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