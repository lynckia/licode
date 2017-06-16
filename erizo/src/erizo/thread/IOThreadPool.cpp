#include "thread/IOThreadPool.h"

#include <memory>
#include <future>  // NOLINT

using erizo::IOThreadPool;
using erizo::IOWorker;

IOThreadPool::IOThreadPool(unsigned int num_io_workers)
    : io_workers_{} {
  for (unsigned int index = 0; index < num_io_workers; index++) {
    io_workers_.push_back(std::make_shared<IOWorker>());
  }
}

IOThreadPool::~IOThreadPool() {
  close();
}

std::shared_ptr<IOWorker> IOThreadPool::getLessUsedIOWorker() {
  std::shared_ptr<IOWorker> chosen_io_worker = io_workers_.front();
  for (auto io_worker : io_workers_) {
    if (chosen_io_worker.use_count() > io_worker.use_count()) {
      chosen_io_worker = io_worker;
    }
  }
  return chosen_io_worker;
}

void IOThreadPool::start() {
  std::vector<std::shared_ptr<std::promise<void>>> promises(io_workers_.size());
  int index = 0;
  for (auto io_worker : io_workers_) {
    promises[index] = std::make_shared<std::promise<void>>();
    io_worker->start(promises[index++]);
  }
  for (auto promise : promises) {
    promise->get_future().wait();
  }
}

void IOThreadPool::close() {
  for (auto io_worker : io_workers_) {
    io_worker->close();
  }
}
