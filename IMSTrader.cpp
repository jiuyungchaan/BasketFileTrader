
#include "IMSTrader.h"
#include "BOServer.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <fstream>

#include <windows.h>

using namespace std;

IMSTrader::IMSTrader(const char *user_id) {
	int date = date_now();
	snprintf(user_id_, sizeof(user_id_), "%s", user_id);
	logined_ = false;
	request_id_ = 1;
	snprintf(config_file_name_, sizeof(config_file_name_), "%s", "ims.txt");
	LoadConfig();
	/*interval_ = 100; // wait 100 milliseconds
	//snprintf(base_directory_, sizeof(base_directory_), "%s", "C:\\ims\\IMSClient_UAT_New");
	snprintf(base_directory_, sizeof(base_directory_), "%s", "C:\\ims\\IMSClient_UAT_20171124(AutoExport)");
	snprintf(order_directory_, sizeof(order_directory_), "%s\\Basket\\Order", base_directory_);
	snprintf(cancel_directory_, sizeof(cancel_directory_), "%s\\Basket\\Cancel", base_directory_);
	snprintf(response_directory_, sizeof(response_directory_), "%s\\Data", base_directory_);*/
	memset(basket_orders_, sizeof(basket_orders_), 0);
	memset(basket_size_, sizeof(basket_size_), 0);
	timer_end_ = 1;
	queue_index_ = 0;
	char log_file_name[64];
	snprintf(log_file_name, sizeof(log_file_name), "%s-%d.log", user_id_, date);
	log_file_.open(log_file_name, ios::out | ios::app);
}

IMSTrader::~IMSTrader() {

}

void IMSTrader::Init() {
	cout << "Initialize trader api successfully" << endl;
	logined_ = true;
	boost::thread scan_response_thread(boost::bind(&IMSTrader::ScanResponseFile, this));
	boost::thread scan_trade_thread(boost::bind(&IMSTrader::ScanTradeFile, this));
}

void IMSTrader::Release() {
	
}

bool IMSTrader::Logined() {
	return logined_;
}

void IMSTrader::LoadConfig() {
	fstream config_file;
	config_file.open(config_file_name_, ios::in);
	if (!config_file.good()) {
		printf("Cannot read configuration file!\n");
		exit(0);
	}
	char line[256];
	vector<string> fields;
	map<string, string> config_map;
	while (!config_file.eof()) {
		config_file.getline(line, 256);
		if (strcmp(line, "") == 0)
			continue;
		fields.clear();
		split(line, "=", fields);
		if (fields.size() == 2) {
			config_map.insert(pair<string, string>(fields[0], fields[1]));
		}
	}

	map<string, string>::iterator it;
	it = config_map.find("base_dir");
	if (it != config_map.end()) {
		snprintf(base_directory_, sizeof(base_directory_), "%s", it->second.c_str());
	}
	it = config_map.find("order_dir");
	if (it != config_map.end()) {
		snprintf(order_directory_, sizeof(order_directory_), "%s", it->second.c_str());
	}
	it = config_map.find("cancel_dir");
	if (it != config_map.end()) {
		snprintf(cancel_directory_, sizeof(cancel_directory_), "%s", it->second.c_str());
	}
	it = config_map.find("response_dir");
	if (it != config_map.end()) {
		snprintf(response_directory_, sizeof(response_directory_), "%s", it->second.c_str());
	}
	it = config_map.find("interval");
	if (it != config_map.end()) {
		interval_ = atoi(it->second.c_str());
	}
}

void IMSTrader::WaitToOrder() {
	Sleep(interval_);
	int queue_idx = queue_index_;
	queue_index_ = (queue_index_ + 1) % 2;
	timer_end_ = 1;
	SendBasketOrders(queue_idx);
}


