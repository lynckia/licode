#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <media/SyntheticInput.h>

#include <string.h>

#include "rtp/RtpHeaders.h"

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
      : config{30000, 0, 5000000},
        clock{std::make_shared<SimulatedClock>()},
        worker{std::make_shared<SimulatedWorker>(clock)},
        input{std::make_shared<SyntheticInput>(config, worker, clock)}
        {
    auto packet = createRembPacket(30000);
    input->deliverFeedback(packet->data, packet->length);
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

  std::shared_ptr<erizo::dataPacket> createRembPacket(uint32_t bitrate) {
    erizo::RtcpHeader *remb_packet = new erizo::RtcpHeader();
    remb_packet->setPacketType(RTCP_PS_Feedback_PT);
    remb_packet->setBlockCount(RTCP_AFB);
    memcpy(&remb_packet->report.rembPacket.uniqueid, "REMB", 4);

    remb_packet->setSSRC(2);
    remb_packet->setSourceSSRC(1);
    remb_packet->setLength(5);
    remb_packet->setREMBBitRate(bitrate);
    remb_packet->setREMBNumSSRC(1);
    remb_packet->setREMBFeedSSRC(55554);
    int remb_length = (remb_packet->getLength() + 1) * 4;
    char *buf = reinterpret_cast<char*>(remb_packet);
    auto packet = std::make_shared<erizo::dataPacket>(0, buf, remb_length, erizo::OTHER_PACKET);
    delete remb_packet;
    return packet;
  }

  std::shared_ptr<erizo::dataPacket> createPLI() {
    erizo::RtcpHeader *pli = new erizo::RtcpHeader();
    pli->setPacketType(RTCP_PS_Feedback_PT);
    pli->setBlockCount(1);
    pli->setSSRC(55554);
    pli->setSourceSSRC(1);
    pli->setLength(2);
    char *buf = reinterpret_cast<char*>(pli);
    int len = (pli->getLength() + 1) * 4;
    auto packet = std::make_shared<erizo::dataPacket>(0, buf, len, erizo::OTHER_PACKET);
    delete pli;
    return packet;
  }

  MockMediaSink sink;
  SyntheticInputConfig config;
  std::shared_ptr<SimulatedClock> clock;
  std::shared_ptr<SimulatedWorker> worker;
  std::shared_ptr<SyntheticInput> input;
};

MATCHER_P(HasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)))->getSeqNumber() == seq_num;
}

MATCHER_P(HasPayloadType, pt, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)))->getPayloadType() == pt;
}

MATCHER_P(HasSsrc, ssrc, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)))->getSSRC() == ssrc;
}

MATCHER(IsKeyframeFirstPacket, "") {
  erizo::RtpHeader *packet = reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg));
  char* data_pointer;
  char* parsing_pointer;
  data_pointer = std::get<0>(arg) + packet->getHeaderLength();
  parsing_pointer = data_pointer;
  if (*parsing_pointer != 0x10) {
    return false;
  }
  parsing_pointer++;
  if (*parsing_pointer == 0x00) {
    return true;
  }
  return false;
}

ACTION_P(SaveTimestamp, target) {
  *target = (reinterpret_cast<erizo::RtpHeader*>(arg0))->getTimestamp();
}

TEST_F(SyntheticInputTest, shouldWriteAudioPackets_whenExpected) {
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).With(Args<0>(HasSequenceNumber(0))).Times(1);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).Times(0);

  executeTasksInNextMs(20);
}

TEST_F(SyntheticInputTest, shouldWriteVideoPackets_whenExpected) {
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(4);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).Times(1);

  executeTasksInNextMs(80);
}

TEST_F(SyntheticInputTest, shouldWriteMultiplePackets_after1Secong) {
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(50);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).Times(15);
  executeTasksInNextMs(1000);
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithExpectedPT) {
  size_t opusPayloadType = 111;
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).With(Args<0>(HasPayloadType(opusPayloadType))).Times(1);

  executeTasksInNextMs(20);
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithExpectedPT) {
  size_t vp8PayloadType = 100;
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(4);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).With(Args<0>(HasPayloadType(vp8PayloadType))).Times(1);

  executeTasksInNextMs(80);
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithExpectedSsrc) {
  size_t audioSsrc = 44444;
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).With(Args<0>(HasSsrc(audioSsrc))).Times(1);

  executeTasksInNextMs(20);
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithExpectedSsrc) {
  size_t videoSsrc = 55543;
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(4);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).With(Args<0>(HasSsrc(videoSsrc))).Times(1);

  executeTasksInNextMs(80);
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithIncreasingSequenceNumbers) {
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).With(Args<0>(HasSequenceNumber(0))).Times(1);
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).With(Args<0>(HasSequenceNumber(1))).Times(1);

  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).Times(0);

  executeTasksInNextMs(40);
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithIncreasingSequenceNumbers) {
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(7);

  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).With(Args<0>(HasSequenceNumber(0))).Times(1);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).With(Args<0>(HasSequenceNumber(1))).Times(1);

  executeTasksInNextMs(140);
}

TEST_F(SyntheticInputTest, shouldWriteAudioFrames_WithIncreasingTimestamps) {
  uint32_t first_timestamp;
  uint32_t second_timestamp;

  {
    InSequence s;
    EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(1).WillOnce(SaveTimestamp(&first_timestamp));
    EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(1).WillOnce(SaveTimestamp(&second_timestamp));
  }

  executeTasksInNextMs(40);

  EXPECT_THAT(first_timestamp, Lt(second_timestamp));
}

TEST_F(SyntheticInputTest, shouldWriteVideoFrames_WithIncreasingTimestamps) {
  uint32_t first_timestamp;
  uint32_t second_timestamp;

  {
    InSequence s;
    EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).Times(1).WillOnce(SaveTimestamp(&first_timestamp));
    EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).Times(1).WillOnce(SaveTimestamp(&second_timestamp));
  }

  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(7);

  executeTasksInNextMs(140);

  EXPECT_THAT(first_timestamp, Lt(second_timestamp));
}

TEST_F(SyntheticInputTest, firstVideoFrame_shouldBeAKeyframe) {
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(7);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).With(Args<0>(IsKeyframeFirstPacket())).Times(1);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).With(Args<0>(Not(IsKeyframeFirstPacket()))).Times(1);

  executeTasksInNextMs(140);
}

TEST_F(SyntheticInputTest, shouldWriteFragmentedKeyFrames_whenExpected) {
  auto packet = createRembPacket(300000);
  input->deliverFeedback(packet->data, packet->length);
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(4);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).With(Args<0>(IsKeyframeFirstPacket())).Times(1);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).With(Args<0>(Not(IsKeyframeFirstPacket()))).Times(2);

  executeTasksInNextMs(80);
}

TEST_F(SyntheticInputTest, shouldWriteKeyFrames_whenPliIsReceived) {
  auto packet = createPLI();
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(7);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).With(Args<0>(IsKeyframeFirstPacket())).Times(2);

  executeTasksInNextMs(80);

  input->deliverFeedback(packet->data, packet->length);

  executeTasksInNextMs(60);
}

TEST_F(SyntheticInputTest, shouldWriteKeyFrames_whenRequestedByControl) {
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(7);
  EXPECT_CALL(sink, deliverVideoDataInternal(_, _)).With(Args<0>(IsKeyframeFirstPacket())).Times(2);

  executeTasksInNextMs(80);

  input->sendPLI();

  executeTasksInNextMs(60);
}
