#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/RtpHeaders.h>
#include <rtp/RtpUtils.h>
#include <MediaDefinitions.h>
#include <bandwidth/TargetVideoBWDistributor.h>

#include <string>
#include <tuple>

#include "../utils/Mocks.h"
#include "../utils/Matchers.h"

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
using erizo::MediaStream;
using erizo::TargetVideoBWDistributor;

using std::make_tuple;

struct StreamConfig {
  uint32_t max_video_bw;
  uint32_t bitrate_sent;
  uint32_t max_quality_bitrate;
  bool slideshow;
  bool simulcast;
};

typedef std::vector<StreamConfig> StreamConfigList;
typedef std::vector<uint32_t> MinList;
typedef std::vector<uint32_t> TargetList;
typedef std::vector<bool>     EnabledList;
typedef std::vector<int32_t>  ExpectedList;

class BasicTargetVideoBWDistributor {
 protected:
  void setUpStreams() {
    for (StreamConfig stream_config : stream_config_list) {
      streams.push_back(addMediaStream(false, stream_config));
    }
  }

  std::shared_ptr<erizo::MockMediaStream> addMediaStream(bool is_publisher, StreamConfig config) {
    std::string id = std::to_string(index);
    std::string label = std::to_string(index);
    uint32_t video_sink_ssrc = getSsrcFromIndex(index);
    uint32_t audio_sink_ssrc = getSsrcFromIndex(index) + 1;
    uint32_t video_source_ssrc = getSsrcFromIndex(index) + 2;
    uint32_t audio_source_ssrc = getSsrcFromIndex(index) + 3;
    auto media_stream = std::make_shared<erizo::MockMediaStream>(nullptr, nullptr, id, label,
     rtp_maps, is_publisher);
    media_stream->setVideoSinkSSRC(video_sink_ssrc);
    media_stream->setAudioSinkSSRC(audio_sink_ssrc);
    media_stream->setVideoSourceSSRC(video_source_ssrc);
    media_stream->setAudioSourceSSRC(audio_source_ssrc);

    EXPECT_CALL(*media_stream, getMaxVideoBW()).Times(AtLeast(0)).WillRepeatedly(Return(config.max_video_bw));
    EXPECT_CALL(*media_stream, getVideoBitrate()).Times(AtLeast(0)).WillRepeatedly(Return(config.bitrate_sent));
    EXPECT_CALL(*media_stream, getBitrateFromMaxQualityLayer()).Times(AtLeast(0))
     .WillRepeatedly(Return(config.max_quality_bitrate));
    EXPECT_CALL(*media_stream, isSlideShowModeEnabled()).Times(AtLeast(0)).WillRepeatedly(Return(config.slideshow));
    EXPECT_CALL(*media_stream, isSimulcast()).Times(AtLeast(0)).WillRepeatedly(Return(config.simulcast));
    EXPECT_CALL(*media_stream, getTargetVideoBitrate()).WillRepeatedly(
      Invoke(media_stream.get(), &erizo::MockMediaStream::MediaStream_getTargetVideoBitrate));

    index++;
    return media_stream;
  }

