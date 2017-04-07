#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stats/StatNode.h>
#include <lib/Clock.h>

#include <memory>
#include <string>
#include <iostream>

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
using erizo::StatNode;
using erizo::StringStat;
using erizo::RateStat;
using erizo::MovingIntervalRateStat;
using erizo::MovingAverageStat;
using erizo::CumulativeStat;
using erizo::SimulatedClock;

class StatNodeTest : public ::testing::Test {
 public:
  StatNodeTest()
      : clock{std::make_shared<SimulatedClock>()} {
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  void advanceClockMs(int time_ms) {
    clock->advanceTime(std::chrono::milliseconds(time_ms));
  }

  StatNode root;
  std::shared_ptr<SimulatedClock> clock;
};

TEST_F(StatNodeTest, shouldWriteAnEmptyJson) {
  EXPECT_THAT(root.toString(), Eq("{}"));
}

TEST_F(StatNodeTest, shouldWriteAJsonWithStringStats) {
  root.insertStat("text", StringStat{"value"});

  EXPECT_THAT(root.toString(), Eq("{\"text\":\"value\"}"));

  root["a"]["b"].insertStat("text2", StringStat{"value2"});

  EXPECT_THAT(root.toString(), Eq("{\"a\":{\"b\":{\"text2\":\"value2\"}},\"text\":\"value\"}"));
}

TEST_F(StatNodeTest, shouldWriteAJsonWithCumulativeStats) {
  root.insertStat("sum", CumulativeStat{30});

  EXPECT_THAT(root.toString(), Eq("{\"sum\":30}"));

  root["sum"]++;

  EXPECT_THAT(root.toString(), Eq("{\"sum\":31}"));
}

TEST_F(StatNodeTest, shouldWriteAJsonWithUpdatedCumulativeStats) {
  root.insertStat("sum", CumulativeStat{30});

  EXPECT_THAT(root.toString(), Eq("{\"sum\":30}"));

  root.insertStat("sum", CumulativeStat{40});

  EXPECT_THAT(root.toString(), Eq("{\"sum\":40}"));
}

TEST_F(StatNodeTest, shouldWriteAJsonWithRateStats) {
  root.insertStat("rate", RateStat{std::chrono::seconds(1), 1. / 8., clock});

  EXPECT_THAT(root.toString(), Eq("{\"rate\":0}"));

  root["rate"] += 10 * 8;
  advanceClockMs(1000);

  EXPECT_THAT(root.toString(), Eq("{\"rate\":10}"));
}

TEST_F(StatNodeTest, rateStatsShouldReturnZeroWhenNotIncreasing) {
  root.insertStat("rate", RateStat{std::chrono::seconds(1), 1. / 8., clock});
  root["rate"] += 10 * 8;
  advanceClockMs(1000);
  EXPECT_THAT(root.toString(), Eq("{\"rate\":10}"));

  advanceClockMs(1000);
  EXPECT_THAT(root.toString(), Eq("{\"rate\":0}"));
}

TEST_F(StatNodeTest, IntervalRateStatCanBeInserted) {
  root.insertStat("rate", MovingIntervalRateStat{std::chrono::milliseconds(100), 5, .1, clock});
  root["rate"] += 100;
  advanceClockMs(100);
  root["rate"] += 100;
  EXPECT_THAT(root.toString(), Eq("{\"rate\":100}"));

  advanceClockMs(1000);
  EXPECT_THAT(root.toString(), Eq("{\"rate\":0}"));
}

