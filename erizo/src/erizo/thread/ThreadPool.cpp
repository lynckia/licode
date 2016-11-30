#include "thread/ThreadPool.h"

#include <memory>

using erizo::ThreadPool;
using erizo::Worker;

ThreadPool::ThreadPool(unsigned int num_workers) : workers_{} {
  for (unsigned int index = 0; index < num_workers; index++) {
    workers_.push_back(std::make_shared<Worker>());
  }
}

ThreadPool::~ThreadPool() {
  close();
}

std::shared_ptr<Worker> ThreadPool::getLessUsedWorker() {
  std::shared_ptr<Worker> chosen_worker = workers_.front();
  for (auto worker : workers_) {
    if (chosen_worker.use_count() > worker.use_count()) {
      chosen_worker = worker;
    }
  }
  return chosen_worker;
}

void ThreadPool::start() {
  for (auto worker : workers_) {
    worker->start();
  }
}

void ThreadPool::close() {
  for (auto worker : workers_) {
    worker->close();
  }
}
