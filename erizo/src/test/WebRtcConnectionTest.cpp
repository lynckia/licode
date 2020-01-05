#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/RtpHeaders.h>
#include <rtp/RtpUtils.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

#include <string>
#include <tuple>

#include "utils/Mocks.h"
#include "utils/Matchers.h"

using testing::_;
using testing::Return;
using testing::Eq;
using testing::Args;
using testing::AtLeast;
using testing::ResultOf;
using testing::Invoke;
using erizo::DataPacket;
using erizo::ExtMap;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::RtpUtils;
using erizo::WebRtcConnection;

typedef std::vector<uint32_t> BitrateList;
typedef std::vector<bool>     EnabledList;
typedef std::vector<int32_t>  ExpectedList;

class WebRtcConnectionTest :
  public ::testing::TestWithParam<std::tr1::tuple<BitrateList,
                                                  uint32_t,
                                                  EnabledList,
                                                  ExpectedList>> {
 protected:
  virtual void SetUp() {
    index = 0;
    simulated_clock = std::make_shared<erizo::SimulatedClock>();
    simulated_worker = std::make_shared<erizo::SimulatedWorker>(simulated_clock);
    simulated_worker->start();
    io_worker = std::make_shared<erizo::IOWorker>();
    io_worker->start();
    connection = std::make_shared<WebRtcConnection>(simulated_worker, io_worker,
      "test_connection", ice_config, rtp_maps, ext_maps, true, nullptr);
    transport = std::make_shared<erizo::MockTransport>("test_connection", true, ice_config,
                                                       simulated_worker, io_worker);
    connection->setTransport(transport);
    connection->updateState(TRANSPORT_READY, transport.get());
    connection->init();
    video_bitrate_list = std::tr1::get<0>(GetParam());
    bitrate_value = std::tr1::get<1>(GetParam());
    add_to_remb_list = std::tr1::get<2>(GetParam());
    expected_bitrates = std::tr1::get<3>(GetParam());

    setUpStreams();
  }

  void setUpStreams() {
    for (uint32_t video_bitrate : video_bitrate_list) {
      streams.push_back(addMediaStream(false, video_bitrate));
    }
  }

  std::shared_ptr<erizo::MockMediaStream> addMediaStream(bool is_publisher, uint32_t video_bitrate) {
    std::string id = std::to_string(index);
    std::string label = std::to_string(index);
    uint32_t video_sink_ssrc = getSsrcFromIndex(index);
    uint32_t audio_sink_ssrc = getSsrcFromIndex(index) + 1;
    uint32_t video_source_ssrc = getSsrcFromIndex(index) + 2;
    uint32_t audio_source_ssrc = getSsrcFromIndex(index) + 3;
    auto media_stream = std::make_shared<erizo::MockMediaStream>(simulated_worker, connection, id, label,
      rtp_maps, is_publisher);
    media_stream->setVideoSinkSSRC(video_sink_ssrc);
    media_stream->setAudioSinkSSRC(audio_sink_ssrc);
    media_stream->setVideoSourceSSRC(video_source_ssrc);
    media_stream->setAudioSourceSSRC(audio_source_ssrc);
    connection->addMediaStream(media_stream);
    simulated_worker->executeTasks();
    EXPECT_CALL(*media_stream, isSlideShowModeEnabled()).WillRepeatedly(Return(false));
    EXPECT_CALL(*media_stream, isSimulcast()).WillRepeatedly(Return(false));
    EXPECT_CALL(*media_stream, getVideoBitrate()).WillRepeatedly(Return(video_bitrate));
    EXPECT_CALL(*media_stream, getMaxVideoBW()).WillRepeatedly(Return(video_bitrate));
    EXPECT_CALL(*media_stream, getBitrateFromMaxQualityLayer()).WillRepeatedly(Return(0));
    EXPECT_CALL(*media_stream, getTargetVideoBitrate()).WillRepeatedly(
      Invoke(media_stream.get(), &erizo::MockMediaStream::MediaStream_getTargetVideoBitrate));
    index++;
    return media_stream;
  }

  void onRembReceived(uint32_t bitrate, std::vector<uint32_t> ids) {
    std::transform(ids.begin(), ids.end(), ids.begin(), [](uint32_t id) {
      return id * 1000;
    });
    auto remb = RtpUtils::createREMB(ids[0], ids, bitrate);
    connection->onTransportData(remb, transport.get());
  }

  void onRembReceived() {
    uint32_t index = 0;
    std::vector<uint32_t> ids;
    for (bool enabled : add_to_remb_list) {
      if (enabled) {
        ids.push_back(index);
      }
      index++;
    }
    onRembReceived(bitrate_value, ids);
  }

  uint32_t getIndexFromSsrc(uint32_t ssrc) {
    return ssrc / 1000;
  }

  uint32_t getSsrcFromIndex(uint32_t index) {
    return index * 1000;
  }

  virtual void TearDown() {
    connection->close();
    simulated_worker->executeTasks();
    streams.clear();
  }

  std::vector<std::shared_ptr<erizo::MockMediaStream>> streams;
  BitrateList video_bitrate_list;
  uint32_t bitrate_value;
  EnabledList add_to_remb_list;
  ExpectedList expected_bitrates;
  IceConfig ice_config;
  std::vector<RtpMap> rtp_maps;
  std::vector<ExtMap> ext_maps;
  uint32_t index;
  std::shared_ptr<erizo::MockTransport> transport;
  std::shared_ptr<WebRtcConnection> connection;
  std::shared_ptr<erizo::MockRtcpProcessor> processor;
  std::shared_ptr<erizo::SimulatedClock> simulated_clock;
  std::shared_ptr<erizo::SimulatedWorker> simulated_worker;
  std::shared_ptr<erizo::IOWorker> io_worker;
  std::queue<std::shared_ptr<DataPacket>> packet_queue;
};

