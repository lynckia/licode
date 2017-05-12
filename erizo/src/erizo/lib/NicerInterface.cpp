/*
 * NicerInterfaceImpl.cpp
 */

#include "./NicerInterface.h"

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

int NicerInterfaceImpl::IceContextCreate(char *label, UINT4 flags, nr_ice_ctx **ctxp) {
  return nr_ice_ctx_create(label, flags, ctxp);
}

int NicerInterfaceImpl::IceContextCreateWithCredentials(char *label, UINT4 flags, char* ufrag, char* pwd,
                                                        nr_ice_ctx **ctxp) {
  return nr_ice_ctx_create_with_credentials(label, flags, ufrag, pwd, ctxp);
}

int NicerInterfaceImpl::IceContextDestroy(nr_ice_ctx **ctxp) {
  return nr_ice_ctx_destroy(ctxp);
}

int NicerInterfaceImpl::IceContextSetTrickleCallback(nr_ice_ctx *ctx, nr_ice_trickle_candidate_cb cb, void *cb_arg) {
  return nr_ice_ctx_set_trickle_cb(ctx, cb, cb_arg);
}

void NicerInterfaceImpl::IceContextSetSocketFactory(nr_ice_ctx *ctx, nr_socket_factory *factory) {
  nr_ice_ctx_set_socket_factory(ctx, factory);
}

int NicerInterfaceImpl::IcePeerContextCreate(nr_ice_ctx *ctx, nr_ice_handler *handler, char *label,
                                             nr_ice_peer_ctx **pctxp) {
  return nr_ice_peer_ctx_create(ctx, handler, label, pctxp);
}

int NicerInterfaceImpl::IcePeerContextDestroy(nr_ice_peer_ctx **pctxp) {
  return nr_ice_peer_ctx_destroy(pctxp);
}

int NicerInterfaceImpl::IceGather(nr_ice_ctx *ctx, NR_async_cb done_cb, void *cb_arg) {
  return nr_ice_gather(ctx, done_cb, cb_arg);
}

int NicerInterfaceImpl::IceAddMediaStream(nr_ice_ctx *ctx, char *label, int components, nr_ice_media_stream **streamp) {
  return nr_ice_add_media_stream(ctx, label, components, streamp);
}
}  // namespace erizo
