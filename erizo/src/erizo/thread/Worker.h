#ifndef ERIZO_SRC_ERIZO_THREAD_WORKER_H_
#define ERIZO_SRC_ERIZO_THREAD_WORKER_H_

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <algorithm>
#include <chrono> // NOLINT
#include <map>
#include <memory>
#include <future>  // NOLINT
#include <vector>

#include "lib/Clock.h"

#include "thread/Scheduler.h"

namespace erizo {

class ScheduledTaskReference {
 public:
  ScheduledTaskReference();
  bool isCancelled();
  void cancel();
 private:
  std::atomic<bool> cancelled;
};

class DurationDistribution {
 public:
  DurationDistribution();
  ~DurationDistribution() {}
  void reset();
  DurationDistribution& operator+=(const DurationDistribution& buf);

 public:
  uint duration_0_10_ms;
  uint duration_10_50_ms;
  uint duration_50_100_ms;
  uint duration_100_1000_ms;
  uint duration_1000_ms;
};

class Worker : public std::enable_shared_from_this<Worker> {
 public:
  typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;
  typedef std::function<void()> Task;
  typedef std::function<bool()> ScheduledTask;

  explicit Worker(std::weak_ptr<Scheduler> scheduler,
                  std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());
  virtual ~Worker();

  virtual void task(Task f);

  virtual void start();
  virtual void start(std::shared_ptr<std::promise<void>> start_promise);
  virtual void close();
  virtual boost::thread::id getId() { return thread_id_; }

  virtual std::shared_ptr<ScheduledTaskReference> scheduleFromNow(Task f, duration delta);
  virtual void unschedule(std::shared_ptr<ScheduledTaskReference> id);

  virtual void scheduleEvery(ScheduledTask f, duration period);

  void resetStats();
  DurationDistribution getDurationDistribution() { return durations_; }

 private:
  void scheduleEvery(ScheduledTask f, duration period, duration next_delay);
  std::function<void()> safeTask(std::function<void(std::shared_ptr<Worker>)> f);
  void addToStats(duration task_duration);

 protected:
  int next_scheduled_ = 0;

 private:
  std::weak_ptr<Scheduler> scheduler_;
  std::shared_ptr<Clock> clock_;
  boost::asio::io_service service_;
  asio_worker service_worker_;
  boost::thread_group group_;
  std::atomic<bool> closed_;
  boost::thread::id thread_id_;
  DurationDistribution durations_;
};

class SimulatedWorker : public Worker {
 public:
  explicit SimulatedWorker(std::shared_ptr<SimulatedClock> the_clock);
  void task(Task f) override;
  void start() override;
  void start(std::shared_ptr<std::promise<void>> start_promise) override;
  void close() override;
  std::shared_ptr<ScheduledTaskReference> scheduleFromNow(Task f, duration delta) override;

  void executeTasks();
  void executePastScheduledTasks();

 private:
  std::shared_ptr<SimulatedClock> clock_;
  std::vector<Task> tasks_;
  std::map<time_point, Task> scheduled_tasks_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_THREAD_WORKER_H_
