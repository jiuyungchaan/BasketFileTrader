#pragma once


#include <map>
#include <set>
#include <vector>
#include <string>
#include <fstream>
#include <boost/thread/thread.hpp>

#include "BasketFileTraderApi.h"

class WinnerTrader : public BasketFileTraderApi
{
public:
	WinnerTrader(const char *user_id);
	virtual ~WinnerTrader();

	void Init();
	void Release();
	bool Logined();

	/// api interfaces
	int ReqUserLogin(BFReqUserLoginField *pLoginField, int nRequestID);
	int ReqUserLogout(BFReqUserLogoutField *pLogoutField, int nRequestID);
	int ReqOrderInsert(BFInputOrderField *pInputOrder, int nRequestID);
	int ReqOrderAction(BFInputOrderActionField *pInputOrderAction, int nRequestID);
	int ReqQryTradingAccount(BFQryTradingAccountField *pTradingAccount, int nRequestID);
	int ReqQryInvestorPosition(BFQryInvestorPositionField *pInvestorPosition, int nRequestID);
	int ReqQryInvestorMarginStock(BFQryInvestorMarginStockField *pMarginStock, int nRequestID);

	void OnRspOrderInsert(BFInputOrderField *pInputOrder, BFRspInfoField *pRspInfo);
	void OnRspOrderAction(BFInputOrderActionField *pInputOrderAction, BFRspInfoField *pRspInfo);
	void OnRtnOrder(BFOrderField *pOrder, BFRspInfoField *pRspInfo);
	void OnRtnTrade(BFTradeField *pTrade, BFRspInfoField *pRspInfo);
	void OnRspQryTradingAccount(BFTradingAccountField *pTradingAccount, BFRspInfoField *pRspInfo);
	void OnRspQryInvestorPosition(BFInvestorPositionField *pInvestorPosition, BFRspInfoField *pRspInfo);
	void OnRspQryInvestorMarginStock(BFInvestorMarginStockField *pMarginStock, BFRspInfoField *pRspInfo);
	void OnRspError(BFRspInfoField *pRspInfo);

private:

	struct TradeInfo {
		int volume;
		double price;
		TradeInfo *next;

		TradeInfo() : volume(0), price(0.0), next(NULL) {}
		TradeInfo(int v, double p) : volume(v), price(p), next(NULL) {}
	};

	struct OrderInfo {
		char status; // LTS TFtdcOrderStatusType
		// Sent-'s' Not in TFtdcOrderStatusType
		// AllTraded-'0' PartTradedQueueing-'1' PartTradedNotQueueing-'2' NoTradeQueueing-'3'
		// NoTradeNotQueueing-'4' Canceled-'5' Unknown-'a' NotTouched-'b' Touched-'c'
		char local_id[64]; // specified with BASKETID_CODE_DIR
		int trade_vol;
		BFInputOrderField input_order;
		TradeInfo *trades;

		OrderInfo(BFInputOrderField &order) {
			status = 's';
			trade_vol = 0;
			trades = NULL;
			memcpy(&input_order, &order, sizeof(input_order));
		}

		bool Finished() {
			if (status == '0' || status == '2' || status == '4' ||
				status == '5') {
				return true;
			}
			return false;
		}

		void PushTrade(TradeInfo *trade) {
			if (trades == NULL)
				trades = trade;
			else {
				TradeInfo *tmp = trades;
				while (true) {
					if (tmp->next == NULL) {
						tmp->next = trade;
						break;
					}
					else {
						tmp = tmp->next;
					}
				} // while
			}
		}

		TradeInfo *PopTrade() {
			if (trades == NULL)
				return trades;
			else {
				TradeInfo *tmp = trades;
				trades = trades->next;
				return tmp;
			}
		}
	};

	struct BasketInfo {
		int trd_line_scanned;
		int rsp_line_scanned;
		std::vector<OrderInfo*> order_list;

		BasketInfo(std::vector<OrderInfo*> &orders) {
			trd_line_scanned = rsp_line_scanned = 0; // pass header line
			order_list = orders;
		}

		bool Finished() {
			bool finished = true;
			for (int i = 0; i < order_list.size(); i++) {
				if (!order_list[i]->Finished()) {
					finished = false;
					break;
				} // if
			} // for
			return finished;
		}

		OrderInfo *Find(const char *local_id) {
			OrderInfo *target_order = NULL;
			for (int i = 0; i < order_list.size(); i++) {
				if (strcmp(local_id, order_list[i]->local_id) == 0) {
					target_order = order_list[i];
					break;
				}
			} // for
			return target_order;
		}
	};

	void LoadBasketID();
	void WaitToOrder();
	void FreshKeyboardInput();
	void TopMostWinner();
	void SendBasketOrders(int idx);
	void ScanResponseFile();
	//void ScanTradeFile();
	//void ScanPositionFile();

	/// members
	char user_id_[32];
	bool logined_;
	int request_id_;
	std::map<std::string, unsigned> client_sys_map_;
	std::map<std::string, BasketInfo*> baskets_map_;
	std::fstream basket_id_file_;
	std::fstream log_file_;

	// multi-thread basket-order timer fields
	const int MAX_BASKET_SIZE = 300;
	int interval_; // milliseconds to sleep
	char base_directory_[64];
	char order_directory_[64];
	char cancel_directory_[64];
	char response_directory_[64];
	char basket_id_file_name_[64];
	BFInputOrderField basket_orders_[2][300];
	int basket_size_[2];
	volatile int timer_end_;
	volatile int queue_index_;
	int file_id_;
	int local_order_id_;
	int date_;
};
