#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <lib/TokenBucket.h>
#include <lib/ClockUtils.h>

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
using erizo::MovingAverageStat;
using erizo::SimulatedClock;
using erizo::TokenBucket;

constexpr erizo::duration k50ms = std::chrono::milliseconds(50);
constexpr erizo::duration k100ms = std::chrono::milliseconds(100);
constexpr erizo::duration k200ms = std::chrono::milliseconds(200);
constexpr erizo::duration k1s = std::chrono::seconds(1);

constexpr int kArbitraryNumberOfLoops = 1000000;

class TokenBucketTest : public ::testing::Test {
 public:
  TokenBucketTest() : clock{std::make_shared<SimulatedClock>()}, bucket{clock} {
  }

 protected:
  virtual void SetUp() {
    bucket.reset(10, 10);
  }

  virtual void TearDown() {
  }

  void expectConsumeAfter(int tokens, erizo::duration duration) {
    clock->advanceTime(duration);
    EXPECT_TRUE(bucket.consume(tokens));
  }

  void expectNotConsumeAfter(int tokens, erizo::duration duration) {
    clock->advanceTime(duration);
    EXPECT_FALSE(bucket.consume(tokens));
  }

  std::shared_ptr<SimulatedClock> clock;
  TokenBucket bucket;
};

TEST_F(TokenBucketTest, shouldConsumeTokens_whenRateIsNotReached) {
  for (int i = 0; i < kArbitraryNumberOfLoops; i++) {
    expectConsumeAfter(1, k100ms);
  }

  for (int i = 0; i < kArbitraryNumberOfLoops; i++) {
    expectConsumeAfter(2, k200ms);
  }

  for (int i = 0; i < kArbitraryNumberOfLoops; i++) {
    expectConsumeAfter(10, k1s);
  }
}

TEST_F(TokenBucketTest, shouldNotConsumeTokens_whenRateIsReached) {
  expectConsumeAfter(1, k100ms);
  expectConsumeAfter(10, k1s);
  expectNotConsumeAfter(2, k100ms);
}

TEST_F(TokenBucketTest, shouldConsume_whenReceivingBurst) {
  bucket.reset(10, 30);

  expectConsumeAfter(30, k100ms);
}

TEST_F(TokenBucketTest, shouldConsumeAfterHavingReceivedBurst) {
  bucket.reset(10, 30);
  expectConsumeAfter(30, k200ms);
  expectNotConsumeAfter(1, k50ms);
}

TEST_F(TokenBucketTest, shouldAdaptToRateAgainAfterBursts) {
  bucket.reset(10, 30);
  expectConsumeAfter(30, k100ms);
  expectNotConsumeAfter(1, k50ms);

  for (int i = 0; i < kArbitraryNumberOfLoops; i++) {
    expectConsumeAfter(1, k100ms);
  }
}

TEST_F(TokenBucketTest, shouldNotConsumeMultipleBursts_whenRateIsReached) {
  bucket.reset(10, 30);
  expectConsumeAfter(30, k100ms);
  expectNotConsumeAfter(1, k50ms);

  for (int i = 0; i < kArbitraryNumberOfLoops; i++) {
    expectConsumeAfter(1, k100ms);
  }
  expectNotConsumeAfter(2, k100ms);
}

TEST_F(TokenBucketTest, shouldConsumeMultipleBursts_whenRateIsNotReached) {
  bucket.reset(10, 30);
  expectConsumeAfter(30, k100ms);
  expectConsumeAfter(30, k1s + k1s + k1s);
}
