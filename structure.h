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
    int round = 0; // current round
    std::chrono::time_point<std::chrono::system_clock> ckpt_send_quest; // time checkpoint when sending curr question
    int correct_answer; // correct answer index of current question
    int main_player_idx = -1; // idx of curr main player
};

struct Question {
    string question_text;
    vector<string> answer_texts;
    int correct_idx;
};