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
#include <string>

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

  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual std::string getName() = 0;

  virtual void read(Context* ctx, std::shared_ptr<DataPacket> packet) = 0;
  virtual void readEOF(Context* ctx) {
    ctx->fireReadEOF();
  }
  virtual void transportActive(Context* ctx) {
    ctx->fireTransportActive();
  }
  virtual void transportInactive(Context* ctx) {
    ctx->fireTransportInactive();
  }

  virtual void write(Context* ctx, std::shared_ptr<DataPacket> packet) = 0;
  virtual void close(Context* ctx) {
    return ctx->fireClose();
  }

  virtual void notifyUpdate() = 0;
  virtual void notifyEvent(MediaEventPtr event) {
  }
};

class InboundHandler : public HandlerBase<InboundHandlerContext> {
 public:
  static const HandlerDir dir = HandlerDir::IN;

  typedef InboundHandlerContext Context;
  virtual ~InboundHandler() = default;

  virtual void enable() = 0;
  virtual void disable() = 0;

  virtual std::string getName() = 0;

  virtual void read(Context* ctx, std::shared_ptr<DataPacket> packet) = 0;
  virtual void readEOF(Context* ctx) {
    ctx->fireReadEOF();
  }
  virtual void transportActive(Context* ctx) {
    ctx->fireTransportActive();
  }
  virtual void transportInactive(Context* ctx) {
    ctx->fireTransportInactive();
  }

  virtual void notifyUpdate() = 0;
  virtual void notifyEvent(MediaEventPtr event) {}
};

class OutboundHandler : public HandlerBase<OutboundHandlerContext> {
 public:
  static const HandlerDir dir = HandlerDir::OUT;

  typedef OutboundHandlerContext Context;
  virtual ~OutboundHandler() = default;

  virtual void enable() = 0;
  virtual void disable() = 0;

  virtual std::string getName() = 0;

  virtual void write(Context* ctx, std::shared_ptr<DataPacket> packet) = 0;
  virtual void close(Context* ctx) {
    return ctx->fireClose();
  }

  virtual void notifyUpdate() = 0;
  virtual void notifyEvent(MediaEventPtr event) {}
};

class HandlerAdapter : public Handler {
 public:
  typedef typename Handler::Context Context;

  void enable() override {
  }

  void disable() override {
  }

  std::string getName() override {
    return "adapter";
  }

  void read(Context* ctx, std::shared_ptr<DataPacket> packet) override {
    ctx->fireRead(std::move(packet));
  }

  void write(Context* ctx, std::shared_ptr<DataPacket> packet) override {
    return ctx->fireWrite(std::move(packet));
  }

  void notifyUpdate() override {
  }

  void notifyEvent(MediaEventPtr event) override {
  }
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_PIPELINE_HANDLER_H_
