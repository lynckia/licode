#include "thread/Worker.h"

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <memory>

using erizo::Worker;

Worker::Worker() : service_{}, service_worker_{new asio_worker::element_type(service_)}, closed_{false} {
}

Worker::~Worker() {
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
