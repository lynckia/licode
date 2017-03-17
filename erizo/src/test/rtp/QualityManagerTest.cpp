#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <queue>

#include <rtp/QualityManager.h>
#include <Stats.h>
#include <stats/StatNode.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

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

const int kBaseSpatialLayer = 0;
const int kBaseTemporalLayer = 0;
const int kBaseBitrate = 100;
const int kSpatialBitrateFactor = 20;
const int kTemporalBitrateFactor = 10;
constexpr duration kTestRateStatIntervalSize = std::chrono::milliseconds(100);
const uint32_t kRateStatIntervals = 10;
const int kArbitraryNumberOfSpatialLayers = 2;
const int kArbitraryNumberOfTemporalLayers = 3;


class QualityManagerTest : public erizo::HandlerTest{
 public:
  QualityManagerTest(): stats_clock{std::make_shared<SimulatedClock>()} {
  }

 protected:
  void setHandler() {
    quality_manager = std::make_shared<QualityManager>(simulated_clock);
    pipeline->addService(quality_manager);
    generateLayersWithGrowingBitrate(kArbitraryNumberOfSpatialLayers, kArbitraryNumberOfTemporalLayers);
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

  void advanceClock(duration time) {
    simulated_clock->advanceTime(time);
  }

  void advanceClockMs(uint64_t time_ms) {
    simulated_clock->advanceTime(std::chrono::milliseconds(time_ms));
  }

  std::shared_ptr<QualityManager> quality_manager;
  std::shared_ptr<SimulatedClock> stats_clock;
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
