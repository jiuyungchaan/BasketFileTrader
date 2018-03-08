
#include "TSICreditTrader.h"
#include "BOServer.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <fstream>

#include <windows.h>

using namespace std;


TSICreditTrader::TSICreditTrader(const char *user_id) {
	date_ = date_now();
	snprintf(user_id_, sizeof(user_id_), "%s", user_id);
	logined_ = false;
	request_id_ = 1;
	local_entrust_no_ = entrust_cancel_no_ = 1;
	interval_ = 100;
	orders_ = new order[50000];
	local_orders_ = new BFInputOrderField*[50000];
	memset(local_orders_, 0, 50000 * sizeof(BFInputOrderField*));
	snprintf(config_file_name_, sizeof(config_file_name_), "%s", "tsi-credit.txt");
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

TSICreditTrader::~TSICreditTrader() {

}

void TSICreditTrader::Init() {
	cout << "Initialize trader api successfully" << endl;
	logined_ = true;
	boost::thread keyboard_thread(boost::bind(&TSICreditTrader::FreshKeyboardInput, this));
	boost::thread scan_result_order_thread(boost::bind(&TSICreditTrader::ScanResultOrderFile, this));
	boost::thread scan_result_cancel_thread(boost::bind(&TSICreditTrader::ScanResultCancelFile, this));
	boost::thread scan_response_thread(boost::bind(&TSICreditTrader::ScanResponseFile, this));
	boost::thread scan_trade_thread(boost::bind(&TSICreditTrader::ScanTradeFile, this));
}

void TSICreditTrader::Release() {

}

bool TSICreditTrader::Logined() {
	return logined_;
}

void TSICreditTrader::LoadConfig() {
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

void TSICreditTrader::WaitToOrder() {
	Sleep(interval_);
	int queue_idx = queue_index_;
	queue_index_ = (queue_index_ + 1) % 2;
	timer_end_ = 1;
	SendBasketOrders(queue_idx);
}

void TSICreditTrader::TopMostTSI() {
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

void TSICreditTrader::FreshKeyboardInput() {
	cout << "Start to fresh virtual keyboard input..." << endl;
	int loop = 0;
	while (true) {
		/*if (loop % 200 == 0) {
		TopMostTSI();
		}*/
		loop = ++loop % 200;
		Sleep(200);
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
//    tag
void TSICreditTrader::ScanResultOrderFile() {
	cout << "Start to scan result order file..." << endl;
	char response_file_name[128];
	snprintf(response_file_name, sizeof(response_file_name), "%s\\result_order.%d.csv", result_directory_, date_);
	int line_scanned = 1;
	int line_read = 0;
	char line[2048];
	//char message[2048];
	vector<string> fields;
	while (true) {
		Sleep(5);
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
			if (fields.size() < 9 || fields.size() > 11) {
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
			if (strcmp(fields[4].c_str(), user_id_) != 0)
				continue;
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
void TSICreditTrader::ScanResultCancelFile() {
	cout << "Start to scan result cancel file..." << endl;
	char response_file_name[128];
	snprintf(response_file_name, sizeof(response_file_name), "%s\\result_cancel.%d.csv", result_directory_, date_);
	int line_scanned = 1;
	int line_read = 0;
	char line[2048];
	//char message[2048];
	vector<string> fields;
	while (true) {
		Sleep(5);
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
			if (strcmp(fields[2].c_str(), user_id_) != 0) {
				continue;
			}
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
//    withdraw_amount, position_str, report_no, report_time, orig_order_id, trade_plat, max_price_levels, trade_time_type, mac, client_version,
//    tag
//    id, init_date, entrust_time, fund_account, asset_prop, batch_no, entrust_no, orig_order_id, exchange_type, stock_code,
//    stock_name, entrust_bs, entrust_price, entrust_amount, entrust_prop, entrust_type, entrust_status, business_amount, business_price, withdraw_amount,
//    cashgroup_prop, compact_id, report_no, cancel_info, trade_name, mac, client_version, tag
void TSICreditTrader::ScanResponseFile() {
	cout << "Start to scan response file..." << endl;
	char response_file_name[128];
	snprintf(response_file_name, sizeof(response_file_name), "%s\\%s_RZRQ_ENTRUST.%d.csv", response_directory_, user_id_, date_);
	int line_scanned = 1;
	int line_read = 0;
	char line[2048];
	char message[2048];
	vector<string> fields;
	int times_waited = 1;
	int no_not_found = -1;
	while (true) {
		Sleep(20);
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
			if (fields.size() < 25 || fields.size() > 28) {
				if (fields.size() == 0)
					continue;
				cout << "Response File Fields:" << endl;
				for (size_t i = 0; i < fields.size(); i++) {
					cout << i << " " << fields[i] << endl;
				}
				cout << endl;
				continue;
			}
			int entrust_no = atoi(fields[6].c_str());
			set<int>::iterator it = entrust_nos_.find(entrust_no);
			if (it == entrust_nos_.end()) {
				if (entrust_no != no_not_found) {
					no_not_found = entrust_no;
					times_waited = 1;
					line_scanned--;
					break; // if entrust no not found, wait for this entrust no to continue
				}
				else {
					times_waited++;
					if (times_waited > 5) {
						entrust_nos_.insert(entrust_no);
					}
					else {
						line_scanned--;
						break;
					}
				}
			}

			// entrust_type: 0:buy-sell 1:query 2:cancel 3:modify
			// entrust_type: 6:funding entrust 7:borrowing entrust 9:credit trading
			int entrust_type = atoi(fields[15].c_str());
			int dir;
			if (entrust_type == 6 || entrust_type == 7 || entrust_type == 9) {
				if (!orders_[entrust_no].init) {
					orders_[entrust_no].init = 1;
					int entrust_bs = atoi(fields[11].c_str());
					if (entrust_bs == 1) {
						if (entrust_type == 6)
							dir = BF_D_BORROWTOBUY;
						else if (entrust_type == 7)
							dir = BF_D_BUYTOPAY;
						else if (entrust_type == 9)
							dir = BF_D_COLLATERALBUY;
					}
					else {
						if (entrust_type == 6)
							dir = BF_D_SELLTOPAY;
						else if (entrust_type == 7)
							dir = BF_D_BORROWTOSELL;
						else if (entrust_type == 9)
							dir = BF_D_COLLATERALSELL;
					}
					snprintf(orders_[entrust_no].instrument_id, sizeof(orders_[entrust_no].instrument_id), "%s", fields[9].c_str());
					orders_[entrust_no].direction = dir;
					orders_[entrust_no].exch = atoi(fields[8].c_str());
					orders_[entrust_no].volume = atoi(fields[13].c_str());
					orders_[entrust_no].limit_price = atof(fields[12].c_str());
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
			if (fields[16] == "2" || fields[16] == "8" ||
				fields[16] == "7") {
				status = "RECEIVED";
			}
			else if (fields[16] == "9") {
				status = "REJECTED";
			}
			if (status == "OTHERS") {
				cout << "Other status:" << fields[16] << ": " << line << endl;
				continue;
			}

			string direction;
			if (dir == BF_D_COLLATERALBUY) {
				direction = "COLLATERALBUY";
			}
			else if (dir == BF_D_COLLATERALSELL) {
				direction = "COLLATERALSELL";
			}
			else if (dir == BF_D_BORROWTOBUY) {
				direction = "BORROWTOBUY";
			}
			else if (dir == BF_D_BORROWTOSELL) {
				direction = "BORROWTOSELL";
			}
			else if (dir == BF_D_BUYTOPAY) {
				direction = "BUYTOPAY";
			}
			else if (dir == BF_D_SELLTOPAY) {
				direction = "SELLTOPAY";
			}

			string exch;
			if (fields[8] == "1") {
				exch = "SH";
			}
			else if (fields[8] == "2") {
				exch = "SZ";
			}

			char symbol[16];
			snprintf(symbol, sizeof(symbol), "%s.%s", fields[9].c_str(), exch.c_str());
			int vol = atoi(fields[13].c_str());
			int fill_vol = atoi(fields[17].c_str());
			int left_vol = vol - fill_vol;

			snprintf(message, sizeof(message), "TYPE=UPDATE;STATUS=%s;SYS_ORDER_ID=%s;"
				"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%s;LIMIT_PRICE=%s;"
				"DIRECTION=%s;FILLED_QUANTITY=%s;AVG_FILLED_PRICE=%s;LEFT_QUANTITY=%d;",
				status.c_str(), fields[6].c_str(), local_order_id.c_str(), symbol, fields[3].c_str(), fields[13].c_str(),
				fields[12].c_str(), direction.c_str(), fields[17].c_str(), "0.0", left_vol);
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
//   report_no, orig_order_id, trade_plat, tag
//   id, init_date, business_time, fund_account, asset_prop, exchange_type, stock_code, stock_name, business_no, business_price,
//   business_amount, entrust_bs, real_type, real_status, business_balance, entrust_no, orig_order_id, entrust_prop, entrust_type, report_no,
//   trade_name, tag
void TSICreditTrader::ScanTradeFile() {
	cout << "Start to scan trade file..." << endl;
	char trade_file_name[128];
	snprintf(trade_file_name, sizeof(trade_file_name), "%s\\%s_RZRQ_DEAL.%d.csv", response_directory_, user_id_, date_);
	int line_scanned = 1;
	int line_read = 0;
	char line[2048];
	char message[2048];
	vector<string> fields;
	while (true) {
		Sleep(20);
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
			if (fields.size() < 21 || fields.size() > 24) {
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
			int entrust_no = atoi(fields[15].c_str());
			int orig_entrust_no = atoi(fields[16].c_str());
			set<int>::iterator it = entrust_nos_.find(orig_entrust_no);
			if (it == entrust_nos_.end()) {
				line_scanned--;
				break; // if entrust no not found, wait for this entrust no to continue
			}

			string status = "PARTIALLYFILLED";
			if (fields[12] == "0") {
				// if deal type is trade
				if (!orders_[entrust_no].init) {
					line_scanned--;
					break; // if order information not initalized, wait for this order
				}
				int fill_vol = atoi(fields[10].c_str());
				orders_[entrust_no].fill_vol += fill_vol;
				if (orders_[entrust_no].fill_vol == orders_[entrust_no].volume) {
					status = "FILLED";
				}
			}
			else if (fields[12] == "2") {
				// if deal type is cancel
				status = "CANCELED";
			}
			map<int, string>::iterator ito = entrust2order_.find(orig_entrust_no);
			if (ito == entrust2order_.end()) {
				break; // if local_order_id matching for entrust_no not found, pass this message
			}
			string local_order_id = ito->second;

			int dir = orders_[entrust_no].direction;
			string direction;
			if (dir == BF_D_COLLATERALBUY) {
				direction = "COLLATERALBUY";
			}
			else if (dir == BF_D_COLLATERALSELL) {
				direction = "COLLATERALSELL";
			}
			else if (dir == BF_D_BORROWTOBUY) {
				direction = "BORROWTOBUY";
			}
			else if (dir == BF_D_BORROWTOSELL) {
				direction = "BORROWTOSELL";
			}
			else if (dir == BF_D_BUYTOPAY) {
				direction = "BUYTOPAY";
			}
			else if (dir == BF_D_SELLTOPAY) {
				direction = "SELLTOPAY";
			}

			string exch;
			if (fields[5] == "1") {
				exch = "SH";
			}
			else if (fields[5] == "2") {
				exch = "SZ";
			}

			char symbol[16];
			snprintf(symbol, sizeof(symbol), "%s.%s", fields[6].c_str(), exch.c_str());

			if (status == "PARTIALLYFILLED" || status == "FILLED") {
				snprintf(message, sizeof(message), "TYPE=TRADE;STATUS=%s;SYS_ORDER_ID=%d;"
					"LOCAL_ORDER_ID=%s;SYMBOL=%s;ACCOUNT=%s;QUANTITY=%d;LIMIT_PRICE=%lf;"
					"DIRECTION=%s;FILLED_QUANTITY=%s;FILLED_PRICE=%s;LEFT_QUANTITY=%d;",
					status.c_str(), orig_entrust_no, local_order_id.c_str(), symbol, fields[3].c_str(), orders_[orig_entrust_no].volume,
					orders_[orig_entrust_no].limit_price, direction.c_str(), fields[10].c_str(), fields[9].c_str(),
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
// pre_interest, pre_fine, pre_interest_tax, rate_kind, hk_enable_balance
// fund_account, asset_prop, fund_asset, market_value, assure_asset, total_debit, per_assurescale_value, enable_bail_balance, used_bail_balance, assure_enbuy_balance,
// fin_enbuy_balance, slo_enrepaid_balance, fin_enrepaid_balance, fin_max_quota, fin_enable_quota, fin_used_quota, fin_used_bail, fin_compact_balance, fin_compact_fare, fin_compact_interest,
// fin_market_value, fin_income, slo_max_quota, slo_enable_quota, slo_used_quota, slo_used_bail, slo_compact_balance, slo_compact_fare, slo_compact_interest, slo_market_value,
// slo_income, other_fare, underly_market_value, fin_unbusi_balance, slo_unbusi_balance, enable_out_asset, fin_ratio, other_compact_balance, other_compact_interest, assure_secudis_balance,
// slo_sell_balance, sum_compact_interest, fin_max_balance, correct_balance, crdt_level, net_asset, refcost_fare, fetch_balance
void TSICreditTrader::ScanFundFile() {
	char fund_file_name[128];
	snprintf(fund_file_name, sizeof(fund_file_name), "%s\\%s_RZRQ_FUND.%d.csv", response_directory_, user_id_, date_);
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
		if (fields.size() < 47 || fields.size() > 48) {
			cout << "Fund Fields:" << fields.size() << endl;
			continue;
		}
		if (strcmp(fields[0].c_str(), user_id_)) {
			continue; // not this account or read the header line
		}
		snprintf(message, sizeof(message), "TYPE=ACCOUNT;ACCOUNT_ID=%s;IS_LAST=%d;"
			"RT_ACCOUNT_NET_WORTH=%lf;RT_CASH_BALANCE=%s;RT_MARKET_VALUE=%s;"
			"RT_TRADING_POWER=%s",
			user_id_, 1, 0.0, fields[7].c_str(), fields[3].c_str(), fields[2].c_str());
		cout << message << endl;
		BOServer::Instance().Callback(message, this);
	} // while
	fund_file.close();
}

// fund_account, exchange_type, stock_account, stock_code, stock_type, stock_name, hold_amount, current_amount, cost_price, income_balance,
// hand_flag, frozen_amount, unfrozen_amount, enable_amount, real_buy_amount, real_sell_amount, uncome_buy_amount, uncome_sell_amount, entrust_sell_amount, asset_price,
// last_price, market_value, position_str, sum_buy_amount, sum_buy_balance, sum_sell_amount, sum_sell_balance, real_buy_balance, real_sell_balance, ffare_kind,
// bfare_kind, profit_flag
// fund_account, asset_prop, exchange_type, stock_code, stock_name, stock_type, current_amount, hold_amount, enable_amount, last_price,
// cost_price, income_balance, hand_flag, market_value, av_buy_price, av_income_balance, cost_balance, uncome_buy_amount, uncome_sell_amount, entrust_sell_amount,
// real_buy_amount, real_sell_amount, asset_price, keep_cost_price, assure_ratio
void TSICreditTrader::ScanPositionFile() {
	char position_file_name[128];
	snprintf(position_file_name, sizeof(position_file_name), "%s\\%s_RZRQ_POSITION.%d.csv", response_directory_, user_id_, date_);
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
		if (fields.size() < 25 || fields.size() > 27) {
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

		int available = atoi(fields[8].c_str());
		int curpos = atoi(fields[6].c_str());
		/*int short_qty = ydpos - available;
		int long_qty = curpos - available;*/
		int short_qty = atoi(fields[21].c_str());
		int long_qty = atoi(fields[20].c_str());
		double avg_price = atof(fields[10].c_str());
		double total_cost = avg_price * (double)curpos;

		snprintf(message, sizeof(message), "TYPE=POSITION;ACCOUNT_ID=%s;POSITION_TYPE=LONG_POSITION;"
			"SYMBOL=%s;QUANTITY=%d;AVAILABLE_QTY=%d;LONG_QTY=%d;SHORT_QTY=%d;AVERAGE_PRICE=%lf;"
			"TOTAL_COST=%lf;MARKET_VALUE=%s;TODAY_POSITION=%s",
			user_id_, symbol, curpos, available, long_qty, short_qty, avg_price, total_cost, fields[13].c_str(), fields[6].c_str());

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

// open_date, compact_id, fund_account, money_type, exchange_type, stock_account, stock_code, stock_name, stock_type, crdt_ratio,
// entrust_no, entrust_price, entrust_amount, business_amount, business_balance, business_fare, compact_type, compact_status, begin_compact_amount, begin_compact_balance,
// begin_compact_fare, real_compact_balance, real_compact_amount, real_compact_fare, real_compact_interest, total_debit, year_rate, ret_end_date, date_clear, used_bail_balance,
// compact_interest, repaid_amount, repaid_balance, repaid_interest, cashgroup_no, compact_source, fin_income, slo_income, business_price, postpone_end_date,
// remark
void TSICreditTrader::ScanCompactFile() {
	char compact_file_name[128];
	snprintf(compact_file_name, sizeof(compact_file_name), "%s\\%s_RZRQ_COMPACT.%d.csv", response_directory_, user_id_, date_);
	char line[2048];
	char message[2048] = { 0 };
	char pre_message[2048] = { 0 };
	vector<string> fields;
	map<string, margin_stock*> position_map;
	ifstream position_file;
	position_file.open(compact_file_name, ifstream::in);
	while (!position_file.eof()) {
		position_file.getline(line, 2048);
		if (strcmp(line, "") == 0 || line[0] == '#')
			continue;
		fields.clear();
		split(string(line), ",", fields);
		if (fields.size() < 40 || fields.size() > 41) {
			cout << "Margin Stock Fields:" << fields.size() << endl;
			continue;
		}
		if (strcmp(fields[2].c_str(), user_id_)) {
			continue; // not this account or read the header line
		}
		string exch;
		if (fields[4] == "1") {
			exch = "SH";
		}
		else if (fields[4] == "2") {
			exch = "SZ";
		}

		char symbol[16];
		snprintf(symbol, sizeof(symbol), "%s.%s", fields[6].c_str(), exch.c_str());

		int d = atoi(fields[0].c_str());
		int short_qty = atoi(fields[22].c_str());
		if (short_qty == 0)
			continue;

		map<string, margin_stock*>::iterator it = position_map.find(string(symbol));
		margin_stock *ms;
		if (it != position_map.end()) {
			ms = it->second;
		}
		else {
			ms = new margin_stock();
			position_map.insert(pair<string, margin_stock*>(string(symbol), ms));
		}
		if (d == date_) {
			ms->td_volume += short_qty;
		}
		else {
			ms->yd_volume += short_qty;
		}
	} // while

	int sz = position_map.size();
	int cnt = 0;
	for (map<string, margin_stock*>::iterator it = position_map.begin(); it != position_map.end(); it++) {
		int is_last = 0;
		if (++cnt == sz)
			is_last = 1;
		snprintf(message, sizeof(message), "TYPE=MARGINSTOCK;ACCOUNT_ID=%s;SYMBOL=%s;TOTAL_MARGIN_QUOTA=0;"
			"AVAILABLE_MARGIN_QUOTA=0;QUANTITY=%d;TODAY_POSITION=%d;IS_LAST=%d;",
			user_id_, it->first.c_str(), it->second->yd_volume, it->second->td_volume, is_last);
		delete it->second;
		cout << message << endl;
		BOServer::Instance().Callback(message, this);
	}
	position_file.close();
}

// send order csv formatc
// local_entrust_no,fund_account,exchange_type,stock_code,entrust_bs,entrust_prop,entrust_price,entrust_amount,batch_no,client_filed1,clientfield2
void TSICreditTrader::SendBasketOrders(int idx) {
	int basket_size = basket_size_[idx];
	basket_size_[idx] = 0;
	char all_str[4096] = "";
	char order_str[256];
	int exch = 0;
	int direction = 1;
	int entrust_type = 0;
	int cashgroup_prop = 1;
	int asset_prop = 7;
	for (int i = 0; i < basket_size; i++) {
		if (basket_orders_[idx][i].ExchangeType == BF_EXCH_SH)
			exch = 1;
		else if (basket_orders_[idx][i].ExchangeType == BF_EXCH_SZ)
			exch = 2;
		if (basket_orders_[idx][i].Direction == BF_D_COLLATERALBUY) {
			direction = 1;
			entrust_type = 9;
		}
		else if (basket_orders_[idx][i].Direction == BF_D_COLLATERALSELL) {
			direction = 2;
			entrust_type = 9;
		}
		else if (basket_orders_[idx][i].Direction == BF_D_BORROWTOBUY) {
			direction = 1;
			entrust_type = 6;
		}
		else if (basket_orders_[idx][i].Direction == BF_D_BORROWTOSELL) {
			direction = 2;
			entrust_type = 7;
			cashgroup_prop = 2;
		}
		else if (basket_orders_[idx][i].Direction == BF_D_BUYTOPAY) {
			direction = 1;
			entrust_type = 7;
			cashgroup_prop = 2;
		}
		else if (basket_orders_[idx][i].Direction == BF_D_SELLTOPAY) {
			direction = 2;
			entrust_type = 6;
		}
		snprintf(order_str, sizeof(order_str), "%d,%s,%d,%s,%d,0,%.2lf,%d,,%d,%d,%d,,,\r\n",
			local_entrust_no_, user_id_, exch, basket_orders_[idx][i].InstrumentID,
			direction, basket_orders_[idx][i].LimitPrice, basket_orders_[idx][i].Volume,
			asset_prop, entrust_type, cashgroup_prop);
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
		basket_file << "local_entrust_no,fund_account,exchange_type,stock_code,entrust_bs,entrust_prop,entrust_price,entrust_amount,batch_no,asset_prop,entrust_type,cashgroup_prop,compact_id,client_filed1,clientfield2\r\n" << all_str;
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

int TSICreditTrader::ReqUserLogin(BFReqUserLoginField *pLoginField, int nRequestID) {
	if (strcmp(pLoginField->UserID, user_id_) != 0) {
		return -1;
	}
	else {
		return 0;
	}
}

int TSICreditTrader::ReqUserLogout(BFReqUserLogoutField *pLogoutField, int nRequestID) {

	return 0;
}

int TSICreditTrader::ReqOrderInsert(BFInputOrderField *pInputOrder, int nRequestID) {
	int idx = queue_index_;
	if (timer_end_) {
		timer_end_ = 0;
		boost::thread wait_thread(boost::bind(&TSICreditTrader::WaitToOrder, this));
	}
	basket_orders_[idx][basket_size_[idx]++] = *pInputOrder;
	return 0;
}

int TSICreditTrader::ReqOrderAction(BFInputOrderActionField *pInputOrderAction, int nRequestID) {
	char cancel_str[128];
	// find entrust_no of InputOrderAction
	int asset_prop = 7;
	snprintf(cancel_str, sizeof(cancel_str), "%d,%s,0,%s,%d,,\r\n",
		entrust_cancel_no_++, user_id_, pInputOrderAction->OrderSysID, asset_prop);
	cout << "Write Cancel Request: " << cancel_str << endl;

	ofstream cancel_file;
	char cancel_file_name[128];
	const char *time_str = time_now_long();
	snprintf(cancel_file_name, sizeof(cancel_file_name), "%s\\cancel_%09d.%s.csv", cancel_directory_, file_id_++, time_str);
	cancel_file.open(cancel_file_name, ofstream::out);
	cancel_file << "local_withdraw_no,fund_account,batch_flag,entrust_no,asset_prop,client_filed1,clientfield2\r\n" << cancel_str;
	cancel_file.flush();
	cancel_file.close();
	return 0;
}


int TSICreditTrader::ReqQryTradingAccount(BFQryTradingAccountField *pTradingAccount, int nRequestID) {
	boost::thread scan_fund_thread(boost::bind(&TSICreditTrader::ScanFundFile, this));
	return 0;
}

int TSICreditTrader::ReqQryInvestorPosition(BFQryInvestorPositionField *pInvestorPosition, int nRequestID) {
	boost::thread scan_position_thread(boost::bind(&TSICreditTrader::ScanPositionFile, this));
	return 0;
}

int TSICreditTrader::ReqQryInvestorMarginStock(BFQryInvestorMarginStockField *pMarginStock, int nRequestID) {
	boost::thread scan_compact_thread(boost::bind(&TSICreditTrader::ScanCompactFile, this));
	return 0;
}

void TSICreditTrader::OnRspOrderInsert(BFInputOrderField *pInputOrder, BFRspInfoField *pRspInfo) {

}

void TSICreditTrader::OnRspOrderAction(BFInputOrderActionField *pInputOrderAction, BFRspInfoField *pRspInfo) {

}

void TSICreditTrader::OnRtnOrder(BFOrderField *pOrder, BFRspInfoField *pRspInfo) {

}

void TSICreditTrader::OnRtnTrade(BFTradeField *pTrade, BFRspInfoField *pRspInfo) {

}

void TSICreditTrader::OnRspQryTradingAccount(BFTradingAccountField *pTradingAccount, BFRspInfoField *pRspInfo) {

}

void TSICreditTrader::OnRspQryInvestorPosition(BFInvestorPositionField *pInvestorPosition, BFRspInfoField *pRspInfo) {

}

void TSICreditTrader::OnRspQryInvestorMarginStock(BFInvestorMarginStockField *pMarginStock, BFRspInfoField *pRspInfo) {

}

void TSICreditTrader::OnRspError(BFRspInfoField *pRspInfo) {

}
