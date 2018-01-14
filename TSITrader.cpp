
#include "TSITrader.h"
#include "BOServer.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <fstream>

#include <windows.h>

using namespace std;


TSITrader::TSITrader(const char *user_id) {
	date_ = date_now();
	snprintf(user_id_, sizeof(user_id_), "%s", user_id);
	logined_ = false;
	request_id_ = 1;
	local_entrust_no_ = entrust_cancel_no_ = 1;
	interval_ = 100;
	orders_ = new order[50000];
	local_orders_ = new BFInputOrderField*[50000];
	memset(local_orders_, 0, 50000 * sizeof(BFInputOrderField*));
	snprintf(config_file_name_, sizeof(config_file_name_), "%s", "tsi.txt");
	LoadConfig();
	memset(basket_orders_, sizeof(basket_orders_), 0);
	memset(basket_size_, sizeof(basket_size_), 0);
	timer_end_ = 1;
	queue_index_ = 0;
	char log_file_name[64];
	snprintf(log_file_name, sizeof(log_file_name), "%s-%d.log", user_id_, date_);
	log_file_.open(log_file_name, ios::out | ios::app);
	file_id_ = 1;
}

TSITrader::~TSITrader() {

}

void TSITrader::Init() {
	cout << "Initialize trader api successfully" << endl;
	logined_ = true;
	//boost::thread keyboard_thread(boost::bind(&TSITrader::FreshKeyboardInput, this));
	boost::thread scan_result_order_thread(boost::bind(&TSITrader::ScanResultOrderFile, this));
	boost::thread scan_result_cancel_thread(boost::bind(&TSITrader::ScanResultCancelFile, this));
	boost::thread scan_response_thread(boost::bind(&TSITrader::ScanResponseFile, this));
	boost::thread scan_trade_thread(boost::bind(&TSITrader::ScanTradeFile, this));
}

void TSITrader::Release() {

}

bool TSITrader::Logined() {
	return logined_;
}

void TSITrader::LoadConfig() {
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
	it = config_map.find("result_dir");
	if (it != config_map.end()) {
		snprintf(result_directory_, sizeof(result_directory_), "%s", it->second.c_str());
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

void TSITrader::WaitToOrder() {
	Sleep(interval_);
	int queue_idx = queue_index_;
	queue_index_ = (queue_index_ + 1) % 2;
	timer_end_ = 1;
	SendBasketOrders(queue_idx);
}

void TSITrader::TopMostTSI() {
	HWND hq = FindWindow(NULL, L"»ªÌ©Ö¤È¯-MATIC½ðÈÚÖÕ¶Ë");
	if (hq == NULL) {
		cout << "TSI Client Not Found!" << endl;
		return;
	}
	DWORD pid, tid;
	tid = GetWindowThreadProcessId(hq, &pid);
	AttachThreadInput(GetWindowThreadProcessId(::GetForegroundWindow(), NULL), tid, TRUE);
	SetForegroundWindow(hq);
	hq = SetFocus(hq);
}

void TSITrader::FreshKeyboardInput() {
	cout << "Start to fresh virtual keyboard input..." << endl;
	int loop = 0;
	while (true) {
		/*if (loop % 200 == 0) {
			TopMostTSI();
		}*/
		loop = ++loop % 200;
		Sleep(10);
		// F1 - F12 : 112 - 123
		// F8 to insert order
		keybd_event(119, 0, 0, 0);
		keybd_event(119, 0, KEYEVENTF_KEYUP, 0);
		/*keybd_event(65, 0, 0, 0);
		keybd_event(65, 0, KEYEVENTF_KEYUP, 0);*/
	}
}

// Result order file header:
//    file_name, local_entrust_no, batch_no, entrust_no, fund_account, result, remark, trade_plat, asset_prop, compact_id
void TSITrader::ScanResultOrderFile() {
	cout << "Start to scan result order file..." << endl;
	char response_file_name[128];
	snprintf(response_file_name, sizeof(response_file_name), "%s\\result_order.%d.csv", result_directory_, date_);
	int line_scanned = 1;
	int line_read = 0;
	char line[2048];
	//char message[2048];
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
			split(string(line), ",", fields);
			if (fields.size() < 9 || fields.size() > 10) {
				if (fields.size() == 0)
					continue;
				cout << "Result order File Fields:" << endl;
				for (size_t i = 0; i < fields.size(); i++) {
					cout << i << " " << fields[i] << endl;
				}
				cout << endl;
				continue;
			}

			cout << "Result order line:" << line << endl;

			int entrust_no = atoi(fields[3].c_str());
			int local_entrust_no = atoi(fields[1].c_str());
			if (fields[5] != "0") {
				// if order insert error
				if (local_orders_[local_entrust_no] != NULL) {
					string status = "REJECTED";
					string exch, direction;
					if (local_orders_[local_entrust_no]->ExchangeType == BF_EXCH_SH)
						exch = "SH";
					else if (local_orders_[local_entrust_no]->ExchangeType == BF_EXCH_SZ)
						exch = "SZ";
					if (local_orders_[local_entrust_no]->Direction == BF_D_BUY)
						direction = "BUY";
					else if (local_orders_[local_entrust_no]->Direction == BF_D_SELL)
						direction = "SELL";

					char symbol[16];
					snprintf(symbol, sizeof(symbol), "%s.%s", local_orders_[local_entrust_no]->InstrumentID, exch.c_str());
					char message[2048];
					snprintf(message, sizeof(message), "TYPE=UPDATE;STATUS=%s;SYS_ORDER_ID=%s;"
						"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%d;LIMIT_PRICE=%lf;"
						"DIRECTION=%s;FILLED_QUANTITY=%s;AVG_FILLED_PRICE=%s;LEFT_QUANTITY=%d;",
						status.c_str(), "0", local_orders_[local_entrust_no]->OrderRef, symbol, user_id_,
						local_orders_[local_entrust_no]->Volume, local_orders_[local_entrust_no]->LimitPrice,
						direction.c_str(), "0", "0.0", local_orders_[local_entrust_no]->Volume);
					cout << message << endl;
					BOServer::Instance().Callback(message, this);
				}
			}
			if (entrust_no != -1 && entrust_nos_.find(entrust_no) == entrust_nos_.end()) {
				entrust_nos_.insert(entrust_no);
			}
			map<int, string>::iterator it = local_entrust2order_.find(local_entrust_no);
			if (it != local_entrust2order_.end()) {
				entrust2order_.insert(pair<int, string>(entrust_no, it->second));
			}
		} // while not eof
		line_read = 0;
		response_file.close();
	} // while
}

