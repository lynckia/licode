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
using erizo::QualityManager;
using erizo::MovingIntervalRateStat;
using erizo::CumulativeStat;
using erizo::Pipeline;
using erizo::duration;

const int kBaseSpatialLayer = 0;
const int kBaseTemporalLayer = 0;
const int kArbitraryEstimatedBitrate = 300000;
constexpr duration kTestRateStatIntervalSize = std::chrono::milliseconds(100);
const uint32_t kRateStatIntervals = 10;

class QualityManagerTest : public erizo::HandlerTest{
 public:
  QualityManagerTest() {}

 protected:
  void setHandler() {
    quality_manager = std::make_shared<QualityManager>(simulated_clock);
    pipeline->addService(quality_manager);
    setInitialStats();
  }

  void setInitialStats() {
      stats->getNode()["total"].insertStat("senderBitrateEstimation", CumulativeStat{kArbitraryEstimatedBitrate});
      generateLayers(2, 3);
  }
  void generateLayers(int spatial_layers, int temporal_layers) {
    for (int spatial_layer = kBaseSpatialLayer; spatial_layer < spatial_layers; spatial_layer++) {
      for (int temporal_layer = kBaseTemporalLayer; temporal_layer < temporal_layers; temporal_layer++) {
        stats->getNode()["qualityLayers"][spatial_layer].insertStat(std::to_string(temporal_layer),
            MovingIntervalRateStat{kTestRateStatIntervalSize, kRateStatIntervals, 1., simulated_clock});
        stats->getNode()["qualityLayers"][spatial_layer][temporal_layer]+= 1000;
//          (kArbitraryEstimatedBitrate + (10 * (spatial_layer*2 + temporal_layer)));
      }
    }
  }

  uint64_t getStatForLayer(int spatial_layer, int temporal_layer) {
    return stats->getNode()["qualityLayers"][spatial_layer][temporal_layer].value();
  }

  void addStatToLayer(int spatial_layer, int temporal_layer, uint64_t value) {
    stats->getNode()["qualityLayers"][spatial_layer][temporal_layer] += value;
  }

  void advanceClock(duration time) {
    simulated_clock->advanceTime(time);
  }

  void advanceClockMs(uint64_t time_ms) {
    simulated_clock->advanceTime(std::chrono::milliseconds(time_ms));
  }
  std::shared_ptr<QualityManager> quality_manager;
};

TEST_F(QualityManagerTest, shouldStartAtBaseLayer) {
  quality_manager->notifyQualityUpdate();
  EXPECT_EQ(quality_manager->getSpatialLayer() , kBaseSpatialLayer);
  EXPECT_EQ(quality_manager->getTemporalLayer() , kBaseTemporalLayer);
}

TEST_F(QualityManagerTest, shouldChangeToHighestLayerBelowEstimatedBitrate) {
  stats->getNode()["total"].insertStat("bitrateCalculated", CumulativeStat{1000000});
  quality_manager->notifyQualityUpdate();
  advanceClockMs(1000);
  generateLayers(2, 3);
  quality_manager->notifyQualityUpdate();
  EXPECT_EQ(quality_manager->getSpatialLayer() , 1);
  EXPECT_EQ(quality_manager->getTemporalLayer() , 2);
}

TEST_F(QualityManagerTest, shouldSwitchLayerWhenEstimatedBitrateDescends) {
}
