#include "thread/IOWorker.h"

extern "C" {
#include <r_errors.h>
#include <async_wait.h>
#include <async_timer.h>
}

#include <chrono>  // NOLINT
#include <nice/nice.h>
#include <nice/interfaces.h>

namespace erizo {

DEFINE_LOGGER(IOWorker, "thread.IOWorker");

IOWorker::IOWorker(bool enable_glib_loop) : started_{false}, closed_{false},
  enable_glib_loop_{enable_glib_loop} {
    ELOG_DEBUG("Creating ioWorker, enable_glib_loop %d", enable_glib_loop);
}

IOWorker::~IOWorker() {
  close();
}

void IOWorker::start() {
  auto promise = std::make_shared<std::promise<void>>();
  start(promise);
}

void IOWorker::start(std::shared_ptr<std::promise<void>> start_promise) {
  if (started_.exchange(true)) {
    start_promise->set_value();
    return;
  }

  if (enable_glib_loop_) {
    glib_promise_ = std::unique_ptr<std::promise<void>>(new std::promise<void>());
    glib_context_ = g_main_context_new();
    glib_loop_ = g_main_loop_new(glib_context_, FALSE);
    glib_thread_ = std::unique_ptr<std::thread>(new std::thread(&IOWorker::mainGlibLoop, this));
    glib_promise_->get_future().wait();
  }

  thread_ = std::unique_ptr<std::thread>(new std::thread([this, start_promise] {
    start_promise->set_value();
    while (!closed_) {
      int events;
      struct timeval towait = {0, 100000};
      struct timeval tv;
      int r = NR_async_event_wait2(&events, &towait);
      if (r == R_EOD) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      gettimeofday(&tv, 0);
      NR_async_timer_update_time(&tv);
      std::vector<Task> tasks;
      {
        std::unique_lock<std::mutex> lock(task_mutex_);
        tasks.swap(tasks_);
      }
      for (Task &task : tasks) {
        task();
      }
    }
  }));
}

void IOWorker::task(Task f) {
  std::unique_lock<std::mutex> lock(task_mutex_);
  tasks_.push_back(f);
}

void IOWorker::close() {
  if (!closed_.exchange(true)) {
    if (thread_ != nullptr) {
      thread_->join();
    }
    tasks_.clear();
    if (enable_glib_loop_) {
      g_main_loop_quit(glib_loop_);
      ELOG_DEBUG("message: main loop quit - joining thread");
      glib_thread_->join();
      ELOG_DEBUG("message: thread join finished");
    }
  }
}


GMainContext* IOWorker::getGlibContext() {
  // we return the context directly, the recipient should check the validity of the ptr
  return glib_context_;
}

void IOWorker::mainGlibLoop() {
  // Start gathering candidates and fire event loop
  ELOG_DEBUG("message: starting g_main_loop");
  glib_promise_->set_value();
  if (glib_loop_ == nullptr) {
    return;
  }
  g_main_loop_run(glib_loop_);
  ELOG_DEBUG("message: finished main loop, unreferencing loop and context");
  g_main_loop_unref(glib_loop_);
  glib_loop_ = nullptr;
  g_main_context_unref(glib_context_);
  glib_context_ = nullptr;
  ELOG_DEBUG("message: finished main loop - complete");
}
}  // namespace erizo
