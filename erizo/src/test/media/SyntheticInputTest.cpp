#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <media/SyntheticInput.h>

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::Return;
using erizo::SyntheticInput;
using erizo::MediaSink;
using erizo::SyntheticInputConfig;
using erizo::SimulatedClock;
using erizo::SimulatedWorker;
using erizo::Worker;

class MockMediaSink : public MediaSink {
 public:
  MOCK_METHOD0(close, void());
  MOCK_METHOD2(deliverAudioDataInternal, void(char*, int));
  MOCK_METHOD2(deliverVideoDataInternal, void(char*, int));

 private:
  int deliverAudioData_(char* buf, int len) override {
    deliverAudioDataInternal(buf, len);
    return 0;
  }
  int deliverVideoData_(char* buf, int len) override {
    deliverVideoDataInternal(buf, len);
    return 0;
  }
};

class SyntheticInputTest : public ::testing::Test {
 public:
  SyntheticInputTest()
      : config{30, 0, 5000},
        clock{std::make_shared<SimulatedClock>()},
        worker{std::make_shared<SimulatedWorker>(clock)},
        input{std::make_shared<SyntheticInput>(config, worker, clock)}
        {
  }

 protected:
  virtual void SetUp() {
    input->setVideoSink(&sink);
    input->setAudioSink(&sink);
    input->start();
  }

  virtual void TearDown() {
    input->close();
  }

  void executeTasksInNextMs(int time) {
    for (int step = 0; step < time + 1; step++) {
      worker->executePastScheduledTasks();
      clock->advanceTime(std::chrono::milliseconds(1));
    }
  }

  MockMediaSink sink;
  SyntheticInputConfig config;
  std::shared_ptr<SimulatedClock> clock;
  std::shared_ptr<SimulatedWorker> worker;
  std::shared_ptr<SyntheticInput> input;
};

TEST_F(SyntheticInputTest, shouldWriteAudioPackets_whenExpected) {
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(1);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).Times(0);

  executeTasksInNextMs(20);
}

TEST_F(SyntheticInputTest, shouldWriteVideoPackets_whenExpected) {
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(2);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).Times(1);

  executeTasksInNextMs(40);
}

TEST_F(SyntheticInputTest, shouldWriteMultiplePackets_after1Secong) {
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(50);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).Times(30);
  executeTasksInNextMs(1000);
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithExpectedPT) {
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithExpectedPT) {
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithExpectedSsrc) {
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithExpectedSsrc) {
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithIncreasingSequenceNumbers) {
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithIncreasingSequenceNumbers) {
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithIncreasingTimestamps) {
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithIncreasingTimestamps) {
}

TEST_F(SyntheticInputTest, shouldWriteFragmentedKeyFrames_whenExpected) {
}

TEST_F(SyntheticInputTest, shouldWriteKeyFrames_whenPLIIsReceived) {
}

TEST_F(SyntheticInputTest, shouldWriteKeyFrames_whenRequestedByControl) {
}

TEST_F(SyntheticInputTest, shouldChangePacketSize_whenRembIsReceived) {
}
