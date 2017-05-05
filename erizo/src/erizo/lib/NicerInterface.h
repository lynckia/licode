/*
 * NicerInterface.h
 */

#ifndef ERIZO_SRC_ERIZO_LIB_NICERINTERFACE_H_
#define ERIZO_SRC_ERIZO_LIB_NICERINTERFACE_H_

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

namespace erizo {

class NicerInterface {
 public:
  virtual int IceContextCreate(char *label, UINT4 flags, nr_ice_ctx **ctxp) = 0;
  virtual int IceContextCreateWithCredentials(char *label, UINT4 flags, char* ufrag, char* pwd, nr_ice_ctx **ctxp) = 0;
  virtual int IceContextDestroy(nr_ice_ctx **ctxp) = 0;
  virtual int IceContextSetTrickleCallback(nr_ice_ctx *ctx, nr_ice_trickle_candidate_cb cb, void *cb_arg) = 0;
  virtual int IcePeerContextCreate(nr_ice_ctx *ctx, nr_ice_handler *handler, char *label, nr_ice_peer_ctx **pctxp) = 0;
  virtual int IcePeerContextDestroy(nr_ice_peer_ctx **pctxp) = 0;
};


class NicerInterfaceImpl: public NicerInterface {
 public:
  int IceContextCreate(char *label, UINT4 flags, nr_ice_ctx **ctxp) override;
  int IceContextCreateWithCredentials(char *label, UINT4 flags, char* ufrag, char* pwd, nr_ice_ctx **ctxp) override;
  int IceContextDestroy(nr_ice_ctx **ctxp) override;
  int IceContextSetTrickleCallback(nr_ice_ctx *ctx, nr_ice_trickle_candidate_cb cb, void *cb_arg) override;
  int IcePeerContextCreate(nr_ice_ctx *ctx, nr_ice_handler *handler, char *label, nr_ice_peer_ctx **pctxp) override;
  int IcePeerContextDestroy(nr_ice_peer_ctx **pctxp) override;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_LIB_NICERINTERFACE_H_
