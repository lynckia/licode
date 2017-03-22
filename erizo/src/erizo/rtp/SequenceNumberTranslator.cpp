#include "rtp/SequenceNumberTranslator.h"

#include <vector>
#include "rtp/RtpUtils.h"

namespace erizo {

constexpr uint16_t kMaxSequenceNumberInBuffer = 1023;  // = 65536 / 128 and more than 10s of audio @50fps
constexpr uint16_t kMaxDistance = 500;

DEFINE_LOGGER(SequenceNumberTranslator, "rtp.SequenceNumberTranslator");

SequenceNumberTranslator::SequenceNumberTranslator()
    : in_out_buffer_{kMaxSequenceNumberInBuffer},
      out_in_buffer_{kMaxSequenceNumberInBuffer},
      first_input_sequence_number_{0},
      last_input_sequence_number_{0},
      last_output_sequence_number_{0},
      offset_{0},
      initialized_{false}, reset_{false} {
}

void SequenceNumberTranslator::add(SequenceNumber sequence_number) {
  in_out_buffer_[sequence_number.input % kMaxSequenceNumberInBuffer] = sequence_number;
  out_in_buffer_[sequence_number.output % kMaxSequenceNumberInBuffer] = sequence_number;
}

uint16_t SequenceNumberTranslator::fill(const uint16_t &first,
                                        const uint16_t &last) {
  if (last < first) {
    fill(first, 65535);
    fill(0, last);
  }
  const SequenceNumber& previous_sequence_number = get(first - 1);
  uint16_t output_sequence_number = previous_sequence_number.output + offset_;

  if (previous_sequence_number.type != SequenceNumberType::Skip) {
    output_sequence_number++;
  }
  for (uint16_t sequence_number = first;
       RtpUtils::sequenceNumberLessThan(sequence_number, last);
       sequence_number++) {
    add(SequenceNumber{sequence_number, output_sequence_number++, SequenceNumberType::Valid});
  }

  return output_sequence_number;
}

SequenceNumber& SequenceNumberTranslator::internalGet(uint16_t input_sequence_number) {
  return in_out_buffer_[input_sequence_number % kMaxSequenceNumberInBuffer];
}

SequenceNumber& SequenceNumberTranslator::internalReverse(uint16_t output_sequence_number) {
  return out_in_buffer_[output_sequence_number % kMaxSequenceNumberInBuffer];
}

SequenceNumber SequenceNumberTranslator::get(uint16_t input_sequence_number) const {
  const SequenceNumber &result = in_out_buffer_[input_sequence_number % kMaxSequenceNumberInBuffer];
  if (result.input == input_sequence_number) {
    return result;
  }
  return SequenceNumber{0, 0, SequenceNumberType::Skip};
}

SequenceNumber SequenceNumberTranslator::generate() {
  uint16_t output_sequence_number = ++offset_ + last_output_sequence_number_;
  SequenceNumber sequence_number{0, output_sequence_number, SequenceNumberType::Generated};
  out_in_buffer_[sequence_number.output % kMaxSequenceNumberInBuffer] = sequence_number;

  return sequence_number;
}

void SequenceNumberTranslator::updateLastOutputSequenceNumber(bool skip, uint16_t output_sequence_number) {
  bool first_packet = !initialized_ && !(reset_ || offset_ > 0);
  if (!skip &&
      (RtpUtils::sequenceNumberLessThan(last_output_sequence_number_, output_sequence_number) || first_packet)) {
    last_output_sequence_number_ = output_sequence_number;
  }
}

SequenceNumber SequenceNumberTranslator::get(uint16_t input_sequence_number, bool skip) {
  if (!initialized_) {
    SequenceNumberType type = skip ? SequenceNumberType::Skip : SequenceNumberType::Valid;
    uint16_t output_sequence_number = (reset_ || offset_ > 0) ?
                      last_output_sequence_number_ + offset_ + 1 : input_sequence_number;
    add(SequenceNumber{input_sequence_number, output_sequence_number, type});
    updateLastOutputSequenceNumber(skip, output_sequence_number);
    if (!skip) {
      last_input_sequence_number_ = input_sequence_number;
      first_input_sequence_number_ = input_sequence_number;
      initialized_ = true;
      reset_ = false;
      offset_ = 0;
    }
    return get(input_sequence_number);
  }

  if (RtpUtils::sequenceNumberLessThan(input_sequence_number, first_input_sequence_number_)) {
    return SequenceNumber{input_sequence_number, 0, SequenceNumberType::Skip};
  }

  if (RtpUtils::sequenceNumberLessThan(last_input_sequence_number_, input_sequence_number)) {
    uint16_t output_sequence_number = fill(last_input_sequence_number_ + 1, input_sequence_number);

    SequenceNumberType type = skip ? SequenceNumberType::Skip : SequenceNumberType::Valid;

    updateLastOutputSequenceNumber(skip, output_sequence_number);
    add(SequenceNumber{input_sequence_number, output_sequence_number, type});
    last_input_sequence_number_ = input_sequence_number;
    if (RtpUtils::sequenceNumberLessThan(first_input_sequence_number_, last_input_sequence_number_ - kMaxDistance)) {
      first_input_sequence_number_ = last_input_sequence_number_ - kMaxDistance;
    }
    SequenceNumber info = get(input_sequence_number);
    offset_ = 0;
    return info;
  } else {
    SequenceNumber& result = internalGet(input_sequence_number);
    if (result.type == SequenceNumberType::Valid) {
      SequenceNumberType type = skip ? SequenceNumberType::Discard : SequenceNumberType::Valid;
      internalReverse(result.output).type = type;
      result.type = type;
    }
    updateLastOutputSequenceNumber(skip, result.output);
    return result;
  }
}

SequenceNumber SequenceNumberTranslator::reverse(uint16_t output_sequence_number) const {
  SequenceNumber result = out_in_buffer_[output_sequence_number % kMaxSequenceNumberInBuffer];
  if (result.output != output_sequence_number ||
      RtpUtils::sequenceNumberLessThan(result.input, first_input_sequence_number_) ||
      RtpUtils::sequenceNumberLessThan(last_input_sequence_number_, result.input)) {
    return SequenceNumber{0, output_sequence_number, SequenceNumberType::Discard};
  }
  return result;
}

void SequenceNumberTranslator::reset() {
  if (!initialized_) {
    return;
  }
  initialized_ = false;
  reset_ = true;
  first_input_sequence_number_ = 0;
  last_input_sequence_number_ = 0;
  in_out_buffer_ = std::vector<SequenceNumber>(kMaxSequenceNumberInBuffer);
  out_in_buffer_ = std::vector<SequenceNumber>(kMaxSequenceNumberInBuffer);
}

}  // namespace erizo
