
#include "WinnerTrader.h"
#include "BOServer.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include <windows.h>

using namespace std;

WinnerTrader::WinnerTrader(const char *user_id) {
	date_ = date_now();
	snprintf(user_id_, sizeof(user_id_), "%s", user_id);
	snprintf(config_file_name_, sizeof(config_file_name_), "%s", "winner.txt");
	LoadConfig();
	/*snprintf(base_directory_, sizeof(base_directory_), "%s", "C:\\lts-winner\\winner_lts_test_file_20171012");
	snprintf(order_directory_, sizeof(order_directory_), "%s\\files\\%s", base_directory_, user_id_);
	snprintf(response_directory_, sizeof(response_directory_), "%s\\files\\%s", base_directory_, user_id_);*/
	snprintf(basket_id_file_name_, sizeof(basket_id_file_name_), "basket-id-record-%d.txt", date_);
	logined_ = false;
	request_id_ = 1;
	//interval_ = 100; // wait 100 milliseconds
	timer_end_ = 1;
	queue_index_ = 0;
	file_id_ = 1;
	local_order_id_ = 1;
	char log_file_name[64];
	snprintf(log_file_name, sizeof(log_file_name), "%s-%d.log", user_id_, date_);
	log_file_.open(log_file_name, ios::out | ios::app);
	basket_id_file_.open(basket_id_file_name_, ios::out | ios::app);
}


WinnerTrader::~WinnerTrader() {

}

void WinnerTrader::Init() {
	cout << "Initialize winner trader api successfully" << endl;
	logined_ = true;
	//TopMostWinner();
	LoadBasketID();
	boost::thread keyboard_thread(boost::bind(&WinnerTrader::FreshKeyboardInput, this));
	boost::thread scan_thread(boost::bind(&WinnerTrader::ScanResponseFile, this));
}

void WinnerTrader::Release() {

}

bool WinnerTrader::Logined() {
	return logined_;
}

void WinnerTrader::LoadConfig() {
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
	it = config_map.find("response_dir");
	if (it != config_map.end()) {
		snprintf(response_directory_, sizeof(response_directory_), "%s", it->second.c_str());
	}
	it = config_map.find("interval");
	if (it != config_map.end()) {
		interval_ = atoi(it->second.c_str());
	}
}

void WinnerTrader::LoadBasketID() {
	fstream basket_id_file;
	basket_id_file.open(basket_id_file_name_, ios::in);
	if (!basket_id_file.good()) {
		return;
	}
	char line[256];
	char basket_id_line[256];
	while (!basket_id_file.eof()) {
		basket_id_file.getline(line, 256);
		if (strcmp(line, "") == 0)
			break;
		snprintf(basket_id_line, sizeof(basket_id_line), "%s", line);
	}
	vector<string> fields;
	split(basket_id_line, "-", fields);
	if (fields.size() == 2) {
		/*int date = atoi(fields[0].c_str());
		if (date == date_) {
			file_id_ = atoi(fields[1].c_str());
			cout << "Load Basket ID: " << fields[1] << endl;
		}*/
		file_id_ = atoi(fields[0].c_str());
		local_order_id_ = atoi(fields[1].c_str());
		cout << "Load Basket ID: " << fields[0] << " Order ID: " << fields[1] << endl;
	}
}

void WinnerTrader::WaitToOrder() {
	Sleep(interval_);
	int queue_idx = queue_index_;
	queue_index_ = (queue_index_ + 1) % 2;
	timer_end_ = 1;
	SendBasketOrders(queue_idx);
}

void WinnerTrader::FreshKeyboardInput() {
	cout << "Start to fresh virtual keyboard input..." << endl;
	int loop = 0;
	while (true) {
		if (loop % 200 == 0) {
			TopMostWinner();
		}
		loop = ++loop % 200;
		Sleep(10);
		// F1 - F12 : 112 - 123
		// F2 to execute basket orders
		keybd_event(113, 0, 0, 0);
		keybd_event(113, 0, KEYEVENTF_KEYUP, 0);
		// F8 to fresh feedback of basket
		keybd_event(119, 0, 0, 0);
		keybd_event(119, 0, KEYEVENTF_KEYUP, 0);
		// F9 to fresh feedback of all orders
		keybd_event(120, 0, 0, 0);
		keybd_event(120, 0, KEYEVENTF_KEYUP, 0);
	}
}

