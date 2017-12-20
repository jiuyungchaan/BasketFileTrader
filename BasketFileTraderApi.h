#pragma once

#include "BasketFileApiStruct.h"


class BasketFileTraderApi {
public:
	BasketFileTraderApi() {};
	
	virtual void Init() = 0;
	virtual int ReqUserLogin(BFReqUserLoginField *pLoginField, int nRequestID) = 0;
	virtual int ReqUserLogout(BFReqUserLogoutField *pLogoutField, int nRequestID) = 0;
	virtual int ReqOrderInsert(BFInputOrderField *pInputOrder, int nRequestID) = 0;
	virtual int ReqOrderAction(BFInputOrderActionField *pInputOrderAction, int nRequestID) = 0;
	virtual int ReqQryTradingAccount(BFQryTradingAccountField *pTradingAccount, int nRequestID) = 0;
	virtual int ReqQryInvestorPosition(BFQryInvestorPositionField *pInvestorPosition, int nRequestID) = 0;
	virtual int ReqQryInvestorMarginStock(BFQryInvestorMarginStockField *pMarginStock, int nRequestID) = 0;

	virtual void OnRspOrderInsert(BFInputOrderField *pInputOrder, BFRspInfoField *pRspInfo) = 0;
	virtual void OnRspOrderAction(BFInputOrderActionField *pInputOrderAction, BFRspInfoField *pRspInfo) = 0;
	virtual void OnRtnOrder(BFOrderField *pOrder, BFRspInfoField *pRspInfo) = 0;
	virtual void OnRtnTrade(BFTradeField *pTrade, BFRspInfoField *pRspInfo) = 0;
	virtual void OnRspQryTradingAccount(BFTradingAccountField *pTradingAccount, BFRspInfoField *pRspInfo) = 0;
	virtual void OnRspQryInvestorPosition(BFInvestorPositionField *pInvestorPosition, BFRspInfoField *pRspInfo) = 0;
	virtual void OnRspQryInvestorMarginStock(BFInvestorMarginStockField *pMarginStock, BFRspInfoField *pRspInfo) = 0;
	virtual void OnRspError(BFRspInfoField *pRspInfo) = 0;

	virtual ~BasketFileTraderApi() {};
};