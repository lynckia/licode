/*
 * NicerConnection.h
 */

#ifndef ERIZO_SRC_ERIZO_NICERCONNECTION_H_
#define ERIZO_SRC_ERIZO_NICERCONNECTION_H_

// Need at least one std header before nicer to explicitly define std namespace
#include <string>

// nICEr includes
extern "C" {
#include <r_types.h>
#include <ice_ctx.h>
}

#include <boost/thread.hpp>
#include <vector>
#include <queue>
#include <map>
#include <mutex>  // NOLINT
#include <future>  // NOLINT

#include "./MediaDefinitions.h"
#include "./SdpInfo.h"
#include "./logger.h"
#include "./IceConnection.h"
#include "lib/NicerInterface.h"
#include "./thread/Worker.h"
#include "./thread/IOWorker.h"

namespace erizo {

typedef struct nr_ice_ctx_ nr_ice_ctx;

/**
 * An ICE connection via nICEr
 * Represents an ICE Connection in an new thread.
 *
 */
class NicerConnection : public IceConnection, public std::enable_shared_from_this<NicerConnection> {
  DECLARE_LOGGER();

 public:
  explicit NicerConnection(std::shared_ptr<IOWorker> io_worker, std::shared_ptr<NicerInterface> interface,
                           const IceConfig& ice_config);

  virtual ~NicerConnection();

  void start() override;
  bool setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool is_bundle) override;
  void gatheringDone(uint stream_id);
  void onCandidate(nr_ice_media_stream *stream, int component_id, nr_ice_candidate *candidate);
  void setRemoteCredentials(const std::string& username, const std::string& password) override;
  int sendData(unsigned int component_id, const void* buf, int len) override;

  void onData(unsigned int component_id, char* buf, int len) override;
  CandidatePair getSelectedPair() override;
  void setReceivedLastCandidate(bool hasReceived) override;
  void close() override;
  bool isClosed() { return closed_; }

  static std::shared_ptr<IceConnection> create(std::shared_ptr<IOWorker> io_worker, const IceConfig& ice_config);

  static void initializeGlobals();

 private:
  std::string getNewUfrag();
  std::string getNewPwd();
  std::string getStringFromAddress(const nr_transport_addr &addr);
  int getPortFromAddress(const nr_transport_addr &addr);
  void setupTurnServer();
  void setupStunServer();
  void startGathering();
  void startChecking();
  void startSync();
  void closeSync();
  void async(function<void(std::shared_ptr<NicerConnection>)> f);
  void setRemoteCredentialsSync(const std::string& username, const std::string& password);

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
  std::shared_ptr<IOWorker> io_worker_;
  std::shared_ptr<NicerInterface> nicer_;
  IceConfig ice_config_;
  std::atomic<bool> closed_;
  const std::string name_;
  nr_ice_ctx *ctx_;
  nr_ice_peer_ctx *peer_;
  nr_ice_media_stream *stream_;
  bool offerer_;
  nr_ice_handler_vtbl* ice_handler_vtbl_;
  nr_ice_handler* ice_handler_;
  std::promise<void> close_promise_;
  std::promise<void> start_promise_;
  std::future<void>  start_future_;
  boost::mutex close_mutex_;
  boost::mutex close_sync_mutex_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_NICERCONNECTION_H_
