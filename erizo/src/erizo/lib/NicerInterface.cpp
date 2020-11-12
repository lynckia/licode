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

int NicerInterfaceImpl::IceContextDestroy(nr_ice_ctx **ctxp) {
  return nr_ice_ctx_destroy(ctxp);
}

int NicerInterfaceImpl::IceContextSetTrickleCallback(nr_ice_ctx *ctx, nr_ice_trickle_candidate_cb cb, void *cb_arg) {
  return nr_ice_ctx_set_trickle_cb(ctx, cb, cb_arg);
}

void NicerInterfaceImpl::IceContextSetSocketFactory(nr_ice_ctx *ctx, nr_socket_factory *factory) {
  nr_ice_ctx_set_socket_factory(ctx, factory);
}

void NicerInterfaceImpl::IceContextFinalize(nr_ice_ctx *ctx, nr_ice_peer_ctx *pctxp) {
  nr_ice_ctx_finalize(ctx, pctxp);
}

int NicerInterfaceImpl::IceContextSetStunServers(nr_ice_ctx *ctx, nr_ice_stun_server *servers, int ct) {
  return nr_ice_ctx_set_stun_servers(ctx, servers, ct);
}

int NicerInterfaceImpl::IceContextSetTurnServers(nr_ice_ctx *ctx, nr_ice_turn_server *servers, int ct) {
  return nr_ice_ctx_set_turn_servers(ctx, servers, ct);
}

void NicerInterfaceImpl::IceContextSetPortRange(nr_ice_ctx *ctx, uint16_t min_port, uint16_t max_port) {
  nr_ice_ctx_set_port_range(ctx, min_port, max_port);
}

int NicerInterfaceImpl::IcePeerContextPairCandidates(nr_ice_peer_ctx *pctxp) {
  return nr_ice_peer_ctx_pair_candidates(pctxp);
}

int NicerInterfaceImpl::IcePeerContextStartChecks2(nr_ice_peer_ctx *pctxp, int type) {
  return nr_ice_peer_ctx_start_checks2(pctxp, type);
}

int NicerInterfaceImpl::IcePeerContextParseStreamAttributes(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *stream,
                                                            char **attributes, size_t length) {
  return nr_ice_peer_ctx_parse_stream_attributes(pctxp, stream, attributes, length);
}

int NicerInterfaceImpl::IceGetNewIceUFrag(char **ufrag) {
  return nr_ice_get_new_ice_ufrag(ufrag);
}

int NicerInterfaceImpl::IceGetNewIcePwd(char **pwd) {
  return nr_ice_get_new_ice_pwd(pwd);
}

int NicerInterfaceImpl::IcePeerContextCreate(nr_ice_ctx *ctx, nr_ice_handler *handler, char *label,
                                             nr_ice_peer_ctx **pctxp) {
  return nr_ice_peer_ctx_create(ctx, handler, label, pctxp);
}

int NicerInterfaceImpl::IcePeerContextDestroy(nr_ice_peer_ctx **pctxp) {
  return nr_ice_peer_ctx_destroy(pctxp);
}
int NicerInterfaceImpl::IcePeerContextParseTrickleCandidate(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *streamp,
                                                            char *cand, const char* mdns_cand) {
  return nr_ice_peer_ctx_parse_trickle_candidate(pctxp, streamp, cand, mdns_cand);
}

int NicerInterfaceImpl::IceGather(nr_ice_ctx *ctx, NR_async_cb done_cb, void *cb_arg) {
  return nr_ice_gather(ctx, done_cb, cb_arg);
}

int NicerInterfaceImpl::IceAddMediaStream(nr_ice_ctx *ctx, const char *label, const char *ufrag, const char *pwd,
        int components, nr_ice_media_stream **streamp) {
  return nr_ice_add_media_stream(ctx, label, ufrag, pwd, components, streamp);
}

int NicerInterfaceImpl::IceMediaStreamSend(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *stream, int component,
                                           unsigned char *buffer, size_t length) {
  return nr_ice_media_stream_send(pctxp, stream, component, buffer, length);
}

int NicerInterfaceImpl::IceRemoveMediaStream(nr_ice_ctx *ctx, nr_ice_media_stream **stream) {
  return nr_ice_remove_media_stream(ctx, stream);
}

int NicerInterfaceImpl::IceMediaStreamGetActive(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *stream, int component,
                                                nr_ice_candidate **local, nr_ice_candidate **remote) {
  return nr_ice_media_stream_get_active(pctxp, stream, component, local, remote);
}
}  // namespace erizo
