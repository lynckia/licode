/*
 * NicerConnection.cpp
 */

#include "NicerConnection.h"

// nICEr includes
extern "C" {
#include <nr_api.h>
#include <registry.h>
#include <async_timer.h>
#include <r_crc32.h>
#include <r_memory.h>
#include <ice_reg.h>
#include <ice_util.h>
#include <transport_addr.h>
#include <nr_crypto.h>
#include <nr_socket.h>
#include <nr_socket_local.h>
#include <nr_proxy_tunnel.h>
#include <stun_client_ctx.h>
#include <stun_reg.h>
#include <stun_server_ctx.h>
#include <stun_util.h>
#include <ice_codeword.h>
#include <ice_ctx.h>
#include <ice_candidate.h>
#include <ice_handler.h>
}

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>

#include <string>
#include <vector>

namespace erizo {

DEFINE_LOGGER(NicerConnection, "NicerConnection");

static bool nicer_initialized = false;
static std::mutex nicer_initialization_mutex;

static int no_op(void **obj) {
  return 0;
}

static int nr_ice_crypto_openssl_random_bytes(UCHAR *buf, int len) {
  RAND_bytes(buf, len);
  return 0;
}

static int nr_ice_crypto_openssl_hmac_sha1(UCHAR *key, int key_l, UCHAR *buf, int buf_l, UCHAR digest[20]) {
  unsigned int rl;

  HMAC(EVP_sha1(),
    key, key_l, buf, buf_l, digest, &rl);

  if (rl != 20)
    ERETURN(R_INTERNAL);

  return 0;
}

static int nr_ice_crypto_openssl_md5(UCHAR *buf, int bufl, UCHAR *result) {
  MD5(buf, bufl, result);
  return 0;
}

static nr_ice_crypto_vtbl nr_ice_crypto_openssl_vtbl = {
  nr_ice_crypto_openssl_random_bytes,
  nr_ice_crypto_openssl_hmac_sha1,
  nr_ice_crypto_openssl_md5
};


int nr_crypto_openssl_set() {
  OpenSSL_add_all_algorithms();

  nr_crypto_vtbl = &nr_ice_crypto_openssl_vtbl;

  return 0;
}

void NicerConnection::gather_callback(NR_SOCKET s, int h, void *arg) {
  NicerConnection *conn = reinterpret_cast<NicerConnection*>(arg);
  conn->gatheringDone(h);
}

int NicerConnection::select_pair(void *obj, nr_ice_media_stream *stream,
                                 int component_id, nr_ice_cand_pair **potentials,
                                 int potential_ct) {
  return 0;
}

int NicerConnection::stream_ready(void *obj, nr_ice_media_stream *stream) {
  return 0;
}

int NicerConnection::stream_failed(void *obj, nr_ice_media_stream *stream) {
  NicerConnection *conn = reinterpret_cast<NicerConnection*>(obj);
  conn->updateIceState(IceState::FAILED);
  return 0;
}

int NicerConnection::ice_checking(void *obj, nr_ice_peer_ctx *pctx) {
  return 0;
}

int NicerConnection::ice_connected(void *obj, nr_ice_peer_ctx *pctx) {
  NicerConnection *conn = reinterpret_cast<NicerConnection*>(obj);
  if (conn->checkIceState() == IceState::FAILED) {
    return 0;
  }
  conn->updateIceState(IceState::READY);
  conn->nicer_->IceContextFinalize(conn->ctx_, pctx);

  return 0;
}

int NicerConnection::ice_disconnected(void *obj, nr_ice_peer_ctx *pctx) {
  return 0;
}

int NicerConnection::msg_recvd(void *obj, nr_ice_peer_ctx *pctx,
                               nr_ice_media_stream *stream, int component_id,
                               unsigned char *msg, int len) {
  NicerConnection *conn = reinterpret_cast<NicerConnection*>(obj);
  conn->onData(component_id, reinterpret_cast<char*> (msg), static_cast<unsigned int> (len));
  return 0;
}

void NicerConnection::trickle_callback(void *arg, nr_ice_ctx *ctx, nr_ice_media_stream *stream,
                                       int component_id, nr_ice_candidate *candidate) {
  NicerConnection *conn = reinterpret_cast<NicerConnection*>(arg);
  conn->onCandidate(stream, component_id, candidate);
}

NicerConnection::NicerConnection(std::shared_ptr<NicerInterface> interface, IceConnectionListener *listener,
                                 const IceConfig& ice_config)
    : IceConnection(listener, ice_config),
      nicer_{interface},
      ice_config_{ice_config},
      closed_{false},
      name_{ice_config_.transport_name},
      ctx_{nullptr},
      peer_{nullptr},
      stream_{nullptr},
      offerer_{!ice_config_.username.empty() && !ice_config_.password.empty()},
      trickle_{ice_config_.should_trickle} {
}

NicerConnection::~NicerConnection() {
  close();
}

void NicerConnection::start() {
  UINT4 flags = NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION;

  ufrag_ = getNewUfrag();
  upass_ = getNewPwd();
  if (ufrag_.empty() || upass_.empty()) {
    return;
  }

  int r = nicer_->IceContextCreateWithCredentials(const_cast<char *>(name_.c_str()),
                                                  flags,
                                                  const_cast<char*>(ufrag_.c_str()),
                                                  const_cast<char*>(upass_.c_str()), &ctx_);
  if (r) {
    ELOG_WARN("%s message: Couldn't create ICE ctx", toLog());
    return;
  }

  r = nicer_->IceContextSetTrickleCallback(ctx_, &NicerConnection::trickle_callback, this);
  if (r) {
    ELOG_WARN("%s message: Couldn't set trickle callback", toLog());
    return;
  }

  // Create the handler objects
  ice_handler_vtbl_ = new nr_ice_handler_vtbl();
  ice_handler_vtbl_->select_pair = &NicerConnection::select_pair;
  ice_handler_vtbl_->stream_ready = &NicerConnection::stream_ready;
  ice_handler_vtbl_->stream_failed = &NicerConnection::stream_failed;
  ice_handler_vtbl_->ice_connected = &NicerConnection::ice_connected;
  ice_handler_vtbl_->msg_recvd = &NicerConnection::msg_recvd;
  ice_handler_vtbl_->ice_checking = &NicerConnection::ice_checking;
  ice_handler_vtbl_->ice_disconnected = &NicerConnection::ice_disconnected;

  ice_handler_ = new nr_ice_handler();
  ice_handler_->vtbl = ice_handler_vtbl_;
  ice_handler_->obj = this;

  // Create the peer ctx. Because we do not support parallel forking, we
  // only have one peer ctx.
  std::string peer_name = name_ + ":default";
  r = nicer_->IcePeerContextCreate(ctx_, ice_handler_, const_cast<char *>(peer_name.c_str()), &peer_);
  if (r) {
    ELOG_WARN("%s message: Couldn't create ICE peer ctx", toLog());
    return;
  }

  peer_->controlling = 0;

  std::string stream_name = name_ + ":stream";
  r = nicer_->IceAddMediaStream(ctx_, const_cast<char *>(stream_name.c_str()), ice_config_.ice_components, &stream_);
  if (r) {
    ELOG_WARN("%s message: Couldn't create ICE stream", toLog());
    return;
  }

  if (ice_config_.username.compare("") != 0 && ice_config_.password.compare("") != 0) {
    ELOG_DEBUG("%s message: setting remote credentials in constructor, ufrag:%s, pass:%s",
               toLog(), ice_config_.username.c_str(), ice_config_.password.c_str());
    setRemoteCredentials(ice_config_.username, ice_config_.password);
  }

  if (!ice_config_.network_interface.empty()) {
    std::string interface = ice_config_.network_interface;
    std::copy(interface.begin(),
              (interface.size() >= 32 ? interface.begin() + 32 : interface.end()),
              ctx_->force_net_interface);
  }

  startGathering();
  startChecking();
}

void NicerConnection::startGathering() {
  UINT4 r = nicer_->IceGather(ctx_, &NicerConnection::gather_callback, this);
  if (r && r != R_WOULDBLOCK) {
    ELOG_WARN("%s message: Couldn't start ICE gathering", toLog());
    assert(false);
  }
  ELOG_INFO("%s message: start gathering", toLog());
}

bool NicerConnection::setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool is_bundle) {
  for (const CandidateInfo &cand : candidates) {
    std::string sdp = cand.sdp;
    std::size_t pos = sdp.find(",");
    std::string candidate = sdp.substr(0, pos);
    UINT4 r = nicer_->IcePeerContextParseTrickleCandidate(peer_, stream_, const_cast<char *>(candidate.c_str()));
    if (r) {
      ELOG_WARN("%s message: Couldn't add remote ICE candidate (%s)", toLog(), candidate.c_str());
    }
  }
  ELOG_INFO("%s message: remote candidate gathering", toLog());
  return true;
}

