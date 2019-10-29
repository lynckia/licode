#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/SequenceNumberTranslator.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

#include <cmath>
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
  Skip = 1,
  Generate = 2,
  Reset = 3
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
    SequenceNumber output;
    if (packet.state == PacketState::Reset) {
      translator.reset();
      continue;
    } else if (packet.state == PacketState::Generate) {
      output = translator.generate();
    } else {
      output = translator.get(packet.sequence_number, skip);
    }
    if (output.type == SequenceNumberType::Valid) {
      EXPECT_THAT(output.output, Eq(packet.expected_output));
    }
    EXPECT_THAT(output.type, Eq(packet.expected_type));

    translator.reverse(packet.expected_output);
    if (output.type == SequenceNumberType::Valid) {
      ASSERT_THAT(output.input, Eq(packet.sequence_number));
    }
    ASSERT_THAT(output.type, Eq(packet.expected_type));
  }
}

std::vector<Packet> getLongQueue(int size, int bits = 16, uint16_t first_seq_num = 0,
         PacketState state = PacketState::Forward, SequenceNumberType type = SequenceNumberType::Valid) {
  std::vector<Packet> queue;
  int mask = std::pow(2, bits);
  for (int i = 0; i <= size; i++) {
    int input_sequence_number = (i + first_seq_num) % mask;
    queue.push_back(Packet{input_sequence_number, state,
                           input_sequence_number, type});
  }
  return queue;
}

