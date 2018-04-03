#ifndef ERIZO_SRC_ERIZO_PIPELINE_SERVICE_H_
#define ERIZO_SRC_ERIZO_PIPELINE_SERVICE_H_

#include "pipeline/Pipeline.h"

namespace erizo {

class PipelineBase;

template <class Context>
class ServiceBase {
 public:
  virtual ~ServiceBase() = default;

  virtual void attachPipeline(Context* /*ctx*/) {}
  virtual void detachPipeline(Context* /*ctx*/) {}
  virtual void notifyEvent(MediaEventPtr event) {}


  Context* getContext() {
    if (attach_count_ != 1) {
      return nullptr;
    }
    assert(service_ctx_);
    return service_ctx_;
  }

 private:
  friend PipelineServiceContext;
  uint64_t attach_count_{0};
  Context* service_ctx_{nullptr};
};

class Service : public ServiceBase<ServiceContext> {
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_PIPELINE_SERVICE_H_
