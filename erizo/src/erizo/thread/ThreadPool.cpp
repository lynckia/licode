#include "thread/ThreadPool.h"

#include <memory>

constexpr int kNumThreadsPerScheduler = 2;

using erizo::ThreadPool;
using erizo::Worker;
using erizo::DurationDistribution;

ThreadPool::ThreadPool(unsigned int num_workers)
    : workers_{}, scheduler_{std::make_shared<Scheduler>(kNumThreadsPerScheduler)} {
  for (unsigned int index = 0; index < num_workers; index++) {
    workers_.push_back(std::make_shared<Worker>(scheduler_));
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
  std::vector<std::shared_ptr<std::promise<void>>> promises(workers_.size());
  int index = 0;
  for (auto worker : workers_) {
    promises[index] = std::make_shared<std::promise<void>>();
    worker->start(promises[index++]);
  }
  for (auto promise : promises) {
    promise->get_future().wait();
  }
}

void ThreadPool::close() {
  for (auto worker : workers_) {
    worker->close();
  }
  scheduler_->stop(true);
}

DurationDistribution ThreadPool::getDurationDistribution() {
  DurationDistribution total_durations;
  for (auto worker : workers_) {
    total_durations += worker->getDurationDistribution();
  }
  return total_durations;
}

void ThreadPool::resetStats() {
  for (auto worker : workers_) {
    worker->resetStats();
  }
}