// Result cancel file header:
//    file_name, local_withdraw_no, fund_account, entrust_no, entrust_no_old, result, remark, asset_prop
void TSITrader::ScanResultCancelFile() {
	cout << "Start to scan result cancel file..." << endl;
	char response_file_name[128];
	snprintf(response_file_name, sizeof(response_file_name), "%s\\result_cancel.%d.csv", result_directory_, date_);
	int line_scanned = 1;
	int line_read = 0;
	char line[2048];
	//char message[2048];
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
			split(string(line), ",", fields);
			if (fields.size() < 7 || fields.size() > 8) {
				if (fields.size() == 0)
					continue;
				cout << "Result cancel File Fields:" << endl;
				for (size_t i = 0; i < fields.size(); i++) {
					cout << i << " " << fields[i] << endl;
				}
				cout << endl;
				continue;
			}

			cout << "Result cancel line:" << line << endl;
			if (fields[5] != "0") {
				continue;
			}
			int entrust_no = atoi(fields[3].c_str());
			if (entrust_nos_.find(entrust_no) == entrust_nos_.end()) {
				entrust_nos_.insert(entrust_no);
			}
			
		} // while not eof
		line_read = 0;
		response_file.close();
	} // while
}

// Response file header:
//    id, init_date, fund_account, batch_no, entrust_no, exchange_type, stock_account, stock_code, stock_name, entrust_bs,
//    entrust_price, entrust_amount, business_amount, business_price, entrust_type, entrust_status, entrust_time, entrust_date, entrust_prop, cancel_info,
//    withdraw_amount, position_str, report_no, report_time, orig_order_id, trade_plat
void TSITrader::ScanResponseFile() {
	cout << "Start to scan response file..." << endl;
	char response_file_name[128];
	snprintf(response_file_name, sizeof(response_file_name), "%s\\%s_ENTRUST.%d.csv", response_directory_, user_id_, date_);
	int line_scanned = 1;
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
			split(string(line), ",", fields);
			if (fields.size() < 25 || fields.size() > 26) {
				if (fields.size() == 0)
				continue;
				cout << "Response File Fields:" << endl;
				for (size_t i = 0; i < fields.size(); i++) {
				cout << i << " " << fields[i] << endl;
				}
				cout << endl;
				continue;
			}
			int entrust_no = atoi(fields[4].c_str());
			set<int>::iterator it = entrust_nos_.find(entrust_no);
			if (it == entrust_nos_.end()) {
				line_scanned--;
				break; // if entrust no not found, wait for this entrust no to continue
			}

			// entrust_type: 0:buy-sell 1:query 2:cancel 3:modify
			if (fields[14] == "0") {
				if (!orders_[entrust_no].init) {
					orders_[entrust_no].init = 1;
					snprintf(orders_[entrust_no].instrument_id, sizeof(orders_[entrust_no].instrument_id), "%s", fields[7].c_str());
					orders_[entrust_no].direction = atoi(fields[9].c_str());
					orders_[entrust_no].exch = atoi(fields[5].c_str());
					orders_[entrust_no].volume = atoi(fields[11].c_str());
					orders_[entrust_no].limit_price = atof(fields[10].c_str());
				}
			}
			cout << "Read response:" << line << endl;
			map<int, string>::iterator ito = entrust2order_.find(entrust_no);
			if (ito == entrust2order_.end()) {
				continue; // if local_order_id matching for entrust_no not found, pass this message
			}
			string local_order_id = ito->second;
			string status = "OTHERS";
			/*if (fields[15] == "2" || fields[15] == "8" ||
				fields[15] == "7" || fields[15] == "5" ||
				fields[15] == "6") {*/
			if (fields[15] == "2" || fields[15] == "8" ||
				fields[15] == "7") {
				status = "RECEIVED";
			}
			else if (fields[15] == "9") {
				status = "REJECTED";
			}
			if (status == "OTHERS") {
				cout << "Other status:" << fields[15] << ": " << line << endl;
				continue;
			}

			string direction;
			if (fields[9] == "1") {
				direction = "BUY";
			}
			else if (fields[9] == "2") {
				direction = "SELL";
			}

			string exch;
			if (fields[5] == "1") {
				exch = "SH";
			}
			else if (fields[5] == "2") {
				exch = "SZ";
			}

			char symbol[16];
			snprintf(symbol, sizeof(symbol), "%s.%s", fields[7].c_str(), exch.c_str());
			int vol = atoi(fields[11].c_str());
			int fill_vol = atoi(fields[12].c_str());
			int left_vol = vol - fill_vol;

			snprintf(message, sizeof(message), "TYPE=UPDATE;STATUS=%s;SYS_ORDER_ID=%s;"
				"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%s;LIMIT_PRICE=%s;"
				"DIRECTION=%s;FILLED_QUANTITY=%s;AVG_FILLED_PRICE=%s;LEFT_QUANTITY=%d;",
				status.c_str(), fields[4].c_str(), local_order_id.c_str(), symbol, fields[2].c_str(), fields[11].c_str(),
				fields[10].c_str(), direction.c_str(), fields[12].c_str(), "0.0", left_vol);
			cout << message << endl;
			BOServer::Instance().Callback(message, this);
		} // while not eof
		line_read = 0;
		response_file.close();
	} // while
}

