#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <media/SyntheticInput.h>

#include <string.h>

#include "rtp/RtpHeaders.h"

#include "../utils/Mocks.h"
#include "../utils/Tools.h"
#include "../utils/Matchers.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::AllArgs;
using ::testing::Not;
using ::testing::Return;
using ::testing::Lt;
using ::testing::InSequence;
using erizo::SyntheticInput;
using erizo::MediaSink;
using erizo::SyntheticInputConfig;
using erizo::SimulatedClock;
using erizo::SimulatedWorker;
using erizo::Worker;

class SyntheticInputTest : public ::testing::Test {
 public:
  SyntheticInputTest()
      : sink{std::make_shared<erizo::MockMediaSink>()},
        config{30000, 0, 5000000},
        clock{std::make_shared<SimulatedClock>()},
        worker{std::make_shared<SimulatedWorker>(clock)},
        input{std::make_shared<SyntheticInput>(config, worker, clock)}
        {
    auto packet = erizo::PacketTools::createRembPacket(30000);
    input->deliverFeedback(packet);
  }

 protected:
  virtual void SetUp() {
    input->setVideoSink(sink);
    input->setAudioSink(sink);
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

  std::shared_ptr<erizo::MockMediaSink> sink;
  SyntheticInputConfig config;
  std::shared_ptr<SimulatedClock> clock;
  std::shared_ptr<SimulatedWorker> worker;
  std::shared_ptr<SyntheticInput> input;
};

ACTION_P(SaveTimestamp, target) {
  *target = (reinterpret_cast<erizo::RtpHeader*>(arg0))->getTimestamp();
}

TEST_F(SyntheticInputTest, shouldWriteAudioPackets_whenExpected) {
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).With(Args<0>(erizo::RtpHasSequenceNumberFromBuffer(0))).Times(1);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).Times(0);

  executeTasksInNextMs(20);
}

TEST_F(SyntheticInputTest, shouldWriteVideoPackets_whenExpected) {
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(4);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).Times(1);

  executeTasksInNextMs(80);
}

TEST_F(SyntheticInputTest, shouldWriteMultiplePackets_after1Secong) {
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(50);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).Times(15);
  executeTasksInNextMs(1000);
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithExpectedPT) {
  size_t opusPayloadType = 111;
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).With(Args<0>(erizo::RtpHasPayloadType(opusPayloadType))).Times(1);

  executeTasksInNextMs(20);
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithExpectedPT) {
  size_t vp8PayloadType = 100;
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(4);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).With(Args<0>(erizo::RtpHasPayloadType(vp8PayloadType))).Times(1);

  executeTasksInNextMs(80);
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithExpectedSsrc) {
  size_t audioSsrc = 44444;
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).With(Args<0>(erizo::RtpHasSsrc(audioSsrc))).Times(1);

  executeTasksInNextMs(20);
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithExpectedSsrc) {
  size_t videoSsrc = 55543;
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(4);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).With(Args<0>(erizo::RtpHasSsrc(videoSsrc))).Times(1);

  executeTasksInNextMs(80);
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithIncreasingSequenceNumbers) {
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).With(Args<0>(erizo::RtpHasSequenceNumberFromBuffer(0))).Times(1);
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).With(Args<0>(erizo::RtpHasSequenceNumberFromBuffer(1))).Times(1);

  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).Times(0);

  executeTasksInNextMs(40);
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithIncreasingSequenceNumbers) {
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(7);

  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).With(Args<0>(erizo::RtpHasSequenceNumberFromBuffer(0))).Times(1);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).With(Args<0>(erizo::RtpHasSequenceNumberFromBuffer(1))).Times(1);

  executeTasksInNextMs(140);
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithIncreasingTimestamps) {
  uint32_t first_timestamp;
  uint32_t second_timestamp;

  {
    InSequence s;
    EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(1).WillOnce(SaveTimestamp(&first_timestamp));
    EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(1).WillOnce(SaveTimestamp(&second_timestamp));
  }

  executeTasksInNextMs(40);

  EXPECT_THAT(first_timestamp, Lt(second_timestamp));
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithIncreasingTimestamps) {
  uint32_t first_timestamp;
  uint32_t second_timestamp;

  {
    InSequence s;
    EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).Times(1).WillOnce(SaveTimestamp(&first_timestamp));
    EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).Times(1).WillOnce(SaveTimestamp(&second_timestamp));
  }

  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(7);

  executeTasksInNextMs(140);

  EXPECT_THAT(first_timestamp, Lt(second_timestamp));
}

TEST_F(SyntheticInputTest, firstVideoFrame_shouldBeAKeyframe) {
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(7);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).With(Args<0>(erizo::IsKeyframeFirstPacket())).Times(1);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).With(Args<0>(Not(erizo::IsKeyframeFirstPacket()))).Times(1);

  executeTasksInNextMs(140);
}

TEST_F(SyntheticInputTest, shouldWriteFragmentedKeyFrames_whenExpected) {
  auto packet = erizo::PacketTools::createRembPacket(350000);
  input->deliverFeedback(packet);
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(4);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).With(Args<0>(erizo::IsKeyframeFirstPacket())).Times(1);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).With(Args<0>(Not(erizo::IsKeyframeFirstPacket()))).Times(2);

  executeTasksInNextMs(80);
}

TEST_F(SyntheticInputTest, shouldWriteKeyFrames_whenPliIsReceived) {
  auto packet = erizo::PacketTools::createPLI();
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(7);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).With(Args<0>(erizo::IsKeyframeFirstPacket())).Times(2);

  executeTasksInNextMs(80);

  input->deliverFeedback(packet);

  executeTasksInNextMs(60);
}

TEST_F(SyntheticInputTest, shouldWriteKeyFrames_whenRequestedByControl) {
  EXPECT_CALL(*sink, deliverAudioDataInternal(_, _)).Times(7);
  EXPECT_CALL(*sink, deliverVideoDataInternal(_, _)).With(Args<0>(erizo::IsKeyframeFirstPacket())).Times(2);

  executeTasksInNextMs(80);

  input->sendPLI();

  executeTasksInNextMs(60);
}
