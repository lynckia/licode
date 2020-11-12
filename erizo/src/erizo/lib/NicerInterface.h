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
  virtual ~NicerInterface() {}
  virtual int IceContextCreate(char *label, UINT4 flags, nr_ice_ctx **ctxp) = 0;
  virtual int IceContextDestroy(nr_ice_ctx **ctxp) = 0;
  virtual int IceContextSetTrickleCallback(nr_ice_ctx *ctx, nr_ice_trickle_candidate_cb cb, void *cb_arg) = 0;
  virtual void IceContextSetSocketFactory(nr_ice_ctx *ctx, nr_socket_factory *factory) = 0;
  virtual void IceContextFinalize(nr_ice_ctx *ctx, nr_ice_peer_ctx *pctxp) = 0;
  virtual int IceContextSetStunServers(nr_ice_ctx *ctx, nr_ice_stun_server *servers, int ct) = 0;
  virtual int IceContextSetTurnServers(nr_ice_ctx *ctx, nr_ice_turn_server *servers, int ct) = 0;
  virtual void IceContextSetPortRange(nr_ice_ctx *ctx, uint16_t min_port, uint16_t max_port) = 0;
  virtual int IcePeerContextCreate(nr_ice_ctx *ctx, nr_ice_handler *handler, char *label, nr_ice_peer_ctx **pctxp) = 0;
  virtual int IcePeerContextDestroy(nr_ice_peer_ctx **pctxp) = 0;
  virtual int IcePeerContextParseTrickleCandidate(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *streamp, char *cand,
          const char *mdns_cand) = 0;
  virtual int IcePeerContextPairCandidates(nr_ice_peer_ctx *pctxp) = 0;
  virtual int IcePeerContextStartChecks2(nr_ice_peer_ctx *pctxp, int type) = 0;
  virtual int IcePeerContextParseStreamAttributes(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *stream,
                                                  char **attributes, size_t length) = 0;

  virtual int IceGetNewIceUFrag(char **ufrag) = 0;
  virtual int IceGetNewIcePwd(char **pwd) = 0;

  virtual int IceGather(nr_ice_ctx *ctx, NR_async_cb done_cb, void *cb_arg) = 0;
  virtual int IceAddMediaStream(nr_ice_ctx *ctx, const char *label, const char *ufrag, const char *pwd,
          int components, nr_ice_media_stream **streamp) = 0;
  virtual int IceMediaStreamSend(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *stream, int component,
                                 unsigned char *buffer, size_t length) = 0;
  virtual int IceRemoveMediaStream(nr_ice_ctx *ctx, nr_ice_media_stream **stream) = 0;
  virtual int IceMediaStreamGetActive(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *stream, int component,
                                      nr_ice_candidate **local, nr_ice_candidate **remote) = 0;
};


class NicerInterfaceImpl: public NicerInterface {
 public:
  int IceContextCreate(char *label, UINT4 flags, nr_ice_ctx **ctxp) override;
  int IceContextDestroy(nr_ice_ctx **ctxp) override;
  int IceContextSetTrickleCallback(nr_ice_ctx *ctx, nr_ice_trickle_candidate_cb cb, void *cb_arg) override;
  void IceContextSetSocketFactory(nr_ice_ctx *ctx, nr_socket_factory *factory) override;
  void IceContextFinalize(nr_ice_ctx *ctx, nr_ice_peer_ctx *pctxp) override;
  int IceContextSetStunServers(nr_ice_ctx *ctx, nr_ice_stun_server *servers, int ct) override;
  int IceContextSetTurnServers(nr_ice_ctx *ctx, nr_ice_turn_server *servers, int ct) override;
  void IceContextSetPortRange(nr_ice_ctx *ctx, uint16_t min_port, uint16_t max_port) override;

  int IcePeerContextCreate(nr_ice_ctx *ctx, nr_ice_handler *handler, char *label, nr_ice_peer_ctx **pctxp) override;
  int IcePeerContextDestroy(nr_ice_peer_ctx **pctxp) override;
  int IcePeerContextParseTrickleCandidate(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *streamp, char *cand,
            const char* mdns_cand) override;
  int IcePeerContextPairCandidates(nr_ice_peer_ctx *pctxp) override;
  int IcePeerContextStartChecks2(nr_ice_peer_ctx *pctxp, int type) override;
  int IcePeerContextParseStreamAttributes(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *stream, char **attributes,
                                          size_t length) override;

  int IceGetNewIceUFrag(char **ufrag) override;
  int IceGetNewIcePwd(char **pwd) override;

  int IceGather(nr_ice_ctx *ctx, NR_async_cb done_cb, void *cb_arg) override;
  int IceAddMediaStream(nr_ice_ctx *ctx, const char *label, const char *ufrag, const char *pwd, int components,
          nr_ice_media_stream **streamp) override;
  int IceMediaStreamSend(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *stream, int component, unsigned char *buffer,
                         size_t length) override;
  int IceRemoveMediaStream(nr_ice_ctx *ctx, nr_ice_media_stream **stream) override;
  int IceMediaStreamGetActive(nr_ice_peer_ctx *pctxp, nr_ice_media_stream *stream, int component,
                              nr_ice_candidate **local, nr_ice_candidate **remote) override;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_LIB_NICERINTERFACE_H_
