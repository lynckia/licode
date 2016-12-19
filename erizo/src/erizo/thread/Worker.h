#ifndef ERIZO_SRC_ERIZO_THREAD_WORKER_H_
#define ERIZO_SRC_ERIZO_THREAD_WORKER_H_

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <algorithm>
#include <chrono> // NOLINT
#include <memory>
#include <vector>

#include "lib/Clock.h"

#include "thread/Scheduler.h"

namespace erizo {

class Worker : public std::enable_shared_from_this<Worker> {
 public:
  typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;
  typedef std::function<void()> Task;
  typedef std::function<bool()> ScheduledTask;

  explicit Worker(std::weak_ptr<Scheduler> scheduler);
  ~Worker();

  void task(Task f);

  void start();
  void close();

  int scheduleFromNow(Task f, duration delta_ms);
  void unschedule(int uuid);

  void scheduleEvery(ScheduledTask f, duration period);

 private:
  void scheduleEvery(ScheduledTask f, duration period, duration next_delay);
  std::function<void()> safeTask(std::function<void(std::shared_ptr<Worker>)> f);
  bool isCancelled(int uuid);

 private:
  std::weak_ptr<Scheduler> scheduler_;
  boost::asio::io_service service_;
  asio_worker service_worker_;
  boost::thread_group group_;
  std::atomic<bool> closed_;
  int next_scheduled_ = 0;
  std::vector<int> cancelled_;
  mutable std::mutex cancel_mutex_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_THREAD_WORKER_H_
