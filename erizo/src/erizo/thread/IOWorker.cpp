#include "thread/IOWorker.h"

extern "C" {
#include <async_wait.h>
#include <async_timer.h>
}

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
      NR_async_event_wait2(&events, &towait);
      gettimeofday(&tv, 0);
      NR_async_timer_update_time(&tv);
    }
  }));
}

void IOWorker::close() {
  if (!closed_.exchange(true)) {
    thread_->join();
  }
}
