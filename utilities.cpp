#include "utilities.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>
#include <map>

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

map<string, string> loadUserData(const char* filename) {
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

void saveUserData(const char* filename, const string& username, const string& password) {
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
