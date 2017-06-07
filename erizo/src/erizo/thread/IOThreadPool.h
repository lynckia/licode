#ifndef ERIZO_SRC_ERIZO_THREAD_IOTHREADPOOL_H_
#define ERIZO_SRC_ERIZO_THREAD_IOTHREADPOOL_H_

#include <memory>
#include <vector>

#include "thread/IOWorker.h"
#include "thread/Scheduler.h"

namespace erizo {

class IOThreadPool {
 public:
  explicit IOThreadPool(unsigned int num_workers);
  ~IOThreadPool();

  std::shared_ptr<IOWorker> getLessUsedIOWorker();
  void start();
  void close();

 private:
  std::vector<std::shared_ptr<IOWorker>> io_workers_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_THREAD_IOTHREADPOOL_H_
