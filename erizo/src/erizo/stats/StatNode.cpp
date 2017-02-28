#include "stats/StatNode.h"

#include <sstream>
#include <string>
#include <memory>
#include <map>
#include <iostream>

#include "lib/ClockUtils.h"

namespace erizo {

typedef std::map<std::string, std::shared_ptr<StatNode>> NodeMap;

StatNode& StatNode::operator[](std::string key) {
  if (!hasChild(key)) {
    node_map_[key] = std::make_shared<StatNode>();
  }
  return *node_map_[key];
}

std::string StatNode::toString() {
  std::ostringstream text;
  text << "{";
  for (NodeMap::iterator node_iterator = node_map_.begin(); node_iterator != node_map_.end();) {
    const std::string &name = node_iterator->first;
    std::shared_ptr<StatNode> &node = node_iterator->second;
    text << "\"" << name << "\":" << node->toString();
    if (++node_iterator != node_map_.end()) {
      text << ",";
    }
  }
  text << "}";
  return text.str();
}

StatNode& StringStat::operator=(std::string text) {
  text_ = text;
  return *this;
}

std::string StringStat::toString() {
  std::ostringstream text;
  text << "\"" << text_ << "\"";
  return text.str();
}

StatNode& CumulativeStat::operator=(uint64_t initial) {
  total_ = initial;
  return *this;
}

StatNode CumulativeStat::operator++(int value) {
  CumulativeStat node{total_};
  total_++;
  return node;
}

StatNode& CumulativeStat::operator+=(uint64_t value) {
  total_ += value;
  return *this;
}

RateStat::RateStat(duration period, double scale, std::shared_ptr<Clock> the_clock)
  : period_{period}, scale_{scale}, calculation_start_{the_clock->now()}, last_{0},
    total_{0}, current_period_total_{0}, last_period_calculated_rate_{0}, clock_{the_clock} {
}

StatNode RateStat::operator++(int value) {
  RateStat node = (*this);
  add(1);
  return node;
}

StatNode& RateStat::operator+=(uint64_t value) {
  add(value);
  return *this;
}

void RateStat::add(uint64_t value) {
  current_period_total_ += value;
  last_ = value;
  total_++;
  checkPeriod();
}

uint64_t RateStat::value() {
  checkPeriod();
  return last_period_calculated_rate_;
}

std::string RateStat::toString() {
  return std::to_string(value());
}

void RateStat::checkPeriod() {
  time_point now = clock_->now();
  duration delay = now - calculation_start_;
  if (delay >= period_) {
    last_period_calculated_rate_ = scale_ * current_period_total_ * 1000. / ClockUtils::durationToMs(delay);
    current_period_total_ = 0;
    calculation_start_ = now;
  }
}

MovingIntervalRateStat::MovingIntervalRateStat(duration interval_size, uint32_t total_intervals, double scale,
  std::shared_ptr<Clock> the_clock): interval_size_ms_{ClockUtils::durationToMs(interval_size)},
  total_intervals_{total_intervals}, scale_{scale}, samples_{new uint64_t[total_intervals_]}, initialized_{false},
  clock_{the_clock} {
}

MovingIntervalRateStat::~MovingIntervalRateStat() {
  delete[] samples_;
}

StatNode MovingIntervalRateStat::operator++(int value) {
  MovingIntervalRateStat node = (*this);
  add(1);
  return node;
}

StatNode& MovingIntervalRateStat::operator+=(uint64_t value) {
  add(value);
  return *this;
}

void MovingIntervalRateStat::add(uint64_t value) {
  time_point now = clock_->now();
  if (!initialized_) {
    calculation_start_ = now;
    current_window_start_ms_ = ClockUtils::timePointToMs(calculation_start_);
  }
}

uint64_t MovingIntervalRateStat::value() {
  return 0;
}

std::string MovingIntervalRateStat::toString() {
  return std::to_string(value());
}

uint64_t MovingIntervalRateStat::calculateRateForInterval(duration interval_to_calculate) {
  return 0;
}


MovingAverageStat::MovingAverageStat(uint32_t window_size)
  :samples_{new uint64_t[window_size]}, window_size_{window_size}, next_sample_position_{0},
  current_average_{0} {
}

MovingAverageStat::~MovingAverageStat() {
  delete[] samples_;
}

StatNode MovingAverageStat::operator++(int value) {
  MovingAverageStat node = (*this);
  add(1);
  return node;
}

StatNode& MovingAverageStat::operator+=(uint64_t value) {
  add(value);
  return *this;
}

uint64_t MovingAverageStat::value() {
  return static_cast<uint64_t>(current_average_);
}

uint64_t MovingAverageStat::value(uint32_t sample_number) {
  return static_cast<uint64_t>(getAverage(sample_number));
}

std::string MovingAverageStat::toString() {
  return std::to_string(value());
}

void MovingAverageStat::add(uint64_t value) {
  if (next_sample_position_ < window_size_) {
    current_average_ = 0;
  } else {
    current_average_ = current_average_ +
      static_cast<double>(value - samples_[next_sample_position_ % window_size_])/window_size_;
  }

  samples_[next_sample_position_] = value;
  next_sample_position_++;
  if (current_average_ == 0) {
    current_average_ = getAverage(window_size_);
  }
}

double MovingAverageStat::getAverage(uint32_t sample_number) {
  uint32_t current_sample_position = next_sample_position_ - 1;
  //  We won't calculate an average for more than the window size
  sample_number = std::min(sample_number, window_size_);
  //  Check if we have enough samples
  sample_number = std::min(sample_number, current_sample_position);
  uint64_t calculated_sum = 0;
  for (uint32_t i = 0; i < sample_number;  i++) {
    calculated_sum += samples_[(current_sample_position - i) % window_size_];
  }
  return static_cast<double>(calculated_sum)/sample_number;
}

}  // namespace erizo
