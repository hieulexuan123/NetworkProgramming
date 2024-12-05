#include <bits/stdc++.h>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include <sys/select.h>
#include <chrono>
#include "structure.h"
#include "utilities.h"

using namespace std;

#define MAXLINE 4096 /*max text line length*/
#define SERV_PORT 3002 /*port*/
#define MAX_CLIENTS 100 /*maximum number of client connections */

const int TIME_LIMIT_INITIAL = 20;
const int TIME_LIMIT = 30;
const int TIME_LIMIT_MAIN = 30;
const int TIME_LIMIT_SECOND = 20;
const int TIME_BREAK = 10;

const int POINT_INITIAL = 10;
const int POINT_PER_CORRECT = 20;

map<string, string> userDb;
mutex dbMutex;

vector<struct ClientInfo *> player_list;
struct Game game;

string handleLoginRequest(const vector<string>& parts, struct ClientInfo* player);
string handleRegisRequest(const vector<string>& parts);
void handleClient(struct ClientInfo * player);
void askQuestionsLoop();
void broadcastQuest(string question_text, vector<string> answer_texts, int round);
string handleAnsRequest(const vector<string>& parts, struct ClientInfo * player);
bool broadcastResult(int round, int num_remain_questions);
int determineFastestPlayer();
int getRandomPlayer();
int sumPointWrongSecondaryPlayer();
int distributePoint(int main_player_point);
int determineGameStatus(int num_remain_questions);
string handleSkipRequest(const vector<string>& parts, struct ClientInfo* player);

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
 
    for ( ; ; ) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(listenfd, &readfds);

        int select_result = select(listenfd+1, &readfds, NULL, NULL, NULL);
        if (select_result == -1){
            perror("select");
            exit(0);
        }

        //handle input from keyboard
        if (FD_ISSET(STDIN_FILENO, &readfds)){
            string input;
            cin >> input;
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

        //handle connections from clients
        if (FD_ISSET(listenfd, &readfds)){
            if (player_list.size() >= MAX_CLIENTS) continue;

            clilen = sizeof(cliaddr);
            connfd = accept (listenfd, (struct sockaddr *) &cliaddr, &clilen);
            if (connfd < 0) {
                cout << "[-]Connection failed\n";
                continue;
            }
            
            string client_info = (string)inet_ntoa(cliaddr.sin_addr) + ":" + to_string(ntohs(cliaddr.sin_port));
            cout << "[-]" << client_info << " has connected to the server\n";

            // Create player info
            struct ClientInfo * player = new ClientInfo;
            player->connfd = connfd;
            player->client_info = client_info;

            // Add player info to player_list
            player_list.push_back(player);
            cout << "[-] Number of clients: " << player_list.size() << endl;

            thread th_client(handleClient, player);
            th_client.detach();
        }
            
        
    }
    //close listening socket
    close (listenfd); 
}

void broadcastQuest(string question_text, vector<string> answer_texts, int round){
    for (int i=0; i<player_list.size(); i++){
        // Only send message to logged in and alive players
        if (player_list[i]->isLoggedIn && !player_list[i]->is_eliminated){
            int time_limit;
            string send_msg = "QUEST";

            if (round==0){
                time_limit = TIME_LIMIT_INITIAL;
            } else if (player_list[i]->is_main_player){
                time_limit = TIME_LIMIT_MAIN;
            } else {
                time_limit = TIME_LIMIT_SECOND;
            }
            send_msg += ";" + to_string(round) + ";" + to_string(time_limit) + ";" + question_text;
            for (string answer_text : answer_texts){
                send_msg += ";" + answer_text;
            }

            if (send(player_list[i]->connfd, send_msg.c_str(), send_msg.length(), 0) < 0)
                perror("[-] Failed to send message to client\n");
        }
    }
}

int determineFastestPlayer(){
    int time_shortest = INT_MAX;
    int player_fastest_idx = -1;

    for (int i=0; i<player_list.size(); i++){
        struct ClientInfo * player = player_list[i];
        if (player->isLoggedIn && !player->is_eliminated && game.correct_answer == player->submitted_answer && player->time_answer < time_shortest){
            time_shortest = player->time_answer;
            player_fastest_idx = i;
        }
    }
    return player_fastest_idx;
}

int getRandomPlayer(){
    vector<int> player_indexes;
    for (int i=0; i<player_list.size(); i++){
        struct ClientInfo * player = player_list[i];
        if (player->isLoggedIn && !player->is_eliminated){
            player_indexes.push_back(i);
        }
    }
    return getRandom(player_indexes);
}

