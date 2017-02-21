#include "rtp/SequenceNumberTranslator.h"

#include <vector>
#include "rtp/RtpUtils.h"

namespace erizo {

constexpr uint16_t kMaxSequenceNumberInBuffer = 511;  // = 65536 / 128 and more than 10s of audio @50fps
constexpr uint16_t kMaxDistance = 200;

DEFINE_LOGGER(SequenceNumberTranslator, "rtp.SequenceNumberTranslator");

SequenceNumberTranslator::SequenceNumberTranslator()
    : in_out_buffer_{kMaxSequenceNumberInBuffer},
      out_in_buffer_{kMaxSequenceNumberInBuffer},
      first_input_sequence_number_{0},
      last_input_sequence_number_{0},
      initial_output_sequence_number_{0},
      initialized_{false}, reset_{false} {
}

void SequenceNumberTranslator::add(SequenceNumber sequence_number) {
  in_out_buffer_[sequence_number.input % kMaxSequenceNumberInBuffer] = sequence_number;
  out_in_buffer_[sequence_number.output % kMaxSequenceNumberInBuffer] = sequence_number;
}

uint16_t SequenceNumberTranslator::fill(const uint16_t &first,
                                        const uint16_t &last) {
  const SequenceNumber& first_sequence_number = get(first);
  uint16_t output_sequence_number = first_sequence_number.output;

  if (first_sequence_number.type != SequenceNumberType::Skip) {
    output_sequence_number++;
  }

  for (uint16_t sequence_number = first_sequence_number.input + 1;
       RtpUtils::sequenceNumberLessThan(sequence_number, last);
       sequence_number++) {
    add(SequenceNumber{sequence_number, output_sequence_number++, SequenceNumberType::Valid});
  }

  return output_sequence_number;
}

SequenceNumber& SequenceNumberTranslator::internalGet(uint16_t input_sequence_number) {
  return in_out_buffer_[input_sequence_number % kMaxSequenceNumberInBuffer];
}

SequenceNumber SequenceNumberTranslator::get(uint16_t input_sequence_number) const {
  return in_out_buffer_[input_sequence_number % kMaxSequenceNumberInBuffer];
}

SequenceNumber SequenceNumberTranslator::get(uint16_t input_sequence_number, bool skip) {
  if (!initialized_) {
    SequenceNumberType type = skip ? SequenceNumberType::Skip : SequenceNumberType::Valid;
    uint16_t output_sequence_number = reset_ ? initial_output_sequence_number_ : input_sequence_number;
    add(SequenceNumber{input_sequence_number, output_sequence_number, type});
    last_input_sequence_number_ = input_sequence_number;
    first_input_sequence_number_ = input_sequence_number;
    initialized_ = true;
    reset_ = false;
    return get(input_sequence_number);
  }

  if (RtpUtils::sequenceNumberLessThan(input_sequence_number, first_input_sequence_number_)) {
    return SequenceNumber{input_sequence_number, 0, SequenceNumberType::Skip};
  }

  if (RtpUtils::sequenceNumberLessThan(last_input_sequence_number_, input_sequence_number)) {
    uint16_t output_sequence_number = fill(last_input_sequence_number_, input_sequence_number);

    SequenceNumberType type = skip ? SequenceNumberType::Skip : SequenceNumberType::Valid;

    add(SequenceNumber{input_sequence_number, output_sequence_number, type});
    last_input_sequence_number_ = input_sequence_number;
    if (last_input_sequence_number_ - first_input_sequence_number_ > kMaxDistance) {
      first_input_sequence_number_ = last_input_sequence_number_ - kMaxDistance;
    }
    return get(input_sequence_number);
  } else {
    SequenceNumber& result = internalGet(input_sequence_number);
    if (result.type == SequenceNumberType::Valid) {
      SequenceNumberType type = skip ? SequenceNumberType::Discard : SequenceNumberType::Valid;
      result.type = type;
    }
    return result;
  }
}

SequenceNumber SequenceNumberTranslator::reverse(uint16_t output_sequence_number) const {
  SequenceNumber result = out_in_buffer_[output_sequence_number % kMaxSequenceNumberInBuffer];
  if (RtpUtils::sequenceNumberLessThan(result.input, first_input_sequence_number_) ||
      RtpUtils::sequenceNumberLessThan(last_input_sequence_number_, result.input)) {
    return SequenceNumber{0, output_sequence_number, SequenceNumberType::Discard};
  }
  return result;
}

void SequenceNumberTranslator::reset() {
  initialized_ = false;
  SequenceNumber &last = internalGet(last_input_sequence_number_);
  initial_output_sequence_number_ = last.output;
  first_input_sequence_number_ = 0;
  last_input_sequence_number_ = 0;
  reset_ = true;
  in_out_buffer_ = std::vector<SequenceNumber>(kMaxSequenceNumberInBuffer);
  out_in_buffer_ = std::vector<SequenceNumber>(kMaxSequenceNumberInBuffer);
}

}  // namespace erizo
