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

#include <boost/thread.hpp>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <mutex>  // NOLINT

#include "./MediaDefinitions.h"
#include "./SdpInfo.h"
#include "./logger.h"
#include "./IceConnection.h"
#include "lib/NicerInterface.h"


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
  explicit NicerConnection(std::shared_ptr<NicerInterface> interface, IceConnectionListener *listener,
                           const IceConfig& ice_config);

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
  std::string getNewUfrag();
  std::string getNewPwd();

  static void initializeGlobals();

  static void gather_callback(NR_SOCKET s, int h, void *arg);  // ICE gather complete
  static int select_pair(void *obj, nr_ice_media_stream *stream,
                         int component_id, nr_ice_cand_pair **potentials,
                         int potential_ct);
  static int stream_ready(void *obj, nr_ice_media_stream *stream);
  static int stream_failed(void *obj, nr_ice_media_stream *stream);
  static int ice_checking(void *obj, nr_ice_peer_ctx *pctx);
  static int ice_connected(void *obj, nr_ice_peer_ctx *pctx);
  static int ice_disconnected(void *obj, nr_ice_peer_ctx *pctx);
  static int msg_recvd(void *obj, nr_ice_peer_ctx *pctx,
                       nr_ice_media_stream *stream, int component_id,
                       unsigned char *msg, int len);
  static void trickle_callback(void *arg, nr_ice_ctx *ctx, nr_ice_media_stream *stream,
                               int component_id, nr_ice_candidate *candidate);

 private:
  std::shared_ptr<NicerInterface> nicer_;
  IceConfig ice_config_;
  bool closed_;
  const std::string name_;
  nr_ice_ctx *ctx_;
  nr_ice_peer_ctx *peer_;
  bool offerer_;
  nr_ice_handler_vtbl* ice_handler_vtbl_;
  nr_ice_handler* ice_handler_;
  bool trickle_;
  std::string ufrag_;
  std::string pwd_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_NICERCONNECTION_H_
