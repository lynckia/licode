#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/RtpHeaders.h>
#include <rtp/RtpUtils.h>
#include <rtp/QualityManager.h>
#include <MediaDefinitions.h>
#include <bandwidth/StreamPriorityBWDistributor.h>
#include <bandwidth/BwDistributionConfig.h>
#include <stats/StatNode.h>
#include <Stats.h>

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
using erizo::QualityManager;
using erizo::RtpMap;
using erizo::RtpUtils;
using erizo::MediaStream;
using erizo::Stats;
using erizo::StreamPriorityBWDistributor;
using erizo::StreamPriorityStep;
using erizo::StreamPriorityStrategy;

using std::make_tuple;

struct StreamPriorityConfig {
  uint32_t max_video_bw;
  uint32_t bitrate_sent;
  uint32_t bitrate_for_max_quality_layer;
  std::string priority;
  std::vector<std::vector<uint64_t>> layer_bitrates;
  bool slideshow;
  bool simulcast;
};

typedef std::vector<StreamPriorityConfig> StreamConfigList;
typedef std::vector<StreamPriorityStep> StrategyVector;
typedef std::vector<bool>     EnabledList;
typedef std::vector<uint32_t>  ExpectedList;

class BasicStreamPriorityBWDistributorTest {
 protected:
  void setUpStreams() {
    for (StreamPriorityConfig stream_config : stream_config_list) {
      streams.push_back(addMediaStream(false, stream_config));
    }
  }

  void setUpStrategy() {
    strategy.initWithVector(strategy_vector);
  }

  void setLayerBitrates(std::shared_ptr<erizo::MockMediaStream> stream, const StreamPriorityConfig& config) {
    for (uint8_t spatial_layer = 0; spatial_layer <= config.layer_bitrates.size() - 1; spatial_layer++) {
      for (uint8_t temporal_layer = 0;
          temporal_layer <= config.layer_bitrates[spatial_layer].size() - 1; temporal_layer++) {
        stream->setBitrateForLayer(spatial_layer, temporal_layer, config.layer_bitrates[spatial_layer][temporal_layer]);
      }
    }
  }

