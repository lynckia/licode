/*
 *  Copyright (c) 2016, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#ifndef ERIZO_SRC_ERIZO_PIPELINE_PIPELINE_INL_H_
#define ERIZO_SRC_ERIZO_PIPELINE_PIPELINE_INL_H_

namespace erizo {

template <class H>
PipelineBase& PipelineBase::addBack(std::shared_ptr<H> handler) {
  typedef typename ContextType<H>::type Context;
  return addHelper(
      std::make_shared<Context>(shared_from_this(), std::move(handler)),
      false);
}

template <class H>
PipelineBase& PipelineBase::addBack(H&& handler) {
  return addBack(std::make_shared<H>(std::forward<H>(handler)));
}

template <class H>
PipelineBase& PipelineBase::addBack(H* handler) {
  return addBack(std::shared_ptr<H>(handler, [](H*){}));  // NOLINT
}

template <class H>
PipelineBase& PipelineBase::addFront(std::shared_ptr<H> handler) {
  typedef typename ContextType<H>::type Context;
  return addHelper(
      std::make_shared<Context>(shared_from_this(), std::move(handler)),
      true);
}

template <class H>
PipelineBase& PipelineBase::addFront(H&& handler) {
  return addFront(std::make_shared<H>(std::forward<H>(handler)));
}

template <class H>
PipelineBase& PipelineBase::addFront(H* handler) {
  return addFront(std::shared_ptr<H>(handler, [](H*){}));  // NOLINT
}

template <class H>
PipelineBase& PipelineBase::removeHelper(H* handler, bool checkEqual) {
  typedef typename ContextType<H>::type Context;
  bool removed = false;
  for (auto it = ctxs_.begin(); it != ctxs_.end(); it++) {
    auto ctx = std::dynamic_pointer_cast<Context>(*it);
    if (ctx && (!checkEqual || ctx->getHandler() == handler)) {
      it = removeAt(it);
      removed = true;
      if (it == ctxs_.end()) {
        break;
      }
    }
  }

  return *this;
}

template <class H>
PipelineBase& PipelineBase::remove() {
  return removeHelper<H>(nullptr, false);
}

template <class H>
PipelineBase& PipelineBase::remove(H* handler) {
  return removeHelper<H>(handler, true);
}

template <class H>
H* PipelineBase::getHandler(int i) {
  return getContext<H>(i)->getHandler();
}

template <class H>
H* PipelineBase::getHandler() {
  auto ctx = getContext<H>();
  return ctx ? ctx->getHandler() : nullptr;
}

template <class H>
typename ContextType<H>::type* PipelineBase::getContext(int i) {
  auto ctx = dynamic_cast<typename ContextType<H>::type*>(ctxs_[i].get());
  CHECK(ctx);
  return ctx;
}

template <class H>
typename ContextType<H>::type* PipelineBase::getContext() {
  for (auto pipelineCtx : ctxs_) {
    auto ctx = dynamic_cast<typename ContextType<H>::type*>(pipelineCtx.get());
    if (ctx) {
      return ctx;
    }
  }
  return nullptr;
}

template <class H>
bool PipelineBase::setOwner(H* handler) {
  typedef typename ContextType<H>::type Context;
  for (auto& ctx : ctxs_) {
    auto ctxImpl = dynamic_cast<Context*>(ctx.get());
    if (ctxImpl && ctxImpl->getHandler() == handler) {
      owner_ = ctx;
      return true;
    }
  }
  return false;
}

template <class Context>
void PipelineBase::addContextFront(Context* ctx) {
  addHelper(std::shared_ptr<Context>(ctx, [](Context*){}), true);  // NOLINT
}

template <class Context>
PipelineBase& PipelineBase::addHelper(
    std::shared_ptr<Context>&& ctx,  // NOLINT
    bool front) {
  ctxs_.insert(front ? ctxs_.begin() : ctxs_.end(), ctx);
  if (Context::dir == HandlerDir::BOTH || Context::dir == HandlerDir::IN) {
    inCtxs_.insert(front ? inCtxs_.begin() : inCtxs_.end(), ctx.get());
  }
  if (Context::dir == HandlerDir::BOTH || Context::dir == HandlerDir::OUT) {
    outCtxs_.insert(front ? outCtxs_.begin() : outCtxs_.end(), ctx.get());
  }
  return *this;
}

template <class S>
void PipelineBase::addService(std::shared_ptr<S> service) {
  typedef typename ServiceContextType<S>::type Context;
  service_ctxs_.push_back(std::make_shared<Context>(shared_from_this(), std::move(service)));
}

template <class S>
void PipelineBase::addService(S&& service) {
  addService(std::make_shared<S>(std::forward<S>(service)));
}

template <class S>
void PipelineBase::addService(S* service) {
  addService(std::shared_ptr<S>(service, [](S*){}));  // NOLINT
}

template <class S>
typename ServiceContextType<S>::type* PipelineBase::getServiceContext() {
  for (auto pipeline_service_ctx : service_ctxs_) {
    auto ctx = dynamic_cast<typename ServiceContextType<S>::type*>(pipeline_service_ctx.get());
    if (ctx) {
      return ctx;
    }
  }
  return nullptr;
}

template <class S>
std::shared_ptr<S> PipelineBase::getService() {
  auto ctx = getServiceContext<S>();
  return ctx ? ctx->getService().lock() : std::shared_ptr<S>();
}

template <class S>
void PipelineBase::removeService() {
  typedef typename ServiceContextType<S>::type Context;
  bool removed = false;
  for (auto it = service_ctxs_.begin(); it != service_ctxs_.end(); it++) {
    auto ctx = std::dynamic_pointer_cast<Context>(*it);
    if (ctx) {
      (*it)->detachPipeline();
      it = service_ctxs_.erase(it);
      removed = true;
      if (it == service_ctxs_.end()) {
        break;
      }
    }
  }
}


}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_PIPELINE_PIPELINE_INL_H_
