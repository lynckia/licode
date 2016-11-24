#ifndef ERIZO_SRC_ERIZO_THREAD_THREADPOOL_H_
#define ERIZO_SRC_ERIZO_THREAD_THREADPOOL_H_

#include <memory>
#include <vector>

#include "thread/Worker.h"

namespace erizo {

class ThreadPool {
 public:
  explicit ThreadPool(unsigned int num_workers) : workers_{} {
    for (unsigned int index = 0; index < num_workers; index++) {
      workers_.push_back(std::make_shared<Worker>());
    }
  }

  ~ThreadPool() {
    close();
  }

  std::shared_ptr<Worker> getLessUsedWorker() {
    std::shared_ptr<Worker> choosed_worker = workers_.front();
    for (auto worker : workers_) {
      if (choosed_worker.use_count() > worker.use_count()) {
        choosed_worker = worker;
      }
    }
    return choosed_worker;
  }

  void start() {
    for (auto worker : workers_) {
      worker->start();
    }
  }

  void close() {
    for (auto worker : workers_) {
      worker->close();
    }
  }

 private:
  std::vector<std::shared_ptr<Worker>> workers_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_THREAD_THREADPOOL_H_
