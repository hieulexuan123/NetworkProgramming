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
int getInputWithCountdown(string &input, int time_limit, bool is_main, int round, int connfd, string& recv_mess_str);
bool sendSkipRequest(int connfd, int round);
void getFinalResult(int connfd);

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
    getFinalResult(sockfd);
}

void getFinalResult(int connfd){
    char recv_buffer[MAXLINE];
    memset(recv_buffer, 0, sizeof(recv_buffer));

    std::cout << "Waiting for final result...\n";

    if(recv(connfd, recv_buffer, MAXLINE, 0) <= 0) {
        perror("[-] Failed to receive response from server\n");
        exit(0);
    }
    // string recv_mess_str(recv_buffer);
    string recv_mess_str(recv_buffer);
    std::cout << "[-] " << recv_mess_str << endl;
    vector<string> parts = split(recv_mess_str, ";");
    if (parts[0]=="FRESULT"){
        int is_winner = stoi(parts[3]);
        string winner_name = parts[2];
        if (is_winner==1){
            std::cout << "You are the winner. Congratulation\n";
        } else {
            if (winner_name=="NONE"){
                std::cout << "Noone wins\n";
            } else {
                std:: cout << "Winner is " << winner_name << endl;
            }
        }
    }
    std::cout << "Game ends\n";
}

bool sendSkipRequest(int connfd, int round){
    char send_buffer[MAXLINE];
    char recv_buffer[MAXLINE];
    memset(send_buffer, 0, sizeof(send_buffer));
    memset(recv_buffer, 0, sizeof(recv_buffer));

    sprintf(send_buffer, "SKIP;%s", to_string(round).c_str());
    if (send(connfd, send_buffer, strlen(send_buffer), 0) < 0)
        perror("[-] Failed to send message to server\n");

    memset(recv_buffer, 0, sizeof(recv_buffer));
    if(recv(connfd, recv_buffer, MAXLINE, 0) <= 0) {
        perror("[-] Failed to receive response from server\n");
        exit(0);
    }
    string recv_mess_str(recv_buffer);
    std::cout << "[-] " << recv_mess_str << endl;
    vector<string> parts = split(recv_mess_str, ";");
    if (parts[0]=="SKIP_RES"){
        int res = stoi(parts[1]);
        if (res==1){
            std::cout << "Skip question successfully\n";
            return true;
        } else if (res==2){
            std::cout << "You must be main player to skip question\n";
        } else if (res==3){
            std::cout << "You already skipped 2 questions\n";
        } else if (res==4){
            std::cout << "Timeout\n";
        } else if (res==5){
            std::cout << "You cannot skip question in initial round\n";
        }
    }
    return false;
}

int getInputWithCountdown(std::string &input, int time_limit, bool is_main, int round, int connfd, std::string &recv_mess_str) {
    fd_set readfds;
    struct timeval timeout;
    char recv_buffer[MAXLINE];

    // Print the prompt once outside the loop
    if (is_main){
        std::cout << "Enter 's' to skip questions\n";
    }

    std::cout << "Enter your choice: ";

    for (int remaining_time = time_limit; remaining_time > 0; --remaining_time) {
        // Clear the file descriptor set and set stdin for monitoring
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(connfd, &readfds);

        // Set up the timeout to 1 second for each loop iteration
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // Display the countdown timer on the same line without newline
        std::cout << "\rTime remaining: " << remaining_time << " seconds... Enter your choice: " << std::flush;

        // Wait for 1 second or until input is detected
        int result = select(max(STDIN_FILENO, connfd) + 1, &readfds, nullptr, nullptr, &timeout);

        if (result == -1) {
            perror("select error");
            return false;
        } else if (result > 0) {
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                cin >> input;

                if (input=="s" && sendSkipRequest(connfd, round)){
                    return 2; // choose to skip
                } else if (isNumber(input)){
                    return 1; // answer
                }
            }

            if (FD_ISSET(connfd, &readfds)) {
                memset(recv_buffer, 0, sizeof(recv_buffer));
                if (recv(connfd, recv_buffer, MAXLINE, 0) <= 0) {
                    perror("[-] Failed to receive response from server\n");
                    exit(0);
                }
                recv_mess_str.assign(recv_buffer);
                std::cout << "\n[-] " << recv_mess_str << std::endl;
                vector<string> parts = split(recv_mess_str, ";");
                if (parts[0]=="RRESULT"){
                    if (stoi(parts[2])==2){
                        std::cout << "Question is skipped by main player\n";
                    }
                    return 3; // question is skipped by main player
                }
            }          
        }
    }

    // If time runs out, display timeout message
    std::cout << "Time's up for this question!\n";
    return 0; // not answer
}

