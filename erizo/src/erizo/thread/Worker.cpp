#include "thread/Worker.h"

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <algorithm>
#include <memory>

#include "lib/ClockUtils.h"

using erizo::Worker;
using erizo::DurationDistribution;
using erizo::SimulatedWorker;
using erizo::ScheduledTaskReference;

ScheduledTaskReference::ScheduledTaskReference() : cancelled{false} {
}

bool ScheduledTaskReference::isCancelled() {
  return cancelled;
}
void ScheduledTaskReference::cancel() {
  cancelled = true;
}

DurationDistribution::DurationDistribution()
    : duration_0_10_ms{0},
      duration_10_50_ms{0},
      duration_50_100_ms{0},
      duration_100_1000_ms{0},
      duration_1000_ms{0} {}

void DurationDistribution::reset() {
  duration_0_10_ms     = 0;
  duration_10_50_ms    = 0;
  duration_50_100_ms   = 0;
  duration_100_1000_ms = 0;
  duration_1000_ms     = 0;
}

DurationDistribution& DurationDistribution::operator+=(const DurationDistribution& durations) {
  duration_0_10_ms     += durations.duration_0_10_ms;
  duration_10_50_ms    += durations.duration_10_50_ms;
  duration_50_100_ms   += durations.duration_50_100_ms;
  duration_100_1000_ms += durations.duration_100_1000_ms;
  duration_1000_ms     += durations.duration_1000_ms;
  return *this;
}

Worker::Worker(std::weak_ptr<Scheduler> scheduler, std::shared_ptr<Clock> the_clock)
    : scheduler_{scheduler},
      clock_{the_clock},
      service_{},
      service_worker_{new asio_worker::element_type(service_)},
      closed_{false} {
}

Worker::~Worker() {
}

void Worker::task(Task f) {
  service_.dispatch(f);
}

void Worker::start() {
  auto promise = std::make_shared<std::promise<void>>();
  start(promise);
}

void Worker::start(std::shared_ptr<std::promise<void>> start_promise) {
  auto this_ptr = shared_from_this();
  auto worker = [this_ptr, start_promise] {
    start_promise->set_value();
    if (!this_ptr->closed_) {
      return this_ptr->service_.run();
    }
    return size_t(0);
  };
  auto thread = new boost::thread(worker);
  thread_id_ = thread->get_id();
  group_.add_thread(thread);
}

void Worker::close() {
  closed_ = true;
  service_worker_.reset();
  group_.join_all();
  service_.stop();
}

std::shared_ptr<ScheduledTaskReference> Worker::scheduleFromNow(Task f, duration delta) {
  auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delta);
  auto id = std::make_shared<ScheduledTaskReference>();
  if (auto scheduler = scheduler_.lock()) {
    scheduler->scheduleFromNow(safeTask([f, id](std::shared_ptr<Worker> this_ptr) {
      this_ptr->task(this_ptr->safeTask([f, id](std::shared_ptr<Worker> this_ptr) {
        {
          if (id->isCancelled()) {
            return;
          }
        }
        f();
      }));
    }), delta_ms);
  }
  return id;
}

void Worker::scheduleEvery(ScheduledTask f, duration period) {
  scheduleEvery(f, period, period);
}

void Worker::scheduleEvery(ScheduledTask f, duration period, duration next_delay) {
  time_point start = clock_->now();
  std::shared_ptr<Clock> clock = clock_;

  scheduleFromNow(safeTask([start, period, next_delay, f, clock](std::shared_ptr<Worker> this_ptr) {
    if (f()) {
      duration clock_skew = clock->now() - start - next_delay;
      duration delay = period - clock_skew;
      this_ptr->scheduleEvery(f, period, delay);
    }
  }), std::max(next_delay, duration{0}));
}

void Worker::unschedule(std::shared_ptr<ScheduledTaskReference> id) {
  id->cancel();
}

std::function<void()> Worker::safeTask(std::function<void(std::shared_ptr<Worker>)> f) {
  std::weak_ptr<Worker> weak_this = shared_from_this();
  return [f, weak_this] {
    if (auto this_ptr = weak_this.lock()) {
      time_point start = this_ptr->clock_->now();
      f(this_ptr);
      time_point end = this_ptr->clock_->now();
      this_ptr->addToStats(end - start);
    }
  };
}

void Worker::addToStats(duration task_duration) {
  if (task_duration <= std::chrono::milliseconds(10)) {
    durations_.duration_0_10_ms++;
  } else if (task_duration <= std::chrono::milliseconds(50)) {
    durations_.duration_10_50_ms++;
  } else if (task_duration <= std::chrono::milliseconds(100)) {
    durations_.duration_50_100_ms++;
  } else if (task_duration <= std::chrono::milliseconds(1000)) {
    durations_.duration_100_1000_ms++;
  } else {
    durations_.duration_1000_ms++;
  }
}

void Worker::resetStats() {
  task(safeTask([](std::shared_ptr<Worker> worker) {
    worker->durations_.reset();
  }));
}

SimulatedWorker::SimulatedWorker(std::shared_ptr<SimulatedClock> the_clock)
    : Worker(std::make_shared<Scheduler>(1), the_clock), clock_{the_clock} {
}

void SimulatedWorker::task(Task f) {
  tasks_.push_back(f);
}

void SimulatedWorker::start() {
}

void SimulatedWorker::start(std::shared_ptr<std::promise<void>> start_promise) {
}

void SimulatedWorker::close() {
  scheduled_tasks_.clear();
  tasks_.clear();
}

std::shared_ptr<ScheduledTaskReference> SimulatedWorker::scheduleFromNow(Task f, duration delta) {
  auto id = std::make_shared<ScheduledTaskReference>();
  scheduled_tasks_[clock_->now() + delta] =  [f, id] {
      if (id->isCancelled()) {
        return;
      }
      f();
    };
  return id;
}

void SimulatedWorker::executeTasks() {
  for (Task f : tasks_) {
    f();
  }
  tasks_.clear();
}

void SimulatedWorker::executePastScheduledTasks() {
  time_point now = clock_->now();
  for (auto iter = scheduled_tasks_.begin(), last_iter = scheduled_tasks_.end(); iter != last_iter; ) {
    if (iter->first <= now) {
      iter->second();
      iter = scheduled_tasks_.erase(iter);
    } else {
      ++iter;
    }
  }
}
