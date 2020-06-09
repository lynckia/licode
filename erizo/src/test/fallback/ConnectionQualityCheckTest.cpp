#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/RtpHeaders.h>
#include <rtp/RtpUtils.h>
#include <MediaDefinitions.h>
#include <bandwidth/ConnectionQualityCheck.h>
#include "rtp/RtcpFeedbackGenerationHandler.h"

#include <string>
#include <tuple>

#include "../utils/Mocks.h"
#include "../utils/Matchers.h"

using testing::_;
using testing::Return;
using testing::Eq;
using testing::Args;
using testing::AtLeast;
using erizo::DataPacket;
using erizo::ExtMap;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::RtpUtils;
using erizo::MediaStream;
using erizo::ConnectionQualityCheck;
using erizo::ConnectionQualityLevel;

using std::make_tuple;

typedef std::vector<int16_t> FractionLostList;

class BasicConnectionQualityCheckTest {
 protected:
  void setUpStreams() {
    simulated_clock = std::make_shared<erizo::SimulatedClock>();
    simulated_worker = std::make_shared<erizo::SimulatedWorker>(simulated_clock);
    simulated_worker->start();
    for (uint32_t index = 0; index < fraction_lost_list.size(); index++) {
      auto mock_stream = addMediaStream(fraction_lost_list[index] >= 0, index);
      auto erizo_stream = std::static_pointer_cast<erizo::MediaStream>(mock_stream);
      streams.push_back(mock_stream);
      erizo_streams.push_back(erizo_stream);
    }
    for (uint32_t index = 0; index < publisher_fraction_lost_list.size(); index++) {
      auto mock_stream = streams[index];
      uint8_t fraction_lost = publisher_fraction_lost_list[index] * 256 / 100;
      EXPECT_CALL(*mock_stream, getPublisherInfo())
        .WillRepeatedly(Return(erizo::PublisherInfo{fraction_lost, fraction_lost}));
    }
  }

  std::shared_ptr<erizo::MockMediaStream> addMediaStream(bool is_muted, uint32_t index) {
    std::string id = std::to_string(index);
    std::string label = std::to_string(index);
    uint32_t video_sink_ssrc = getSsrcFromIndex(index);
    uint32_t audio_sink_ssrc = getSsrcFromIndex(index) + 1;
    uint32_t video_source_ssrc = getSsrcFromIndex(index) + 2;
    uint32_t audio_source_ssrc = getSsrcFromIndex(index) + 3;
    auto media_stream = std::make_shared<erizo::MockMediaStream>(simulated_worker, nullptr, id, label,
     rtp_maps, false);
    media_stream->setVideoSinkSSRC(video_sink_ssrc);
    media_stream->setAudioSinkSSRC(audio_sink_ssrc);
    media_stream->setVideoSourceSSRC(video_source_ssrc);
    media_stream->setAudioSourceSSRC(audio_source_ssrc);

    return media_stream;
  }

  void onFeedbackReceived() {
    for (uint8_t i = 0; i < ConnectionQualityCheck::kNumberOfPacketsPerStream * 10; i++) {
      uint32_t index = 0;
      for (int16_t fraction_lost : fraction_lost_list) {
        if (fraction_lost >= 0) {
          uint8_t f_lost = fraction_lost  * 256 / 100;
          auto packet = RtpUtils::createReceiverReport(getSsrcFromIndex(index) + 1, f_lost);
          connection_quality_check->onFeedback(packet, erizo_streams);
          auto packet2 = RtpUtils::createReceiverReport(getSsrcFromIndex(index), f_lost);
          connection_quality_check->onFeedback(packet2, erizo_streams);
        }
        index++;
      }
    }
    simulated_worker->executeTasks();
  }

  uint32_t getIndexFromSsrc(uint32_t ssrc) {
    return ssrc / 1000;
  }

  uint32_t getSsrcFromIndex(uint32_t index) {
    return index * 1000;
  }

  std::shared_ptr<erizo::SimulatedClock> simulated_clock;
  std::shared_ptr<erizo::SimulatedWorker> simulated_worker;
  std::vector<std::shared_ptr<erizo::MockMediaStream>> streams;
  std::vector<std::shared_ptr<erizo::MediaStream>> erizo_streams;
  FractionLostList fraction_lost_list;
  FractionLostList publisher_fraction_lost_list;
  ConnectionQualityLevel expected_quality_level;
  std::vector<RtpMap> rtp_maps;
  std::vector<ExtMap> ext_maps;
  std::shared_ptr<ConnectionQualityCheck> connection_quality_check;
};

