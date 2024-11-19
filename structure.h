#include <bits/stdc++.h>

using namespace std;

struct ClientInfo {
    int connfd;
    string name;
    string client_info;
    bool isLoggedIn = false;

    int point = 0;
    bool is_main_player= false;
    bool is_eliminated = false;
    int submitted_answer = -1;
    int round = -1;
    int time_answer; 
};

struct Game {
    bool isStart = false;
    int round = 0;
    std::chrono::time_point<std::chrono::system_clock> ckpt_send_quest;
    int correct_answer;
};