void questionAnswer(int connfd){
    char send_buffer[MAXLINE];
    char recv_buffer[MAXLINE];
    string recv_mess_str;
    bool is_main = false;
    
    while (true){
        memset(send_buffer, 0, sizeof(send_buffer));
        memset(recv_buffer, 0, sizeof(recv_buffer));

        std::cout << "Waiting question from server ...\n";
        if(recv(connfd, recv_buffer, MAXLINE, 0) <= 0) {
            perror("[-] Failed to receive response from server\n");
            exit(0);
        }

        // string recv_mess_str(recv_buffer);
        recv_mess_str.assign(recv_buffer);
        std::cout << "[-] " << recv_mess_str << endl;
        vector<string> parts = split(recv_mess_str, ";");

        if (parts[0]=="QUEST"){
            int round = stoi(parts[1]);
            int time_limit = stoi(parts[2]);
            string choice;
            
            std::cout << "Round " << round << endl; 
            std::cout << parts[3] << endl;
            for (int i=4; i<parts.size(); i++){
                std::cout << i-3 << ") " << parts[i] << endl;
            }

            int answered_status = getInputWithCountdown(choice, time_limit, is_main, round, connfd, recv_mess_str);
            if (answered_status==1 && !choice.empty()) {
                sprintf(send_buffer, "ANS;%s;%s", parts[1].c_str(), choice.c_str());
                if (send(connfd, send_buffer, strlen(send_buffer), 0) < 0)
                    perror("[-] Failed to send message to server\n");
                std::cout << "Answer successfully. Your choice is " << send_buffer << endl;

                memset(recv_buffer, 0, sizeof(recv_buffer));
                if(recv(connfd, recv_buffer, MAXLINE, 0) <= 0) {
                    perror("[-] Failed to receive response from server\n");
                    exit(0);
                }
                // string recv_mess_str(recv_buffer);
                recv_mess_str.assign(recv_buffer);
                std::cout << "[-] " << recv_mess_str << endl;
                vector<string> parts = split(recv_mess_str, ";");

                if (parts[0]=="ANS_RES"){
                    if (stoi(parts[1])==1){
                        std::cout << "Send choice successfully!\n";
                    } else if (stoi(parts[1])==2){
                        std::cout << "Timeout\n";
                    }
                }
            } else if (answered_status==0){
                std::cout << "You did not answer this question.\n";
            }

            // Receive result from server
            std::cout << "Waiting result from server...\n";
            if (answered_status!=3){
                memset(recv_buffer, 0, sizeof(recv_buffer));
                if(recv(connfd, recv_buffer, MAXLINE, 0) <= 0) {
                    perror("[-] Failed to receive response from server\n");
                    exit(0);
                }

                // string recv_mess_str(recv_buffer);
                recv_mess_str.assign(recv_buffer);
                std::cout << "[-] " << recv_mess_str << endl;
            }

            vector<string> parts = split(recv_mess_str, ";");
            if (parts[0]=="RRESULT"){
                int user_status = stoi(parts[1]);
                int round_status = stoi(parts[2]);
                int game_status = stoi(parts[3]);
                int point = stoi(parts[4]);

                if (user_status==1){
                    is_main = true;
                    if (round==0 || round_status==2){
                        std::cout << "You are the main player in next round\n";
                    } else std::cout << "You answer correctly. You are the main player in next round\n";
                } else if (user_status==2){
                    is_main = false;
                    if (round==0 || round_status==2){
                        std::cout << "You are the secondary player in next round\n";
                    } else std::cout << "You answer correctly. You are secondary player in next round\n";
                } else if (user_status==3){
                    is_main = false;
                    std::cout << "You answer wrong. You are eliminated\n";
                    break;
                }
                std::cout << "Your point: " << point << endl;
                if (game_status==2){
                    break;
                }
            }
        }
        std::cout << "--------------------------------\n"; 
    }
};

void receiveStartSignal(int connfd){
    char recv_buffer[MAXLINE];
    std::cout << "Waiting server to start game ...\n";
    while (true){
        memset(recv_buffer, 0, sizeof(recv_buffer));
        if(recv(connfd, recv_buffer, MAXLINE, 0) <= 0) {
            perror("[-] Failed to receive response from server\n");
            exit(0);
        }
        string recv_mess_str(recv_buffer);
        std::cout << "[-] " << recv_mess_str << endl;
        if (recv_mess_str == "start") {
            std::cout << "Game start ...\n";
            return;
        }
    }
}

void authenticate(int connfd){
    char send_buffer[MAXLINE];
    char recv_buffer[MAXLINE];
    string username;
    string password;    

    string choice;

    while (true){
        std::cout << "Choose:\n1.Register\n2.Log in\n-------------------------\n";
        cin >> choice;

        if (choice=="1"){
            std::cout << "Register\n";
            std::cout << "Enter username: ";
            cin >> username;
            std::cout << "Enter password: ";
            cin >> password;

            sprintf(send_buffer, "REGIS;%s;%s", username.c_str(), password.c_str());
        } else if (choice=="2"){
            std::cout << "Log in\n";
            std::cout << "Enter username: ";
            cin >> username;
            std::cout << "Enter password: ";
            cin >> password;
            memset(send_buffer, 0, sizeof(send_buffer));
            sprintf(send_buffer, "LOGIN;%s;%s", username.c_str(), password.c_str());
        } else {
            continue;
        }
    
        if(send(connfd, send_buffer, MAXLINE,0) < 0) {
            perror("[-] Failed to send message to server\n");
            exit(0);
        }
        memset(recv_buffer, 0, sizeof(recv_buffer));
        if(recv(connfd, recv_buffer, MAXLINE, 0) <= 0) {
            perror("[-] Failed to receive response from server\n");
            exit(0);
        }

        std::cout << "[-] " << recv_buffer << endl;
        string recv_mess_str(recv_buffer);
        vector<string> parts = split(recv_mess_str, ";");

        if (parts[0]=="REGIS_RES"){
            if (stoi(parts[1])==1){
                std::cout << "Create account successfully\n";
            } else if (stoi(parts[1])==2){
                std::cout << "Fail. Account is existing\n";
            }          
        }
        else if (parts[0]=="LOGIN_RES"){
            if (stoi(parts[1])==1){
                std::cout << "Logged in successfully\n";
                break;
            } else if (stoi(parts[1])==2){
                std::cout << "You are already logged in\n";
            } else if (stoi(parts[1])==3){
                std::cout << "Fail. Game is already started. Please wait until game ends\n";
            } else if (stoi(parts[1])==4){
                std::cout << "Fail. Wrong username or password\n";
            }
        }
    }
}