#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stats/StatNode.h>
#include <lib/Clock.h>

#include "../utils/Mocks.h"
#include "../utils/Tools.h"
#include "../utils/Matchers.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::AllArgs;
using ::testing::Not;
using ::testing::Return;
using ::testing::Eq;
using erizo::MovingIntervalRateStat;
using erizo::SimulatedClock;
using erizo::duration;
using erizo::ClockUtils;

constexpr duration kArbitraryIntervalSizeMs = std::chrono::milliseconds(100);
constexpr uint32_t kArbitraryNumberOfIntervals = 5;
constexpr double kArbitraryScale = .1;

class MovingIntervalRateStatTest : public ::testing::Test {
 public:
  MovingIntervalRateStatTest(): clock{std::make_shared<SimulatedClock>()},
    moving_interval_stat{kArbitraryIntervalSizeMs, kArbitraryNumberOfIntervals, kArbitraryScale,
    clock} {
  }

 protected:
  void advanceClockMs(int time_ms) {
    clock->advanceTime(std::chrono::milliseconds(time_ms));
  }
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  std::shared_ptr<SimulatedClock> clock;
  MovingIntervalRateStat moving_interval_stat;
};

TEST_F(MovingIntervalRateStatTest, shouldCalculateAverageForLessThanWindowSize) {
  for (int i = 0; i < 3; i++) {
    moving_interval_stat+=(100+10*i);
    advanceClockMs(100);
  }
  EXPECT_EQ(moving_interval_stat.value(), (100 + 110 + 120)/3);
}


TEST_F(MovingIntervalRateStatTest, shouldReturnAverageOfWindowSize) {
  const int kTotalSamples = 10;
  for (int i = 0; i < kTotalSamples; i++) {
    moving_interval_stat+=(100+10*i);
    advanceClockMs(101);
  }
  uint64_t mean = 0;
  for (int i = kTotalSamples - kArbitraryNumberOfIntervals; i < kTotalSamples; i++) {
      mean = mean + 100 + i * 10;
  }
  mean = mean/ kArbitraryNumberOfIntervals;

  EXPECT_EQ(moving_interval_stat.value(), mean);
}

TEST_F(MovingIntervalRateStatTest, shouldCalculateAverageForAGivenInterval) {
  const uint64_t kArbitraryIntervalToCalculate = 200;
  const int kTotalSamples = 10;

  for (int i = 0; i < kTotalSamples; i++) {
    moving_interval_stat+=(100+10*i);
    advanceClockMs(101);
  }
  uint64_t mean = 0;
  const int kNumberOfIntervals = kArbitraryIntervalToCalculate / ClockUtils::durationToMs(kArbitraryIntervalSizeMs);
  for (int i = kTotalSamples - kNumberOfIntervals; i < kTotalSamples; i++) {
      mean = mean + 100 + i * 10;
  }
  mean = mean/kNumberOfIntervals;
  EXPECT_EQ(moving_interval_stat.value(kArbitraryIntervalToCalculate), mean);
}

TEST_F(MovingIntervalRateStatTest, shouldIntroduceZerosInTimeGapsWhenAdding) {
  const int kGapSizeInIntervals = 3;
  moving_interval_stat += 100;
  moving_interval_stat += 100;
  advanceClockMs(kGapSizeInIntervals * 100);
  moving_interval_stat += 50;
  advanceClockMs(100);

  uint32_t mean = (200 + 50) / 4;

  EXPECT_EQ(moving_interval_stat.value(), mean);
}

TEST_F(MovingIntervalRateStatTest, shouldIntroduceZerosInTimeGapsWhenCalculating) {
  const int kGapSizeInIntervals = 2;
  moving_interval_stat += 100;
  advanceClockMs(110);
  moving_interval_stat += 100;

  advanceClockMs(kGapSizeInIntervals * 110);

  uint32_t mean = (200) / 4;
  EXPECT_EQ(moving_interval_stat.value(), mean);
}

TEST_F(MovingIntervalRateStatTest, shouldCleanWindowIfValueIsOverWindow) {
  const int kGapSizeInIntervals = 10;
  moving_interval_stat += 100;
  moving_interval_stat += 100;
  advanceClockMs(kGapSizeInIntervals * 100);
  moving_interval_stat += 50;
  uint32_t mean = (50) / 5;

  EXPECT_EQ(moving_interval_stat.value(0), mean);
}

TEST_F(MovingIntervalRateStatTest, shouldReturn0IfTimeIsOutOfWindow) {
  const int kGapSizeInIntervals = 25;
  moving_interval_stat += 100;
  moving_interval_stat += 100;
  advanceClockMs(kGapSizeInIntervals * 100);

  EXPECT_EQ(moving_interval_stat.value(0), 0);
}
