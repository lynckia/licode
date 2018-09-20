#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <queue>

#include <rtp/QualityManager.h>
#include <Stats.h>
#include <stats/StatNode.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>
#include <rtp/LayerDetectorHandler.h>

#include <string>
#include <vector>

#include "../utils/Mocks.h"
#include "../utils/Tools.h"
#include "../utils/Matchers.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::Return;
using erizo::WebRtcConnection;
using erizo::ClockUtils;
using erizo::Clock;
using erizo::SimulatedClock;
using erizo::QualityManager;
using erizo::MovingIntervalRateStat;
using erizo::CumulativeStat;
using erizo::Pipeline;
using erizo::duration;
using erizo::LayerInfoChangedEvent;

const int kBaseSpatialLayer = 0;
const int kBaseTemporalLayer = 0;
const int kBaseBitrate = 100;
const int kSpatialBitrateFactor = 20;
const int kTemporalBitrateFactor = 10;
constexpr duration kTestRateStatIntervalSize = std::chrono::milliseconds(100);
const uint32_t kRateStatIntervals = 10;
const int kArbitraryNumberOfSpatialLayers = 2;
const int kArbitraryNumberOfTemporalLayers = 3;

const int kVideoWidthSL1 = 640;
const int kVideoHeightSL1 = 480;
const int kVideoWidthSL0 = 320;
const int kVideoHeightSL0 = 240;
const int kFrameRateTL2 = 30;
const int kFrameRateTL1 = 15;
const int kFrameRateTL0 = 5;

class QualityManagerBaseTest : public erizo::BaseHandlerTest{
 public:
  QualityManagerBaseTest(): stats_clock{std::make_shared<SimulatedClock>()} {
  }

  void internalSetHandler() {
    quality_manager = std::make_shared<QualityManager>(simulated_clock);
    pipeline->addService(quality_manager);
    generateLayersWithGrowingBitrate(kArbitraryNumberOfSpatialLayers, kArbitraryNumberOfTemporalLayers);
  }

  void afterPipelineSetup() {
    quality_manager->enable();
  }

  void generateLayersWithGrowingBitrate(int spatial_layers, int temporal_layers) {
    for (int spatial_layer = kBaseSpatialLayer; spatial_layer < spatial_layers; spatial_layer++) {
      for (int temporal_layer = kBaseTemporalLayer; temporal_layer < temporal_layers; temporal_layer++) {
        stats->getNode()["qualityLayers"][spatial_layer].insertStat(std::to_string(temporal_layer),
            MovingIntervalRateStat{kTestRateStatIntervalSize, kRateStatIntervals, 1., stats_clock});
        stats->getNode()["qualityLayers"][spatial_layer][temporal_layer]+=
          kBaseBitrate + (kSpatialBitrateFactor * spatial_layer) + (kTemporalBitrateFactor * temporal_layer);
      }
    }
    stats_clock->advanceTime(kTestRateStatIntervalSize - std::chrono::milliseconds(50));
  }

  uint64_t getStatForLayer(int spatial_layer, int temporal_layer) {
    return stats->getNode()["qualityLayers"][spatial_layer][temporal_layer].value();
  }

  void clearLayer(int spatial_layer, int temporal_layer) {
    stats->getNode()["qualityLayers"][spatial_layer].insertStat(std::to_string(temporal_layer),
        MovingIntervalRateStat{kTestRateStatIntervalSize, kRateStatIntervals, 1., stats_clock});
  }

  uint64_t addStatToLayer(int spatial_layer, int temporal_layer, uint64_t value) {
    stats->getNode()["qualityLayers"][spatial_layer][temporal_layer]+=value;
    return stats->getNode()["qualityLayers"][spatial_layer][temporal_layer].value();
  }

  void setSenderBitrateEstimation(uint64_t bitrate) {
    stats->getNode()["total"].insertStat("senderBitrateEstimation", CumulativeStat{bitrate});
  }