std::vector<Packet> concat(std::vector<Packet> origin, std::vector<Packet> target) {
  origin.insert(origin.end(), target.begin(), target.end());
  return origin;
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
                         {     2, PacketState::Forward,               2, SequenceNumberType::Valid}}),

    // Reset after having received skipped packets
    std::vector<Packet>({{  5059, PacketState::Skip,               5059, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {  1032, PacketState::Skip,               1032, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         { 23537, PacketState::Forward,           23537, SequenceNumberType::Valid}}),


    // Reset after having received skipped packets
    std::vector<Packet>({{  5059, PacketState::Skip,               5059, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         { 23537, PacketState::Forward,           23537, SequenceNumberType::Valid}}),

    // Reset after having received skipped packets
    std::vector<Packet>({{  5058, PacketState::Forward,            5058, SequenceNumberType::Valid},
                         {  5059, PacketState::Skip,               5059, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         { 23537, PacketState::Forward,            5059, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{ 50000, PacketState::Forward,           50000, SequenceNumberType::Valid},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         { 23537, PacketState::Forward,           50001, SequenceNumberType::Valid},
                         { 23538, PacketState::Forward,           50002, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     0, PacketState::Generate,              1, SequenceNumberType::Generated},
                         {     6, PacketState::Forward,               2, SequenceNumberType::Valid},
                         {     7, PacketState::Forward,               3, SequenceNumberType::Valid},
                         {     8, PacketState::Forward,               4, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {    10, PacketState::Skip,                 10, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   301, PacketState::Skip,                  7, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   901, PacketState::Forward,               7, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {    10, PacketState::Skip,                 10, SequenceNumberType::Skip},
                         {     9, PacketState::Forward,               9, SequenceNumberType::Valid},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   301, PacketState::Skip,                301, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   901, PacketState::Forward,              10, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {    10, PacketState::Skip,                 10, SequenceNumberType::Skip},
                         {     0, PacketState::Generate,              7, SequenceNumberType::Generated},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   301, PacketState::Skip,                301, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   901, PacketState::Forward,               8, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{  5059, PacketState::Skip,               5059, SequenceNumberType::Skip},
                         {    30, PacketState::Forward,              30, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,             31, SequenceNumberType::Generated},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {     0, PacketState::Generate,             32, SequenceNumberType::Generated},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {     6, PacketState::Forward,              33, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{  5059, PacketState::Skip,               5059, SequenceNumberType::Skip},
                         {    30, PacketState::Forward,              30, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,             31, SequenceNumberType::Generated},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {     0, PacketState::Generate,             32, SequenceNumberType::Generated},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {     6, PacketState::Forward,              33, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     6, PacketState::Forward,               7, SequenceNumberType::Valid},
                         {     7, PacketState::Forward,               8, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     6, PacketState::Forward,               7, SequenceNumberType::Valid},
                         {     7, PacketState::Forward,               8, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              9, SequenceNumberType::Generated},
                         {     8, PacketState::Forward,              10, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     0, PacketState::Generate,              7, SequenceNumberType::Generated},
                         {     6, PacketState::Forward,               8, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     6, PacketState::Skip,                  5, SequenceNumberType::Skip},
                         {     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     7, PacketState::Skip,                  6, SequenceNumberType::Skip},
                         {     8, PacketState::Forward,               7, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     0, PacketState::Generate,              7, SequenceNumberType::Generated},
                         {     6, PacketState::Forward,               8, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              9, SequenceNumberType::Generated},
                         {     0, PacketState::Generate,              10, SequenceNumberType::Generated},
                         {     7, PacketState::Forward,               11, SequenceNumberType::Valid}}),

    // Support multiple loops with skip packets
    concat({             {     5, PacketState::Forward,               5, SequenceNumberType::Valid}},
                         concat(getLongQueue(65536 * 2, 16, 6, PacketState::Skip, SequenceNumberType::Skip),
                        {{     7, PacketState::Forward,               6, SequenceNumberType::Valid}})),

    // Support multiple loops with skip and generated packet
    concat({             {     5, PacketState::Forward,               5, SequenceNumberType::Valid}},
                         concat(getLongQueue(65536 * 2, 16, 6, PacketState::Skip, SequenceNumberType::Skip),
                        {{     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     7, PacketState::Forward,               7, SequenceNumberType::Valid}})),

    // Support multiple loops with generated and a skip packet
    concat({             {     5, PacketState::Forward,               5, SequenceNumberType::Valid}},
                        concat(getLongQueue(65535 * 2, 16, 0, PacketState::Generate, SequenceNumberType::Generated),
                        {{     6, PacketState::Skip,                  5, SequenceNumberType::Skip},
                         {     7, PacketState::Forward,               5, SequenceNumberType::Valid}})),

    // Support multiple loops with valid packets
    getLongQueue(65536 * 2)));


class PictureIdTranslatorTest : public ::testing::TestWithParam<std::vector<Packet>> {
 public:
  PictureIdTranslatorTest() : translator{512, 250, 15} {
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

TEST_P(PictureIdTranslatorTest, shouldReturnRightOutputSequenceNumbers) {
  for (Packet packet : queue) {
    bool skip = packet.state == PacketState::Skip;
    SequenceNumber output;
    if (packet.state == PacketState::Reset) {
      translator.reset();
      continue;
    } else if (packet.state == PacketState::Generate) {
      output = translator.generate();
    } else {
      output = translator.get(packet.sequence_number, skip);
    }
    if (output.type == SequenceNumberType::Valid) {
      EXPECT_THAT(output.output, Eq(packet.expected_output));
    }
    EXPECT_THAT(output.type, Eq(packet.expected_type));
  }
}

INSTANTIATE_TEST_CASE_P(
  PictureIdScenarios, PictureIdTranslatorTest, testing::Values(
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
    std::vector<Packet>({{ 32767, PacketState::Forward,           32767, SequenceNumberType::Valid},
                         {     0, PacketState::Forward,               0, SequenceNumberType::Valid}}),

    // Rollover with packets reordering
    std::vector<Packet>({{ 32767, PacketState::Forward,           32767, SequenceNumberType::Valid},
                         {     1, PacketState::Forward,               1, SequenceNumberType::Valid},
                         {     0, PacketState::Forward,               0, SequenceNumberType::Valid}}),

    // Rollover with packets skipped
    std::vector<Packet>({{ 32767, PacketState::Forward,           32767, SequenceNumberType::Valid},
                         {     0, PacketState::Skip,                  0, SequenceNumberType::Skip},
                         {     1, PacketState::Forward,               0, SequenceNumberType::Valid},
                         {     2, PacketState::Forward,               1, SequenceNumberType::Valid}}),

    // Rollover with packets skipped
    std::vector<Packet>({{ 32767, PacketState::Forward,           32767, SequenceNumberType::Valid},
                         {     1, PacketState::Forward,               1, SequenceNumberType::Valid},
                         {     0, PacketState::Skip,                  0, SequenceNumberType::Discard},
                         {     2, PacketState::Forward,               2, SequenceNumberType::Valid}}),

    // Reset after having received skipped packets
    std::vector<Packet>({{  5059, PacketState::Skip,               5059, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {  1032, PacketState::Skip,               1032, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         { 23537, PacketState::Forward,           23537, SequenceNumberType::Valid}}),


    // Reset after having received skipped packets
    std::vector<Packet>({{  5059, PacketState::Skip,               5059, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         { 23537, PacketState::Forward,           23537, SequenceNumberType::Valid}}),

    // Reset after having received skipped packets
    std::vector<Packet>({{  5058, PacketState::Forward,            5058, SequenceNumberType::Valid},
                         {  5059, PacketState::Skip,               5059, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         { 23537, PacketState::Forward,            5059, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{ 20000, PacketState::Forward,           20000, SequenceNumberType::Valid},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         { 23537, PacketState::Forward,           20001, SequenceNumberType::Valid},
                         { 23538, PacketState::Forward,           20002, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     0, PacketState::Generate,              1, SequenceNumberType::Generated},
                         {     6, PacketState::Forward,               2, SequenceNumberType::Valid},
                         {     7, PacketState::Forward,               3, SequenceNumberType::Valid},
                         {     8, PacketState::Forward,               4, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {    10, PacketState::Skip,                 10, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   301, PacketState::Skip,                  7, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   901, PacketState::Forward,               7, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {    10, PacketState::Skip,                 10, SequenceNumberType::Skip},
                         {     9, PacketState::Forward,               9, SequenceNumberType::Valid},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   301, PacketState::Skip,                301, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   901, PacketState::Forward,              10, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     6, PacketState::Forward,               6, SequenceNumberType::Valid},
                         {    10, PacketState::Skip,                 10, SequenceNumberType::Skip},
                         {     0, PacketState::Generate,              7, SequenceNumberType::Generated},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   301, PacketState::Skip,                301, SequenceNumberType::Skip},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {   901, PacketState::Forward,               8, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{  5059, PacketState::Skip,               5059, SequenceNumberType::Skip},
                         {    30, PacketState::Forward,              30, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,             31, SequenceNumberType::Generated},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {     0, PacketState::Generate,             32, SequenceNumberType::Generated},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {     6, PacketState::Forward,              33, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{  5059, PacketState::Skip,               5059, SequenceNumberType::Skip},
                         {    30, PacketState::Forward,              30, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,             31, SequenceNumberType::Generated},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {     0, PacketState::Generate,             32, SequenceNumberType::Generated},
                         {     0, PacketState::Reset,                 0, SequenceNumberType::Skip},
                         {     6, PacketState::Forward,              33, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     6, PacketState::Forward,               7, SequenceNumberType::Valid},
                         {     7, PacketState::Forward,               8, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     6, PacketState::Forward,               7, SequenceNumberType::Valid},
                         {     7, PacketState::Forward,               8, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              9, SequenceNumberType::Generated},
                         {     8, PacketState::Forward,              10, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     0, PacketState::Generate,              7, SequenceNumberType::Generated},
                         {     6, PacketState::Forward,               8, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     6, PacketState::Skip,                  5, SequenceNumberType::Skip},
                         {     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     7, PacketState::Skip,                  6, SequenceNumberType::Skip},
                         {     8, PacketState::Forward,               7, SequenceNumberType::Valid}}),

    //                     input                        expected_output
    std::vector<Packet>({{     5, PacketState::Forward,               5, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     0, PacketState::Generate,              7, SequenceNumberType::Generated},
                         {     6, PacketState::Forward,               8, SequenceNumberType::Valid},
                         {     0, PacketState::Generate,              9, SequenceNumberType::Generated},
                         {     0, PacketState::Generate,              10, SequenceNumberType::Generated},
                         {     7, PacketState::Forward,               11, SequenceNumberType::Valid}}),

    // Support multiple loops with skip packets
    concat({             {     5, PacketState::Forward,               5, SequenceNumberType::Valid}},
                         concat(getLongQueue(32768 * 2, 15, 6, PacketState::Skip, SequenceNumberType::Skip),
                        {{     7, PacketState::Forward,               6, SequenceNumberType::Valid}})),

    // Support multiple loops with skip and generated packet
    concat({             {     5, PacketState::Forward,               5, SequenceNumberType::Valid}},
                         concat(getLongQueue(32768 * 2, 15, 6, PacketState::Skip, SequenceNumberType::Skip),
                        {{     0, PacketState::Generate,              6, SequenceNumberType::Generated},
                         {     7, PacketState::Forward,               7, SequenceNumberType::Valid}})),

    // Support multiple loops with generated and a skip packet
    concat({             {     5, PacketState::Forward,               5, SequenceNumberType::Valid}},
                        concat(getLongQueue(32767 * 2, 15, 0, PacketState::Generate, SequenceNumberType::Generated),
                        {{     6, PacketState::Skip,                  5, SequenceNumberType::Skip},
                         {     7, PacketState::Forward,               5, SequenceNumberType::Valid}})),

    // Support multiple loops with valid packets
    getLongQueue(32768 * 2, 15)));