// Trade File Header:
//   id, date, exchange_type, fund_account, stock_account, stock_code, entrust_bs, business_price, business_amount, business_time,
//   real_type, real_status, entrust_no, business_balance, stock_name, position_str, entrust_prop, business_no, serial_no, business_times,
//   report_no, orig_order_id, trade_plat
void TSITrader::ScanTradeFile() {
	cout << "Start to scan trade file..." << endl;
	char trade_file_name[128];
	snprintf(trade_file_name, sizeof(trade_file_name), "%s\\%s_DEAL.%d.csv", response_directory_, user_id_, date_);
	int line_scanned = 1;
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


			/*cout << "Response: scanned:" << line_scanned << " read:" << line_read << endl;
			cout << line << endl;*/
			if (line[0] == '#') {
				continue;
			}
			fields.clear();
			split(string(line), ",", fields);
			if (fields.size() < 22 || fields.size() > 23) {
				if (fields.size() == 0)
					continue;
				cout << "Response File Fields:" << endl;
				for (size_t i = 0; i < fields.size(); i++) {
					cout << i << " " << fields[i] << endl;
				}
				cout << endl;
				continue;
			}
			cout << "Read trade:" << line << endl;
			int entrust_no = atoi(fields[12].c_str());
			int orig_entrust_no = atoi(fields[21].c_str());
			set<int>::iterator it = entrust_nos_.find(orig_entrust_no);
			if (it == entrust_nos_.end()) {
				line_scanned--;
				break; // if entrust no not found, wait for this entrust no to continue
			}

			string status = "PARTIALLYFILLED";
			if (fields[10] == "0") {
				// if deal type is trade
				if (!orders_[entrust_no].init) {
					line_scanned--;
					break; // if order information not initalized, wait for this order
				}
				int fill_vol = atoi(fields[8].c_str());
				orders_[entrust_no].fill_vol += fill_vol;
				if (orders_[entrust_no].fill_vol == orders_[entrust_no].volume) {
					status = "FILLED";
				}
			}else if (fields[10] == "2") {
				// if deal type is cancel
				status = "CANCELED";
			}
			map<int, string>::iterator ito = entrust2order_.find(orig_entrust_no);
			if (ito == entrust2order_.end()) {
				break; // if local_order_id matching for entrust_no not found, pass this message
			}
			string local_order_id = ito->second;

			string direction;
			if (fields[6] == "1") {
				direction = "BUY";
			}
			else if (fields[6] == "2") {
				direction = "SELL";
			}

			string exch;
			if (fields[2] == "1") {
				exch = "SH";
			}
			else if (fields[2] == "2") {
				exch = "SZ";
			}

			char symbol[16];
			snprintf(symbol, sizeof(symbol), "%s.%s", fields[5].c_str(), exch.c_str());

			if (status == "PARTIALLYFILLED" || status == "FILLED") {
				snprintf(message, sizeof(message), "TYPE=TRADE;STATUS=%s;SYS_ORDER_ID=%d;"
					"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%d;LIMIT_PRICE=%lf;"
					"DIRECTION=%s;FILLED_QUANTITY=%s;FILLED_PRICE=%s;LEFT_QUANTITY=%d;",
					status.c_str(), orig_entrust_no, local_order_id.c_str(), symbol, fields[3].c_str(), orders_[orig_entrust_no].volume,
					orders_[orig_entrust_no].limit_price, direction.c_str(), fields[8].c_str(), fields[7].c_str(), 
					orders_[orig_entrust_no].volume - orders_[orig_entrust_no].fill_vol);
				cout << message << endl;
				BOServer::Instance().Callback(message, this);
			}
			snprintf(message, sizeof(message), "TYPE=UPDATE;STATUS=%s;SYS_ORDER_ID=%d;"
				"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%d;LIMIT_PRICE=%lf;"
				"DIRECTION=%s;FILLED_QUANTITY=%s;FILLED_PRICE=%s;LEFT_QUANTITY=%d;",
				status.c_str(), orig_entrust_no, local_order_id.c_str(), symbol, fields[3].c_str(), orders_[orig_entrust_no].volume,
				orders_[orig_entrust_no].limit_price, direction.c_str(), "0", "0.0", orders_[orig_entrust_no].volume - orders_[orig_entrust_no].fill_vol);
			cout << message << endl;
			BOServer::Instance().Callback(message, this);
		} // while not eof
		line_read = 0;
		trade_file.close();
	}
}

