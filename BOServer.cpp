#include "BOServer.h"
#include "TraderManager.h"
#include "utils.h"

#include <wchar.h>
#include <time.h>
#include <string.h>
#include <memory>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>

using namespace std;



#pragma comment(lib, "ws2_32.lib")

BOClient::BOClient(SOCKET connection, SOCKADDR_IN address) {
	fd_ = connection;
	addr_ = address;
	start_ = false;
	trader_ = NULL;
	log_file_.open("bo.log", std::ios::out | std::ios::app);
}

bool BOClient::start() {
	boost::thread recv_thread(boost::bind(&BOClient::receice_message, this));
	return true;
}

void BOClient::send_msg(const char *buffer, int len, BasketFileTraderApi *trader) {
	if (start_ && trader == trader_) {
		log_file_ << time_now_ms() << " - Send :" << buffer << endl;
		send(fd_, buffer, len, 0);
	}
}

void BOClient::receice_message() {
	static const int BUF_SIZE = 4096;
	start_ = true;
	char message[BUF_SIZE];
	char *total_msg = NULL;
	int recv_len;
	int sec_cnt = 0;
	int total_len = 0;

	while (true) {
		recv_len = recv(fd_, message, BUF_SIZE, 0);
		if (recv_len <= 0) {
			log_file_ << time_now_ms() << "- Failed to receive data from client socket!" << recv_len << std::endl;
			break;
		}

		sec_cnt++;
		char *tmp_ptr = (char *)malloc((sec_cnt + 1)*BUF_SIZE * sizeof(char));
		if (sec_cnt > 1) {
			memcpy(tmp_ptr, total_msg, total_len);
			free(total_msg);
		}
		memcpy(tmp_ptr + total_len, message, recv_len);
		total_msg = tmp_ptr;
		total_len += recv_len;

		if (total_len > 1 && total_msg[total_len - 1] == '\0')
		{
			log_file_ << time_now_ms() << "- Data total received " << total_len << std::endl;
			char *start = total_msg;
			while (true) {
				char *p = strchr(start, '\0');
				if (p == NULL)
					break;
				if (p - total_msg < total_len - 1) {
					log_file_ << time_now_ms() << "- Multi datagram: " << (long)(p - start)
						<< " : " << start << std::endl;
					// TODO
					char tmp_msg[BUF_SIZE];
					size_t sec_len = (size_t)(p - start);
					strncpy_s(tmp_msg, sizeof(tmp_msg), start, sec_len);
					tmp_msg[sec_len] = '\0';
					decode(tmp_msg);
					log_file_ << time_now_ms() << "- Decode command : " << tmp_msg << std::endl;
					start = p + 1;
				}
				else {
					log_file_ << time_now_ms() << "- Last datagram: " << (long)(p - start)
						<< " : " << start << std::endl;
					// TODO
					char tmp_msg[BUF_SIZE];
					size_t sec_len = (size_t)(p - start);
					strncpy_s(tmp_msg, sizeof(tmp_msg), start, sec_len);
					tmp_msg[sec_len] = '\0';
					decode(tmp_msg);
					log_file_ << time_now_ms() << "- Decode command : " << tmp_msg << std::endl;
					break;
				}
			}

			sec_cnt = 0;
			free(total_msg);
			total_msg = NULL;
			total_len = 0;
		}

	}
	log_file_ << "Disconnecting..." << std::endl;
	cout << "Disconnecting..." << std::endl;
	closesocket(fd_);
	start_ = false;
	//trader_->Release();
}

void BOClient::decode(const char *message) {
	vector<string> pairs;
	split(string(message), ";", pairs);
	if (pairs.size() < 1) {
		return;
	}

	map<string, string> properties;
	vector<string> key_val;
	for (size_t i = 0; i < pairs.size(); i++) {
		key_val.clear();
		split(pairs[i], "=", key_val);
		if (key_val.size() != 2) {
			continue;
		}
		properties[key_val[0]] = key_val[1];
	}

	string msg_type = properties["COMMAND"];

	if (msg_type == "LOGIN") {
		ReqUserLogin(properties);
	} 
	else if (msg_type == "SENDORDER") {
		ReqOrderInsert(properties);
	}
	else if (msg_type == "CANCELORDER") {
		ReqOrderAction(properties);
	}
	else if (msg_type == "QUERYPOSITION") {
		ReqQryInvestorPosition(properties);
	}
	else if (msg_type == "QUERYACCOUNT") {
		ReqQryInvestorAccount(properties);
	}
	else if (msg_type == "QUERYMARGINSTOCK") {
		ReqQryInvestorMarginStock(properties);
	}
}