// Response file header:
//    产品 | 资产单元 | 投资组合 | 资金帐号 | 市场 | 合约代码 | 序列号 | 委托时间 | 合同号 | 委托数量 |
//    委托价格 | 买卖方向 | 成交数量 | 剩余数量 | 委托状态 | 委托状态描述 | 投顾交易员 | 投机套保标志 | 开仓平仓标志 | 篮子ID |
//    备注 | 币种 | 算法ID | 指令ID | 审核员 | 交易员 |
void IMSTrader::ScanResponseFile() {
	cout << "Start to scan response file..." << endl;
	char response_file_name[128];
	snprintf(response_file_name, sizeof(response_file_name), "%s\\OrderInfo.txt", response_directory_);
	int line_scanned = 0;
	int line_read = 0;
	char line[2048];
	char message[2048];
	vector<string> fields;
	while (true) {
		Sleep(50);
		ifstream response_file;
		response_file.open(response_file_name, ifstream::in);
		while (!response_file.eof()) {
			response_file.getline(line, 2048);
			if (strcmp(line, "") == 0)
				continue;
			if (line_scanned == line_read) {
				line_scanned++;
				line_read++;
			}
			else {
				line_read++;
				continue;
			}

			/*cout << "Response: scanned:" << line_scanned << " read:" << line_read << endl;
			cout << line << endl;*/
			if (line[0] == '#') {
				continue;
			}
			fields.clear();
			split(string(line), "|", fields);
			if (fields.size() != 26) {
				/*if (fields.size() == 0)
					continue;
				cout << "Response File Fields:" << endl;
				for (int i = 0; i < fields.size(); i++) {
					cout << i << " " << fields[i] << endl;
				}
				cout << endl;*/
				continue;
			}
			string status;
			if (fields[14] == "N_TRD_Q" || fields[14] == "ALLTRD" ||
				fields[14] == "P_TRD_Q" || fields[14] == "CANCEL" ||
				fields[14] == "P_TRD_NQ") {
				status = "RECEIVED";
			}
			/*else if (fields[14] == "P_TRD_Q") {
				status = "PARTIALLYFILLED";
			}
			else if (fields[14] == "CANCEL") {
				status = "CANCELED";
			}
			else if (fields[14] == "P_TRD_NQ") {
				status = "PARTIALLYFILLEDUROUT";
			}*/
			else if (fields[14] == "SUSPEND_F") {
				status = "REJECTED";
			}

			string direction;
			if (fields[11] == "B") {
				direction = "BUY";
			}
			else if (fields[11] == "S") {
				direction = "SELL";
			}

			string exch;
			if (fields[4] == "0") {
				exch = "SH";
			}
			else if (fields[4] == "1") {
				exch = "SZ";
			}

			char symbol[16];
			snprintf(symbol, sizeof(symbol), "%s.%s", fields[5].c_str(), exch.c_str());

			snprintf(message, sizeof(message), "TYPE=UPDATE;STATUS=%s;SYS_ORDER_ID=%s;"
				"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%s;LIMIT_PRICE=%s;"
				"DIRECTION=%s;FILLED_QUANTITY=%s;AVG_FILLED_PRICE=%s;LEFT_QUANTITY=%s;",
				status.c_str(), fields[8].c_str(), fields[20].c_str(), symbol, fields[25].c_str(), fields[9].c_str(),
				fields[10].c_str(), direction.c_str(), fields[12].c_str(), "0.0", fields[13].c_str());
			cout << message << endl;
			BOServer::Instance().Callback(message, this);
		} // while not eof
		line_read = 0;
		response_file.close();
	} // while
}