void WinnerTrader::TopMostWinner() {
	HWND hq = FindWindow(NULL, L"XXXX - 盈佳证券交易终端");
	if (hq == NULL) {
		cout << "LTS Winner Client Not Found!" << endl;
		return;
	}
	DWORD pid, tid;
	tid = GetWindowThreadProcessId(hq, &pid);
	AttachThreadInput(GetWindowThreadProcessId(::GetForegroundWindow(), NULL), tid, TRUE);
	SetForegroundWindow(hq);
	hq = SetFocus(hq);
}


// CJHB: 证券代码,交易所,报单号,报单引用,本地报单号,方向,开平,成交数量,成交价格,成交额,
//       成交时间,成交编号,备注
// WTHB: 证券代码,交易所,报单号,报单引用,本地报单号,方向,开平,委托数量,剩余数量,委托价格,
//       价格类型,提交状态,委托时间,撤单时间,委托状态,状态信息,错误代码,备注
void WinnerTrader::ScanResponseFile() {
	cout << "Start to scan response file..." << endl;
	char response_file_name[128], trade_file_name[128];
	//snprintf(response_file_name, sizeof(response_file_name), "%s\\%d_orders.csv", response_directory_, date_);
	int line_scanned = 1; // pass the first line
	int line_read = 0;
	char line[2048];
	char message[2048];
	vector<string> fields;
	while (true) {
		Sleep(100);
		map<string, BasketInfo*>::iterator it;
		vector<string> finished_basket_ids;
		for (it = baskets_map_.begin(); it != baskets_map_.end(); it++) {
			BasketInfo *basket = it->second;
			snprintf(trade_file_name, sizeof(trade_file_name), "%s\\%s_CJHB.csv", response_directory_, it->first.c_str());
			//cout << "Read CJHB file: " << trade_file_name << endl;
			ifstream trade_file;
			trade_file.open(trade_file_name, ifstream::in);
			if (trade_file.good())
			{
				int line_read = 0;
				while (!trade_file.eof()) {
					trade_file.getline(line, 2048);
					if (strcmp(line, "") == 0)
						continue;
					//line_read++;
					if (basket->trd_line_scanned == line_read) {
						basket->trd_line_scanned++;
						line_read++;
						if (basket->trd_line_scanned == 1) {
							continue;
						}
					}
					else {
						line_read++;
						continue;
					}

					//cout << "CJHB scanned:" << basket->trd_line_scanned << " read:" << line_read << endl;
					cout << line << endl;
					/*if (line[0] == '#') {
					continue;
					}*/
					fields.clear();
					split(line, string(","), fields);
					if (fields.size() != 12) {
						/*if (fields.size() == 0)
						continue;
						cout << "Response File Fields:" << endl;
						for (int i = 0; i < fields.size(); i++) {
						cout << i << " " << fields[i] << endl;
						}
						cout << endl;*/
						continue;
					}

					string direction;
					char dir;
					//if (fields[5] == "买入") {
					if (fields[5] == "0") {
						direction = "BUY";
						dir = '0';
					}
					//else if (fields[5] == "卖出") {
					else if (fields[5] == "1") {
						direction = "SELL";
						dir = '1';
					}

					char local_id[64];
					/*snprintf(local_id, sizeof(local_id), "%s_%s_%c",
						it->first.c_str(), fields[0].c_str(), dir);*/
					int cur_order_id = atoi(fields[3].c_str());
					snprintf(local_id, sizeof(local_id), "%012d", cur_order_id);
					OrderInfo *target_order = basket->Find(local_id);
					if (target_order == NULL) {
						cout << local_id << " not found!" << endl;
						continue;
					}
					if (target_order->status == 's') {
						// target order is not return the first feedback
						TradeInfo *trade = new TradeInfo(atoi(fields[7].c_str()), atof(fields[8].c_str()));
						target_order->PushTrade(trade);
					}
					else {
						string exch;
						string status = "PARTIALLYFILLED";
						if (fields[1] == "SSE") {
							exch = "SH";
						}
						else if (fields[1] == "SZE") {
							exch = "SZ";
						}

						char symbol[16];
						snprintf(symbol, sizeof(symbol), "%s.%s", fields[0].c_str(), exch.c_str());

						int vol = atoi(fields[7].c_str());
						double prc = atof(fields[8].c_str());
						target_order->trade_vol += vol;
						snprintf(message, sizeof(message), "TYPE=TRADE;STATUS=%s;SYS_ORDER_ID=%s;"
							"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%d;LIMIT_PRICE=%.2lf;"
							"DIRECTION=%s;FILLED_QUANTITY=%d;FILLED_PRICE=%.2lf;",
							status.c_str(), fields[2].c_str(), target_order->input_order.OrderRef, symbol, user_id_,
							target_order->input_order.Volume, target_order->input_order.LimitPrice,
							direction.c_str(), vol, prc);
						cout << message << endl;
						BOServer::Instance().Callback(message, this);
					}
				} // while reading trade file
			}
			trade_file.close();

			snprintf(response_file_name, sizeof(response_file_name), "%s\\%s_WTHB.csv", response_directory_, it->first.c_str());
			//cout << "Read WTHB file: " << response_file_name << endl;
			ifstream response_file;
			response_file.open(response_file_name, ifstream::in);
			if (response_file.good())
			{
				int line_read = 0;
				while (!response_file.eof()) {
					response_file.getline(line, 2048);
					if (strcmp(line, "") == 0)
						continue;
					//line_read++;
					if (basket->rsp_line_scanned == line_read) {
						basket->rsp_line_scanned++;
						line_read++;
						if (basket->rsp_line_scanned == 1) {
							continue;
						}
					}
					else {
						line_read++;
						continue;
					}

					//cout << "WTHB scanned:" << basket->rsp_line_scanned << " read:" << line_read << endl;
					cout << line << endl;
					/*if (line[0] == '#') {
						continue;
					}*/
					fields.clear();
					split(line, string(","), fields);
					if (fields.size() != 17) {
						/*if (fields.size() == 0)
						continue;
						cout << "Response File Fields:" << fields.size() << endl;
						for (int i = 0; i < fields.size(); i++) {
						cout << i << " " << fields[i] << endl;
						}
						cout << endl;*/
						continue;
					}

					string direction;
					char dir;
					//if (fields[5] == "买入") {
					if (fields[5] == "0") {
						direction = "BUY";
						dir = '0';
					}
					//else if (fields[5] == "卖出") {
					else if (fields[5] == "1") {
						direction = "SELL";
						dir = '1';
					}
					char local_id[64];
					/*snprintf(local_id, sizeof(local_id), "%s_%s_%c",
						it->first.c_str(), fields[0].c_str(), dir);*/
					int cur_order_id = atoi(fields[3].c_str());
					snprintf(local_id, sizeof(local_id), "%012d", cur_order_id);
					OrderInfo *target_order = basket->Find(local_id);
					if (target_order == NULL) {
						cout << local_id << " not found!" << endl;
						continue;
					}

					target_order->status = fields[14][0];

					string status;
					if (fields[16] != "0") {
						cout << "Rejected order with error code:" << fields[16] << endl;
						status = "REJECTED";
					}
					else {
						if (target_order->status == '3') {
							status = "RECEIVED";
						}
						else if (target_order->status == '0') {
							status = "FILLED";
						}
						else if (target_order->status == '2') {
							status = "PARTIALLYFILLEDUROUT";
						}
						else if (target_order->status == '4' || target_order->status == '5') {
							status = "CANCELED";
						}
					}

					string exch;
					if (fields[1] == "SSE") {
						exch = "SH";
					}
					else if (fields[1] == "SZE") {
						exch = "SZ";
					}

					char symbol[16];
					snprintf(symbol, sizeof(symbol), "%s.%s", fields[0].c_str(), exch.c_str());

					int remain_vol = atoi(fields[8].c_str());
					if (remain_vol < target_order->input_order.Volume - target_order->trade_vol) {
						while (true) {
							TradeInfo *trade = target_order->PopTrade();
							if (trade == NULL)
								break;
							target_order->trade_vol += trade->volume;
							snprintf(message, sizeof(message), "TYPE=TRADE;STATUS=%s;SYS_ORDER_ID=%s;"
								"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%d;LIMIT_PRICE=%.2lf;"
								"DIRECTION=%s;FILLED_QUANTITY=%d;FILLED_PRICE=%.2lf;",
								status.c_str(), fields[2].c_str(), target_order->input_order.OrderRef, symbol, user_id_,
								target_order->input_order.Volume, target_order->input_order.LimitPrice,
								direction.c_str(), trade->volume, trade->price);
							cout << "Late Trade: " << message << endl;
							BOServer::Instance().Callback(message, this);
						} // while
						if (remain_vol < target_order->input_order.Volume - target_order->trade_vol) {
							cout << "Miss trade with volume " << target_order->input_order.Volume - target_order->trade_vol - remain_vol << endl;
						}
					}

					snprintf(message, sizeof(message), "TYPE=UPDATE;STATUS=%s;SYS_ORDER_ID=%s;"
						"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%s;LIMIT_PRICE=%s;"
						"DIRECTION=%s;FILLED_QUANTITY=%s;AVG_FILLED_PRICE=%s;LEFT_QUANTITY=%s;",
						status.c_str(), fields[2].c_str(), target_order->input_order.OrderRef, symbol, user_id_, fields[7].c_str(),
						fields[9].c_str(), direction.c_str(), "0", "0.0", fields[8].c_str());
					cout << message << endl;
					BOServer::Instance().Callback(message, this);
				} // while not eof
			}
			line_read = 0;
			response_file.close();

			if (basket->Finished()) {
				finished_basket_ids.push_back(it->first);
			}
		} // for
		for (size_t i = 0; i < finished_basket_ids.size(); i++) {
			baskets_map_.erase(finished_basket_ids[i]);
		}
	} // while
}

