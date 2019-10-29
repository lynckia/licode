#ifndef ERIZO_SRC_ERIZO_THREAD_SCHEDULER_H_
#define ERIZO_SRC_ERIZO_THREAD_SCHEDULER_H_

#include <boost/function.hpp>
#include <boost/thread.hpp>

#include <chrono>  // NOLINT
#include <map>
#include <mutex>  // NOLINT
#include <condition_variable>  // NOLINT
#include <atomic>

//
// Simple class for background tasks that should be run
// periodically or once "after a while"

class Scheduler {
 public:
  explicit Scheduler(int n_threads_servicing_queue);
  ~Scheduler();

  typedef boost::function<void(void)> Function;

  void schedule(Function f, std::chrono::system_clock::time_point t);

  void scheduleFromNow(Function f, std::chrono::milliseconds delta_ms);

  // void scheduleEvery(Function f, std::chrono::milliseconds delta_ms);

  // Tell any threads running serviceQueue to stop as soon as they're
  // done servicing whatever task they're currently servicing (drain=false)
  // or when there is no work left to be done (drain=true)
  void stop(bool drain = false);

 private:
  void serviceQueue();
  std::chrono::system_clock::time_point getFirstTime();

 private:
  std::multimap<std::chrono::system_clock::time_point, Function> task_queue_;
  std::condition_variable new_task_scheduled_;
  mutable std::mutex new_task_mutex_;
  std::atomic<int> n_threads_servicing_queue_;
  bool stop_requested_;
  bool stop_when_empty_;
  boost::thread_group group_;
};

#endif  // ERIZO_SRC_ERIZO_THREAD_SCHEDULER_H_
