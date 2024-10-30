#include <bits/stdc++.h>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include <sys/select.h>
#include "structure.h"
#include "utilities.h"

using namespace std;

#define MAXLINE 4096 /*max text line length*/
#define SERV_PORT 3002 /*port*/
#define MAX_CLIENTS 100 /*maximum number of client connections */

const int TIME_LIMIT_INITIAL = 30;
const int TIME_LIMIT = 60;
const int TIME_LIMIT_MAIN = 60;
const int TIME_LIMIT_SECOND = 30;
const int TIME_BREAK = 10;

map<string, string> userDb;
mutex dbMutex;

vector<struct ClientInfo *> player_list;
struct Game game;

string handleLoginRequest(const vector<string>& parts, bool& isLoggedIn, map<string, string> userDb);
string handleRegisRequest(const vector<string>& parts, bool& isLoggedIn, map<string, string> userDb);
void handleClient(struct ClientInfo * player);
void askQuestionsLoop();
void broadcast_quest(string question_text, vector<string> answer_texts, bool is_first_round);

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

    //Listen to both listenfd and stdin
    fd_set readfds;
        
    cout << "Server is running\n";
    cout << "Is game start: " << game.isStart << endl;
        
    for ( ; ; ) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(listenfd, &readfds);

        int select_result = select(listenfd+1, &readfds, NULL, NULL, NULL);
        if (select_result == -1){
            perror("select");
            exit(0);
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)){
            string input;
            cin >> input;
            cout << "Input: " << input << endl;
            if (input=="start" && !game.isStart){
                cout << "Starting game...\n";
                game.isStart = true;
                char send_msg[] = "start";
                for (int i=0; i<player_list.size(); i++){
                    //Only send "start" message to logged in players
                    if (player_list[i]->isLoggedIn){
                        if (send(player_list[i]->connfd, send_msg, strlen(send_msg), 0) < 0)
                            perror("[-] Failed to send message to client\n");
                    }
                }

                thread th_questions(askQuestionsLoop);
                th_questions.detach();
            }
        }
        if (FD_ISSET(listenfd, &readfds)){
            if (player_list.size() >= MAX_CLIENTS) continue;

            clilen = sizeof(cliaddr);
            connfd = accept (listenfd, (struct sockaddr *) &cliaddr, &clilen);
            if (connfd < 0) {
                cout << "[-]Connection failed\n";
                continue;
            }
            
            string client_info = (string)inet_ntoa(cliaddr.sin_addr) + ":" + to_string(ntohs(cliaddr.sin_port));
            cout << client_info << " has connected to the server\n";

            // Create player info
            struct ClientInfo * player = new ClientInfo;
            player->connfd = connfd;
            player->client_info = client_info;

            // Add player info to player_list
            player_list.push_back(player);
            cout << "Number of players: " << player_list.size() << endl;

            thread th_client(handleClient, player);
            th_client.detach();
        }
            
        
    }
    //close listening socket
    close (listenfd); 
    cout << "Is game start: " << game.isStart << endl;
    cout << "Stop accepting connections\n";
}

void broadcast_quest(string question_text, vector<string> answer_texts, bool is_first_round = false){
    string send_msg = "QUEST;" + question_text;
    for (string answer_text : answer_texts){
        send_msg += ";" + answer_text;
    }

    for (int i=0; i<player_list.size(); i++){
        //Only send message to logged in and ineliminated players
        if (player_list[i]->isLoggedIn && !player_list[i]->is_eliminated){
            int time_limit;
            if (is_first_round){
                time_limit = TIME_LIMIT_INITIAL;
            } else if (player_list[i]->is_main_player){
                time_limit = TIME_LIMIT_MAIN;
            } else {
                time_limit = TIME_LIMIT_SECOND;
            }
            string send_msg_with_time = send_msg + ";" + to_string(time_limit);
            if (send(player_list[i]->connfd, send_msg_with_time.c_str(), send_msg_with_time.length(), 0) < 0)
                perror("[-] Failed to send message to client\n");
        }
    }
}

void askQuestionsLoop(){

    std::this_thread::sleep_for(std::chrono::seconds(TIME_BREAK));

    string question_text = "What is result 1+9=";
    vector<string> answer_texts;
    answer_texts.push_back("10");
    answer_texts.push_back("9");
    answer_texts.push_back("8");
    answer_texts.push_back("7");

    game.correct_answer = 1;
    game.round = 0;

    cout << "Send question...\n";
    broadcast_quest(question_text, answer_texts);
}

