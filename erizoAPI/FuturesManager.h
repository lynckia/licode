#ifndef ERIZOAPI_FUTURESMANAGER_H_
#define ERIZOAPI_FUTURESMANAGER_H_

#include <logger.h>
#include <vector>
#include <boost/thread/future.hpp>  // NOLINT

/*
 * Wrapper class of erizo::ExternalInput
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class FuturesManager {
 public:
  DECLARE_LOGGER();
  FuturesManager();
  ~FuturesManager();
  void add(boost::future<void> future);
  void cleanResolvedFutures();
  int numberOfUnresolvedFutures();

 private:
  std::vector<boost::future<void>> futures;
};

#endif  // ERIZOAPI_FUTURESMANAGER_H_
