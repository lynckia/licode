#ifndef ERIZO_SRC_ERIZO_THREAD_WORKER_H_
#define ERIZO_SRC_ERIZO_THREAD_WORKER_H_

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <memory>

namespace erizo {

class Worker : public std::enable_shared_from_this<Worker> {
 public:
  typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

  Worker() : service_{}, service_worker_{new asio_worker::element_type(service_)}, closed_{false} {
  }

  template<class F>
  void task(F f) {
    service_.post(f);
  }

  void start() {
    auto this_ptr = shared_from_this();
    auto worker = [this_ptr] {
      if (!this_ptr->closed_) {
        return this_ptr->service_.run();
      }
      return size_t(0);
    };
    group_.add_thread(new boost::thread(worker));
  }

  void close() {
    closed_ = true;
    service_worker_.reset();
    group_.join_all();
    service_.stop();
  }

  ~Worker() {
  }

 private:
  boost::asio::io_service service_;
  asio_worker service_worker_;
  boost::thread_group group_;
  std::atomic<bool> closed_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_THREAD_WORKER_H_
