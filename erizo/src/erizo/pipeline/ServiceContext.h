#ifndef ERIZO_SRC_ERIZO_PIPELINE_SERVICECONTEXT_H_
#define ERIZO_SRC_ERIZO_PIPELINE_SERVICECONTEXT_H_

namespace erizo {

class PipelineBase;

class ServiceContext {
 public:
  virtual ~ServiceContext() = default;

  virtual PipelineBase* getPipeline() = 0;
  virtual std::shared_ptr<PipelineBase> getPipelineShared() = 0;
};

}  // namespace erizo

#include <pipeline/ServiceContext-inl.h>

#endif  // ERIZO_SRC_ERIZO_PIPELINE_SERVICECONTEXT_H_