  std::shared_ptr<erizo::MockMediaStream> addMediaStream(bool is_publisher, StreamPriorityConfig config) {
    std::string id = std::to_string(index);
    std::string label = std::to_string(index);
    uint32_t video_sink_ssrc = getSsrcFromIndex(index);
    uint32_t audio_sink_ssrc = getSsrcFromIndex(index) + 1;
    uint32_t video_source_ssrc = getSsrcFromIndex(index) + 2;
    uint32_t audio_source_ssrc = getSsrcFromIndex(index) + 3;
    auto media_stream = std::make_shared<erizo::MockMediaStream>(nullptr, nullptr, id, label,
     rtp_maps, is_publisher, true, true, config.priority);
    media_stream->setVideoSinkSSRC(video_sink_ssrc);
    media_stream->setAudioSinkSSRC(audio_sink_ssrc);
    media_stream->setVideoSourceSSRC(video_source_ssrc);
    media_stream->setAudioSourceSSRC(audio_source_ssrc);

    setLayerBitrates(media_stream, config);

    EXPECT_CALL(*media_stream, getMaxVideoBW()).Times(AtLeast(0)).WillRepeatedly(Return(config.max_video_bw));
    EXPECT_CALL(*media_stream,
        getBitrateFromMaxQualityLayer()).Times(AtLeast(0)).WillRepeatedly(Return(config.bitrate_for_max_quality_layer));
    EXPECT_CALL(*media_stream, getVideoBitrate()).Times(AtLeast(0)).WillRepeatedly(Return(config.bitrate_sent));
    EXPECT_CALL(*media_stream, isSlideShowModeEnabled()).Times(AtLeast(0)).WillRepeatedly(Return(config.slideshow));
    EXPECT_CALL(*media_stream, isSimulcast()).Times(AtLeast(0)).WillRepeatedly(Return(config.simulcast));

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
  StrategyVector strategy_vector;
  StreamPriorityStrategy strategy;

  uint32_t bitrate_value;
  EnabledList add_to_remb_list;
  ExpectedList expected_bitrates;
  IceConfig ice_config;
  std::vector<RtpMap> rtp_maps;
  std::vector<ExtMap> ext_maps;
  uint32_t index;
  std::shared_ptr<StreamPriorityBWDistributor> distributor;
  std::queue<std::shared_ptr<DataPacket>> packet_queue;
  std::shared_ptr<Stats> stats;
};

class StreamPriorityBWDistributorTest : public BasicStreamPriorityBWDistributorTest,
  public ::testing::TestWithParam<std::tr1::tuple<StreamConfigList,
                                                  StrategyVector,
                                                  uint32_t,
                                                  EnabledList,
                                                  ExpectedList>> {
 protected:
  virtual void SetUp() {
    index = 0;
    stream_config_list = std::tr1::get<0>(GetParam());
    strategy_vector = std::tr1::get<1>(GetParam());
    bitrate_value = std::tr1::get<2>(GetParam());
    add_to_remb_list = std::tr1::get<3>(GetParam());
    expected_bitrates = std::tr1::get<4>(GetParam());
    stats = std::make_shared<erizo::Stats>();
    setUpStrategy();

    distributor = std::make_shared<erizo::StreamPriorityBWDistributor>(strategy, stats);

    setUpStreams();
  }

  virtual void TearDown() {
    streams.clear();
  }
};

uint32_t HasRembWithValue3(std::tuple<std::shared_ptr<erizo::DataPacket>> arg) {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getREMBBitRate();
}

TEST_P(StreamPriorityBWDistributorTest, forwardRembToStreams_When_TheyExist) {
  uint32_t index = 0;
  for (bool enabled : add_to_remb_list) {
    if (enabled) {
      EXPECT_CALL(*(streams[index]), onTransportData(_, _))
         .With(Args<0>(ResultOf(&HasRembWithValue3, Eq(static_cast<uint32_t>(expected_bitrates[index]))))).Times(1);
    } else {
      EXPECT_CALL(*streams[index], onTransportData(_, _)).Times(0);
    }
    index++;
  }
  onRembReceived();
}

INSTANTIATE_TEST_CASE_P(
  REMB_values, StreamPriorityBWDistributorTest, testing::Values(
    // StreamConfigList {{max_vide_bw, bitrate_sent, bitrate_for_max,quality_layer,  priority, vector<>layer_bitrates,
    // slideshow, simulcast}}, StrategyVector, vector<>enabled_streams, vector<>expected_bitrates
    make_tuple(StreamConfigList{{50, 0, 450, "20",
      std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}},
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1")
      },
      500, EnabledList{1},    ExpectedList{
        50}),
    make_tuple(StreamConfigList{{1000, 0, 450, "20",
      std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}},
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1")
      },
      500, EnabledList{1},    ExpectedList{
        static_cast<uint32_t>(450 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{{1000, 0, 450, "20",
        std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}},
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      1500, EnabledList{1},    ExpectedList{
        static_cast<uint32_t>(450 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{{1000, 0, 450, "0",
        std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}},
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      500, EnabledList{1},    ExpectedList{
        static_cast<uint32_t>(200 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      500, EnabledList{1, 1},    ExpectedList{
        static_cast<uint32_t>(200 * (1 + QualityManager::kIncreaseLayerBitrateThreshold)),
        static_cast<uint32_t>(200 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{
      {450, 0, 450, "10", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, false},
      {1000, 0, 450, "10", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("10", "1"),
      StreamPriorityStep("20", "1")
      },
      1000, EnabledList{1, 1},   ExpectedList{
        450,
        static_cast<uint32_t>(450 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{
      {450, 0, 450, "10", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, false},
      {1000, 0, 450, "10", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("10", "1"),
      StreamPriorityStep("20", "1")
      },
      1000, EnabledList{1, 1},    ExpectedList{
        450,
        static_cast<uint32_t>(450 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      500, EnabledList{1, 1},    ExpectedList{
        static_cast<uint32_t>(200 * (1 + QualityManager::kIncreaseLayerBitrateThreshold)),
        280}),
    make_tuple(StreamConfigList{
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      550, EnabledList{1, 1},    ExpectedList{
        static_cast<uint32_t>(300 * (1 + QualityManager::kIncreaseLayerBitrateThreshold)),
        static_cast<uint32_t>(200 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      1500, EnabledList{1, 1},    ExpectedList{
        static_cast<uint32_t>(200 * (1 + QualityManager::kIncreaseLayerBitrateThreshold)),
        static_cast<uint32_t>(450 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      500, EnabledList{1, 1},    ExpectedList{
        170,
        static_cast<uint32_t>(300 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {300, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "10", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "10", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      2500, EnabledList{1, 1, 1, 1},    ExpectedList{
        static_cast<uint32_t>(450 * (1 + QualityManager::kIncreaseLayerBitrateThreshold)),
        300,
        static_cast<uint32_t>(300 * (1 + QualityManager::kIncreaseLayerBitrateThreshold)),
        static_cast<uint32_t>(300 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      1000, EnabledList{1, 1, 1, 1},    ExpectedList{
        static_cast<uint32_t>(300 * (1 + QualityManager::kIncreaseLayerBitrateThreshold)),
        static_cast<uint32_t>(300 * (1 + QualityManager::kIncreaseLayerBitrateThreshold)),
        170,
        170}),
    make_tuple(StreamConfigList{
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 300 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      100, EnabledList{1, 1, 1, 1},    ExpectedList{50, 50, 0, 0}),
      make_tuple(StreamConfigList{
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "0"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "max"),
      StreamPriorityStep("20", "1"),
      StreamPriorityStep("20", "2")
      },
      1500, EnabledList{1, 1},    ExpectedList{
        1000,
        static_cast<uint32_t>(450 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{
      {1000, 0, 450, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "0", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "max"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      },
      1500, EnabledList{1, 1},    ExpectedList{
        1000,
        static_cast<uint32_t>(200 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))}),
    make_tuple(StreamConfigList{
      {1000, 0, 600, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true},
      {1000, 0, 600, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "max"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("0", "0"),
      },
      1500, EnabledList{1, 1},    ExpectedList{
        750,
        750}),
    make_tuple(StreamConfigList{
      {1000, 0, 600, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true},
      {1000, 0, 600, "20", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "10", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true},
      {1000, 0, 450, "10", std::vector<std::vector<uint64_t>>({ { 100, 150, 200 }, { 250, 300, 450} }), false, true}
      },
      StrategyVector{
      StreamPriorityStep("20", "max"),
      StreamPriorityStep("10", "0"),
      StreamPriorityStep("10", "1"),
      },
      2500, EnabledList{1, 1},    ExpectedList{
        1000,
        1000,
        static_cast<uint32_t>(200 * (1 + QualityManager::kIncreaseLayerBitrateThreshold)),
        static_cast<uint32_t>(200 * (1 + QualityManager::kIncreaseLayerBitrateThreshold))})
));
