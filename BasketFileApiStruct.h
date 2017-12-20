#pragma once

#include "BasketFileApiDataType.h"

struct BFReqUserLoginField {
	char UserID[32];
	char Password[32];
	char BrokerID[32];
};

struct BFRspUserLoginField {
	char UserID[32];
	char BrokerID[32];
};

struct BFReqUserLogoutField {
	char UserID[32];
	char BrokerID[32];
};

struct BFRspUserLogoutField {
	char UserID[32];
	char BrokerID[32];
};


struct BFRspInfoField {
	int ErrorID;
	char ErrorMsg[128];
};

struct BFInputOrderField {
	char InvestorID[32];
	char UserID[32];
	char InstrumentID[16];
	char OrderRef[32];
	int ExchangeType;
	int Direction;
	int OrderType;
	int OffsetType;
	int Volume;
	double LimitPrice;
};

struct BFOrderField {
	char InvestorID[32];
	char UserID[32];
	char InstrumentID[16];
	char OrderRef[32];
	char OrderSysID[32];
	int ExchangeType;
	int Direction;
	int OrderType;
	int OffsetType;
	int Volume;
	int VolumeTraded;
	int Status;
};

struct BFInputOrderActionField {
	char InvestorID[32];
	char UserID[32];
	char InstrumentID[16];
	char OrderActionRef[32];
	char OrderRef[32];
	char OrderSysID[32];
	int ExchangeType;
};

struct BFTradeField {
	char InvestorID[32];
	char UserID[32];
	char InstrumentID[16];
	char OrderRef[32];
	char OrderSysID[32];
	char TradeID[32];
	int ExchangeType;
	int Direction;
	int OrderType;
	int OffsetType;
	int Volume;
	int Price;
};

struct BFQryTradingAccountField {
	char BrokerID[32];
	char InvestorID[32];
};

struct BFQryInvestorPositionField {
	char BrokerID[32];
	char InvestorID[32];
	char InstrumentID[16];
};

struct BFQryInvestorMarginStockField {
	char BrokerID[32];
	char InvestorID[32];
	char InstruementID[16];
};

struct BFTradingAccountField {
	char BrokerID[32];
	char AccountID[32];
	double FrozenMargin;
	double FrozenCash;
	double CurrMargin;
	double Commission;
	double CloseProfit;
	double Balance;
	double Available;
	double Credit;
};

struct BFInvestorPositionField {
	char BrokerID[32];
	char InvestorID[32];
	char InstrumentID[16];
	int PosiDirection;
	int YdPosition;
	int Position;
	int OpenVolume;
	int CloseVolume;
	int BuyVolume;
	int SellVolume;
	double OpenAmount;
	double CloseAmount;
	double BuyAmount;
	double SellAmount;
};

struct BFInvestorMarginStockField {
	char BrokerID[32];
	char InvestorID[32];
	char InstrumentID[16];
};