void NicerConnection::gatheringDone(uint stream_id) {
  ELOG_INFO("%s message: gathering done, stream_id: %u", toLog(), stream_id);
  updateIceState(IceState::CANDIDATES_RECEIVED);
}

void NicerConnection::startChecking() {
  UINT4 r = nicer_->IcePeerContextPairCandidates(peer_);
  if (r) {
    ELOG_WARN("%s message: Error pairing candidates", toLog());
    return;
  }

  r = nicer_->IcePeerContextStartChecks2(peer_, 1);
  if (r) {
    if (r == R_NOT_FOUND) {
      ELOG_DEBUG("%s message: Could not start ICE checks, assuming trickle", toLog());
    } else {
      ELOG_WARN("%s message: Could not start peer checks", toLog());
    }
  }
}

std::string NicerConnection::getStringFromAddress(const nr_transport_addr &addr) {
  char str[INET_ADDRSTRLEN];
  if (addr.ip_version == NR_IPV4) {
    inet_ntop(AF_INET, &(reinterpret_cast<sockaddr_in*>(addr.addr)->sin_addr), str, INET_ADDRSTRLEN);
  } else {
    inet_ntop(AF_INET, &(reinterpret_cast<sockaddr_in6*>(addr.addr)->sin6_addr), str, INET_ADDRSTRLEN);
  }
  return std::string(str);
}