uint32_t HasRembWithValue(std::tuple<std::shared_ptr<erizo::DataPacket>> arg) {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getREMBBitRate();
}

TEST_P(WebRtcConnectionTest, forwardRembToStreams_When_StreamTheyExist) {
  uint32_t index = 0;
  for (int32_t expected_bitrate : expected_bitrates) {
    if (expected_bitrate > 0) {
      EXPECT_CALL(*(streams[index]), onTransportData(_, _))
         .With(Args<0>(ResultOf(&HasRembWithValue, Eq(static_cast<uint32_t>(expected_bitrate))))).Times(1);
    } else {
      EXPECT_CALL(*streams[index], onTransportData(_, _)).Times(0);
    }
    index++;
  }

  onRembReceived();
}

INSTANTIATE_TEST_CASE_P(
  REMB_values, WebRtcConnectionTest, testing::Values(
    //                bitrate_list     remb    streams enabled,    expected remb
    std::make_tuple(BitrateList{300},      100, EnabledList{1},    ExpectedList{100}),
    std::make_tuple(BitrateList{300},      600, EnabledList{1},    ExpectedList{300}),

    std::make_tuple(BitrateList{300, 300}, 300, EnabledList{1, 0}, ExpectedList{300, -1}),
    std::make_tuple(BitrateList{300, 300}, 300, EnabledList{0, 1}, ExpectedList{-1, 300}),
    std::make_tuple(BitrateList{300, 300}, 300, EnabledList{1, 1}, ExpectedList{150, 150}),
    std::make_tuple(BitrateList{100, 300}, 300, EnabledList{1, 1}, ExpectedList{100, 200}),
    std::make_tuple(BitrateList{300, 100}, 300, EnabledList{1, 1}, ExpectedList{200, 100}),
    std::make_tuple(BitrateList{100, 100}, 300, EnabledList{1, 1}, ExpectedList{100, 100}),

    std::make_tuple(BitrateList{300, 300, 300}, 300, EnabledList{1, 0, 0}, ExpectedList{300,  -1, -1}),
    std::make_tuple(BitrateList{300, 300, 300}, 300, EnabledList{0, 1, 0}, ExpectedList{ -1, 300, -1}),
    std::make_tuple(BitrateList{300, 300, 300}, 300, EnabledList{1, 1, 0}, ExpectedList{150, 150, -1}),
    std::make_tuple(BitrateList{100, 300, 300}, 300, EnabledList{1, 1, 0}, ExpectedList{100, 200, -1}),
    std::make_tuple(BitrateList{300, 100, 300}, 300, EnabledList{1, 1, 0}, ExpectedList{200, 100, -1}),
    std::make_tuple(BitrateList{100, 100, 300}, 300, EnabledList{1, 1, 0}, ExpectedList{100, 100, -1}),

    std::make_tuple(BitrateList{300, 300, 300}, 300, EnabledList{0, 1, 0}, ExpectedList{-1, 300,  -1}),
    std::make_tuple(BitrateList{300, 300, 300}, 300, EnabledList{0, 0, 1}, ExpectedList{-1,  -1, 300}),
    std::make_tuple(BitrateList{300, 300, 300}, 300, EnabledList{0, 1, 1}, ExpectedList{-1, 150, 150}),
    std::make_tuple(BitrateList{300, 100, 300}, 300, EnabledList{0, 1, 1}, ExpectedList{-1, 100, 200}),
    std::make_tuple(BitrateList{300, 300, 100}, 300, EnabledList{0, 1, 1}, ExpectedList{-1, 200, 100}),
    std::make_tuple(BitrateList{300, 100, 100}, 300, EnabledList{0, 1, 1}, ExpectedList{-1, 100, 100}),

    std::make_tuple(BitrateList{100, 100, 100}, 300, EnabledList{1, 1, 1}, ExpectedList{100, 100, 100}),
    std::make_tuple(BitrateList{100, 100, 100}, 600, EnabledList{1, 1, 1}, ExpectedList{100, 100, 100}),
    std::make_tuple(BitrateList{300, 300, 300}, 600, EnabledList{1, 1, 1}, ExpectedList{200, 200, 200}),
    std::make_tuple(BitrateList{100, 200, 300}, 600, EnabledList{1, 1, 1}, ExpectedList{100, 200, 300}),
    std::make_tuple(BitrateList{300, 200, 100}, 600, EnabledList{1, 1, 1}, ExpectedList{300, 200, 100}),
    std::make_tuple(BitrateList{100, 500, 500}, 800, EnabledList{1, 1, 1}, ExpectedList{100, 350, 350})));