class ConnectionQualityCheckTest : public BasicConnectionQualityCheckTest,
  public ::testing::TestWithParam<std::tr1::tuple<FractionLostList, FractionLostList,
                                                  ConnectionQualityLevel>> {
 protected:
  virtual void SetUp() {
    fraction_lost_list = std::tr1::get<0>(GetParam());
    publisher_fraction_lost_list = std::tr1::get<1>(GetParam());
    expected_quality_level = std::tr1::get<2>(GetParam());

    connection_quality_check = std::make_shared<erizo::ConnectionQualityCheck>();

    setUpStreams();
  }

  virtual void TearDown() {
    streams.clear();
  }
};

TEST_P(ConnectionQualityCheckTest, notifyConnectionQualityEvent_When_ItChanges) {
  std::for_each(streams.begin(), streams.end(), [this](const std::shared_ptr<erizo::MockMediaStream> &stream) {
    if (expected_quality_level != ConnectionQualityLevel::GOOD) {
      EXPECT_CALL(*stream, deliverEventInternal(_)).
        With(Args<0>(erizo::ConnectionQualityEventWithLevel(expected_quality_level))).Times(1);
    } else {
      EXPECT_CALL(*stream, deliverEventInternal(_)).Times(0);
    }
  });


  onFeedbackReceived();
}

INSTANTIATE_TEST_CASE_P(
  FractionLost_Values, ConnectionQualityCheckTest, testing::Values(
    //                          fraction_losts (%)  publisher_fraction_lost (%)  expected_quality_level
    make_tuple(FractionLostList{ 99, 99, 99, 99}, FractionLostList{0, 0, 0, 0},  ConnectionQualityLevel::HIGH_LOSSES),
    make_tuple(FractionLostList{ -1, 99, 99, 99}, FractionLostList{0, 0, 0, 0},  ConnectionQualityLevel::HIGH_LOSSES),
    make_tuple(FractionLostList{ 25, 25, 25, 25}, FractionLostList{0, 0, 0, 0},  ConnectionQualityLevel::HIGH_LOSSES),
    make_tuple(FractionLostList{  0,  0, 41, 41}, FractionLostList{0, 0, 0, 0},  ConnectionQualityLevel::HIGH_LOSSES),
    make_tuple(FractionLostList{ 19, 19, 19, 19}, FractionLostList{0, 0, 0, 0},  ConnectionQualityLevel::LOW_LOSSES),
    make_tuple(FractionLostList{ 10, 10, 10, 10}, FractionLostList{0, 0, 0, 0},  ConnectionQualityLevel::LOW_LOSSES),
    make_tuple(FractionLostList{  0,  0, 20, 20}, FractionLostList{0, 0, 0, 0},  ConnectionQualityLevel::LOW_LOSSES),
    make_tuple(FractionLostList{  4,  4,  4,  4}, FractionLostList{0, 0, 0, 0},  ConnectionQualityLevel::GOOD),
    make_tuple(FractionLostList{  0,  0,  0,  0}, FractionLostList{0, 0, 0, 0},  ConnectionQualityLevel::GOOD),
    make_tuple(FractionLostList{ -1,  0,  0,  0}, FractionLostList{0, 0, 0, 0},  ConnectionQualityLevel::GOOD),
    make_tuple(FractionLostList{ 99, 99, 99, 99}, FractionLostList{99, 99, 99, 99}, ConnectionQualityLevel::GOOD),
    make_tuple(FractionLostList{ 99, 99, 99, 99}, FractionLostList{80, 80, 80, 80}, ConnectionQualityLevel::LOW_LOSSES),
    make_tuple(FractionLostList{  0,  0, 41, 41}, FractionLostList{0, 0, 21, 21},  ConnectionQualityLevel::LOW_LOSSES),
    make_tuple(FractionLostList{ 30, 30, 30, 30}, FractionLostList{5, 5, 5, 5}, ConnectionQualityLevel::HIGH_LOSSES),
    make_tuple(FractionLostList{ 45, 45, 45, 45}, FractionLostList{26, 26, 26, 26}, ConnectionQualityLevel::LOW_LOSSES),
    make_tuple(FractionLostList{ 45, 45, 45, 45}, FractionLostList{41, 41, 41, 41}, ConnectionQualityLevel::GOOD)
));
