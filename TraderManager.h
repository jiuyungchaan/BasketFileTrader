#ifndef _TRADER_MANAGER_H
#define _TRADER_MANAGER_H

#include "IMSTrader.h"
#include "WinnerTrader.h"
#include <map>
#include <string>
#include <boost/thread.hpp>

class TraderManager {
public:
	static TraderManager *GetInstance();
	virtual ~TraderManager();

	BasketFileTraderApi *RegisterTrader(const std::string &account_id, const std::string &client_type);
	BasketFileTraderApi *GetTrader(const std::string &account_id);

private:
	TraderManager();

	/// multi-trader would not work
	std::map<std::string, BasketFileTraderApi*> trader_map_;
	BasketFileTraderApi *trader_;
	char trader_user_id_[32];
	boost::mutex mutex_;
};

#endif