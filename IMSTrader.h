#ifndef _IMS_TRADER_H
#define _IMS_TRADER_H



#include <map>
#include <string>
#include <fstream>
#include <boost/thread/thread.hpp>

#include "BasketFileTraderApi.h"

class IMSTrader : public BasketFileTraderApi
{
public:
	IMSTrader(const char *user_id);
	virtual ~IMSTrader();

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
	struct request_ptr {
	public:
		static const int TYPE_INVALID = 0;
		static const int TYPE_ORDER_TICKET = 1;
		static const int TYPE_CANCEL_TICKET = 2;
		static const int TYPE_TRADE_ACCOUNT = 3;
		static const int TYPE_QUERY_BALANCE = 4;
		static const int TYPE_QUERY_POSITION = 5;
		static const int TYPE_QUERY_MARGINSTOCK = 6;

		int request_type;
		void *ptr;

		request_ptr() : request_type(TYPE_INVALID), ptr(NULL) {}
		request_ptr(int type, void *p) : request_type(type), ptr(p) {}
	};

	void WaitToOrder();
	void ScanResponseFile();
	void ScanTradeFile();
	void ScanFundFile();
	void ScanPositionFile();
	void SendBasketOrders(int idx);

	/// members
	char user_id_[32];
	bool logined_;
	int request_id_;
	request_ptr* requests_[50000];
	std::map<std::string, unsigned> client_sys_map_;
	std::fstream log_file_;

	// multi-thread basket-order timer fields
	const int MAX_BASKET_SIZE = 300;
	int interval_; // milliseconds to sleep
	char base_directory_[64];
	char order_directory_[64];
	char cancel_directory_[64];
	char response_directory_[64];
	BFInputOrderField basket_orders_[2][300];
	int basket_size_[2];
	volatile int timer_end_;
	volatile int queue_index_;
	int file_id_;
};

#endif
