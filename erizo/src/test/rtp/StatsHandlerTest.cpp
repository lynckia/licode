#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <queue>

#include <rtp/StatsHandler.h>
#include <rtp/RtpHeaders.h>
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
using erizo::DataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::IncomingStatsHandler;
using erizo::OutgoingStatsHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;


class StatsHandlerTest : public erizo::HandlerTest {
 public:
  StatsHandlerTest() {}

 protected:
  void setHandler() {
    incoming_stats_handler = std::make_shared<IncomingStatsHandler>();
    outgoing_stats_handler = std::make_shared<OutgoingStatsHandler>();
    pipeline->addBack(incoming_stats_handler);
    pipeline->addBack(outgoing_stats_handler);
  }

  std::shared_ptr<IncomingStatsHandler> incoming_stats_handler;
  std::shared_ptr<OutgoingStatsHandler> outgoing_stats_handler;
};

TEST_F(StatsHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(StatsHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}
