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

constexpr uint32_t kArbitraryIntervalSizeMs = 100;
constexpr uint32_t kArbitraryNumberOfIntervals = 10;
constexpr double kBytesToBitsScale = .1;

class MovingIntervalRateStatTest : public ::testing::Test {
 public:
  MovingIntervalRateStatTest(): clock{std::make_shared<SimulatedClock>()},
    moving_interval_stat{kArbitraryIntervalSizeMs, kArbitraryNumberOfIntervals, kBytesToBitsScale,
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
  for (int i = 0; i < 8; i++) {
    moving_interval_stat+=(100+10*i);
    advanceClockMs(101);
  }
  EXPECT_EQ(moving_interval_stat.value(), 135);
}


TEST_F(MovingIntervalRateStatTest, shouldCalculateAverageForMoreThanWindowSize) {
  for (int i = 0; i < 25; i++) {
    moving_interval_stat+=(100+10*i);
    advanceClockMs(101);
  }
  EXPECT_EQ(moving_interval_stat.value(), 135);
}