int sumPointWrongSecondaryPlayer(){
    int total = 0;
    for (int i=0; i<player_list.size(); i++){
        struct ClientInfo * player = player_list[i];
        if (player->isLoggedIn && !player->is_eliminated && !player->is_main_player && game.correct_answer != player->submitted_answer){
            total += player->point;
        }
    }
    return total;
}

int distributePoint(int main_player_point){
    int num_correct_secondary_players = 0;
    for (int i=0; i<player_list.size(); i++){
        struct ClientInfo * player = player_list[i];
        if (player->isLoggedIn && !player->is_eliminated && !player->is_main_player && game.correct_answer == player->submitted_answer){
            num_correct_secondary_players ++;
        }
    }
    if (num_correct_secondary_players == 0){
        return 0;
    }
    else return (int) (main_player_point / num_correct_secondary_players);
}

int determineGameStatus(int num_remain_questions){
    if (num_remain_questions==0){
        return 2; //game ends
    }

    int num_correct_players = 0;
    for (int i=0; i<player_list.size(); i++){
        struct ClientInfo * player = player_list[i];
        if (player->isLoggedIn && !player->is_eliminated && game.correct_answer == player->submitted_answer){
            num_correct_players ++;
        }
    }
    
    if (num_correct_players > 1){
        return 1; //game continues
    }
    else return 2;
}

bool broadcastResult(int round, int num_remain_questions){
    string send_msg;
    int user_status;
    int game_status;
    int round_status;

    // Initial round to determind role
    if (round==0){
        cout << "[-] Determine roles\n";
        game_status = 1;
        round_status = 1;

        int main_player_idx = determineFastestPlayer();
        // nobody answers correctly -> determine randomly
        if (main_player_idx == -1){
            cout << "[-] Nobody answers correctly. Randomly assign main player.\n";
            main_player_idx = getRandomPlayer();
        }
        cout << "[-] Main player idx: " << main_player_idx << endl;

        for (int i=0; i<player_list.size(); i++){
            struct ClientInfo * player = player_list[i];
            if (player->isLoggedIn && !player->is_eliminated){
                player->point += POINT_INITIAL; // everybody in first round gets initial point
                //  main player
                if (i == main_player_idx){
                    player->is_main_player = true;
                    game.main_player_idx = i;
                    user_status = 1;
                }
                // secondary players 
                else {
                    player->is_main_player = false;
                    user_status = 2;
                } 

                cout << player->name << ", point: " << to_string(player->point) << ", status: " << to_string(user_status) << endl;
                send_msg = "RRESULT;" + to_string(user_status) + ";" + to_string(round_status) + ";" + to_string(game_status) + ";" + to_string(player->point);
                
                //Send result to clients
                if (send(player->connfd, send_msg.c_str(), send_msg.length(), 0) < 0)
                    perror("[-] Failed to send message to client\n");

                //Reset answer
                player->submitted_answer = -1;
            }
        }

    } 
    // Other rounds
    else {
        // Main player skips question 
        if (game.is_skipped){
            game_status = 1;
            round_status = 2;
            cout << "[-] Round: " << round << " Main player skips\n";

            int num_secondary_players = 0;
            for (int i=0; i<player_list.size(); i++){
                struct ClientInfo * player = player_list[i];
                if (player->isLoggedIn && !player->is_eliminated && !player->is_main_player){
                    num_secondary_players ++;
                }
            }
            int secondary_added_point = player_list[game.main_player_idx]->point / (2*num_secondary_players);
            cout << "Secondary player receive " << secondary_added_point << endl; 

            for (int i=0; i<player_list.size(); i++){
                struct ClientInfo * player = player_list[i];
                if (player->isLoggedIn && !player->is_eliminated){
                    //  main player
                    if (player->is_main_player){
                        player->point /= 2;
                        player->is_main_player = true;
                        game.main_player_idx = i;
                        user_status = 1;
                    }
                    // secondary players
                    else {
                        player->point += secondary_added_point;
                        player->is_main_player = false;
                        user_status = 2;
                    } 

                    cout << player->name << ", point: " << to_string(player->point) << ", status: " << to_string(user_status) << endl;
                    send_msg = "RRESULT;" + to_string(user_status) + ";" + to_string(round_status) + ";" + to_string(game_status) + ";" + to_string(player->point);
                    
                    //Send result to clients
                    if (send(player->connfd, send_msg.c_str(), send_msg.length(), 0) < 0)
                        perror("[-] Failed to send message to client\n");

                    //Reset answer
                    player->submitted_answer = -1;
                }
            }
        }
        // main player answers correctly
        else if (player_list[game.main_player_idx]->submitted_answer == game.correct_answer){
            game_status = determineGameStatus(num_remain_questions);
            round_status = 1;

            cout << "[-] Round: " << round << " Main player answer correctly\n";
            int main_player_added_point = sumPointWrongSecondaryPlayer();
            cout << "Main player receive " << main_player_added_point << endl;
            for (int i=0; i<player_list.size(); i++){
                struct ClientInfo * player = player_list[i];
                if (player->isLoggedIn && !player->is_eliminated){
                    //  main player
                    if (player->is_main_player){
                        player->point += POINT_PER_CORRECT + main_player_added_point;
                        player->is_main_player = true;
                        game.main_player_idx = i;
                        user_status = 1;
                    }
                    // secondary players who answer correctly
                    else if (player->submitted_answer == game.correct_answer){
                        player->point += POINT_PER_CORRECT;
                        player->is_main_player = false;
                        user_status = 2;
                    } 
                    // players who answer wrong
                    else {
                        player->is_eliminated = true;
                        user_status = 3;
                    }

                    cout << player->name << ", point: " << to_string(player->point) << ", status: " << to_string(user_status) << endl;
                    send_msg = "RRESULT;" + to_string(user_status) + ";" + to_string(round_status) + ";" + to_string(game_status) + ";" + to_string(player->point);
                    //Send result to clients
                    if (send(player->connfd, send_msg.c_str(), send_msg.length(), 0) < 0)
                        perror("[-] Failed to send message to client\n");

                    //Reset answer
                    player->submitted_answer = -1;
                }
            }
        }
        // main player answers wrong
        else {
            game_status = determineGameStatus(num_remain_questions);
            round_status = 1; 

            cout << "[-] Round: " << round << " Main player answer wrong\n";

            int player_fastest_idx = determineFastestPlayer();
            int main_player_point = player_list[game.main_player_idx]->point;
            int distributed_point = distributePoint(main_player_point);
            cout << "Correct secondary player receive " << distributed_point << endl;

            for (int i=0; i<player_list.size(); i++){
                struct ClientInfo * player = player_list[i];
                if (player->isLoggedIn && !player->is_eliminated){
                    //  fastest secondary player
                    if (i == player_fastest_idx){
                        player->point += distributed_point;
                        player->is_main_player = true;
                        game.main_player_idx = i;
                        user_status = 1;
                    }
                    // secondary players who answer correctly
                    else if (player->submitted_answer == game.correct_answer){
                        player->point += distributed_point;
                        player->is_main_player = false;
                        user_status = 2;
                    } 
                    // players who answer wrong
                    else {
                        player->is_eliminated = true;
                        user_status = 3;
                    }

                    cout << player->name << ", point: " << to_string(player->point) << ", status: " << to_string(user_status) << endl;
                    send_msg = "RRESULT;" + to_string(user_status) + ";" + to_string(round_status) + ";" + to_string(game_status) + ";" + to_string(player->point);
                    
                    //Send result to clients
                    if (send(player->connfd, send_msg.c_str(), send_msg.length(), 0) < 0)
                        perror("[-] Failed to send message to client\n");

                    //Reset answer
                    player->submitted_answer = -1;
                }
            }
        }
    }
    if (game_status == 1){
        return true; // game continues
    } else return false; // game ends
}

