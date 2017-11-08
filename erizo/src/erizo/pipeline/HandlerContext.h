/*
 *  Copyright (c) 2016, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#ifndef ERIZO_SRC_ERIZO_PIPELINE_HANDLERCONTEXT_H_
#define ERIZO_SRC_ERIZO_PIPELINE_HANDLERCONTEXT_H_

#include <memory>

#include "./MediaDefinitions.h"

namespace erizo {

class PipelineBase;

class HandlerContext {
 public:
  virtual ~HandlerContext() = default;

  virtual void fireRead(std::shared_ptr<DataPacket> packet) = 0;
  virtual void fireReadEOF() = 0;
  virtual void fireTransportActive() = 0;
  virtual void fireTransportInactive() = 0;

  virtual void fireWrite(std::shared_ptr<DataPacket> packet) = 0;
  virtual void fireClose() = 0;

  virtual PipelineBase* getPipeline() = 0;
  virtual std::shared_ptr<PipelineBase> getPipelineShared() = 0;
};

class InboundHandlerContext {
 public:
  virtual ~InboundHandlerContext() = default;

  virtual void fireRead(std::shared_ptr<DataPacket> packet) = 0;
  virtual void fireReadEOF() = 0;
  virtual void fireTransportActive() = 0;
  virtual void fireTransportInactive() = 0;

  virtual PipelineBase* getPipeline() = 0;
  virtual std::shared_ptr<PipelineBase> getPipelineShared() = 0;
};

class OutboundHandlerContext {
 public:
  virtual ~OutboundHandlerContext() = default;

  virtual void fireWrite(std::shared_ptr<DataPacket> packet) = 0;
  virtual void fireClose() = 0;

  virtual PipelineBase* getPipeline() = 0;
  virtual std::shared_ptr<PipelineBase> getPipelineShared() = 0;
};

// #include <windows.h> has blessed us with #define IN & OUT, typically mapped
// to nothing, so letting the preprocessor delete each of these symbols, leading
// to interesting compiler errors around HandlerDir.
#ifdef IN
#  undef IN
#endif
#ifdef OUT
#  undef OUT
#endif

enum class HandlerDir {
  IN,
  OUT,
  BOTH
};

}  // namespace erizo

#include <pipeline/HandlerContext-inl.h>

#endif  // ERIZO_SRC_ERIZO_PIPELINE_HANDLERCONTEXT_H_
