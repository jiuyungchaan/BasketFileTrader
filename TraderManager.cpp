#include "TraderManager.h"

using namespace std;

TraderManager *p_instance_ = NULL;

TraderManager::TraderManager() : trader_(NULL), trader_user_id_{0} {}

TraderManager::~TraderManager() {}

TraderManager *TraderManager::GetInstance() {
	if (p_instance_ == NULL) {
		p_instance_ = new TraderManager();
	}
	return p_instance_;
}

BasketFileTraderApi *TraderManager::RegisterTrader(const string &account_id, const string &client_type) {
	boost::unique_lock<boost::mutex> trader_lock(mutex_);
	if (trader_ != NULL) {
		if (strcmp(trader_user_id_, account_id.c_str()) == 0) {
			return trader_;
		}
		else {
			/// if UserID not match, initialize a dummy trader which does not login
			if (client_type == "IMS") {
				IMSTrader *dummy_trader = new IMSTrader(account_id.c_str());
				return dummy_trader;
			}
			else if (client_type == "WINNER") {
				return trader_;
			}
			else if (client_type == "TSI") {
				return trader_;
			}
			else {
				IMSTrader *dummy_trader = new IMSTrader(account_id.c_str());
				return dummy_trader;
			} // default for IMS
		}
	}
	else {
		snprintf(trader_user_id_, sizeof(trader_user_id_), "%s", account_id.c_str());
		if (client_type == "IMS") {
			trader_ = new IMSTrader(account_id.c_str());
			trader_->Init();
			return trader_;
		}
		else if (client_type == "WINNER") {
			trader_ = new WinnerTrader(account_id.c_str());
			trader_->Init();
			return trader_;
		}
		else if (client_type == "TSI") {
			trader_ = new TSITrader(account_id.c_str());
			trader_->Init();
			return trader_;
		}
		else {
			trader_ = new IMSTrader(account_id.c_str());
			trader_->Init();
			return trader_;
		} // default for IMS
	}
}

BasketFileTraderApi *TraderManager::GetTrader(const string &account_id) {
	boost::unique_lock<boost::mutex> trader_lock(mutex_);
	if (trader_ != NULL) {
		if (strcmp(trader_user_id_, account_id.c_str()) == 0) {
			return trader_;
		}
		else {
			/// if UserID not match, initialize a dummy trader which does not 
			IMSTrader *dummy_trader = new IMSTrader(account_id.c_str());
			return dummy_trader;
		}
	}
	else {
		return RegisterTrader(account_id, "IMS");
	}
}