#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>

#include <atomic>
#include <chrono>  // NOLINT
#include <string>
#include <vector>
#include <thread>  // NOLINT
#include <condition_variable>  // NOLINT

using testing::Eq;
using testing::Not;

class SchedulerTest : public ::testing::TestWithParam<int> {
 public:
  virtual void SetUp() {
    counter = 0;
    counter_limit = 0;
    scheduler = std::make_shared<Scheduler>(GetParam());
  }
  virtual void TearDown() {
  }

  std::function<void()> getCounterTask() {
    return [this] {
      if (++counter == counter_limit) {
        counter_incremented.notify_one();
      }
    };
  }

  void scheduleCounterIncrement(int delta_ms) {
    scheduler->scheduleFromNow(getCounterTask(), std::chrono::milliseconds(delta_ms));
  }

  std::cv_status wait_until(int delta_ms) {
    std::unique_lock<std::mutex> lock(mutex);
    return counter_incremented.wait_until(lock, std::chrono::system_clock::now() + std::chrono::milliseconds(delta_ms));
  }

  std::atomic<int> counter;
  int counter_limit;
  std::condition_variable counter_incremented;
  std::mutex mutex;
  std::shared_ptr<Scheduler> scheduler;
};

TEST_P(SchedulerTest, execute_a_simple_task) {
  counter_limit = 1;
  scheduleCounterIncrement(20);

  auto reason = wait_until(100);

  EXPECT_THAT(reason, Not(Eq(std::cv_status::timeout)));
  EXPECT_THAT(counter, Eq(counter_limit));
}

TEST_P(SchedulerTest, execute_multiple_concurrent_tasks) {
  counter_limit = 4;
  scheduleCounterIncrement(20);
  scheduleCounterIncrement(20);
  scheduleCounterIncrement(20);
  scheduleCounterIncrement(20);

  auto reason = wait_until(300);

  EXPECT_THAT(reason, Not(Eq(std::cv_status::timeout)));
  EXPECT_THAT(counter, Eq(counter_limit));
}

TEST_P(SchedulerTest, execute_tasks_on_different_times) {
  counter_limit = 4;
  scheduleCounterIncrement(20);
  scheduleCounterIncrement(40);
  scheduleCounterIncrement(60);
  scheduleCounterIncrement(80);

  auto reason = wait_until(500);

  EXPECT_THAT(reason, Not(Eq(std::cv_status::timeout)));
  EXPECT_THAT(counter, Eq(counter_limit));
}

TEST_P(SchedulerTest, stop_can_drain_scheduled_tasks) {
  counter_limit = 2;
  scheduleCounterIncrement(200);
  scheduleCounterIncrement(200);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  scheduler->stop(true);

  EXPECT_THAT(counter, Eq(counter_limit));
}

TEST_P(SchedulerTest, stop_can_avoid_draining_scheduled_tasks) {
  scheduleCounterIncrement(100);
  scheduleCounterIncrement(100);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  scheduler->stop(false);

  EXPECT_THAT(counter, Eq(0));
}

TEST_P(SchedulerTest, destroy_does_not_drain_tasks) {
  scheduleCounterIncrement(300);
  scheduleCounterIncrement(600);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  scheduler.reset();

  EXPECT_THAT(counter, Eq(0));
}

TEST_P(SchedulerTest, does_not_execute_tasks_when_stopped) {
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  scheduler->stop(false);

  scheduleCounterIncrement(20);
  auto reason = wait_until(50);

  EXPECT_THAT(reason, Eq(std::cv_status::timeout));
  EXPECT_THAT(counter, Eq(0));
}

INSTANTIATE_TEST_CASE_P(
  TestWithMultipleThreads, SchedulerTest, testing::Values(1, 2, 3, 10));
