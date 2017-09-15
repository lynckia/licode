/*
 *  Copyright (c) 2016, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#ifndef ERIZO_SRC_ERIZO_PIPELINE_HANDLERCONTEXT_INL_H_
#define ERIZO_SRC_ERIZO_PIPELINE_HANDLERCONTEXT_INL_H_

#include "./MediaDefinitions.h"

namespace erizo {

class PipelineContext {
 public:
  virtual ~PipelineContext() = default;

  virtual void attachPipeline() = 0;
  virtual void detachPipeline() = 0;

  virtual void notifyUpdate() = 0;
  virtual void notifyEvent(MediaEventPtr event) = 0;
  virtual std::string getName() = 0;
  virtual void enable() = 0;
  virtual void disable() = 0;

  template <class H, class HandlerContext>
  void attachContext(H* handler, HandlerContext* ctx) {
    if (++handler->attachCount_ == 1) {
      handler->ctx_ = ctx;
    } else {
      handler->ctx_ = nullptr;
    }
  }

  virtual void setNextIn(PipelineContext* ctx) = 0;
  virtual void setNextOut(PipelineContext* ctx) = 0;

  virtual HandlerDir getDirection() = 0;
};

class InboundLink {
 public:
  virtual ~InboundLink() = default;
  virtual void read(std::shared_ptr<DataPacket> packet) = 0;
  virtual void readEOF() = 0;
  virtual void transportActive() = 0;
  virtual void transportInactive() = 0;
};

class OutboundLink {
 public:
  virtual ~OutboundLink() = default;
  virtual void write(std::shared_ptr<DataPacket> packet) = 0;
  virtual void close() = 0;
};

template <class H, class Context>
class ContextImplBase : public PipelineContext {
 public:
  ~ContextImplBase() = default;

  H* getHandler() {
    return handler_.get();
  }

  void initialize(
      std::weak_ptr<PipelineBase> pipeline,
      std::shared_ptr<H> handler) {
    pipelineWeak_ = pipeline;
    pipelineRaw_ = pipeline.lock().get();
    handler_ = std::move(handler);
  }

  void notifyUpdate() override {
    handler_->notifyUpdate();
  }

  void notifyEvent(MediaEventPtr event) override {
    handler_->notifyEvent(event);
  }

  std::string getName() override {
    return handler_->getName();
  }

  void enable() override {
    handler_->enable();
  }

  void disable() override {
    handler_->disable();
  }

  // PipelineContext overrides
  void attachPipeline() override {
    if (!attached_) {
      this->attachContext(handler_.get(), impl_);
      handler_->attachPipeline(impl_);
      attached_ = true;
    }
  }

  void detachPipeline() override {
    handler_->detachPipeline(impl_);
    attached_ = false;
  }

  void setNextIn(PipelineContext* ctx) override {
    if (!ctx) {
      nextIn_ = nullptr;
      return;
    }
    auto nextIn = dynamic_cast<InboundLink*>(ctx);
    if (nextIn) {
      nextIn_ = nextIn;
    }
  }

  void setNextOut(PipelineContext* ctx) override {
    if (!ctx) {
      nextOut_ = nullptr;
      return;
    }
    auto nextOut = dynamic_cast<OutboundLink*>(ctx);
    if (nextOut) {
      nextOut_ = nextOut;
    }
  }

  HandlerDir getDirection() override {
    return H::dir;
  }

 protected:
  Context* impl_;
  std::weak_ptr<PipelineBase> pipelineWeak_;
  PipelineBase* pipelineRaw_;
  std::shared_ptr<H> handler_;
  InboundLink* nextIn_{nullptr};
  OutboundLink* nextOut_{nullptr};

 private:
  bool attached_{false};
};

template <class H>
class ContextImpl
  : public HandlerContext,
    public InboundLink,
    public OutboundLink,
    public ContextImplBase<H, HandlerContext> {
 public:
  static const HandlerDir dir = HandlerDir::BOTH;

  explicit ContextImpl(
      std::weak_ptr<PipelineBase> pipeline,
      std::shared_ptr<H> handler) {
    this->impl_ = this;
    this->initialize(pipeline, std::move(handler));
  }

  // For StaticPipeline
  ContextImpl() {
    this->impl_ = this;
  }

  ~ContextImpl() = default;

  // HandlerContext overrides
  void fireRead(std::shared_ptr<DataPacket> packet) override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextIn_) {
      this->nextIn_->read(std::move(packet));
    }
  }

  void fireReadEOF() override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextIn_) {
      this->nextIn_->readEOF();
    }
  }

  void fireTransportActive() override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextIn_) {
      this->nextIn_->transportActive();
    }
  }

  void fireTransportInactive() override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextIn_) {
      this->nextIn_->transportInactive();
    }
  }

  void fireWrite(std::shared_ptr<DataPacket> packet) override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextOut_) {
      this->nextOut_->write(std::move(packet));
    }
  }

  void fireClose() override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextOut_) {
      this->nextOut_->close();
    }
  }

  PipelineBase* getPipeline() override {
    return this->pipelineRaw_;
  }

  std::shared_ptr<PipelineBase> getPipelineShared() override {
    return this->pipelineWeak_.lock();
  }

  // InboundLink overrides
  void read(std::shared_ptr<DataPacket> packet) override {
    auto guard = this->pipelineWeak_.lock();
    this->handler_->read(this, std::move(packet));
  }

  void readEOF() override {
    auto guard = this->pipelineWeak_.lock();
    this->handler_->readEOF(this);
  }

  void transportActive() override {
    auto guard = this->pipelineWeak_.lock();
    this->handler_->transportActive(this);
  }

  void transportInactive() override {
    auto guard = this->pipelineWeak_.lock();
    this->handler_->transportInactive(this);
  }

  // OutboundLink overrides
  void write(std::shared_ptr<DataPacket> packet) override {
    auto guard = this->pipelineWeak_.lock();
    this->handler_->write(this, std::move(packet));
  }

  void close() override {
    auto guard = this->pipelineWeak_.lock();
    this->handler_->close(this);
  }
};

template <class H>
class InboundContextImpl
  : public InboundHandlerContext,
    public InboundLink,
    public ContextImplBase<H, InboundHandlerContext> {
 public:
  static const HandlerDir dir = HandlerDir::IN;

  explicit InboundContextImpl(
      std::weak_ptr<PipelineBase> pipeline,
      std::shared_ptr<H> handler) {
    this->impl_ = this;
    this->initialize(pipeline, std::move(handler));
  }

  // For StaticPipeline
  InboundContextImpl() {
    this->impl_ = this;
  }

  ~InboundContextImpl() = default;

  // InboundHandlerContext overrides
  void fireRead(std::shared_ptr<DataPacket> packet) override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextIn_) {
      this->nextIn_->read(std::move(packet));
    }
  }

  void fireReadEOF() override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextIn_) {
      this->nextIn_->readEOF();
    }
  }

  void fireTransportActive() override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextIn_) {
      this->nextIn_->transportActive();
    }
  }

  void fireTransportInactive() override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextIn_) {
      this->nextIn_->transportInactive();
    }
  }

  PipelineBase* getPipeline() override {
    return this->pipelineRaw_;
  }

  std::shared_ptr<PipelineBase> getPipelineShared() override {
    return this->pipelineWeak_.lock();
  }

  // InboundLink overrides
  void read(std::shared_ptr<DataPacket> packet) override {
    auto guard = this->pipelineWeak_.lock();
    this->handler_->read(this, std::move(packet));
  }

  void readEOF() override {
    auto guard = this->pipelineWeak_.lock();
    this->handler_->readEOF(this);
  }

  void transportActive() override {
    auto guard = this->pipelineWeak_.lock();
    this->handler_->transportActive(this);
  }

  void transportInactive() override {
    auto guard = this->pipelineWeak_.lock();
    this->handler_->transportInactive(this);
  }
};

template <class H>
class OutboundContextImpl
  : public OutboundHandlerContext,
    public OutboundLink,
    public ContextImplBase<H, OutboundHandlerContext> {
 public:
  static const HandlerDir dir = HandlerDir::OUT;

  explicit OutboundContextImpl(
      std::weak_ptr<PipelineBase> pipeline,
      std::shared_ptr<H> handler) {
    this->impl_ = this;
    this->initialize(pipeline, std::move(handler));
  }

  // For StaticPipeline
  OutboundContextImpl() {
    this->impl_ = this;
  }

  ~OutboundContextImpl() = default;

  // OutboundHandlerContext overrides
  void fireWrite(std::shared_ptr<DataPacket> packet) override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextOut_) {
      return this->nextOut_->write(std::move(packet));
    }
  }

  void fireClose() override {
    auto guard = this->pipelineWeak_.lock();
    if (this->nextOut_) {
      return this->nextOut_->close();
    }
  }

  PipelineBase* getPipeline() override {
    return this->pipelineRaw_;
  }

  std::shared_ptr<PipelineBase> getPipelineShared() override {
    return this->pipelineWeak_.lock();
  }

  // OutboundLink overrides
  void write(std::shared_ptr<DataPacket> packet) override {
    auto guard = this->pipelineWeak_.lock();
    return this->handler_->write(this, std::move(packet));
  }

  void close() override {
    auto guard = this->pipelineWeak_.lock();
    return this->handler_->close(this);
  }
};

template <class Handler>
struct ContextType {
  typedef typename std::conditional<
    Handler::dir == HandlerDir::BOTH,
    ContextImpl<Handler>,
    typename std::conditional<
      Handler::dir == HandlerDir::IN,
      InboundContextImpl<Handler>,
      OutboundContextImpl<Handler>
    >::type>::type
  type;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_PIPELINE_HANDLERCONTEXT_INL_H_
