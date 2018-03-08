#ifndef _TSI_CREDIT_TRADER_H
#define _TSI_CREDIT_TRADER_H

#include <set>
#include <map>
#include <string>
#include <fstream>
#include <boost/thread/thread.hpp>

#include "BasketFileTraderApi.h"

class TSICreditTrader : public BasketFileTraderApi
{
public:
	TSICreditTrader(const char *user_id);
	virtual ~TSICreditTrader();

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
	struct order {
	public:
		char local_order_id[64];
		char instrument_id[32];
		int exch;
		int direction;
		int volume;
		int fill_vol;
		int status; // 0:inserted, 1:partially traded 2:all traded 3:canceled
		int init;
		double limit_price;

		order() : local_order_id{ 0 }, instrument_id{ 0 }, fill_vol(0), status(0), init(0) {}
	};

	struct margin_stock {
	public:
		int td_volume;
		int yd_volume;

		margin_stock() : td_volume(0), yd_volume(0) {}
	};

	void LoadConfig();
	void WaitToOrder();
	void TopMostTSI();
	void FreshKeyboardInput();
	void ScanResultOrderFile();
	void ScanResultCancelFile();
	void ScanResponseFile();
	void ScanTradeFile();
	void ScanFundFile();
	void ScanPositionFile();
	void ScanCompactFile();
	void SendBasketOrders(int idx);

	/// members
	char user_id_[32];
	bool logined_;
	int request_id_;
	int local_entrust_no_, entrust_cancel_no_;
	std::map<int, std::string> local_entrust2order_; // map match local_entrust_no for local_order_id
	std::map<int, std::string> entrust2order_; // map match entrust no for local_order_id
	std::set<int> entrust_nos_; // entrust_no set
	order *orders_;
	BFInputOrderField **local_orders_;

	std::fstream log_file_;

	// multi-thread basket-order timer fields
	const int MAX_BASKET_SIZE = 300;
	int date_;
	int interval_; // milliseconds to sleep
	char base_directory_[64];
	char order_directory_[64];
	char cancel_directory_[64];
	char result_directory_[64];
	char response_directory_[64];
	char config_file_name_[64];
	BFInputOrderField basket_orders_[2][300];
	int basket_size_[2];
	volatile int timer_end_;
	volatile int queue_index_;
	int file_id_;
};


#endif


