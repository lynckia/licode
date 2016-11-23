#ifndef ERIZO_SRC_ERIZO_THREAD_TIMER_H_
#define ERIZO_SRC_ERIZO_THREAD_TIMER_H_

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/asio/deadline_timer.hpp>

#include <chrono>  // NOLINT
#include <memory>

namespace erizo {

class Timer;

class TimerListener {
 public:
  virtual void clear(std::weak_ptr<Timer> timer) = 0;
};

class Timer : public std::enable_shared_from_this<Timer> {
 public:
  typedef typename std::chrono::milliseconds duration_type;

  Timer(TimerListener *listener, std::function<void()> func,
        boost::asio::io_service& service,  // NOLINT
        duration_type delay, bool repeat = false)
      : listener_{listener}, func_{func}, delay_{to_posix_duration(delay)}, repeat_{repeat}, timer_{service} {
    schedule();
  }

  ~Timer() {
    cancel();
  }

 private:
  void schedule() {
    timer_.expires_from_now(boost::posix_time::time_duration(delay_));
    timer_.async_wait(boost::bind(&Timer::execute, this, boost::asio::placeholders::error));
  }

  void execute(const boost::system::error_code& error_code) {
    if (error_code == boost::asio::error::operation_aborted) {
      listener_->clear(shared_from_this());
      return;
    }

    func_();

    if (!repeat_) {
      listener_->clear(shared_from_this());
      return;
    }
    schedule();
  }

  void cancel() {
    timer_.cancel();
  }

  static  boost::posix_time::time_duration to_posix_duration(duration_type duration) {
    using std::chrono::duration_cast;

    auto in_sec = duration_cast<std::chrono::seconds>(duration);
    auto in_msec = duration_cast<std::chrono::milliseconds>(duration - in_sec);

    boost::posix_time::time_duration result =
      boost::posix_time::seconds(in_sec.count()) +
      boost::posix_time::milliseconds(in_msec.count());
    return result;
  }

 private:
  TimerListener *listener_;
  std::function<void()> func_;
  boost::posix_time::time_duration delay_;
  bool repeat_;
  boost::asio::deadline_timer timer_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_THREAD_TIMER_H_