// fund_account, money_type, current_balance, begin_balance, enable_balance, foregift_balance, mortgage_balance, frozen_balance, unfrozen_balance, fetch_balance,
// fetch_cash, entrust_buy_balance, market_value, asset_balance, correct_balance, fund_balance, opfund_market_value, interest, integral_balance, fine_integral,
// pre_interest, pre_fine, pre_interest_tax, rate_kind
void TSITrader::ScanFundFile() {
	char fund_file_name[128];
	snprintf(fund_file_name, sizeof(fund_file_name), "%s\\%s_FUND.%d.csv", response_directory_, user_id_, date_);
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
		split(string(line), ",", fields);
		if (fields.size() < 23 || fields.size() > 24) {
			cout << "Fund Fields:" << fields.size() << endl;
			continue;
		}
		if (strcmp(fields[0].c_str(), user_id_)) {
			continue; // not this account or read the header line
		}
		snprintf(message, sizeof(message), "TYPE=ACCOUNT;ACCOUNT_ID=%s;IS_LAST=%d;"
			"RT_ACCOUNT_NET_WORTH=%lf;RT_CASH_BALANCE=%s;RT_MARKET_VALUE=%s;"
			"RT_TRADING_POWER=%s",
			user_id_, 1, 0.0, fields[2].c_str(), fields[12].c_str(), fields[4].c_str());
		cout << message << endl;
		BOServer::Instance().Callback(message, this);
	} // while
	fund_file.close();
}

