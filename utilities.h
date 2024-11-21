#ifndef UTILITIES_H
#define UTILITIES_H

#include <vector>
#include <map>
#include <string>

using namespace std;

vector<string> split(string s, string delimiter);
map<string, string> loadUserData(const string filename);
void saveUserData(const string filename, const string& username, const string& password);
vector<struct Question> loadQuestionBank(const string fileName);
int getRandom(const vector<int>& nums);

#endif