  void onRembReceived(uint32_t bitrate, std::vector<uint32_t> ids) {
    std::transform(ids.begin(), ids.end(), ids.begin(), [](uint32_t id) {
      return id * 1000;
    });
    std::vector<std::shared_ptr<MediaStream>> enabled_streams;
    for (uint8_t index = 0; index < ids.size(); index++) {
      uint32_t ssrc_feed = ids[index];
      std::for_each(streams.begin(), streams.end(), [ssrc_feed, &enabled_streams]
      (const std::shared_ptr<MediaStream> &stream) {
        if (stream->isSinkSSRC(ssrc_feed)) {
          enabled_streams.push_back(stream);
        }
      });
    }
    distributor->distribute(bitrate, ids[0], enabled_streams, nullptr);
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

  std::vector<std::shared_ptr<erizo::MockMediaStream>> streams;
  StreamConfigList stream_config_list;
  uint32_t bitrate_value;
  EnabledList add_to_remb_list;
  ExpectedList expected_bitrates;
  IceConfig ice_config;
  std::vector<RtpMap> rtp_maps;
  std::vector<ExtMap> ext_maps;
  uint32_t index;
  std::shared_ptr<TargetVideoBWDistributor> distributor;
  std::queue<std::shared_ptr<DataPacket>> packet_queue;
};

class TargetVideoBWDistributorTest : public BasicTargetVideoBWDistributor,
  public ::testing::TestWithParam<std::tr1::tuple<StreamConfigList,
                                                  uint32_t,
                                                  EnabledList,
                                                  ExpectedList>> {
 protected:
  virtual void SetUp() {
    index = 0;
    stream_config_list = std::tr1::get<0>(GetParam());
    bitrate_value = std::tr1::get<1>(GetParam());
    add_to_remb_list = std::tr1::get<2>(GetParam());
    expected_bitrates = std::tr1::get<3>(GetParam());

    distributor = std::make_shared<erizo::TargetVideoBWDistributor>();

    setUpStreams();
  }

  virtual void TearDown() {
    streams.clear();
  }
};

uint32_t HasRembWithValue2(std::tuple<std::shared_ptr<erizo::DataPacket>> arg) {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getREMBBitRate();
}

TEST_P(TargetVideoBWDistributorTest, forwardRembToStreams_When_TheyExist) {
  uint32_t index = 0;
  for (int32_t expected_bitrate : expected_bitrates) {
    if (expected_bitrate > 0) {
      EXPECT_CALL(*(streams[index]), onTransportData(_, _))
         .With(Args<0>(ResultOf(&HasRembWithValue2, Eq(static_cast<uint32_t>(expected_bitrate))))).Times(1);
    } else {
      EXPECT_CALL(*streams[index], onTransportData(_, _)).Times(0);
    }
    index++;
  }

  onRembReceived();
}

INSTANTIATE_TEST_CASE_P(
  REMB_values, TargetVideoBWDistributorTest, testing::Values(
    //                           max  sent qua  slides  sim    remb   enabled               exp

    // Test common cases with 1 stream (maxBW > remb)
    make_tuple(StreamConfigList{{300, 300, 300, false, false}}, 100, EnabledList{1},    ExpectedList{100}),
    make_tuple(StreamConfigList{{300,  40, 300,  true, false}}, 100, EnabledList{1},    ExpectedList{100}),
    make_tuple(StreamConfigList{{300,  40, 300,  true,  true}}, 100, EnabledList{1},    ExpectedList{100}),

    // Test common cases with 1 stream (maxBW < remb)
    make_tuple(StreamConfigList{{300, 300, 300, false, false}}, 400, EnabledList{1},    ExpectedList{300}),
    make_tuple(StreamConfigList{{300,  40, 300,  true, false}}, 400, EnabledList{1},    ExpectedList{300}),
    make_tuple(StreamConfigList{{300,  40, 300,  true,  true}}, 400, EnabledList{1},    ExpectedList{300}),

    // Check other simulcast boundaries
    make_tuple(StreamConfigList{{300,  40, 400,  true,  true}}, 400, EnabledList{1},    ExpectedList{300}),
    make_tuple(StreamConfigList{{400,  40, 300,  true,  true}}, 400, EnabledList{1},    ExpectedList{400}),
    make_tuple(StreamConfigList{{300, 200, 400, false,  true}}, 400, EnabledList{1},    ExpectedList{300}),
    make_tuple(StreamConfigList{{400, 200, 300, false,  true}}, 400, EnabledList{1},    ExpectedList{400}),

    // Check other slideshow boundaries (remb < bitrate)
    make_tuple(StreamConfigList{{300,  40, 300,  true, false}},  10, EnabledList{1},    ExpectedList{10}),
    make_tuple(StreamConfigList{{300,  40, 300,  true,  true}},  10, EnabledList{1},    ExpectedList{10}),

    make_tuple(StreamConfigList{{300, 300, 300, false, false},
                                {300, 300, 300, false, false}},
                                                                200, EnabledList{1, 1}, ExpectedList{100, 100}),
    make_tuple(StreamConfigList{{300, 300, 300, false, false},
                                {300, 300, 300, false, false}},
                                                                200, EnabledList{1, 0}, ExpectedList{200,  -1}),
    make_tuple(StreamConfigList{{300, 300, 300, false, false},
                                {300, 300, 300, false, false}},
                                                                200, EnabledList{0, 1}, ExpectedList{ -1, 200}),

    make_tuple(StreamConfigList{{300,  40, 300, true, false},
                                {300, 300, 300, false, false}},
                                                                200, EnabledList{1, 1}, ExpectedList{100, 160}),
    make_tuple(StreamConfigList{{300, 300, 300, false, false},
                                {300,  40, 300,  true, false}},
                                                                200, EnabledList{1, 1}, ExpectedList{160, 100}),
    make_tuple(StreamConfigList{{300, 300, 300, false, false},
                                {300, 300, 300, false, false}},
                                                                300, EnabledList{1, 0}, ExpectedList{300,  -1}),
    make_tuple(StreamConfigList{{300, 300, 300, false, false},
                                {300, 300, 300, false, false}},
                                                                300, EnabledList{1, 1}, ExpectedList{150, 150}),
    make_tuple(StreamConfigList{{100, 100, 100, false, false},
                                {300, 300, 300, false, false}},
                                                                300, EnabledList{1, 1}, ExpectedList{100, 200}),
    make_tuple(StreamConfigList{{300, 300, 300, false, false},
                                {100, 100, 100, false, false}},
                                                                300, EnabledList{1, 1}, ExpectedList{200, 100}),
    make_tuple(StreamConfigList{{100, 100, 100, false, false},
                                {100, 100, 100, false, false}},
                                                                300, EnabledList{1, 1}, ExpectedList{100, 100}),

    make_tuple(StreamConfigList{{900, 100, 300, false,  true},
                                {900, 100, 300, false,  true}},
                                                                300, EnabledList{1, 0}, ExpectedList{300,  -1}),
    make_tuple(StreamConfigList{{900, 100, 300, false,  true},
                                {900, 100, 300, false,  true}},
                                                                300, EnabledList{1, 1}, ExpectedList{150, 150}),
    make_tuple(StreamConfigList{{900, 100, 100, false,  true},
                                {900, 100, 300, false,  true}},
                                                                300, EnabledList{1, 1}, ExpectedList{150, 200}),
    make_tuple(StreamConfigList{{900, 100, 300, false,  true},
                                {900, 100, 100, false,  true}},
                                                                300, EnabledList{1, 1}, ExpectedList{200, 150}),
    make_tuple(StreamConfigList{{900, 100, 100, false,  true},
                                {900, 100, 100, false,  true}},
                                                                300, EnabledList{1, 1}, ExpectedList{150, 200}),
    make_tuple(StreamConfigList{{200, 100, 900, false,  true},
                                {200, 100, 900, false,  true}},
                                                                300, EnabledList{1, 1}, ExpectedList{150, 150}),
    make_tuple(StreamConfigList{{200, 100, 900, false,  true},
                                {200, 100, 900, false,  true}},
                                                                300, EnabledList{1, 0}, ExpectedList{200,  -1}),
    make_tuple(StreamConfigList{{200, 100, 900, false,  true},
                                {200, 100, 900, false,  true}},
                                                                300, EnabledList{0, 1}, ExpectedList{ -1, 200}),

    make_tuple(StreamConfigList{{300, 300, 300, false,  true},
                                {300, 300, 300, false,  true},
                                {300, 300, 300, false,  true}},
                                                                300, EnabledList{1, 1, 1}, ExpectedList{100, 100, 100}),
    make_tuple(StreamConfigList{{300, 300, 300, false,  true},
                                {300, 300, 300, false,  true},
                                {300, 300, 300, false,  true}},
                                                                300, EnabledList{1, 0, 0}, ExpectedList{300, -1, -1}),
    make_tuple(StreamConfigList{{300, 300, 300, false,  true},
                                {300, 300, 300, false,  true},
                                {300, 300, 300, false,  true}},
                                                                300, EnabledList{0, 1, 0}, ExpectedList{ -1, 300, -1}),
    make_tuple(StreamConfigList{{300, 300, 300, false,  true},
                                {300, 300, 300, false,  true},
                                {300, 300, 300, false,  true}},
                                                                300, EnabledList{1, 1, 0}, ExpectedList{150, 150, -1}),
    make_tuple(StreamConfigList{{100, 100, 100, false,  true},
                                {300, 300, 300, false,  true},
                                {300, 300, 300, false,  true}},
                                                                300, EnabledList{1, 1, 0}, ExpectedList{100, 200, -1}),
    make_tuple(StreamConfigList{{300, 300, 300, false,  true},
                                {100, 100, 100, false,  true},
                                {300, 300, 300, false,  true}},
                                                                300, EnabledList{1, 1, 0}, ExpectedList{200, 100, -1}),
    make_tuple(StreamConfigList{{100, 100, 100, false,  true},
                                {100, 100, 100, false,  true},
                                {300, 300, 300, false,  true}},
                                                                300, EnabledList{1, 1, 0}, ExpectedList{100, 100, -1}),

    make_tuple(StreamConfigList{{900, 300, 300, false,  true},
                                {900, 300, 300, false,  true},
                                {900, 300, 300, false,  true}},
                                                                300, EnabledList{1, 1, 1}, ExpectedList{100, 100, 100}),
    make_tuple(StreamConfigList{{900, 300, 300, false,  true},
                                {900, 300, 300, false,  true},
                                {900, 300, 300, false,  true}},
                                                                300, EnabledList{1, 0, 0}, ExpectedList{300, -1, -1}),
    make_tuple(StreamConfigList{{900, 300, 300, false,  true},
                                {900, 300, 300, false,  true},
                                {900, 300, 300, false,  true}},
                                                                300, EnabledList{0, 1, 0}, ExpectedList{ -1, 300, -1}),
    make_tuple(StreamConfigList{{900, 300, 300, false,  true},
                                {900, 300, 300, false,  true},
                                {900, 300, 300, false,  true}},
                                                                300, EnabledList{1, 1, 0}, ExpectedList{150, 150, -1}),
    make_tuple(StreamConfigList{{100, 100, 100, false,  true},
                                {900, 300, 300, false,  true},
                                {900, 300, 300, false,  true}},
                                                                300, EnabledList{1, 1, 0}, ExpectedList{100, 200, -1}),
    make_tuple(StreamConfigList{{900, 300, 300, false,  true},
                                {100, 100, 100, false,  true},
                                {900, 300, 300, false,  true}},
                                                                300, EnabledList{1, 1, 0}, ExpectedList{200, 100, -1}),
    make_tuple(StreamConfigList{{100, 100, 100, false,  true},
                                {100, 100, 100, false,  true},
                                {900, 300, 300, false,  true}},
                                                                300, EnabledList{1, 1, 0}, ExpectedList{100, 100, -1}),
    make_tuple(StreamConfigList{{900, 300, 300, false,  true},
                                {900, 300, 300, false,  true},
                                {900, 300, 300, false,  true}},
                                                               1000, EnabledList{1, 1, 1}, ExpectedList{333, 350, 400}),
    make_tuple(StreamConfigList{{900, 300, 100, false,  true},
                                {900, 300, 200, false,  true},
                                {900, 300, 300, false,  true}},
                                                               1000, EnabledList{1, 1, 1}, ExpectedList{333, 450, 700}),
    make_tuple(StreamConfigList{{900, 300, 100, false,  true},
                                {900, 300, 500, false,  true},
                                {900, 300, 500, false,  true}},
                                                                800, EnabledList{1, 1, 1}, ExpectedList{266, 350, 350}),
    make_tuple(StreamConfigList{{900, 300, 300, false,  true},
                                {900, 300, 300, false,  true},
                                {900, 300, 300, false,  true}},
                                                                600, EnabledList{1, 1, 1}, ExpectedList{200, 200, 200}),
    make_tuple(StreamConfigList{{900,  40, 300, true,  true},
                                {900, 300, 300, false, true},
                                {900, 300, 300, false, true}},
                                                                500, EnabledList{1, 1, 1}, ExpectedList{166, 230, 230}),
    make_tuple(StreamConfigList{{900,  40, 300, true,  true},
                                {100, 100, 100, false, true},
                                {100, 100, 100, false, true}},
                                                              500, EnabledList{1, 1, 1}, ExpectedList{166, 100, 100})));

class MultipleTargetVideoBWDistributorTest : public BasicTargetVideoBWDistributor,
                                            public ::testing::Test {
 protected:
  virtual void SetUp() {
    index = 0;
    stream_config_list = StreamConfigList{};
    bitrate_value = 2000;
    add_to_remb_list = EnabledList{};
    expected_bitrates = ExpectedList{};
    for (int index = 0; index < 200; index++) {
      stream_config_list.push_back({900, 100, 100, true, true});
      add_to_remb_list.push_back(1);
      expected_bitrates.push_back(10);
    }
    distributor = std::make_shared<erizo::TargetVideoBWDistributor>();

    setUpStreams();
  }

  virtual void TearDown() {
    streams.clear();
  }
};

TEST_F(MultipleTargetVideoBWDistributorTest, forwardRembToStreams_When_TheyExist) {
  uint32_t index = 0;
  for (int32_t expected_bitrate : expected_bitrates) {
    if (expected_bitrate > 0) {
      EXPECT_CALL(*(streams[index]), onTransportData(_, _))
        .With(Args<0>(erizo::RembHasBitrateValue(static_cast<uint32_t>(expected_bitrate)))).Times(1);
    } else {
      EXPECT_CALL(*streams[index], onTransportData(_, _)).Times(0);
    }
    index++;
  }

  onRembReceived();
}
