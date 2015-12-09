using namespace std;

#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <sstream>
#include <cstdio>
#include <fstream>

int main(int argc,char * argv[]) {

	int g_horizon = 0;
	int g_mode = 0;
	string g_path = "";
	string g_domain = "";

	vector<string> problems;

	for (int i = 1; i < argc; ++i) {
		string arg = string(argv[i]);

		if (arg.compare("--r") == 0) {
			i++;
			g_path = argv[i];
		}

		if (arg.compare("--d") == 0) {
			i++;
			g_domain = argv[i];
		}

		if (arg.compare("--m") == 0) {
			i++;
			g_mode = atoi(argv[i]);
		}

		if (arg.compare("--h") == 0) {
			i++;
			g_horizon = atoi(argv[i]);
		}

		if (arg.compare("--p") == 0) {
			i++;
			while (i < argc) {
				problems.push_back(new Problem(argv[i]);			
			}
		}
	}
}
