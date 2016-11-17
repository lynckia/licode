#ifndef ERIZO_SRC_ERIZO_THREAD_WORKER_H_
#define ERIZO_SRC_ERIZO_THREAD_WORKER_H_

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <memory>

namespace erizo {

class Worker {
 public:
  typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

  Worker() : service(), service_worker(new asio_worker::element_type(service)) {
    auto worker = [this] { return service.run(); };
    grp.add_thread(new boost::thread(worker));
  }

  template<class F>
  void task(F f) {
    service.post(f);
  }

  ~Worker() {
    service_worker.reset();
    grp.join_all();
    service.stop();
  }

 private:
  boost::asio::io_service service;
  asio_worker service_worker;
  boost::thread_group grp;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_THREAD_WORKER_H_
