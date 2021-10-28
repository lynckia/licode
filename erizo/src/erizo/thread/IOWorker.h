#ifndef ERIZO_SRC_ERIZO_THREAD_IOWORKER_H_
#define ERIZO_SRC_ERIZO_THREAD_IOWORKER_H_

#include <atomic>
#include <memory>
#include <mutex>  // NOLINT
#include <future>  // NOLINT
#include <thread>  // NOLINT
#include <vector>

#include "./logger.h"
#include "lib/LibNiceInterface.h"


namespace erizo {

class IOWorker : public std::enable_shared_from_this<IOWorker> {
  DECLARE_LOGGER();

 public:
  typedef std::function<void()> Task;
  explicit IOWorker(bool enable_glib_loop = false);
  virtual ~IOWorker();

  virtual void start();
  virtual void start(std::shared_ptr<std::promise<void>> start_promise);
  virtual void close();

  virtual void task(Task f);

  GMainContext* getGlibContext();

 private:
  void mainGlibLoop();

 private:
  std::atomic<bool> started_;
  std::atomic<bool> closed_;
  bool enable_glib_loop_;
  std::unique_ptr<std::thread> thread_;
  std::unique_ptr<std::thread> glib_thread_;
  std::vector<Task> tasks_;
  std::unique_ptr<std::promise<void>> glib_promise_;
  mutable std::mutex task_mutex_;
  GMainContext* glib_context_;
  GMainLoop* glib_loop_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_THREAD_IOWORKER_H_
