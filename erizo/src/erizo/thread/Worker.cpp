#include "thread/Worker.h"

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <memory>

using erizo::Worker;

Worker::Worker(std::weak_ptr<Scheduler> scheduler)
    : scheduler_{scheduler},
      service_{},
      service_worker_{new asio_worker::element_type(service_)},
      closed_{false} {
}

Worker::~Worker() {
}

void Worker::task(Task f) {
  service_.post(f);
}

void Worker::start() {
  auto this_ptr = shared_from_this();
  auto worker = [this_ptr] {
    if (!this_ptr->closed_) {
      return this_ptr->service_.run();
    }
    return size_t(0);
  };
  group_.add_thread(new boost::thread(worker));
}

void Worker::close() {
  closed_ = true;
  service_worker_.reset();
  group_.join_all();
  service_.stop();
}

int Worker::scheduleFromNow(Task f, std::chrono::milliseconds delta_ms) {
  int uuid = next_scheduled_++;
  if (auto scheduler = scheduler_.lock()) {
    scheduler->scheduleFromNow(safeTask([f, uuid](std::shared_ptr<Worker> this_ptr) {
      this_ptr->task(this_ptr->safeTask([f, uuid](std::shared_ptr<Worker> this_ptr) {
        std::unique_lock<std::mutex> lock(this_ptr->cancel_mutex_);
        if (this_ptr->isCancelled(uuid)) {
          return;
        }
        f();
      }));
    }), delta_ms);
  }
  return uuid;
}

void Worker::unschedule(int uuid) {
  if (uuid < 0) {
    return;
  }
  std::unique_lock<std::mutex> lock(cancel_mutex_);
  cancelled_.push_back(uuid);
}

bool Worker::isCancelled(int uuid) {
  if (std::find(cancelled_.begin(), cancelled_.end(), uuid) != cancelled_.end()) {
    cancelled_.erase(std::remove(cancelled_.begin(), cancelled_.end(), uuid), cancelled_.end());
    return true;
  }
  return false;
}

std::function<void()> Worker::safeTask(std::function<void(std::shared_ptr<Worker>)> f) {
  std::weak_ptr<Worker> weak_this = shared_from_this();
  return [f, weak_this] {
    if (auto this_ptr = weak_this.lock()) {
      f(this_ptr);
    }
  };
}
