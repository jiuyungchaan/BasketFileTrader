// BasketFileTrader.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "BOServer.h"
#include "TraderManager.h"

#include <iostream>
#include <string>
#include <memory>
#include <windows.h>


#include <objbase.h>
#include <stdio.h>

using namespace std;

/// there is a big difference between main and _tmain
int main(int argc, char** argv)
{
	int port = 5555;
	string account, client_type;
	for (int i = 0; i < argc;) {
		if (strcmp(argv[i], "-p") == 0) {
			port = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-a") == 0) {
			account = argv[++i];
		}
		else if (strcmp(argv[i], "-t") == 0) {
			client_type = argv[++i];
		}
		++i;
	}
	TraderManager::GetInstance()->RegisterTrader(account, client_type);
	BOServer::Instance().RegisterServer(port);
	cout << "Register server successfully" << endl;
	while (true) {
		Sleep(1);
	}
	system("pause");

	return 0;
}
