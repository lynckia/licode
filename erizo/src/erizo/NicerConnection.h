/*
 * NicerConnection.h
 */

#ifndef ERIZO_SRC_ERIZO_NICERCONNECTION_H_
#define ERIZO_SRC_ERIZO_NICERCONNECTION_H_

// nICEr includes
extern "C" {
#include <r_types.h>
#include <ice_ctx.h>
}

#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <string>
#include <vector>
#include <queue>
#include <map>

#include "./MediaDefinitions.h"
#include "./SdpInfo.h"
#include "./logger.h"
#include "./IceConnection.h"

namespace erizo {

typedef struct nr_ice_ctx_ nr_ice_ctx;

/**
 * An ICE connection via nICEr
 * Represents an ICE Connection in an new thread.
 *
 */
class NicerConnection : public IceConnection {
  DECLARE_LOGGER();

 public:
  explicit NicerConnection(IceConnectionListener *listener, const IceConfig& ice_config);

  virtual ~NicerConnection();

  void start() override;
  bool setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool is_bundle) override;
  void gatheringDone(uint stream_id) override;
  void getCandidate(uint stream_id, uint component_id, const std::string &foundation) override;
  void setRemoteCredentials(const std::string& username, const std::string& password) override;
  int sendData(unsigned int compId, const void* buf, int len) override;

  void updateIceState(IceState state) override;
  IceState checkIceState() override;
  void updateComponentState(unsigned int compId, IceState state) override;
  void onData(unsigned int component_id, char* buf, int len) override;
  CandidatePair getSelectedPair() override;
  void setReceivedLastCandidate(bool hasReceived) override;
  void close() override;

  static IceConnection* create(IceConnectionListener *listener, const IceConfig& ice_config);

 private:
  nr_ice_ctx *ctx_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_NICERCONNECTION_H_
