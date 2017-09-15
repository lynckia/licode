#ifndef ERIZO_SRC_ERIZO_PIPELINE_SERVICECONTEXT_INL_H_
#define ERIZO_SRC_ERIZO_PIPELINE_SERVICECONTEXT_INL_H_

namespace erizo {

class PipelineServiceContext {
 public:
  virtual ~PipelineServiceContext() = default;

  virtual void attachPipeline() = 0;
  virtual void detachPipeline() = 0;
  virtual void notifyEvent(MediaEventPtr event) = 0;

  template <class S, class Context>
  void attachContext(S* service, Context* ctx) {
    if (++service->attach_count_ == 1) {
      service->service_ctx_ = ctx;
    } else {
      service->service_ctx_ = nullptr;
    }
  }
};

template <class S, class Context>
class ServiceContextImplBase : public PipelineServiceContext {
 public:
  ~ServiceContextImplBase() = default;

  std::weak_ptr<S> getService() {
    return service_;
  }

  void initialize(
      std::weak_ptr<PipelineBase> pipeline,
      std::weak_ptr<S> service) {
    pipeline_weak_ = pipeline;
    pipeline_raw_ = pipeline.lock().get();
    service_ = std::move(service);
  }

  // PipelineContext overrides
  void attachPipeline() override {
    if (!attached_) {
      auto service_ptr = service_.lock();
      if (!service_ptr) {
        return;
      }
      attachContext(service_ptr.get(), impl_);
      service_ptr->attachPipeline(impl_);
      attached_ = true;
    }
  }

  void detachPipeline() override {
    auto service_ptr = service_.lock();
    if (!service_ptr) {
      return;
    }
    service_ptr->detachPipeline(impl_);
    attached_ = false;
  }

  void notifyEvent(MediaEventPtr event) override {
    auto service_ptr = service_.lock();
    if (!service_ptr) {
      return;
    }
    service_ptr->notifyEvent(event);
  }

 protected:
  Context* impl_;
  std::weak_ptr<PipelineBase> pipeline_weak_;
  PipelineBase* pipeline_raw_;
  std::weak_ptr<S> service_;

 private:
  bool attached_{false};
};

template <class S>
class ServiceContextImpl
  : public ServiceContext,
    public ServiceContextImplBase<S, ServiceContext> {
 public:
  explicit ServiceContextImpl(
      std::weak_ptr<PipelineBase> pipeline,
      std::weak_ptr<S> service) {
    this->impl_ = this;
    this->initialize(pipeline, std::move(service));
  }

  // For StaticPipeline
  ServiceContextImpl() {
    this->impl_ = this;
  }

  ~ServiceContextImpl() = default;

  PipelineBase* getPipeline() override {
    return this->pipeline_raw_;
  }

  std::shared_ptr<PipelineBase> getPipelineShared() override {
    return this->pipeline_weak_.lock();
  }
};

template <class Service>
struct ServiceContextType {
  typedef ServiceContextImpl<Service> type;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_PIPELINE_SERVICECONTEXT_INL_H_
