#ifndef UTILITIES_H
#define UTILITIES_H

#include <vector>
#include <map>
#include <string>

using namespace std;

vector<string> split(string s, string delimiter);
map<string, string> loadUserData(const char* filename);
void saveUserData(const char* filename, const string& username, const string& password);

#endif