  void notifyLayerInfo() {
    std::vector<uint32_t> video_frame_width_list;
    std::vector<uint32_t> video_frame_height_list;
    std::vector<uint64_t> video_frame_rate_list;

    video_frame_width_list.push_back(kVideoWidthSL0);
    video_frame_height_list.push_back(kVideoHeightSL0);

    video_frame_width_list.push_back(kVideoWidthSL1);
    video_frame_height_list.push_back(kVideoHeightSL1);

    video_frame_rate_list.push_back(kFrameRateTL0);
    video_frame_rate_list.push_back(kFrameRateTL1);
    video_frame_rate_list.push_back(kFrameRateTL2);

    auto event = std::make_shared<LayerInfoChangedEvent>(video_frame_width_list,
      video_frame_height_list, video_frame_rate_list);
    quality_manager->notifyEvent(event);
  }

  void setVideoConstraints(int max_width, int max_height, int max_frame_rate) {
    quality_manager->setVideoConstraints(max_width, max_height, max_frame_rate);
  }

  void advanceClock(duration time) {
    simulated_clock->advanceTime(time);
  }

  void advanceClockMs(uint64_t time_ms) {
    simulated_clock->advanceTime(std::chrono::milliseconds(time_ms));
  }

  std::shared_ptr<QualityManager> quality_manager;
  std::shared_ptr<SimulatedClock> stats_clock;
};

class QualityManagerTest : public ::testing::Test, public QualityManagerBaseTest {
 public:
  QualityManagerTest() {}

  void setHandler() override {
    internalSetHandler();
  }

 protected:
  void SetUp() override {
    internalSetUp();
  }

  void TearDown() override {
    internalTearDown();
  }
};

TEST_F(QualityManagerTest, shouldStartAtBaseLayer) {
  EXPECT_EQ(quality_manager->getSpatialLayer() , kBaseSpatialLayer);
  EXPECT_EQ(quality_manager->getTemporalLayer() , kBaseTemporalLayer);
}

TEST_F(QualityManagerTest, shouldChangeToHighestLayerBelowEstimatedBitrate) {
  quality_manager->notifyQualityUpdate();
  const int kArbitrarySpatialLayer = 1;
  const int kArbitraryTemporalLayer = 1;
  advanceClock(erizo::QualityManager::kMinLayerSwitchInterval + std::chrono::milliseconds(1));

  float margin = 1. + QualityManager::kIncreaseLayerBitrateThreshold;

  setSenderBitrateEstimation(getStatForLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer) * margin + 1);
  quality_manager->notifyQualityUpdate();

  EXPECT_EQ(quality_manager->getSpatialLayer() , kArbitrarySpatialLayer);
  EXPECT_EQ(quality_manager->getTemporalLayer() , kArbitraryTemporalLayer);
}

TEST_F(QualityManagerTest, shouldSwitchLayerImmediatelyWhenEstimatedBitrateIsLowerThanCurrentLayer) {
  const int kArbitrarySpatialLayer = 1;
  const int kArbitraryTemporalLayer = 1;
  setSenderBitrateEstimation(getStatForLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer) + 1);

  quality_manager->notifyQualityUpdate();

  quality_manager->setTemporalLayer(kArbitraryTemporalLayer);
  quality_manager->setSpatialLayer(kArbitrarySpatialLayer);

  const int kArbitraryLowerSpatialLayer = 0;
  const int kArbitraryLowerTemporalLayer = 1;

  setSenderBitrateEstimation(getStatForLayer(kArbitraryLowerSpatialLayer, kArbitraryLowerTemporalLayer) + 1);
  quality_manager->notifyQualityUpdate();

  EXPECT_EQ(quality_manager->getSpatialLayer() , kArbitraryLowerSpatialLayer);
  EXPECT_EQ(quality_manager->getTemporalLayer() , kArbitraryLowerTemporalLayer);
}

TEST_F(QualityManagerTest, shouldSwitchLayerImmediatelyWhenCurrentLayerRaisesOverEstimate) {
  const int kArbitrarySpatialLayer = 1;
  const int kArbitraryTemporalLayer = 1;
  setSenderBitrateEstimation(getStatForLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer) + 1);

  quality_manager->notifyQualityUpdate();

  quality_manager->setTemporalLayer(kArbitraryTemporalLayer);
  quality_manager->setSpatialLayer(kArbitrarySpatialLayer);

  const int kExpectedLowerSpatialLayer = 1;
  const int kExpectedLowerTemporalLayer = 0;

  addStatToLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer, 100);
  quality_manager->notifyQualityUpdate();

  EXPECT_EQ(quality_manager->getSpatialLayer() , kExpectedLowerSpatialLayer);
  EXPECT_EQ(quality_manager->getTemporalLayer() , kExpectedLowerTemporalLayer);
}