// Trade File Header:
//   产品 | 资产单元 | 投资组合 | 资金帐号 | 市场 | 合约代码 | 序列号 | 委托时间 | 合同号 | 委托数量 | 
//   委托价格 | 买卖方向 | 成交数量 | 成交价格 | 成交编号 | 剩余数量 | 成交状态 | 投顾交易员 | 投机套保标志 | 开仓平仓标志 |
//   篮子ID | 备注 | 成交时间 | 币种 | 算法ID | 指令ID | 审核员 | 交易员 |
void IMSTrader::ScanTradeFile() {
	cout << "Start to scan trade file..." << endl;
	char trade_file_name[128];
	snprintf(trade_file_name, sizeof(trade_file_name), "%s\\KnockInfo.txt", response_directory_);
	int line_scanned = 0;
	int line_read = 0;
	char line[2048];
	char message[2048];
	vector<string> fields;
	while (true) {
		Sleep(50);
		ifstream trade_file;
		trade_file.open(trade_file_name, ifstream::in);
		while (!trade_file.eof()) {
			trade_file.getline(line, 2048);
			if (strcmp(line, "") == 0)
				continue;
			if (line_scanned == line_read) {
				line_scanned++;
				line_read++;
			}
			else {
				line_read++;
				continue;
			}

			/*cout << "Trade: scanned:" << line_scanned << " read:" << line_read << endl;
			cout << line << endl;*/
			if (line[0] == '#') {
				continue;
			}
			fields.clear();
			split(string(line), "|", fields);
			string status;
			if (fields.size() != 28) {
				/*if (fields.size() == 0)
					continue;
				cout << "Trade File Fields:" << endl;
				for (int i = 0; i < fields.size(); i++) {
					cout << i << " " << fields[i] << endl;
				}
				cout << endl;*/
				continue;
			}
			if (fields[16] == "P_FILLED") {
				status = "PARTIALLYFILLED";
			}
			/*else if (fields[14] == "P_TRD_Q") {
			status = "PARTIALLYFILLED";
			}*/
			else if (fields[16] == "CANCELED") {
				status = "CANCELED";
			}
			else if (fields[16] == "F_FILLED") {
				status = "FILLED";
			}

			string direction;
			if (fields[11] == "B") {
				direction = "BUY";
			}
			else if (fields[11] == "S") {
				direction = "SELL";
			}

			string exch;
			if (fields[4] == "0") {
				exch = "SH";
			}
			else if (fields[4] == "1") {
				exch = "SZ";
			}

			char symbol[16];
			snprintf(symbol, sizeof(symbol), "%s.%s", fields[5].c_str(), exch.c_str());

			if (status == "PARTIALLYFILLED" || status == "FILLED") {
				snprintf(message, sizeof(message), "TYPE=TRADE;STATUS=%s;SYS_ORDER_ID=%s;"
					"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%s;LIMIT_PRICE=%s;"
					"DIRECTION=%s;FILLED_QUANTITY=%s;FILLED_PRICE=%s;LEFT_QUANTITY=%s;",
					status.c_str(), fields[8].c_str(), fields[21].c_str(), symbol, fields[27].c_str(), fields[9].c_str(),
					fields[10].c_str(), direction.c_str(), fields[12].c_str(), fields[13].c_str(), fields[15].c_str());
				cout << message << endl;
				BOServer::Instance().Callback(message, this);
			}
			snprintf(message, sizeof(message), "TYPE=UPDATE;STATUS=%s;SYS_ORDER_ID=%s;"
				"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%s;LIMIT_PRICE=%s;"
				"DIRECTION=%s;FILLED_QUANTITY=%s;FILLED_PRICE=%s;LEFT_QUANTITY=%s;",
				status.c_str(), fields[8].c_str(), fields[21].c_str(), symbol, fields[27].c_str(), fields[9].c_str(),
				fields[10].c_str(), direction.c_str(), "0", "0.0", fields[15].c_str());
			cout << message << endl;
			BOServer::Instance().Callback(message, this);
		} // while not eof
		line_read = 0;
		trade_file.close();
	}
}

// 产品 | 资产单元 | 资产类别 | 资金可用 | 资金冻结 | 资金市值 | 上日余额 | 浮动盈亏 | 保证金占用 | 保证金冻结 |
// 上日保证金 | 总权益 | 定时盈亏 | 风险度 | 可取金额 | 港股通可用 | 币种 |
void IMSTrader::ScanFundFile() {
	char fund_file_name[128];
	snprintf(fund_file_name, sizeof(fund_file_name), "%s\\FundInfo.txt", response_directory_);
	char line[2048];
	char message[2048];
	vector<string> fields;
	ifstream fund_file;
	fund_file.open(fund_file_name, ifstream::in);
	while (!fund_file.eof()) {
		fund_file.getline(line, 2048);
		if (strcmp(line, "") == 0 || line[0] == '#')
			continue;
		fields.clear();
		split(string(line), "|", fields);
		if (fields.size() != 17) {
			cout << "Fund Fields:" << fields.size() << endl;
			continue;
		}
		snprintf(message, sizeof(message), "TYPE=ACCOUNT;ACCOUNT_ID=%s;IS_LAST=%d;"
			"RT_ACCOUNT_NET_WORTH=%lf;RT_CASH_BALANCE=%s;RT_MARKET_VALUE=%s;"
			"RT_TRADING_POWER=%s",
			user_id_, 1, 0.0, fields[6].c_str(), fields[5].c_str(), fields[3].c_str());
		cout << message << endl;
		BOServer::Instance().Callback(message, this);
	} // while
	fund_file.close();
}