void askQuestionsLoop(){
    vector<Question> questions = loadQuestionBank("data/question_bank.txt");
    int num_remain_questions = questions.size();
    bool game_continued;

    // ask first question to determine role
    std::this_thread::sleep_for(std::chrono::seconds(TIME_BREAK));

    game.correct_answer = questions[0].correct_idx;
    game.round = 0;
    cout << "[-] Send question...\n";
    broadcastQuest(questions[0].question_text, questions[0].answer_texts, game.round);
    game.ckpt_send_quest = std::chrono::system_clock::now();

    std::this_thread::sleep_for(std::chrono::seconds(TIME_LIMIT_INITIAL));
    cout << "[-] Send result...\n";
    broadcastResult(game.round, --num_remain_questions);

    // ask subsequent questions until game ends
    for (int i=1; i<questions.size(); i++){       
        std::this_thread::sleep_for(std::chrono::seconds(TIME_BREAK));

        game.correct_answer = questions[i].correct_idx;
        game.round ++;
        cout << "[-] Send question...\n";
        broadcastQuest(questions[i].question_text, questions[i].answer_texts, game.round);
        game.ckpt_send_quest = std::chrono::system_clock::now();

        // std::this_thread::sleep_for(std::chrono::seconds(TIME_LIMIT));
        for (int remaining_time = TIME_LIMIT; remaining_time > 0; --remaining_time) {
            // Check if a skip occurred during the countdown
            if (game.is_skipped) {
                // game.skip_occurred = false; // Reset the skip flag
                cout << "[-] Skip detected. Sending result immediately.\n";
                game_continued = broadcastResult(game.round, --num_remain_questions);
                break; // Exit the countdown loop to proceed to the next question
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // If no skip occurred, send the result after the countdown
        if (!game.is_skipped) {
            cout << "[-] Send result...\n";
            game_continued = broadcastResult(game.round, --num_remain_questions);
        }

        game.is_skipped = false; // reset is_skipped
        if (!game_continued){
            break;
        }
    }
    cout << "[-] Game ends\n";
}

void handleClient(struct ClientInfo * player){
    int connfd = player->connfd;
    char send_buffer[MAXLINE];
    char recv_buffer[MAXLINE];

    while (true){
        memset(send_buffer, 0, sizeof(send_buffer));
        memset(recv_buffer, 0, sizeof(recv_buffer));
        int recv_code = recv(connfd, recv_buffer, MAXLINE,0);

        //player disconnects
        if (recv_code == 0) {
            cout << "[-]" << player->client_info << " has disconnected\n";
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
        cout << recv_mess_str << endl;
        
        if (parts[0]=="REGIS"){
            send_mess_str = handleRegisRequest(parts);
        } else if (parts[0]=="LOGIN"){
            send_mess_str = handleLoginRequest(parts, player);
        } else if (parts[0]=="ANS"){
            send_mess_str = handleAnsRequest(parts, player); 
        } else if (parts[0]=="SKIP"){
            send_mess_str = handleSkipRequest(parts, player);
        } else {
            send_mess_str = "FAIL;wrong_format";
        }
    
        strcpy(send_buffer, send_mess_str.c_str());
        if (send(connfd, send_buffer, MAXLINE, 0) < 0) {
            perror("[-]Failed to send response\n");
        }
    }
    
    
    close(connfd);
}

string handleRegisRequest(const vector<string>& parts){
    string response;

    if (parts.size() !=3 ) {
        return "FAIL;wrong_format";
    }

    string username = parts[1];
    string password = parts[2];

    dbMutex.lock();
    if (userDb.find(username) == userDb.end()) {
        userDb[username] = password;
        saveUserData("data/user_account.txt", username, password);
        //success
        response = "REGIS_RES;1";
    } else {
        //fail, existing account
        response = "REGIS_RES;2";
    }
    dbMutex.unlock();

    return response;
}

string handleLoginRequest(const vector<string>& parts, struct ClientInfo* player){
    string response;

    //fail, already logged in
    if (player->isLoggedIn){
        return "LOGIN_RES;2";
    }
    if (parts.size() !=3 ) {
        return "FAIL;wrong_format";
    }

    string username = parts[1];
    string password = parts[2];

    dbMutex.lock();
    if (userDb.find(username) != userDb.end() && userDb[username] == password) {
        //success
        if (!game.isStart){
            for (int i=0; i<player_list.size(); i++){
                struct ClientInfo * player = player_list[i];
                if (player->name == username && player->isLoggedIn){
                    dbMutex.unlock();
                    return "LOGIN_RES;2";
                }
            }
            player->isLoggedIn = true;
            player->name = username;
            response = "LOGIN_RES;1";
        //fail, game already started
        } else {
            response = "LOGIN_RES;3";
        }
    //fail, wrong username or password
    } else {
        player->isLoggedIn = false;
        response = "LOGIN_RES;4";
    }
    dbMutex.unlock();

    return response;
}

string handleAnsRequest(const vector<string>& parts, struct ClientInfo * player){
    // unauthorized
    if (!player->isLoggedIn){
        return "ANS_RES;3";
    }

    int round = stoi(parts[1]);
    // timeout
    if (round != game.round){
        return "ANS_RES;2";
    }

    player->submitted_answer = stoi(parts[2]);
    // player->round = round;
    player->time_answer = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - game.ckpt_send_quest).count();
    cout << "[-] User " << player->name << " answered in " << player->time_answer << endl;
    // success
    return "ANS_RES;1";
};

string handleSkipRequest(const vector<string>& parts, struct ClientInfo* player){
    if (!player->is_main_player){
        return "SKIP_RES;2";
    }
    if (player->remaining_skips<=0){
        return "SKIP_RES;3";
    }

    int round = stoi(parts[1]);
    if (round==0){
        return "SKIP_RES;5";
    }
    if (round != game.round){
        return "SKIP_RES;4";
    }
    game.is_skipped = true;
    player->remaining_skips--;
    return "SKIP_RES;1";
}

