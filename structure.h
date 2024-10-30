#include <bits/stdc++.h>

using namespace std;

struct ClientInfo {
    int connfd;
    string name;
    string client_info;
    bool isLoggedIn = false;

    int point = 0;
    bool is_main_player;
    bool is_eliminated = false;
    int submitted_answer;
};

struct Game {
    bool isStart = false;
    int round = 0;
    int correct_answer;
};