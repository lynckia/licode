#ifndef ERIZO_SRC_ERIZO_THREAD_IOWORKER_H_
#define ERIZO_SRC_ERIZO_THREAD_IOWORKER_H_

#include <atomic>
#include <memory>
#include <thread>  // NOLINT

namespace erizo {

class IOWorker : public std::enable_shared_from_this<IOWorker> {
 public:
  IOWorker();
  ~IOWorker();

  virtual void start();
  virtual void close();

 private:
  std::atomic<bool> started_;
  std::atomic<bool> closed_;
  std::unique_ptr<std::thread> thread_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_THREAD_IOWORKER_H_
