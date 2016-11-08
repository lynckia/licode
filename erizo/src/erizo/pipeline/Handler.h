/*
 *  Copyright (c) 2016, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#ifndef ERIZO_SRC_ERIZO_PIPELINE_HANDLER_H_
#define ERIZO_SRC_ERIZO_PIPELINE_HANDLER_H_

#include <cassert>

#include "pipeline/Pipeline.h"
#include "./MediaDefinitions.h"

namespace erizo {

template <class Context>
class HandlerBase {
 public:
  virtual ~HandlerBase() = default;

  virtual void attachPipeline(Context* /*ctx*/) {}
  virtual void detachPipeline(Context* /*ctx*/) {}

  Context* getContext() {
    if (attachCount_ != 1) {
      return nullptr;
    }
    assert(ctx_);
    return ctx_;
  }

 private:
  friend PipelineContext;
  uint64_t attachCount_{0};
  Context* ctx_{nullptr};
};

class Handler : public HandlerBase<HandlerContext> {
 public:
  static const HandlerDir dir = HandlerDir::BOTH;

  typedef HandlerContext Context;
  virtual ~Handler() = default;

  virtual void read(Context* ctx, std::shared_ptr<dataPacket> packet) = 0;
  virtual void readEOF(Context* ctx) {
    ctx->fireReadEOF();
  }
  virtual void transportActive(Context* ctx) {
    ctx->fireTransportActive();
  }
  virtual void transportInactive(Context* ctx) {
    ctx->fireTransportInactive();
  }

  virtual void write(Context* ctx, std::shared_ptr<dataPacket> packet) = 0;
  virtual void close(Context* ctx) {
    return ctx->fireClose();
  }
};

class InboundHandler : public HandlerBase<InboundHandlerContext> {
 public:
  static const HandlerDir dir = HandlerDir::IN;

  typedef InboundHandlerContext Context;
  virtual ~InboundHandler() = default;

  virtual void read(Context* ctx, std::shared_ptr<dataPacket> packet) = 0;
  virtual void readEOF(Context* ctx) {
    ctx->fireReadEOF();
  }
  virtual void transportActive(Context* ctx) {
    ctx->fireTransportActive();
  }
  virtual void transportInactive(Context* ctx) {
    ctx->fireTransportInactive();
  }
};

class OutboundHandler : public HandlerBase<OutboundHandlerContext> {
 public:
  static const HandlerDir dir = HandlerDir::OUT;

  typedef OutboundHandlerContext Context;
  virtual ~OutboundHandler() = default;

  virtual void write(Context* ctx, std::shared_ptr<dataPacket> packet) = 0;
  virtual void close(Context* ctx) {
    return ctx->fireClose();
  }
};

class HandlerAdapter : public Handler {
 public:
  typedef typename Handler::Context Context;

  void read(Context* ctx, std::shared_ptr<dataPacket> packet) override {
    ctx->fireRead(packet);
  }

  void write(Context* ctx, std::shared_ptr<dataPacket> packet) override {
    return ctx->fireWrite(packet);
  }
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_PIPELINE_HANDLER_H_
