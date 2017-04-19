#include <gmock/gmock.h>
#include <gtest/gtest.h>


#include <rtp/LayerBitrateCalculationHandler.h>

#include "../utils/Mocks.h"
#include "../utils/Tools.h"
#include "../utils/Matchers.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::Return;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::Pipeline;
using erizo::LayerBitrateCalculationHandler;


class LayerBitrateCalculationHandlerTest : public erizo::HandlerTest {
 public:
  LayerBitrateCalculationHandlerTest() {}

 protected:
  void setHandler() {
    layer_bitrate_handler = std::make_shared<LayerBitrateCalculationHandler>();
    pipeline->addBack(layer_bitrate_handler);
  }

  std::shared_ptr<LayerBitrateCalculationHandler> layer_bitrate_handler;
};

TEST_F(LayerBitrateCalculationHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}
