#include "thread/Scheduler.h"

#include <assert.h>

#include <boost/bind.hpp>
#include <utility>


Scheduler::Scheduler(int n_threads_servicing_queue)
: n_threads_servicing_queue_(n_threads_servicing_queue), stop_requested_(false), stop_when_empty_(false) {
  stop_requested_ = false;
  stop_when_empty_ = false;
  for (int index = 0; index < n_threads_servicing_queue; index++) {
    group_.create_thread(boost::bind(&Scheduler::serviceQueue, this));
  }
}

Scheduler::~Scheduler() {
  stop(false);
  assert(n_threads_servicing_queue_ == 0);
}

std::chrono::system_clock::time_point Scheduler::getFirstTime() {
  return task_queue_.empty() ? std::chrono::system_clock::now() : task_queue_.begin()->first;
}

void Scheduler::serviceQueue() {
  std::unique_lock<std::mutex> lock(new_task_mutex_);

  while (!stop_requested_ && !(stop_when_empty_ && task_queue_.empty())) {
    try {
      while (!stop_requested_ && !stop_when_empty_ && task_queue_.empty()) {
        new_task_scheduled_.wait(lock);
      }

      std::chrono::system_clock::time_point time = getFirstTime();
      while (!stop_requested_ && !task_queue_.empty() &&
             new_task_scheduled_.wait_until(lock, time) != std::cv_status::timeout) {
        time = getFirstTime();
      }
      if (stop_requested_) {
        break;
      }

      if (task_queue_.empty()) {
        continue;
      }

      Function f = task_queue_.begin()->second;
      task_queue_.erase(task_queue_.begin());

      lock.unlock();
      f();
      lock.lock();
    } catch (...) {
      --n_threads_servicing_queue_;
      assert(false && "An exception has been thrown inside Scheduler");
    }
  }
  --n_threads_servicing_queue_;
}

void Scheduler::stop(bool drain) {
  {
    std::unique_lock<std::mutex> lock(new_task_mutex_);
    if (drain) {
      stop_when_empty_ = true;
    } else {
      stop_requested_ = true;
    }
  }
  new_task_scheduled_.notify_all();
  group_.join_all();
}

void Scheduler::schedule(Scheduler::Function f, std::chrono::system_clock::time_point t) {
  {
    std::unique_lock<std::mutex> lock(new_task_mutex_);
    // Pairs in this multimap are sorted by the Key value, so begin() will always point to the
    // earlier task
    task_queue_.insert(std::make_pair(t, f));
  }
  new_task_scheduled_.notify_one();
}

void Scheduler::scheduleFromNow(Scheduler::Function f, std::chrono::milliseconds delta_ms) {
  schedule(f, std::chrono::system_clock::now() + delta_ms);
}

// TODO(javier): Make it possible to unschedule repeated tasks before enable this code
// static void Repeat(Scheduler* s, Scheduler::Function f, std::chrono::milliseconds delta_ms) {
//   f();
//   s->scheduleFromNow(boost::bind(&Repeat, s, f, delta_ms), delta_ms);
// }

// void Scheduler::scheduleEvery(Scheduler::Function f, std::chrono::milliseconds delta_ms) {
//   scheduleFromNow(boost::bind(&Repeat, this, f, delta_ms), delta_ms);
// }
