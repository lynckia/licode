#ifndef ERIZO_SRC_ERIZO_RTP_SEQUENCENUMBERTRANSLATOR_H_
#define ERIZO_SRC_ERIZO_RTP_SEQUENCENUMBERTRANSLATOR_H_

#include <utility>
#include <vector>

#include "./logger.h"
#include "pipeline/Service.h"

namespace erizo {

enum class SequenceNumberType : uint8_t {
  Valid = 0,
  Skip = 1,
  Discard = 2,
  Generated = 3
};

struct SequenceNumber {
  SequenceNumber() : input{0}, output{0}, type{SequenceNumberType::Valid} {}
  SequenceNumber(uint16_t input_, uint16_t output_, SequenceNumberType type_) :
      input{input_}, output{output_}, type{type_} {}

  uint16_t input;
  uint16_t output;
  SequenceNumberType type;
};

class SequenceNumberTranslator: public Service, public std::enable_shared_from_this<SequenceNumberTranslator> {
  DECLARE_LOGGER();

 public:
  SequenceNumberTranslator();
  SequenceNumberTranslator(uint16_t max_number_in_buffer, uint16_t max_distance, uint16_t bits);

  SequenceNumber get(uint16_t input_sequence_number) const;
  SequenceNumber get(uint16_t input_sequence_number, bool skip);
  SequenceNumber reverse(uint16_t output_sequence_number) const;
  void reset();
  SequenceNumber generate();

 private:
  void add(SequenceNumber sequence_number);
  uint16_t fill(const uint16_t &first, const uint16_t &last);
  SequenceNumber& internalGet(uint16_t input_sequence_number);
  SequenceNumber& internalReverse(uint16_t output_sequence_number);
  void updateLastOutputSequenceNumber(bool skip, uint16_t output_sequence_number);
  uint16_t boundedSequenceNumber(const uint16_t& input);

 private:
  std::vector<SequenceNumber> in_out_buffer_;
  std::vector<SequenceNumber> out_in_buffer_;
  uint16_t first_input_sequence_number_;
  uint16_t last_input_sequence_number_;
  uint16_t last_output_sequence_number_;
  uint16_t max_number_in_buffer_;
  uint16_t max_distance_;
  uint16_t bits_;
  uint32_t mask_;
  uint16_t offset_;
  bool initialized_;
  bool reset_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_SEQUENCENUMBERTRANSLATOR_H_