int NicerConnection::getPortFromAddress(const nr_transport_addr &addr) {
  if (addr.ip_version == NR_IPV4) {
    return ntohs(reinterpret_cast<sockaddr_in*>(addr.addr)->sin_port);
  } else {
    return ntohs(reinterpret_cast<sockaddr_in6*>(addr.addr)->sin6_port);
  }
}

void NicerConnection::onCandidate(nr_ice_media_stream *stream, int component_id, nr_ice_candidate *cand) {
  CandidateInfo cand_info;
  cand_info.componentId = cand->component_id;
  cand_info.foundation = std::string(cand->foundation);
  cand_info.priority = cand->priority;

  cand_info.hostAddress = getStringFromAddress(cand->addr);
  cand_info.hostPort = getPortFromAddress(cand->addr);
  if (cand_info.hostPort == 0) {
    return;
  }
  cand_info.mediaType = ice_config_.media_type;

  switch (cand->type) {
    case nr_ice_candidate_type::HOST:
      cand_info.hostType = HOST;
      break;
    case SERVER_REFLEXIVE:
      cand_info.hostType = SRFLX;
      cand_info.rAddress = getStringFromAddress(cand->base);
      cand_info.rPort = getPortFromAddress(cand->base);
      break;
    case PEER_REFLEXIVE:
      cand_info.hostType = PRFLX;
      break;
    case RELAYED:
      cand_info.hostType = RELAY;
      cand_info.rAddress = getStringFromAddress(cand->base);
      cand_info.rPort = getPortFromAddress(cand->base);
      break;
    default:
      break;
  }
  cand_info.netProtocol = "udp";
  cand_info.transProtocol = ice_config_.transport_name;
  cand_info.username = ufrag_;
  cand_info.password = upass_;

  if (auto listener = getIceListener()) {
    ELOG_DEBUG("%s message: Candidate (%s, %s, %s)", toLog(), cand_info.hostAddress.c_str(),
                                                     ufrag_.c_str(), upass_.c_str());
    listener->onCandidate(cand_info, this);
  }
}

void NicerConnection::setRemoteCredentials(const std::string& username, const std::string& password) {
  ELOG_DEBUG("%s message: Setting remote credentials", toLog());
  std::vector<char *> attributes;
  std::string ufrag = std::string("ice-ufrag: ") + username;
  std::string pwd = std::string("ice-pwd: ") + password;
  attributes.push_back(const_cast<char *>(ufrag.c_str()));
  attributes.push_back(const_cast<char *>(pwd.c_str()));
  UINT4 r = nicer_->IcePeerContextParseStreamAttributes(peer_,
                                                        stream_,
                                                        attributes.size() ? &attributes[0] : nullptr,
                                                        attributes.size());
  if (r) {
    ELOG_WARN("%s message: Error parsing stream attributes", toLog());
  }
}

int NicerConnection::sendData(unsigned int compId, const void* buf, int len) {
  UINT4 r = nicer_->IceMediaStreamSend(peer_,
                                       stream_,
                                       compId,
                                       reinterpret_cast<unsigned char*>(const_cast<void*>(buf)),
                                       len);
  if (r) {
    ELOG_WARN("%s message: Couldn't send data on ICE", toLog());
  }
  return len;
}

void NicerConnection::updateComponentState(unsigned int compId, IceState state) {
}

CandidatePair NicerConnection::getSelectedPair() {
  return CandidatePair{};
}