void BOClient::ReqUserLogin(map<string, string>& properties) {
	string account = properties["ACCOUNT"];

	char message[1024];
	trader_ = TraderManager::GetInstance()->GetTrader(account);
	snprintf(user_id_, sizeof(user_id_), "%s", account.c_str());
	BFReqUserLoginField field;
	memset(&field, 0, sizeof(field));
	snprintf(field.UserID, sizeof(field.UserID), "%s", account.c_str());
	if (!trader_->ReqUserLogin(&field, 0)) {
		snprintf(message, sizeof(message), "TYPE=LOGIN;ERROR=0;MESSAGE=");
		cout << "Login success" << endl;
	}
	else {
		snprintf(message, sizeof(message), "TYPE=LOGIN;ERROR=2201;MESSAGE=Account[%s] not exist", account.c_str());
	}
	send_msg(message, strlen(message) + 1, trader_);
}

void BOClient::ReqOrderInsert(map<string, string>& properties) {
	BFInputOrderField field;
	memset(&field, 0, sizeof(field));

	string account = properties["ACCOUNT"];
	if (strcmp(account.c_str(), user_id_) != 0)
		return ;

	string action = properties["ACTION"];
	string symbol = properties["SYMBOL"];
	string quantity = properties["QUANTITY"];
	string type = properties["TYPE"];
	string limit_price = properties["LIMITPRICE"];
	string duration = properties["DURATION"];
	string client_id = properties["CLIENTID"];

	vector<string> ins_exch;
	string readable_symbol = symbol;
	string exchange;
	split(symbol, ".", ins_exch);
	if (ins_exch.size() == 2) {
		readable_symbol = ins_exch[0];
		exchange = ins_exch[1];
	}
	snprintf(field.InstrumentID, sizeof(field.InstrumentID), readable_symbol.c_str());
	if (exchange == "SZ") {
		field.ExchangeType = BF_EXCH_SZ;
	}
	else if (exchange == "SH") {
		field.ExchangeType = BF_EXCH_SH;
	}
	if (action == "BUY") {
		field.Direction = BF_D_BUY;
	}
	else if (action == "SELL") {
		field.Direction = BF_D_SELL;
	}
	else if (action == "COLLATERALBUY") {
		field.Direction = BF_D_COLLATERALBUY;
	}
	else if (action == "COLLATERALSELL") {
		field.Direction = BF_D_COLLATERALSELL;
	}
	else if (action == "BORROWTOBUY") {
		field.Direction = BF_D_BORROWTOBUY;
	}
	else if (action == "BORROWTOSELL") {
		field.Direction = BF_D_BORROWTOSELL;
	}
	else if (action == "BUYTOPAY") {
		field.Direction = BF_D_BUYTOPAY;
	}
	else if (action == "SELLTOPAY") {
		field.Direction = BF_D_SELLTOPAY;
	}
	else if (action == "PAYBYCASH") {
		field.Direction = BF_D_PAYBYCASH;
	}
	else if (action == "PAYBYSTOCK") {
		field.Direction = BF_D_PAYBYSTOCK;
	}
	field.OrderType = BF_OT_LIMITPRICE;
	field.Volume = atoi(quantity.c_str());
	field.LimitPrice = atof(limit_price.c_str());
	snprintf(field.OrderRef, sizeof(field.OrderRef), "%s", client_id.c_str());

	trader_->ReqOrderInsert(&field, 0);

	return;
}

void BOClient::ReqOrderAction(map<string, string>& properties) {
	BFInputOrderActionField field;
	memset(&field, 0, sizeof(field));

	string client_id = properties["CLIENTID"];
	string sys_id = properties["SYSID"];
	string exchange = properties["EXCHANGE"];
	string instrument_id = properties["INSTRUMENT"];

	vector<string> ins_exch;
	string readable_symbol = instrument_id;
	//string exchange;
	split(instrument_id, ".", ins_exch);
	if (ins_exch.size() == 2) {
		readable_symbol = ins_exch[0];
		exchange = ins_exch[1];
	}
	snprintf(field.InstrumentID, sizeof(field.InstrumentID), readable_symbol.c_str());
	snprintf(field.OrderRef, sizeof(field.OrderRef), client_id.c_str());
	snprintf(field.OrderSysID, sizeof(field.OrderSysID), sys_id.c_str());
	if (exchange == "SZ") {
		field.ExchangeType = BF_EXCH_SZ;
	}
	else if (exchange == "SH") {
		field.ExchangeType = BF_EXCH_SH;
	}
	// InstrumentID
	// TODO : Fill fields of ActionField

	trader_->ReqOrderAction(&field, 0);

	return;
}