// 产品 | 资产单元 | 投资组合 | 市场 | 合约代码 | 买卖方向 | 投机套保标志 | 总持仓数 | 持仓可用数 | 冻结数量 |
// 昨日持仓量 | 今日持仓量 | 开仓冻结数量 | 平仓冻结数量 | 最新价 | 开盘价 | 今仓成本 | 昨结算价 | 今结算价 | 保证金占用 |
// 保证金冻结 | 盯市盈亏 | 浮动盈亏 | 平仓盈亏 | 市值 | 合约名称 | 盈亏金额 | 摊薄成本 | 币种 |
void IMSTrader::ScanPositionFile() {
	char position_file_name[128];
	snprintf(position_file_name, sizeof(position_file_name), "%s\\HoldingInfo.txt", response_directory_);
	char line[2048];
	char message[2048] = { 0 };
	char pre_message[2048] = { 0 };
	vector<string> fields;
	ifstream position_file;
	position_file.open(position_file_name, ifstream::in);
	while (!position_file.eof()) {
		position_file.getline(line, 2048);
		if (strcmp(line, "") == 0 || line[0] == '#')
			continue;
		fields.clear();
		split(string(line), "|", fields);
		if (fields.size() != 29) {
			cout << "Position Fields:" << fields.size() << endl;
			continue;
		}
		if (fields[7] == "0") {
			//  total position == 0, invalid position item
			continue;
		}
		if (strcmp(message, "") != 0) {
			snprintf(pre_message, sizeof(pre_message), "%s;IS_LAST=%d;", message, 0);
		}
		string exch;
		if (fields[3] == "0") {
			exch = "SH";
		}
		else if (fields[3] == "1") {
			exch = "SZ";
		}

		char symbol[16];
		snprintf(symbol, sizeof(symbol), "%s.%s", fields[4].c_str(), exch.c_str());

		int available = atoi(fields[8].c_str());
		int ydpos = atoi(fields[10].c_str());
		int curpos = atoi(fields[7].c_str());
		int short_qty = ydpos - available;
		int long_qty = curpos - available;
		double avg_price = atof(fields[27].c_str());
		double total_cost = avg_price * (double)curpos;

		snprintf(message, sizeof(message), "TYPE=POSITION;ACCOUNT_ID=%s;POSITION_TYPE=LONG_POSITION;"
			"SYMBOL=%s;QUANTITY=%d;AVAILABLE_QTY=%d;LONG_QTY=%d;SHORT_QTY=%d;AVERAGE_PRICE=%lf;"
			"TOTAL_COST=%lf;MARKET_VALUE=%s;TODAY_POSITION=%s",
			user_id_, symbol, curpos, available, long_qty, short_qty, avg_price, total_cost, fields[24].c_str(), fields[11].c_str());

		if (strcmp(pre_message, "") != 0) {
			cout << pre_message << endl;
			BOServer::Instance().Callback(pre_message, this);
		}
	} // while

	if (strcmp(message, "") != 0) {
		snprintf(pre_message, sizeof(pre_message), "%s;IS_LAST=%d;", message, 1);
		cout << pre_message << endl;
		BOServer::Instance().Callback(pre_message, this);
	}
	position_file.close();
}

void IMSTrader::SendBasketOrders(int idx) {
	int basket_size = basket_size_[idx];
	basket_size_[idx] = 0;
	char all_str[4096] = "";
	char order_str[256];
	int exch = 0;
	char direction = 'B';
	for (int i = 0; i < basket_size; i++) {
		if (basket_orders_[idx][i].ExchangeType == BF_EXCH_SH)
			exch = 0;
		else if (basket_orders_[idx][i].ExchangeType == BF_EXCH_SZ)
			exch = 1;
		if (basket_orders_[idx][i].Direction == BF_D_BUY)
			direction = 'B';
		else if (basket_orders_[idx][i].Direction == BF_D_SELL)
			direction = 'S';
		snprintf(order_str, sizeof(order_str), "%d|%s|%c||LIMIT|F|%.2lf|%d|%s\r\n",
			exch, basket_orders_[idx][i].InstrumentID, direction,
			basket_orders_[idx][i].LimitPrice, basket_orders_[idx][i].Volume,
			basket_orders_[idx][i].OrderRef);
		cout << "Write order: " << order_str << endl;
		strcat_s(all_str,sizeof(all_str), order_str);
	}

	ofstream basket_file;
	char basket_file_name[128];
	const char *time_str = time_now_long();
	snprintf(basket_file_name, sizeof(basket_file_name), "%s\\%s.txt", order_directory_, time_str);
	basket_file.open(basket_file_name, ofstream::out);
	//basket_file.open("C:\\ims\\IMSClient_UAT_New\\Basket\\Order\\test.txt", ofstream::out);
	basket_file << all_str;
	basket_file.flush();
	basket_file.close();
}

