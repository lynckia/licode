/*
 * LibNiceConnection.h
 */

#ifndef ERIZO_SRC_ERIZO_LIBNICECONNECTION_H_
#define ERIZO_SRC_ERIZO_LIBNICECONNECTION_H_

#include <nice/nice.h>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <string>
#include <vector>
#include <queue>
#include <map>

#include "./IceConnection.h"
#include "./MediaDefinitions.h"
#include "./SdpInfo.h"
#include "./logger.h"
#include "lib/LibNiceInterface.h"
#include "./thread/Worker.h"
#include "./thread/IOWorker.h"

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainContext GMainContext;
typedef struct _GMainLoop GMainLoop;

typedef unsigned int uint;

namespace erizo {

#define NICE_STREAM_MAX_UFRAG   256 + 1  /* ufrag + NULL */
#define NICE_STREAM_MAX_UNAME   256 * 2 + 1 + 1 /* 2*ufrag + colon + NULL */
#define NICE_STREAM_MAX_PWD     256 + 1  /* pwd + NULL */
#define NICE_STREAM_DEF_UFRAG   4 + 1    /* ufrag + NULL */
#define NICE_STREAM_DEF_PWD     22 + 1   /* pwd + NULL */

// forward declarations
typedef std::shared_ptr<DataPacket> packetPtr;
class CandidateInfo;
class WebRtcConnection;

class LibNiceConnection : public IceConnection, public std::enable_shared_from_this<LibNiceConnection>  {
  DECLARE_LOGGER();

 public:
  LibNiceConnection(std::shared_ptr<LibNiceInterface> libnice, std::shared_ptr<IOWorker> io_worker,
  const IceConfig& ice_config);

  virtual ~LibNiceConnection();
  /**
   * Starts Gathering candidates in a new thread.
   */
  void start() override;
  bool setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool is_bundle) override;
  bool setRemoteCandidatesSync(const std::vector<CandidateInfo> &cands, bool is_bundle,
    std::shared_ptr<std::promise<void>> remote_candidates_promise);
  void gatheringDone(uint stream_id);
  void getCandidate(uint stream_id, uint component_id, const std::string &foundation);
  void setRemoteCredentials(const std::string& username, const std::string& password) override;
  int sendData(unsigned int component_id, const void* buf, int len) override;

  void updateComponentState(unsigned int component_id, IceState state);
  void onData(unsigned int component_id, char* buf, int len) override;
  CandidatePair getSelectedPair() override;
  void maybeRestartIce(std::string remote_ufrag, std::string remote_pass) override;
  void restartIceSync(std::string remote_ufrag, std::string remote_pass);
  void close() override;

  static LibNiceConnection* create(std::shared_ptr<IOWorker> io_worker, const IceConfig& ice_config);

 private:
  void mainLoop();
  void async(std::function<void(std::shared_ptr<LibNiceConnection>)> f);
  void startSync();
  void closeSync();
  void setRemoteCredentialsSync(const std::string& username, const std::string& password);
  void forEachComponent(std::function<void(uint comp_id)> func);

// static callbacks for libnice
  static void receive_message(NiceAgent* agent, guint stream_id, guint component_id,
    guint len, gchar* buf, gpointer user_data);

  static void new_local_candidate(NiceAgent *agent, guint stream_id, guint component_id, gchar *foundation,
    gpointer user_data);

  static void candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer user_data);

  static void component_state_change(NiceAgent *agent, guint stream_id,
    guint component_id, guint state, gpointer user_data);

  static void new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
    NiceCandidate *lcandidate, NiceCandidate *rcandidate, gpointer user_data);

  static std::string getHostTypeFromCandidate(NiceCandidate *candidate);

 private:
  std::shared_ptr<LibNiceInterface> lib_nice_;
  std::shared_ptr<IOWorker> io_worker_;
  NiceAgent* agent_;
  GMainContext* context_;
  GMainLoop* loop_;

  unsigned int cands_delivered_;

  boost::thread main_loop_thread_;
  boost::mutex close_mutex_;
  boost::condition_variable cond_;
  std::string remote_ufrag_;
  std::string remote_upass_;

  boost::shared_ptr<std::vector<CandidateInfo> > local_candidates;
  bool enable_ice_lite_;
  int stream_id_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_LIBNICECONNECTION_H_