// fund_account, exchange_type, stock_account, stock_code, stock_type, stock_name, hold_amount, current_amount, cost_price, income_balance,
// hand_flag, frozen_amount, unfrozen_amount, enable_amount, real_buy_amount, real_sell_amount, uncome_buy_amount, uncome_sell_amount, entrust_sell_amount, asset_price,
// last_price, market_value, position_str, sum_buy_amount, sum_buy_balance, sum_sell_amount, sum_sell_balance, real_buy_balance, real_sell_balance, ffare_kind,
// bfare_kind, profit_flag
void TSITrader::ScanPositionFile() {
	char position_file_name[128];
	snprintf(position_file_name, sizeof(position_file_name), "%s\\%s_POSITION.%d.csv", response_directory_, user_id_, date_);
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
		split(string(line), ",", fields);
		if (fields.size() < 31 || fields.size() > 32) {
			cout << "Position Fields:" << fields.size() << endl;
			continue;
		}
		if (strcmp(fields[0].c_str(), user_id_)) {
			continue; // not this account or read the header line
		}
		if (strcmp(message, "") != 0) {
			snprintf(pre_message, sizeof(pre_message), "%s;IS_LAST=%d;", message, 0);
		}
		string exch;
		if (fields[1] == "1") {
			exch = "SH";
		}
		else if (fields[1] == "2") {
			exch = "SZ";
		}

		char symbol[16];
		snprintf(symbol, sizeof(symbol), "%s.%s", fields[3].c_str(), exch.c_str());

		int available = atoi(fields[13].c_str());
		int ydpos = atoi(fields[8].c_str());
		int curpos = atoi(fields[7].c_str());
		/*int short_qty = ydpos - available;
		int long_qty = curpos - available;*/
		int short_qty = atoi(fields[15].c_str());
		int long_qty = atoi(fields[14].c_str());
		double avg_price = atof(fields[8].c_str());
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

// send order csv formatc
// local_entrust_no,fund_account,exchange_type,stock_code,entrust_bs,entrust_prop,entrust_price,entrust_amount,batch_no,client_filed1,clientfield2
void TSITrader::SendBasketOrders(int idx) {
	int basket_size = basket_size_[idx];
	basket_size_[idx] = 0;
	char all_str[4096] = "";
	char order_str[256];
	int exch = 0;
	int direction = 1;
	for (int i = 0; i < basket_size; i++) {
		if (basket_orders_[idx][i].ExchangeType == BF_EXCH_SH)
			exch = 1;
		else if (basket_orders_[idx][i].ExchangeType == BF_EXCH_SZ)
			exch = 2;
		if (basket_orders_[idx][i].Direction == BF_D_BUY)
			direction = 1;
		else if (basket_orders_[idx][i].Direction == BF_D_SELL)
			direction = 2;
		snprintf(order_str, sizeof(order_str), "%d,%s,%d,%s,%d,0,%.2lf,%d,,,\r\n",
			local_entrust_no_, user_id_, exch, basket_orders_[idx][i].InstrumentID,
			direction, basket_orders_[idx][i].LimitPrice, basket_orders_[idx][i].Volume);
		local_entrust2order_.insert(pair<int, string>(local_entrust_no_, basket_orders_[idx][i].OrderRef));
		local_orders_[local_entrust_no_] = new BFInputOrderField();
		memcpy(local_orders_[local_entrust_no_], &basket_orders_[idx][i], sizeof(BFInputOrderField));
		local_entrust_no_++;
		cout << "Write order: " << order_str << endl;
		strcat_s(all_str, sizeof(all_str), order_str);
	}

	ofstream basket_file;
	char basket_file_name[128];
	const char *time_str = time_now_long();
	snprintf(basket_file_name, sizeof(basket_file_name), "%s\\order_%09d.%s.csv", order_directory_, file_id_++, time_str);
	cout << basket_file_name << endl;
	basket_file.open(basket_file_name, ofstream::out);
	if (basket_file.good()) {
		basket_file << "local_entrust_no,fund_account,exchange_type,stock_code,entrust_bs,entrust_prop,entrust_price,entrust_amount,batch_no,client_filed1,clientfield2\r\n" << all_str;
		basket_file.flush();
		basket_file.close();
	}
	else {
		cout << "Cannot open file:" << basket_file_name << endl;
	}
	//TopMostTSI();
	//Sleep(100);
	// F8 to send order, double press
	keybd_event(119, 0, 0, 0);
	keybd_event(119, 0, KEYEVENTF_KEYUP, 0);
	//Sleep(5);
	keybd_event(119, 0, 0, 0);
	keybd_event(119, 0, KEYEVENTF_KEYUP, 0);
}

int TSITrader::ReqUserLogin(BFReqUserLoginField *pLoginField, int nRequestID) {
	if (strcmp(pLoginField->UserID, user_id_) != 0) {
		return -1;
	}
	else {
		return 0;
	}
}

int TSITrader::ReqUserLogout(BFReqUserLogoutField *pLogoutField, int nRequestID) {

	return 0;
}

int TSITrader::ReqOrderInsert(BFInputOrderField *pInputOrder, int nRequestID) {
	int idx = queue_index_;
	if (timer_end_) {
		timer_end_ = 0;
		boost::thread wait_thread(boost::bind(&TSITrader::WaitToOrder, this));
	}
	basket_orders_[idx][basket_size_[idx]++] = *pInputOrder;
	return 0;
}

int TSITrader::ReqOrderAction(BFInputOrderActionField *pInputOrderAction, int nRequestID) {
	char cancel_str[128];
	// find entrust_no of InputOrderAction
	snprintf(cancel_str, sizeof(cancel_str), "%d,%s,0,%s,,\r\n",
		entrust_cancel_no_++, user_id_, pInputOrderAction->OrderSysID);
	cout << "Write Cancel Request: " << cancel_str << endl;

	ofstream cancel_file;
	char cancel_file_name[128];
	const char *time_str = time_now_long();
	snprintf(cancel_file_name, sizeof(cancel_file_name), "%s\\cancel_%09d.%s.csv", cancel_directory_, file_id_++, time_str);
	cancel_file.open(cancel_file_name, ofstream::out);
	cancel_file << "local_withdraw_no,fund_account,batch_flag,entrust_no,client_filed1,clientfield2\r\n" << cancel_str;
	cancel_file.flush();
	cancel_file.close();
	return 0;
}

int TSITrader::ReqQryTradingAccount(BFQryTradingAccountField *pTradingAccount, int nRequestID) {
	boost::thread scan_fund_thread(boost::bind(&TSITrader::ScanFundFile, this));
	return 0;
}

int TSITrader::ReqQryInvestorPosition(BFQryInvestorPositionField *pInvestorPosition, int nRequestID) {
	boost::thread scan_position_thread(boost::bind(&TSITrader::ScanPositionFile, this));
	return 0;
}

int TSITrader::ReqQryInvestorMarginStock(BFQryInvestorMarginStockField *pMarginStock, int nRequestID) {

	return 0;
}

void TSITrader::OnRspOrderInsert(BFInputOrderField *pInputOrder, BFRspInfoField *pRspInfo) {

}

void TSITrader::OnRspOrderAction(BFInputOrderActionField *pInputOrderAction, BFRspInfoField *pRspInfo) {

}

void TSITrader::OnRtnOrder(BFOrderField *pOrder, BFRspInfoField *pRspInfo) {

}

void TSITrader::OnRtnTrade(BFTradeField *pTrade, BFRspInfoField *pRspInfo) {

}

void TSITrader::OnRspQryTradingAccount(BFTradingAccountField *pTradingAccount, BFRspInfoField *pRspInfo) {

}

void TSITrader::OnRspQryInvestorPosition(BFInvestorPositionField *pInvestorPosition, BFRspInfoField *pRspInfo) {

}

void TSITrader::OnRspQryInvestorMarginStock(BFInvestorMarginStockField *pMarginStock, BFRspInfoField *pRspInfo) {

}

void TSITrader::OnRspError(BFRspInfoField *pRspInfo) {

}