void NicerConnection::setReceivedLastCandidate(bool hasReceived) {
}

void NicerConnection::close() {
  if (!closed_) {
    closed_ = true;
    if (stream_) {
      int r = nicer_->IceRemoveMediaStream(ctx_, &stream_);
      if (r) {
        ELOG_WARN("%s message: Couldn't remove media stream", toLog());
      }
    }
    if (peer_) {
      nicer_->IcePeerContextDestroy(&peer_);
    }
    if (ctx_) {
      nicer_->IceContextDestroy(&ctx_);
    }
    delete ice_handler_vtbl_;
    delete ice_handler_;
  }
}

void NicerConnection::onData(unsigned int component_id, char* buf, int len) {
  IceState state;
  IceConnectionListener* listener;
  {
    boost::mutex::scoped_lock(closeMutex_);
    state = this->checkIceState();
    listener = listener_;
  }
  if (state == IceState::READY) {
    packetPtr packet (new dataPacket());
    memcpy(packet->data, buf, len);
    packet->comp = component_id;
    packet->length = len;
    packet->received_time_ms = ClockUtils::timePointToMs(clock::now());
    listener->onPacketReceived(packet);
  }
}

void NicerConnection::initializeGlobals() {
  std::lock_guard<std::mutex> guard(nicer_initialization_mutex);
  if (!nicer_initialized) {
    nicer_initialized = true;
    NR_reg_init(NR_REG_MODE_LOCAL);

    nr_crypto_openssl_set();

    // Set the priorites for candidate type preferences.
    // These numbers come from RFC 5245 S. 4.1.2.2
    NR_reg_set_uchar(const_cast<char *>(NR_ICE_REG_PREF_TYPE_SRV_RFLX), 100);
    NR_reg_set_uchar(const_cast<char *>(NR_ICE_REG_PREF_TYPE_PEER_RFLX), 110);
    NR_reg_set_uchar(const_cast<char *>(NR_ICE_REG_PREF_TYPE_HOST), 126);
    NR_reg_set_uchar(const_cast<char *>(NR_ICE_REG_PREF_TYPE_RELAYED), 5);
    NR_reg_set_uchar(const_cast<char *>(NR_ICE_REG_PREF_TYPE_RELAYED_TCP), 0);


    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.rl0"), 255);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.wi0"), 254);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.lo0"), 253);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.en1"), 252);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.en0"), 251);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.eth0"), 252);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.eth1"), 251);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.eth2"), 249);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.ppp"), 250);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.ppp0"), 249);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.en2"), 248);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.en3"), 247);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.em0"), 251);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.em1"), 252);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.vmnet0"), 240);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.vmnet1"), 241);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.vmnet3"), 239);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.vmnet4"), 238);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.vmnet5"), 237);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.vmnet6"), 236);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.vmnet7"), 235);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.vmnet8"), 234);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.virbr0"), 233);
    NR_reg_set_uchar(const_cast<char *>("ice.pref.interface.wlan0"), 232);

    NR_reg_set_uint4(const_cast<char *>("stun.client.maximum_transmits"), 7);
    NR_reg_set_uint4(const_cast<char *>(NR_ICE_REG_TRICKLE_GRACE_PERIOD), 5000);

    NR_reg_set_char(const_cast<char *>(NR_ICE_REG_ICE_TCP_DISABLE), true);
    NR_reg_set_char(const_cast<char *>(NR_STUN_REG_PREF_ALLOW_LINK_LOCAL_ADDRS), 1);
  }
}

std::string NicerConnection::getNewUfrag() {
  char* ufrag;
  int r;

  if ((r=nicer_->IceGetNewIceUFrag(&ufrag))) {
    ELOG_WARN("%s message: Unable to get new ice ufrag", toLog());
    return "";
  }

  std::string ufragStr = ufrag;
  RFREE(ufrag);

  return ufragStr;
}

std::string NicerConnection::getNewPwd() {
  char* pwd;
  int r;

  if ((r=nicer_->IceGetNewIcePwd(&pwd))) {
    ELOG_WARN("%s message: Unable to get new ice pwd", toLog());
    return "";
  }

  std::string pwdStr = pwd;
  RFREE(pwd);

  return pwdStr;
}

IceConnection* NicerConnection::create(IceConnectionListener *listener, const IceConfig& ice_config,
                                       std::shared_ptr<Worker> worker) {
  auto nicer = new NicerConnection(std::make_shared<NicerInterfaceImpl>(), listener, ice_config, worker);

  NicerConnection::initializeGlobals();

  return nicer;
}

} /* namespace erizo */
