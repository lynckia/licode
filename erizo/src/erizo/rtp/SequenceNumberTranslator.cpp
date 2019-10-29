#include "rtp/SequenceNumberTranslator.h"

#include <cmath>
#include <vector>
#include "rtp/RtpUtils.h"

namespace erizo {

constexpr uint16_t kMaxSequenceNumberInBuffer = 1024;  // = 65536 / 64 and more than 10s of audio @50fps
constexpr uint16_t kMaxDistance = 500;
constexpr uint16_t kBitsInSequenceNumber = 16;

DEFINE_LOGGER(SequenceNumberTranslator, "rtp.SequenceNumberTranslator");

SequenceNumberTranslator::SequenceNumberTranslator()
    : in_out_buffer_{kMaxSequenceNumberInBuffer},
      out_in_buffer_{kMaxSequenceNumberInBuffer},
      first_input_sequence_number_{0},
      last_input_sequence_number_{0},
      last_output_sequence_number_{0},
      max_number_in_buffer_{kMaxSequenceNumberInBuffer},
      max_distance_{kMaxDistance},
      bits_{kBitsInSequenceNumber},
      mask_{static_cast<uint16_t>(std::pow(2, bits_))},
      offset_{0},
      initialized_{false}, reset_{false} {
}

SequenceNumberTranslator::SequenceNumberTranslator(uint16_t max_number_in_buffer, uint16_t max_distance, uint16_t bits)
    : in_out_buffer_{max_number_in_buffer},
      out_in_buffer_{max_number_in_buffer},
      first_input_sequence_number_{0},
      last_input_sequence_number_{0},
      last_output_sequence_number_{0},
      max_number_in_buffer_{max_number_in_buffer},
      max_distance_{max_distance},
      bits_{bits},
      mask_{static_cast<uint16_t>(std::pow(2, bits_))},
      offset_{0},
      initialized_{false}, reset_{false} {
}

void SequenceNumberTranslator::add(SequenceNumber sequence_number) {
  in_out_buffer_[sequence_number.input % max_number_in_buffer_] = sequence_number;
  out_in_buffer_[sequence_number.output % max_number_in_buffer_] = sequence_number;
}

uint16_t SequenceNumberTranslator::boundedSequenceNumber(const uint16_t& input) {
  if (bits_ != 16) {
    return input % mask_;
  }
  return input;
}

uint16_t SequenceNumberTranslator::fill(const uint16_t &first_input,
                                        const uint16_t &last_input) {
  uint16_t first = boundedSequenceNumber(first_input);
  uint16_t last = boundedSequenceNumber(last_input);
  if (last < first) {
    fill(first, 65535);
    fill(0, last);
  }
  uint16_t output_sequence_number = boundedSequenceNumber(last_output_sequence_number_ + offset_ + 1);
  for (uint16_t sequence_number = first;
       RtpUtils::numberLessThan(sequence_number, last, bits_);
       sequence_number++) {
    add(SequenceNumber{boundedSequenceNumber(sequence_number),
                       boundedSequenceNumber(output_sequence_number++),
                       SequenceNumberType::Valid});
  }

  return output_sequence_number;
}

SequenceNumber& SequenceNumberTranslator::internalGet(uint16_t input_sequence_number) {
  return in_out_buffer_[input_sequence_number % max_number_in_buffer_];
}

SequenceNumber& SequenceNumberTranslator::internalReverse(uint16_t output_sequence_number) {
  return out_in_buffer_[output_sequence_number % max_number_in_buffer_];
}

SequenceNumber SequenceNumberTranslator::get(uint16_t input_sequence_number) const {
  const SequenceNumber &result = in_out_buffer_[input_sequence_number % max_number_in_buffer_];
  if (result.input == input_sequence_number) {
    return result;
  }
  return SequenceNumber{0, 0, SequenceNumberType::Skip};
}

SequenceNumber SequenceNumberTranslator::generate() {
  uint16_t output_sequence_number = boundedSequenceNumber(++offset_ + last_output_sequence_number_);
  SequenceNumber sequence_number{0, output_sequence_number, SequenceNumberType::Generated};
  out_in_buffer_[sequence_number.output % max_number_in_buffer_] = sequence_number;

  return sequence_number;
}

void SequenceNumberTranslator::updateLastOutputSequenceNumber(bool skip, uint16_t output_sequence_number) {
  bool first_packet = !initialized_ && !(reset_ || offset_ > 0);
  if (!skip &&
      (RtpUtils::numberLessThan(last_output_sequence_number_, output_sequence_number, bits_) || first_packet)) {
    last_output_sequence_number_ = output_sequence_number;
  }
}

SequenceNumber SequenceNumberTranslator::get(uint16_t input_sequence_number, bool skip) {
  if (!initialized_) {
    SequenceNumberType type = skip ? SequenceNumberType::Skip : SequenceNumberType::Valid;
    uint16_t output_sequence_number = (reset_ || offset_ > 0) ?
                      last_output_sequence_number_ + offset_ + 1 : input_sequence_number;
    output_sequence_number = boundedSequenceNumber(output_sequence_number);
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

  if (RtpUtils::numberLessThan(input_sequence_number, first_input_sequence_number_, bits_)) {
    return SequenceNumber{input_sequence_number, 0, SequenceNumberType::Skip};
  }

  if (RtpUtils::numberLessThan(last_input_sequence_number_, input_sequence_number, bits_)) {
    uint16_t output_sequence_number = fill(last_input_sequence_number_ + 1, input_sequence_number);

    SequenceNumberType type = skip ? SequenceNumberType::Skip : SequenceNumberType::Valid;

    updateLastOutputSequenceNumber(skip, output_sequence_number);
    add(SequenceNumber{input_sequence_number, output_sequence_number, type});
    last_input_sequence_number_ = input_sequence_number;
    if (RtpUtils::numberLessThan(first_input_sequence_number_, last_input_sequence_number_ - max_distance_, bits_)) {
      first_input_sequence_number_ = last_input_sequence_number_ - max_distance_;
    }
    SequenceNumber info = get(input_sequence_number);
    if (!skip) {
      offset_ = 0;
    }
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
  SequenceNumber result = out_in_buffer_[output_sequence_number % max_number_in_buffer_];
  if (result.output != output_sequence_number ||
      RtpUtils::numberLessThan(result.input, first_input_sequence_number_, bits_) ||
      RtpUtils::numberLessThan(last_input_sequence_number_, result.input, bits_)) {
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
  in_out_buffer_ = std::vector<SequenceNumber>(max_number_in_buffer_);
  out_in_buffer_ = std::vector<SequenceNumber>(max_number_in_buffer_);
}

}  // namespace erizo
