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
  return 0;
}

int NicerConnection::ice_checking(void *obj, nr_ice_peer_ctx *pctx) {
  return 0;
}

int NicerConnection::ice_connected(void *obj, nr_ice_peer_ctx *pctx) {
  return 0;
}

int NicerConnection::ice_disconnected(void *obj, nr_ice_peer_ctx *pctx) {
  return 0;
}

int NicerConnection::msg_recvd(void *obj, nr_ice_peer_ctx *pctx,
                               nr_ice_media_stream *stream, int component_id,
                               unsigned char *msg, int len) {
  return 0;
}

void NicerConnection::trickle_callback(void *arg, nr_ice_ctx *ctx, nr_ice_media_stream *stream,
                                       int component_id, nr_ice_candidate *candidate) {
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
      offerer_{!ice_config_.username.empty() && !ice_config_.password.empty()},
      trickle_{ice_config_.should_trickle},
      ufrag_{""},
      pwd_{""} {
}

NicerConnection::~NicerConnection() {
  close();
}

void NicerConnection::start() {
  UINT4 flags = NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION;

  ufrag_ = getNewUfrag();
  pwd_ = getNewPwd();
  if (ufrag_.empty() || pwd_.empty()) {
    return;
  }

  int r = nicer_->IceContextCreateWithCredentials(const_cast<char *>(name_.c_str()), flags,
                                  const_cast<char*>(ufrag_.c_str()), const_cast<char*>(pwd_.c_str()), &ctx_);
  if (r) {
    ELOG_ERROR("Couldn't create ICE ctx for %s", name_.c_str());
    return;
  }

  if (trickle_) {
    r = nicer_->IceContextSetTrickleCallback(ctx_, &NicerConnection::trickle_callback, this);
    if (r) {
      ELOG_ERROR("Couldn't set trickle callback for %s", name_.c_str());
      return;
    }
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
    ELOG_ERROR("Couldn't create ICE peer ctx for %s", name_.c_str());
    return;
  }
}

bool NicerConnection::setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool is_bundle) {
  return true;
}

void NicerConnection::gatheringDone(uint stream_id) {
}

void NicerConnection::getCandidate(uint stream_id, uint component_id, const std::string &foundation) {
}

void NicerConnection::setRemoteCredentials(const std::string& username, const std::string& password) {
}

int NicerConnection::sendData(unsigned int compId, const void* buf, int len) {
  return 0;
}

void NicerConnection::updateIceState(IceState state) {
}

IceState NicerConnection::checkIceState() {
  return IceState::READY;
}

void NicerConnection::updateComponentState(unsigned int compId, IceState state) {
}

void NicerConnection::onData(unsigned int component_id, char* buf, int len) {
}

CandidatePair NicerConnection::getSelectedPair() {
  return CandidatePair{};
}

void NicerConnection::setReceivedLastCandidate(bool hasReceived) {
}

void NicerConnection::close() {
  if (!closed_) {
    closed_ = true;
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

  if ((r=nr_ice_get_new_ice_ufrag(&ufrag))) {
    ELOG_ERROR("Unable to get new ice ufrag");
    return "";
  }

  std::string ufragStr = ufrag;
  RFREE(ufrag);

  return ufragStr;
}

std::string NicerConnection::getNewPwd() {
  char* pwd;
  int r;

  if ((r=nr_ice_get_new_ice_pwd(&pwd))) {
    ELOG_ERROR("Unable to get new ice pwd");
    return "";
  }

  std::string pwdStr = pwd;
  RFREE(pwd);

  return pwdStr;
}

IceConnection* NicerConnection::create(IceConnectionListener *listener, const IceConfig& ice_config) {
  auto nicer = new NicerConnection(std::make_shared<NicerInterfaceImpl>(), listener, ice_config);

  NicerConnection::initializeGlobals();

  return nicer;
}

} /* namespace erizo */
