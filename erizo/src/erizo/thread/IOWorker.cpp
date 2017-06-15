#include "thread/IOWorker.h"

extern "C" {
#include <r_errors.h>
#include <async_wait.h>
#include <async_timer.h>
}

#include <chrono>  // NOLINT

using erizo::IOWorker;

IOWorker::IOWorker() : started_{false}, closed_{false} {
}

IOWorker::~IOWorker() {
  close();
}

void IOWorker::start() {
  if (started_.exchange(true)) {
    return;
  }

  thread_ = std::unique_ptr<std::thread>(new std::thread([this] {
    while (!closed_) {
      int events;
      struct timeval towait = {0, 100000};
      struct timeval tv;
      int r = NR_async_event_wait2(&events, &towait);
      if (r == R_EOD) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      gettimeofday(&tv, 0);
      NR_async_timer_update_time(&tv);
      std::vector<Task> tasks;
      {
        std::unique_lock<std::mutex> lock(task_mutex_);
        tasks.swap(tasks_);
      }
      for (Task &task : tasks) {
        task();
      }
    }
  }));
}

void IOWorker::task(Task f) {
  std::unique_lock<std::mutex> lock(task_mutex_);
  tasks_.push_back(f);
}

void IOWorker::close() {
  if (!closed_.exchange(true)) {
    if (thread_ != nullptr) {
      thread_->join();
    }
    tasks_.clear();
  }
}