TEST_F(QualityManagerTest, shouldNotGoToHigherLayerInEarlierThanInterval) {
  const int kArbitrarySpatialLayer = 1;
  const int kArbitraryTemporalLayer = 1;

  quality_manager->notifyQualityUpdate();
  quality_manager->setSpatialLayer(kBaseSpatialLayer);
  quality_manager->setTemporalLayer(kBaseTemporalLayer);

  setSenderBitrateEstimation(getStatForLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer) + 1);
  advanceClock(erizo::QualityManager::kMinLayerSwitchInterval - std::chrono::milliseconds(1));
  quality_manager->notifyQualityUpdate();

  EXPECT_EQ(quality_manager->getSpatialLayer() , kBaseSpatialLayer);
  EXPECT_EQ(quality_manager->getTemporalLayer() , kBaseTemporalLayer);
}

TEST_F(QualityManagerTest, shouldSwitchLayerImmediatelyWhenLayerIsGone) {
  const int kArbitrarySpatialLayer = 1;
  const int kArbitraryTemporalLayer = 1;

  quality_manager->notifyQualityUpdate();
  quality_manager->setSpatialLayer(kArbitrarySpatialLayer);
  quality_manager->setTemporalLayer(kArbitraryTemporalLayer);

  setSenderBitrateEstimation(getStatForLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer) + 1);
  clearLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer);

  quality_manager->notifyQualityUpdate();
  EXPECT_FALSE(quality_manager->getSpatialLayer() == kArbitrarySpatialLayer
      && quality_manager->getTemporalLayer() == kArbitraryTemporalLayer);
}

TEST_F(QualityManagerTest, shouldStickToForcedLayer) {
  const int kArbitrarySpatialLayer = 1;
  const int kArbitraryTemporalLayer = 1;

  quality_manager->notifyQualityUpdate();
  quality_manager->forceLayers(kBaseSpatialLayer, kBaseTemporalLayer);
  quality_manager->setSpatialLayer(kArbitrarySpatialLayer);
  quality_manager->setSpatialLayer(kArbitraryTemporalLayer);

  EXPECT_EQ(quality_manager->getSpatialLayer() , kBaseSpatialLayer);
  EXPECT_EQ(quality_manager->getTemporalLayer() , kBaseTemporalLayer);
}

TEST_F(QualityManagerTest, shouldNotGoBelowMinDesiredSpatialLayerIfAvailable) {
  const int kArbitrarySpatialLayer = 1;
  const int kArbitraryTemporalLayer = 0;

  const int kArbitraryDesiredMinSpatialLayer = 1;
  setSenderBitrateEstimation(getStatForLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer) + 1);

  quality_manager->notifyQualityUpdate();

  quality_manager->setSpatialLayer(kArbitrarySpatialLayer);
  quality_manager->setTemporalLayer(kArbitraryTemporalLayer);
  quality_manager->enableSlideShowBelowSpatialLayer(true, kArbitraryDesiredMinSpatialLayer);
  addStatToLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer, 150);
  advanceClock(erizo::QualityManager::kMinLayerSwitchInterval + std::chrono::milliseconds(1));

  quality_manager->notifyQualityUpdate();
  EXPECT_EQ(quality_manager->getSpatialLayer() , kArbitraryDesiredMinSpatialLayer);
}

TEST_F(QualityManagerTest, shouldGoBelowMinDesiredSpatialLayerIfNotAvailable) {
  const int kArbitrarySpatialLayer = 1;
  const int kArbitraryTemporalLayer = 0;

  const int kArbitraryDesiredMinSpatialLayer = 1;
  setSenderBitrateEstimation(getStatForLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer) + 1);

  quality_manager->notifyQualityUpdate();

  quality_manager->setSpatialLayer(kArbitrarySpatialLayer);
  quality_manager->setTemporalLayer(kArbitraryTemporalLayer);
  quality_manager->enableSlideShowBelowSpatialLayer(true, kArbitraryDesiredMinSpatialLayer);
  clearLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer);

  quality_manager->notifyQualityUpdate();
  EXPECT_FALSE(quality_manager->getSpatialLayer() == kArbitraryDesiredMinSpatialLayer);
}