int IMSTrader::ReqUserLogin(BFReqUserLoginField *pLoginField, int nRequestID) {
	if (strcmp(pLoginField->UserID, user_id_) != 0) {
		return -1;
	}
	else {
		return 0;
	}
}

int IMSTrader::ReqUserLogout(BFReqUserLogoutField *pLogoutField, int nRequestID) {

	return 0;
}

int IMSTrader::ReqOrderInsert(BFInputOrderField *pInputOrder, int nRequestID) {
	int idx = queue_index_;
	if (timer_end_) {
		timer_end_ = 0;
		boost::thread wait_thread(boost::bind(&IMSTrader::WaitToOrder, this));
	}
	basket_orders_[idx][basket_size_[idx]++] = *pInputOrder;
	return 0;
}

int IMSTrader::ReqOrderAction(BFInputOrderActionField *pInputOrderAction, int nRequestID) {
	char cancel_str[128];
	int exch = 0;
	if (pInputOrderAction->ExchangeType == BF_EXCH_SH)
		exch = 0;
	else if (pInputOrderAction->ExchangeType == BF_EXCH_SZ)
		exch = 1;
	snprintf(cancel_str, sizeof(cancel_str), "%d|%s|%s|%s||\r\n",
		exch, pInputOrderAction->InstrumentID, pInputOrderAction->OrderSysID, user_id_);
	cout << "Write Cancel Request: " << cancel_str << endl;

	ofstream cancel_file;
	char cancel_file_name[128];
	const char *time_str = time_now_long();
	snprintf(cancel_file_name, sizeof(cancel_file_name), "%s\\%s.txt", cancel_directory_, time_str);
	cancel_file.open(cancel_file_name, ofstream::out);
	cancel_file << cancel_str << endl;
	cancel_file.flush();
	cancel_file.close();
	return 0;
}

int IMSTrader::ReqQryTradingAccount(BFQryTradingAccountField *pTradingAccount, int nRequestID) {
	boost::thread scan_fund_thread(boost::bind(&IMSTrader::ScanFundFile, this));
	return 0;
}

int IMSTrader::ReqQryInvestorPosition(BFQryInvestorPositionField *pInvestorPosition, int nRequestID) {
	boost::thread scan_position_thread(boost::bind(&IMSTrader::ScanPositionFile, this));
	return 0;
}

int IMSTrader::ReqQryInvestorMarginStock(BFQryInvestorMarginStockField *pMarginStock, int nRequestID) {

	return 0;
}

void IMSTrader::OnRspOrderInsert(BFInputOrderField *pInputOrder, BFRspInfoField *pRspInfo) {

}

void IMSTrader::OnRspOrderAction(BFInputOrderActionField *pInputOrderAction, BFRspInfoField *pRspInfo) {

}

void IMSTrader::OnRtnOrder(BFOrderField *pOrder, BFRspInfoField *pRspInfo) {

}

void IMSTrader::OnRtnTrade(BFTradeField *pTrade, BFRspInfoField *pRspInfo) {

}

void IMSTrader::OnRspQryTradingAccount(BFTradingAccountField *pTradingAccount, BFRspInfoField *pRspInfo) {

}

void IMSTrader::OnRspQryInvestorPosition(BFInvestorPositionField *pInvestorPosition, BFRspInfoField *pRspInfo) {

}

void IMSTrader::OnRspQryInvestorMarginStock(BFInvestorMarginStockField *pMarginStock, BFRspInfoField *pRspInfo) {

}

void IMSTrader::OnRspError(BFRspInfoField *pRspInfo) {

}

