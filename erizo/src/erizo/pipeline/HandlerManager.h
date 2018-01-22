#ifndef ERIZO_SRC_ERIZO_PIPELINE_HANDLERMANAGER_H_
#define ERIZO_SRC_ERIZO_PIPELINE_HANDLERMANAGER_H_

#include "pipeline/Service.h"

namespace erizo {

class HandlerManagerListener {
 public:
  virtual ~HandlerManagerListener() = default;

  virtual void notifyUpdateToHandlers() = 0;
};

class HandlerManager : public Service {
 public:
  explicit HandlerManager(std::weak_ptr<HandlerManagerListener> listener) : listener_{listener} {}
  virtual ~HandlerManager() = default;

  void notifyUpdateToHandlers() {
    if (auto listener = listener_.lock()) {
      listener->notifyUpdateToHandlers();
    }
  }
 private:
  std::weak_ptr<HandlerManagerListener> listener_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_PIPELINE_HANDLERMANAGER_H_