void WinnerTrader::SendBasketOrders(int idx) {
	int basket_size = basket_size_[idx];
	basket_size_[idx] = 0;
	char basket_id[64];
	snprintf(basket_id, sizeof(basket_id), "%d%05d", date_, file_id_++);
	char all_str[4096] = "";
	char order_str[256];
	char local_id[64];
	string exch = "SZE";
	char direction = '0';
	char offset = '0'; // 0 for Open, 1 for Close, 3 for CloseToday, 4 for CloseYesterday
	char price_type = '2'; // Limit Price
	vector<OrderInfo*> order_list;
	for (int i = 0; i < basket_size; i++) {
		if (basket_orders_[idx][i].ExchangeType == BF_EXCH_SH)
			exch = "SSE";
		else if (basket_orders_[idx][i].ExchangeType == BF_EXCH_SZ)
			exch = "SZE";
		if (basket_orders_[idx][i].Direction == BF_D_BUY)
			direction = '0';
		else if (basket_orders_[idx][i].Direction == BF_D_SELL)
			direction = '1';
		/*snprintf(order_str, sizeof(order_str), "%s,%s,%c,%c,%d,%.2lf,%c,%s\r\n",
			basket_orders_[idx][i].InstrumentID, exch.c_str(), direction, offset,
			basket_orders_[idx][i].Volume, basket_orders_[idx][i].LimitPrice,
			price_type, basket_orders_[idx][i].OrderRef);*/
		int cur_order_id = local_order_id_++;
		snprintf(order_str, sizeof(order_str), "%s,%s,%c,%c,%d,%.2lf,%c,%012d\r\n",
			basket_orders_[idx][i].InstrumentID, exch.c_str(), direction, offset,
			basket_orders_[idx][i].Volume, basket_orders_[idx][i].LimitPrice,
			price_type, cur_order_id);
		/*snprintf(local_id, sizeof(local_id), "%s_%s_%c",
			basket_id, basket_orders_[idx][i].InstrumentID, direction);*/
		snprintf(local_id, sizeof(local_id), "%012d", cur_order_id);
		cout << "Write order: " << order_str << endl;
		strcat_s(all_str, sizeof(all_str), order_str);
		OrderInfo *order_info = new OrderInfo(basket_orders_[idx][i]);
		snprintf(order_info->local_id, sizeof(order_info->local_id), "%s", local_id);
		order_list.push_back(order_info);
	}
	BasketInfo *basket = new BasketInfo(order_list);
	baskets_map_.insert(pair<string, BasketInfo*>(string(basket_id), basket));

	ofstream basket_file;
	char basket_file_name[128];
	snprintf(basket_file_name, sizeof(basket_file_name), "%s\\%s.csv", order_directory_, basket_id);
	basket_file.open(basket_file_name, ofstream::out);
	basket_file << all_str;
	basket_file.flush();
	basket_file.close();
	char basket_id_record[256];
	snprintf(basket_id_record, sizeof(basket_id_record), "%05d-%012ld\r\n", file_id_, local_order_id_);
	basket_id_file_ << basket_id_record;
	basket_id_file_.flush();
}

