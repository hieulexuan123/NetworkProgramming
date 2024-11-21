#include "utilities.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>
#include <map>
#include <random>
#include "structure.h"

using namespace std;

vector<string> split(string s, string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    string token;
    vector<string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr(pos_start));
    return res;
}

map<string, string> loadUserData(const string filename) {
    map<string, string> userDb;
    ifstream file(filename);

    // Check if the file opened successfully
    if (!file) {
        cerr << "Error: Cannot open file " << filename << endl;
        return userDb;
    }

    string username, password;

    // Read each line and split it into username and password
    while (file >> username >> password) {
        userDb[username] = password;
    }

    // Close the file
    file.close();

    return userDb;
}

void saveUserData(const string filename, const string& username, const string& password) {
    // Open the file in append mode
    ofstream file(filename, std::ios::app);

    // Check if the file opened successfully
    if (!file) {
        cerr << "Error: Cannot open file " << filename << endl;
        return;
    }

    // Write username and password to the file
    file << username << " " << password << std::endl;

    // Close the file
    file.close();
}

vector<struct Question> loadQuestionBank(const string fileName){
    vector<Question> questions;
    ifstream file(fileName);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << fileName << std::endl;
        return questions;
    }

    string line;
    while (getline(file, line)) {
        if (line.empty()) continue; // Skip blank lines

        Question q;
        q.question_text = line; // First line is the question

        // Read the correct answer index
        if (!std::getline(file, line)) break;
        q.correct_idx = stoi(line);

        // Read the choices until a blank line or end of file
        while (getline(file, line) && !line.empty()) {
            q.answer_texts.push_back(line);
        }

        // Add the question to the vector
        questions.push_back(q);
    }

    file.close();
    return questions;
}

int getRandom(const vector<int>& nums){
    std::random_device rd;  // Seed for the random number engine
    std::mt19937 gen(rd()); // Mersenne Twister random number engine
    std::uniform_int_distribution<> distrib(0, nums.size() - 1); // Random index between 0 and size-1

    // Pick a random number from the array
    int randomIndex = distrib(gen);
    int randomNumber = nums[randomIndex];

    return randomNumber;
}
