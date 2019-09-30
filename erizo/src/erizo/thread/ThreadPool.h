#ifndef ERIZO_SRC_ERIZO_THREAD_THREADPOOL_H_
#define ERIZO_SRC_ERIZO_THREAD_THREADPOOL_H_

#include <memory>
#include <vector>

#include "thread/Worker.h"
#include "thread/Scheduler.h"

namespace erizo {

class ThreadPool {
 public:
  explicit ThreadPool(unsigned int num_workers);
  ~ThreadPool();

  std::shared_ptr<Worker> getLessUsedWorker();
  void start();
  void close();

  void resetStats();
  DurationDistribution getDurationDistribution();

 private:
  std::vector<std::shared_ptr<Worker>> workers_;
  std::shared_ptr<Scheduler> scheduler_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_THREAD_THREADPOOL_H_
