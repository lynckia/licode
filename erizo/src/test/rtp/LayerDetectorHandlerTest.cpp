#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>
#include <rtp/LayerDetectorHandler.h>
#include <rtp/PacketCodecParser.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

#include <queue>
#include <string>
#include <tuple>
#include <vector>

#include "../utils/Mocks.h"
#include "../utils/Tools.h"
#include "../utils/Matchers.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::Return;
using ::testing::AllOf;
using erizo::DataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::RtpHeader;
using erizo::LayerDetectorHandler;
using erizo::PacketCodecParser;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;

constexpr uint32_t kArbitrarySsrc1 = 1001;
constexpr uint32_t kArbitrarySsrc2 = 1002;

class LayerDetectorHandlerVp8Test : public erizo::BaseHandlerTest,
                                    public ::testing::TestWithParam<std::tr1::tuple<int, int, int, bool, int, bool>> {
 public:
  LayerDetectorHandlerVp8Test() {
    ssrc = std::tr1::get<0>(GetParam());
    tid = std::tr1::get<1>(GetParam());
    spatial_layer_id = std::tr1::get<2>(GetParam());
    spatial_layer_supported = std::tr1::get<3>(GetParam());
    temporal_layer_id = std::tr1::get<4>(GetParam());
    temporal_layer_supported = std::tr1::get<5>(GetParam());
  }

 protected:
  void createVP8Packet(uint32_t ssrc, int temporal_id, bool is_keyframe) {
    packet = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, false, false);
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    rtp_header->setSSRC(ssrc);

    unsigned char* data = reinterpret_cast<unsigned char*>(packet->data + rtp_header->getHeaderLength());
    *data |= 0x80;  // set extension bit
    data++;
    *data |= 0x20;  // set hasTID bit
    data++;
    *data |= ((temporal_id & 0x3) << 6);  // set tID bit
    data++;
    *data = is_keyframe ? 0x00: 0x01;
  }

  void setHandler() override {
    std::vector<RtpMap>& payloads = media_stream->getRemoteSdpInfo()->getPayloadInfos();
    payloads.push_back({96, "VP8"});
    payloads.push_back({98, "VP9"});
    codec_parser_handler = std::make_shared<PacketCodecParser>();
    layer_detector_handler = std::make_shared<LayerDetectorHandler>();
    pipeline->addBack(codec_parser_handler);
    pipeline->addBack(layer_detector_handler);

    media_stream->setVideoSourceSSRCList({kArbitrarySsrc1, kArbitrarySsrc2});
    createVP8Packet(ssrc, tid, false);
  }

  void SetUp() override {
    internalSetUp();
  }

  void TearDown() override {
    pipeline->read(packet);
    internalTearDown();
  }

  std::shared_ptr<PacketCodecParser> codec_parser_handler;
  std::shared_ptr<LayerDetectorHandler> layer_detector_handler;
  std::shared_ptr<DataPacket> packet;
  int ssrc;
  int tid;
  int spatial_layer_id;
  bool spatial_layer_supported;
  int temporal_layer_id;
  bool temporal_layer_supported;
};

TEST_P(LayerDetectorHandlerVp8Test, basicBehaviourShouldReadPackets) {
  EXPECT_CALL(*reader.get(), read(_, _)).
    With(AllOf(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber)),
               Args<1>(erizo::PacketIsNotKeyframe()))).Times(1);
}

TEST_P(LayerDetectorHandlerVp8Test, shouldDetectKeyFrameFirstPackets) {
  createVP8Packet(ssrc, tid, true);
  EXPECT_CALL(*reader.get(), read(_, _)).
    With(AllOf(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber)),
               Args<1>(erizo::PacketIsKeyframe()))).Times(1);
}

TEST_P(LayerDetectorHandlerVp8Test, shouldGetDesiredSpatialLayerFromVp8) {
  if (spatial_layer_supported) {
    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::PacketBelongsToSpatialLayer(spatial_layer_id))).Times(1);
  } else {
    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::PacketDoesNotBelongToSpatialLayer(spatial_layer_id))).Times(1);
  }
}

TEST_P(LayerDetectorHandlerVp8Test, shouldGetBasicTemporalLayerFromVp8) {
  if (temporal_layer_supported) {
    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::PacketBelongsToTemporalLayer(temporal_layer_id))).Times(1);
  } else {
    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::PacketDoesNotBelongToTemporalLayer(temporal_layer_id))).Times(1);
  }
}

INSTANTIATE_TEST_CASE_P(
  VP8_layers, LayerDetectorHandlerVp8Test, testing::Values(
    //                         ssrc  tid  spatial_layer_id  supported temporal_layer_id supported
    std::make_tuple(kArbitrarySsrc1,   0,                0,      true,                0,     true),
    std::make_tuple(kArbitrarySsrc1,   0,                1,     false,                0,     true),
    std::make_tuple(kArbitrarySsrc1,   0,                0,      true,                1,     true),
    std::make_tuple(kArbitrarySsrc1,   0,                0,      true,                2,     true),

    std::make_tuple(kArbitrarySsrc1,   2,                0,      true,                0,    false),
    std::make_tuple(kArbitrarySsrc1,   2,                0,      true,                1,    false),
    std::make_tuple(kArbitrarySsrc1,   2,                0,      true,                2,     true),

    std::make_tuple(kArbitrarySsrc1,   1,                0,      true,                0,    false),
    std::make_tuple(kArbitrarySsrc1,   1,                0,      true,                1,     true),
    std::make_tuple(kArbitrarySsrc1,   1,                0,      true,                2,     true),

    std::make_tuple(kArbitrarySsrc2,   0,                1,      true,                0,     true),
    std::make_tuple(kArbitrarySsrc2,   0,                0,     false,                0,     true),
    std::make_tuple(kArbitrarySsrc2,   0,                1,      true,                1,     true),
    std::make_tuple(kArbitrarySsrc2,   0,                1,      true,                2,     true),

    std::make_tuple(kArbitrarySsrc2,   2,                1,      true,                0,    false),
    std::make_tuple(kArbitrarySsrc2,   2,                1,      true,                1,    false),
    std::make_tuple(kArbitrarySsrc2,   2,                1,      true,                2,     true),

    std::make_tuple(kArbitrarySsrc2,   1,                1,      true,                0,    false),
    std::make_tuple(kArbitrarySsrc2,   1,                1,      true,                1,     true),
    std::make_tuple(kArbitrarySsrc2,   1,                1,      true,                2,     true)));