void BOClient::ReqQryInvestorAccount(map<string, string>& properties) {
	BFQryTradingAccountField field;
	memset(&field, 0, sizeof(field));
	string account = properties["ACCOUNT"];
	if (strcmp(account.c_str(), user_id_) != 0)
		return;
	snprintf(field.InvestorID, sizeof(field.InvestorID), "%s", account.c_str());
	trader_->ReqQryTradingAccount(&field, 0);
	return;
}

void BOClient::ReqQryInvestorPosition(map<string, string>& properties) {
	BFQryInvestorPositionField field;
	memset(&field, 0, sizeof(field));
	string account = properties["ACCOUNT"];
	if (strcmp(account.c_str(), user_id_) != 0)
		return;
	snprintf(field.InvestorID, sizeof(field.InvestorID), "%s", account.c_str());
	trader_->ReqQryInvestorPosition(&field, 0);
	return;
}

void BOClient::ReqQryInvestorMarginStock(map<string, string>& properties) {
	BFQryInvestorMarginStockField field;
	memset(&field, 0, sizeof(field));
	string account = properties["ACCOUNT"];
	if (strcmp(account.c_str(), user_id_) != 0)
		return;
	snprintf(field.InvestorID, sizeof(field.InvestorID), "%s", account.c_str());
	trader_->ReqQryInvestorMarginStock(&field, 0);
	return;
}





BOServer::~BOServer() {};

BOServer& BOServer::Instance() {
	static BOServer server;
	return server;
}

// Easy Language Inter interfaces
bool BOServer::RegisterServer(int port) {
	socket_port_ = port;
	clients_ = new BOClient*[1000];
	memset(clients_, 0, sizeof(BOClient*) * 1000);
	client_size_ = 0;
	boost::thread server_thread(boost::bind(&BOServer::listen_thread, this));
	// server_thread.join();
	return true;
}


bool BOServer::Callback(const char* message, BasketFileTraderApi *trader) {
	int con_size = client_size_;
	for (int i = 0; i < con_size; i++) {
		clients_[i]->send_msg(message, strlen(message) + 1, trader);
	}
	// int ret = send(client_con_, buffer, strlen(buffer) + 1, 0);
	return true;
}

BOServer::BOServer() {
	socket_port_ = 0;
}

void BOServer::listen_thread() {
	WORD version_request;
	WSADATA wsa_data;
	version_request = MAKEWORD(2, 2);
	int err = WSAStartup(version_request, &wsa_data);
	if (!err) {
		// log_file_ << "Open winsock successfully!" << std::endl;
		cout << "Open winsock successfully!" << endl;
	}
	else {
		// log_file_ << "Failed to open winsock lib!" << std::endl;
		cout << "Failed to open winsock lib!" << endl;
		return;
	}

	server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
	address_.sin_family = AF_INET;
	address_.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	address_.sin_port = htons(socket_port_);
	int ret = ::bind(server_socket_, (SOCKADDR*)&address_, sizeof(SOCKADDR));
	if (ret == SOCKET_ERROR) {
		// log_file_ << "Bind error!" << std::endl;
		cout << "Bind error!" << endl;
		return;
	}
	// listen(server_socket, 5);
	if (listen(server_socket_, 5) == SOCKET_ERROR) {
		// log_file_ << "Listen error!" << std::endl;
		cout << "Listen error!" << endl;
		return;
	}
	cout << "Bind and listen..." << endl;

	while (true) {
		SOCKADDR_IN client_addr;
		int len = sizeof(SOCKADDR);
		SOCKET client_con = accept(server_socket_, (SOCKADDR*)&client_addr, &len);
		// log_file_ << "Accept socket..." << std::endl;
		cout << "Accept socket..." << endl;
		// multi-client connection
		BOClient *client = new BOClient(client_con, client_addr);
		clients_[client_size_++] = client;
		client->start();
	}

	WSACleanup();
}