int WinnerTrader::ReqUserLogin(BFReqUserLoginField *pLoginField, int nRequestID) {
	if (strcmp(pLoginField->UserID, user_id_) != 0) {
		return -1;
	}
	else {
		return 0;
	}
}


int WinnerTrader::ReqUserLogout(BFReqUserLogoutField *pLogoutField, int nRequestID) {

	return 0;
}

int WinnerTrader::ReqOrderInsert(BFInputOrderField *pInputOrder, int nRequestID) {
	int idx = queue_index_;
	if (timer_end_) {
		timer_end_ = 0;
		boost::thread wait_thread(boost::bind(&WinnerTrader::WaitToOrder, this));
	}
	basket_orders_[idx][basket_size_[idx]++] = *pInputOrder;
	return 0;
}

int WinnerTrader::ReqOrderAction(BFInputOrderActionField *pInputOrderAction, int nRequestID) {

	return 0;
}

int WinnerTrader::ReqQryTradingAccount(BFQryTradingAccountField *pTradingAccount, int nRequestID) {

	return 0;
}

int WinnerTrader::ReqQryInvestorPosition(BFQryInvestorPositionField *pInvestorPosition, int nRequestID) {

	return 0;
}

int WinnerTrader::ReqQryInvestorMarginStock(BFQryInvestorMarginStockField *pMarginStock, int nRequestID) {

	return 0;
}

void WinnerTrader::OnRspOrderInsert(BFInputOrderField *pInputOrder, BFRspInfoField *pRspInfo) {

}

void WinnerTrader::OnRspOrderAction(BFInputOrderActionField *pInputOrderAction, BFRspInfoField *pRspInfo) {

}

void WinnerTrader::OnRtnOrder(BFOrderField *pOrder, BFRspInfoField *pRspInfo) {

}

void WinnerTrader::OnRtnTrade(BFTradeField *pTrade, BFRspInfoField *pRspInfo) {

}

void WinnerTrader::OnRspQryTradingAccount(BFTradingAccountField *pTradingAccount, BFRspInfoField *pRspInfo) {

}

void WinnerTrader::OnRspQryInvestorPosition(BFInvestorPositionField *pInvestorPosition, BFRspInfoField *pRspInfo) {

}

void WinnerTrader::OnRspQryInvestorMarginStock(BFInvestorMarginStockField *pMarginStock, BFRspInfoField *pRspInfo) {

}

void WinnerTrader::OnRspError(BFRspInfoField *pRspInfo) {

}

