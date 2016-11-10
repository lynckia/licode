#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Headers for RtpPacketQueue.h tests
#include <rtp/RtpRetransmissionHandler.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

static constexpr uint16_t kVideoSsrc = 1;
static constexpr uint16_t kAudioSsrc = 2;

using ::testing::_;
using ::testing::IsNull;
using ::testing::Return;
using erizo::dataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpRetransmissionHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;

class MockWebRtcConnection: public WebRtcConnection {
 public:
  MockWebRtcConnection() :
    WebRtcConnection("", true, true, ice_config, nullptr) {
    ON_CALL(*this, getVideoSinkSSRC()).WillByDefault(Return(kVideoSsrc));
    ON_CALL(*this, getAudioSinkSSRC()).WillByDefault(Return(kAudioSsrc));
  }
  virtual ~MockWebRtcConnection() {
  }

  MOCK_METHOD0(getVideoSinkSSRC, uint16_t(void));
  MOCK_METHOD0(getAudioSinkSSRC, uint16_t(void));
  MOCK_METHOD1(read, void(std::shared_ptr<dataPacket>));
  MOCK_METHOD1(write, void(std::shared_ptr<dataPacket>));

 private:
  IceConfig ice_config;
};

class Reader : public InboundHandler {
 public:
  MOCK_METHOD2(read, void(Context*, std::shared_ptr<dataPacket>));
};

class Writer : public OutboundHandler {
 public:
  MOCK_METHOD2(write, void(Context*, std::shared_ptr<dataPacket>));
};

class RtpRetransmissionHandlerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    connection = std::make_shared<MockWebRtcConnection>();
    pipeline = Pipeline::create();
    rtx_handler = std::make_shared<RtpRetransmissionHandler>(connection.get());
    reader = std::make_shared<Reader>();
    writer = std::make_shared<Writer>();

    pipeline->addBack(writer);
    pipeline->addBack(rtx_handler);
    pipeline->addBack(reader);
    pipeline->finalize();
  }

  virtual void TearDown() {
  }

  std::shared_ptr<dataPacket> createDataPacket(uint16_t seq_number, packetType type) {
    erizo::RtpHeader *header = new erizo::RtpHeader();
    header->setSeqNumber(seq_number);

    if (type == AUDIO_PACKET) {
      header->setSSRC(kAudioSsrc);
    } else {
      header->setSSRC(kVideoSsrc);
    }

    return std::make_shared<dataPacket>(0, reinterpret_cast<char*>(header), sizeof(erizo::RtpHeader), type, 0);
  }

  std::shared_ptr<MockWebRtcConnection> connection;
  Pipeline::Ptr pipeline;
  std::shared_ptr<Reader> reader;
  std::shared_ptr<Writer> writer;
  std::shared_ptr<RtpRetransmissionHandler> rtx_handler;
};

TEST_F(RtpRetransmissionHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = createDataPacket(12, AUDIO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _));
    pipeline->read(packet);
}

TEST_F(RtpRetransmissionHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = createDataPacket(12, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _));
    pipeline->write(packet);
}