class QualityManagerConstraintsTest : public QualityManagerBaseTest,
                           public ::testing::TestWithParam<std::tr1::tuple<int, int, int, int, int>> {
 public:
  QualityManagerConstraintsTest() {
    max_video_width = std::tr1::get<0>(GetParam());
    max_video_height = std::tr1::get<1>(GetParam());
    max_frame_rate = std::tr1::get<2>(GetParam());
    expected_spatial_layer = std::tr1::get<3>(GetParam());
    expected_temporal_layer = std::tr1::get<4>(GetParam());
  }

  void setHandler() override {
    internalSetHandler();
  }

 protected:
  void SetUp() override {
    internalSetUp();
  }

  void TearDown() override {
    internalTearDown();
  }
  int max_video_width;
  int max_video_height;
  int max_frame_rate;
  int expected_spatial_layer;
  int expected_temporal_layer;
};

TEST_P(QualityManagerConstraintsTest, shouldChooseCorrectLayersBasedOnConstraints) {
  quality_manager->notifyQualityUpdate();
  const int kArbitrarySpatialLayer = 1;
  const int kArbitraryTemporalLayer = 2;
  advanceClock(erizo::QualityManager::kMinLayerSwitchInterval + std::chrono::milliseconds(1));

  float margin = 1. + QualityManager::kIncreaseLayerBitrateThreshold;

  setSenderBitrateEstimation(getStatForLayer(kArbitrarySpatialLayer, kArbitraryTemporalLayer) * margin + 1);
  notifyLayerInfo();
  quality_manager->notifyQualityUpdate();

  setVideoConstraints(max_video_width,
                      max_video_height,
                      max_frame_rate);

  EXPECT_EQ(quality_manager->getSpatialLayer() , expected_spatial_layer);
  EXPECT_EQ(quality_manager->getTemporalLayer() , expected_temporal_layer);
}

INSTANTIATE_TEST_CASE_P(
  QualityManagerConstraints1, QualityManagerConstraintsTest, testing::Values(
    //                 video width,    video height,    frame rate, expected SL, expected TL
    std::make_tuple(kVideoWidthSL1, kVideoHeightSL1, kFrameRateTL2,           1,           2),
    std::make_tuple(kVideoWidthSL1, kVideoHeightSL1, kFrameRateTL1,           1,           1),
    std::make_tuple(kVideoWidthSL1, kVideoHeightSL1, kFrameRateTL0,           1,           0),
    std::make_tuple(kVideoWidthSL1, kVideoHeightSL1,             0,           1,           0),

    std::make_tuple(kVideoWidthSL0, kVideoHeightSL0, kFrameRateTL2,           0,           2),
    std::make_tuple(kVideoWidthSL0, kVideoHeightSL0, kFrameRateTL1,           0,           1),
    std::make_tuple(kVideoWidthSL0, kVideoHeightSL0, kFrameRateTL0,           0,           0),
    std::make_tuple(kVideoWidthSL0, kVideoHeightSL0,             0,           0,           0),

    std::make_tuple(0,                            0, kFrameRateTL2,           0,           2),
    std::make_tuple(0,                            0, kFrameRateTL1,           0,           1),
    std::make_tuple(0,                            0, kFrameRateTL0,           0,           0),
    std::make_tuple(0,                            0,             0,           0,           0),

    std::make_tuple(kVideoWidthSL1,              -1, kFrameRateTL2,           1,           2),
    std::make_tuple(kVideoWidthSL1,              -1, kFrameRateTL1,           1,           1),
    std::make_tuple(kVideoWidthSL1,              -1, kFrameRateTL0,           1,           0),
    std::make_tuple(kVideoWidthSL1,              -1,             0,           1,           0),

    std::make_tuple(-1,             kVideoHeightSL1, kFrameRateTL2,           1,           2),
    std::make_tuple(-1,             kVideoHeightSL1, kFrameRateTL1,           1,           1),
    std::make_tuple(-1,             kVideoHeightSL1, kFrameRateTL0,           1,           0),
    std::make_tuple(-1,             kVideoHeightSL1,             0,           1,           0),

    std::make_tuple(-1,                          -1, kFrameRateTL2,           1,           2),
    std::make_tuple(-1,                          -1, kFrameRateTL1,           1,           1),
    std::make_tuple(-1,                          -1, kFrameRateTL0,           1,           0),
    std::make_tuple(-1,                          -1,             0,           1,           0)));

