#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/SequenceNumberTranslator.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

#include <queue>
#include <string>
#include <vector>

#include "../utils/Mocks.h"
#include "../utils/Tools.h"
#include "../utils/Matchers.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Eq;
using ::testing::Args;
using ::testing::Return;
using erizo::SequenceNumberTranslator;
using erizo::SequenceNumber;
using erizo::SequenceNumberType;

enum class PacketState {
  Forward = 0,
  Skip = 1
};

struct Packet {
  int sequence_number;
  PacketState state;
  int expected_output;
  SequenceNumberType expected_type;
};

class SequenceNumberTranslatorTest : public ::testing::TestWithParam<std::vector<Packet>> {
 public:
  SequenceNumberTranslatorTest() {
    queue = GetParam();
  }
 protected:
  void SetUp() {
  }

  void TearDown() {
  }

  SequenceNumberTranslator translator;
  std::vector<Packet> queue;
};

TEST_P(SequenceNumberTranslatorTest, shouldReturnRightOutputSequenceNumbers) {
  for (Packet packet : queue) {
    bool skip = packet.state == PacketState::Skip;
    SequenceNumber output = translator.get(packet.sequence_number, skip);
    EXPECT_THAT(output.output, Eq(packet.expected_output));
    EXPECT_THAT(output.type, Eq(packet.expected_type));
  }
}

TEST_P(SequenceNumberTranslatorTest, shouldReturnRightInputSequenceNumbers) {
  for (Packet packet : queue) {
    bool skip = packet.state == PacketState::Skip;
    translator.get(packet.sequence_number, skip);
  }

  // Reverse look-up
  for (Packet packet : queue) {
    if (packet.expected_type != SequenceNumberType::Valid) {
      continue;
    }
    SequenceNumber output = translator.reverse(packet.expected_output);
    EXPECT_THAT(output.input, Eq(packet.sequence_number));
    EXPECT_THAT(output.type, Eq(packet.expected_type));
  }
}

INSTANTIATE_TEST_CASE_P(
  SequenceNumberScenarios, SequenceNumberTranslatorTest, testing::Values(
    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {     7, PacketState::Forward,               7, SequenceNumberType::Valid},
                         {     8, PacketState::Forward,               8, SequenceNumberType::Valid}}),

    // Packet reordering
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {     8, PacketState::Forward,               8, SequenceNumberType::Valid},
                         {     7, PacketState::Forward,               7, SequenceNumberType::Valid}}),

    // Skip packets
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {     7, PacketState::Skip,                  7, SequenceNumberType::Skip},
                         {     8, PacketState::Forward,               7, SequenceNumberType::Valid},
                         {     9, PacketState::Forward,               8, SequenceNumberType::Valid}}),

    // Discard packets
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {     8, PacketState::Forward,               8, SequenceNumberType::Valid},
                         {     7, PacketState::Skip,                  7, SequenceNumberType::Discard},
                         {     9, PacketState::Forward,               9, SequenceNumberType::Valid}}),


    // Retransmissions
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {     7, PacketState::Forward,               7, SequenceNumberType::Valid},
                         {     8, PacketState::Forward,               8, SequenceNumberType::Valid},
                         {     7, PacketState::Forward,               7, SequenceNumberType::Valid}}),

    // Retransmissions of skipped packets
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {     7, PacketState::Skip,                  7, SequenceNumberType::Skip},
                         {     8, PacketState::Forward,               7, SequenceNumberType::Valid},
                         {     7, PacketState::Skip,                  7, SequenceNumberType::Skip}}),

    // Rollover
    std::vector<Packet>({{ 65535, PacketState::Forward,           65535, SequenceNumberType::Valid},
                         {     0, PacketState::Forward,               0, SequenceNumberType::Valid}}),

    // Rollover with packets reordering
    std::vector<Packet>({{ 65535, PacketState::Forward,           65535, SequenceNumberType::Valid},
                         {     1, PacketState::Forward,               1, SequenceNumberType::Valid},
                         {     0, PacketState::Forward,               0, SequenceNumberType::Valid}}),

    // Rollover with packets skipped
    std::vector<Packet>({{ 65535, PacketState::Forward,           65535, SequenceNumberType::Valid},
                         {     0, PacketState::Skip,                  0, SequenceNumberType::Skip},
                         {     1, PacketState::Forward,               0, SequenceNumberType::Valid},
                         {     2, PacketState::Forward,               1, SequenceNumberType::Valid}}),

    // Rollover with packets skipped
    std::vector<Packet>({{ 65535, PacketState::Forward,           65535, SequenceNumberType::Valid},
                         {     1, PacketState::Forward,               1, SequenceNumberType::Valid},
                         {     0, PacketState::Skip,                  0, SequenceNumberType::Discard},
                         {     2, PacketState::Forward,               2, SequenceNumberType::Valid}})));
