#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include "FuturesManager.h"

DEFINE_LOGGER(FuturesManager, "ErizoAPI.FuturesManager");

FuturesManager::FuturesManager() {
}

FuturesManager::~FuturesManager() {
}

void FuturesManager::add(boost::future<void> future) {
  futures.push_back(std::move(future));
}

void FuturesManager::cleanResolvedFutures() {
  futures.erase(std::remove_if(futures.begin(), futures.end(),
    [](const boost::future<void>& future) {
        return future.is_ready();
    }), futures.end());
  ELOG_DEBUG("message: futures after removing resolved, size: %d", numberOfUnresolvedFutures());
}

int FuturesManager::numberOfUnresolvedFutures() {
  return futures.size();
}