INSTANTIATE_TEST_CASE_P(
  QualityManagerConstraints2, QualityManagerConstraintsTest, testing::Values(
    //                 video width,    video height,    frame rate, expected SL, expected TL
    std::make_tuple(kVideoWidthSL0, kVideoHeightSL1, kFrameRateTL2,           1,           2),
    std::make_tuple(0,              kVideoHeightSL1, kFrameRateTL2,           1,           2),
    std::make_tuple(-1,             kVideoHeightSL0, kFrameRateTL2,           0,           2),
    std::make_tuple(kVideoWidthSL1, kVideoHeightSL0, kFrameRateTL2,           1,           2),
    std::make_tuple(kVideoWidthSL1,               0, kFrameRateTL2,           1,           2),
    std::make_tuple(kVideoWidthSL0,               0, kFrameRateTL2,           0,           2),
    std::make_tuple(kVideoWidthSL0,              -1, kFrameRateTL2,           0,           2),

    std::make_tuple(kVideoWidthSL0, kVideoHeightSL1, kFrameRateTL1,           1,           1),
    std::make_tuple(0,              kVideoHeightSL1, kFrameRateTL1,           1,           1),
    std::make_tuple(-1,             kVideoHeightSL0, kFrameRateTL1,           0,           1),
    std::make_tuple(kVideoWidthSL1, kVideoHeightSL0, kFrameRateTL1,           1,           1),
    std::make_tuple(kVideoWidthSL1,               0, kFrameRateTL1,           1,           1),
    std::make_tuple(kVideoWidthSL0,               0, kFrameRateTL1,           0,           1),
    std::make_tuple(kVideoWidthSL0,              -1, kFrameRateTL1,           0,           1),

    std::make_tuple(kVideoWidthSL0, kVideoHeightSL1, kFrameRateTL0,           1,           0),
    std::make_tuple(0,              kVideoHeightSL1, kFrameRateTL0,           1,           0),
    std::make_tuple(-1,             kVideoHeightSL0, kFrameRateTL0,           0,           0),
    std::make_tuple(kVideoWidthSL1, kVideoHeightSL0, kFrameRateTL0,           1,           0),
    std::make_tuple(kVideoWidthSL1,               0, kFrameRateTL0,           1,           0),
    std::make_tuple(kVideoWidthSL0,              -1, kFrameRateTL0,           0,           0),

    std::make_tuple(kVideoWidthSL0, kVideoHeightSL1,             0,           1,           0),
    std::make_tuple(0,              kVideoHeightSL1,             0,           1,           0),
    std::make_tuple(-1,             kVideoHeightSL0,             0,           0,           0),
    std::make_tuple(kVideoWidthSL1, kVideoHeightSL0,             0,           1,           0),
    std::make_tuple(kVideoWidthSL1,               0,             0,           1,           0),
    std::make_tuple(kVideoWidthSL0,              -1,             0,           0,           0),

    std::make_tuple(kVideoWidthSL0, kVideoHeightSL1,            -1,           1,           2),
    std::make_tuple(0,              kVideoHeightSL1,            -1,           1,           2),
    std::make_tuple(-1,             kVideoHeightSL0,            -1,           0,           2),
    std::make_tuple(kVideoWidthSL1, kVideoHeightSL0,            -1,           1,           2),
    std::make_tuple(kVideoWidthSL1,               0,            -1,           1,           2),
    std::make_tuple(kVideoWidthSL0,              -1,            -1,           0,           2),

    std::make_tuple(kVideoWidthSL1+1, kVideoHeightSL1+1, kFrameRateTL2+1,     1,           2),
    std::make_tuple(-1,                          -1,            -1,           1,           2)));