void handleClient(struct ClientInfo * player){
    int connfd = player->connfd;
    
    bool isLoggedIn = false;

    while (true){
        char send_buffer[MAXLINE];
        char recv_buffer[MAXLINE];

        int recv_code = recv(connfd, recv_buffer, MAXLINE,0);

        if (recv_code == 0) {
            cout << player->client_info << " has disconnected\n";
            for (int i=0; i<player_list.size(); i++){
                if (player_list[i]->connfd == player->connfd){
                    player_list.erase(player_list.begin() + i);
                    delete player;
                    break;
                }
            }
            break;
        }
        else if(recv_code < 0) {
            perror("");
            break;
        }
        string recv_mess_str(recv_buffer);
        string send_mess_str;
        vector<string> parts = split(recv_mess_str, ";");
        
        if (parts[0]=="REGIS"){
            send_mess_str = handleRegisRequest(parts, isLoggedIn, userDb);
            player->isLoggedIn = isLoggedIn;
        } else if (parts[0]=="LOGIN"){
            send_mess_str = handleLoginRequest(parts, isLoggedIn, userDb);
            player->isLoggedIn = isLoggedIn;
        } else {
            send_mess_str = "FAIL;wrong_format";
        }
    
        strcpy(send_buffer, send_mess_str.c_str());
        if (send(connfd, send_buffer, MAXLINE, 0) < 0) {
            perror("[-]Failed to send response\n");
        }
    }
    // while(!isLoggedIn){
    //     char send_buffer[MAXLINE];
    //     char recv_buffer[MAXLINE];

    //     int recv_code = recv(connfd, recv_buffer, MAXLINE,0);

    //     if (recv_code == 0) {

    //         cout << player.client_info << " has disconnected\n";
    //         return;
    //     }
    //     else if(recv_code < 0) {
    //         perror("");
    //         return;
    //     }

    //     string recv_mess_str(recv_buffer);
    //     vector<string> parts = split(recv_mess_str, ";");
    //     if (parts.size() !=3 ) {
    //         sprintf(send_buffer, "FAIL;wrong_format");
    //     }
    //     else if (parts[0] == "REGIS"){
    //         string username = parts[1];
    //         string password = parts[2];

    //         dbMutex.lock();
    //         if (userDb.find(username) == userDb.end()) {
    //             userDb[username] = password;
    //             saveUserData("data/user_account.txt", username, password); 
    //             sprintf(send_buffer, "SUCCESS");
    //             isLoggedIn = true;
    //         } else {
    //             sprintf(send_buffer, "FAIL;account_exist");
    //         }
    //         dbMutex.unlock();
    //     } else if (parts[0] == "LOGIN") {
    //         string username = parts[1];
    //         string password = parts[2];

    //         dbMutex.lock();
    //         if (userDb.find(username) != userDb.end() && userDb[username] == password) {
    //             sprintf(send_buffer, "SUCCESS");
    //             isLoggedIn = true;
    //         } else {
    //             sprintf(send_buffer, "FAIL;invalid_credential");
    //         }
    //         dbMutex.unlock();
    //     } else {
    //         sprintf(send_buffer, "FAIL;unauthorized");
    //     }

    //     if (send(connfd, send_buffer, MAXLINE, 0) < 0) {
    //         perror("[-]Failed to send response\n");
    //     }
    
    close(connfd);
}

string handleRegisRequest(const vector<string>& parts, bool& isLoggedIn, map<string, string> userDb){
    string response;

    if (isLoggedIn){
        return "FAIL;already_authenticate";
    }
    if (parts.size() !=3 ) {
        return "FAIL;wrong_format";
    }

    string username = parts[1];
    string password = parts[2];

    dbMutex.lock();
    if (userDb.find(username) == userDb.end()) {
        userDb[username] = password;
        saveUserData("data/user_account.txt", username, password); 
        if (!game.isStart){
            response = "SUCCESS";
            isLoggedIn = true;
        } else {
            response = "FAIL;game_already_started";
        }
        
    } else {
        response = "FAIL;account_exist";
    }
    dbMutex.unlock();

    return response;
}

string handleLoginRequest(const vector<string>& parts, bool& isLoggedIn, map<string, string> userDb){
    string response;

    if (isLoggedIn){
        return "FAIL;already_authenticate";
    }
    if (parts.size() !=3 ) {
        return "FAIL;wrong_format";
    }

    string username = parts[1];
    string password = parts[2];

    dbMutex.lock();
    if (userDb.find(username) != userDb.end() && userDb[username] == password) {
        if (!game.isStart){
            response = "SUCCESS";
            isLoggedIn = true;
        } else {
            response = "FAIL;game_already_started";
        }
    } else {
        response = "FAIL;invalid_credential";
    }
    dbMutex.unlock();

    return response;
}

