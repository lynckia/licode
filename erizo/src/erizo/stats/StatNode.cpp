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

MovingIntervalRateStat::MovingIntervalRateStat(uint64_t interval_size_ms, uint32_t intervals, double scale,
  std::shared_ptr<Clock> the_clock): interval_size_ms_{interval_size_ms}, intervals_in_window_{intervals},
  scale_{scale}, current_interval_{0}, accumulated_intervals_{0}, calculation_start_ms_{0},
  samples_{new uint64_t[intervals]}, initialized_{false}, clock_{the_clock} {
    std::memset(samples_, 0, intervals_in_window_ * sizeof(uint64_t));
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
  uint64_t now_ms = ClockUtils::timePointToMs(clock_->now());
  if (!initialized_) {
    calculation_start_ms_ = now_ms;
    initialized_ = true;
    accumulated_intervals_ = 1;
  }
  //  if sample is more than a window ahead from last sample
  //  We clean up and set the new value as the newest
  if (now_ms > calculation_start_ms_ + (accumulated_intervals_ + intervals_in_window_) * interval_size_ms_) {
    std::memset(samples_, 0, intervals_in_window_ * sizeof(uint64_t));
    uint32_t corresponding_interval = getIntervalForTimeMs(now_ms);
    current_interval_ = (corresponding_interval + intervals_in_window_ - 1) % intervals_in_window_;
    samples_[corresponding_interval]+= value;
    accumulated_intervals_ += intervals_in_window_;
    return;
  }

  uint32_t next_interval = getNextInterval(current_interval_);
  uint32_t corresponding_interval = getIntervalForTimeMs(now_ms);
  uint32_t moving_interval = next_interval;
  if (corresponding_interval != current_interval_) {
    do {
      samples_[moving_interval] = 0;
      accumulated_intervals_++;
      if (moving_interval == corresponding_interval) {
        break;
      }
      moving_interval = getNextInterval(moving_interval);
    } while (true);
  }

  current_interval_ = corresponding_interval;
  samples_[current_interval_]+= value;
}

uint64_t MovingIntervalRateStat::value() {
  return calculateRateForInterval(intervals_in_window_ * interval_size_ms_);
}

uint64_t MovingIntervalRateStat::value(uint64_t interval_ms) {
  return calculateRateForInterval(interval_ms);
}

std::string MovingIntervalRateStat::toString() {
  return std::to_string(value());
}

uint64_t MovingIntervalRateStat::calculateRateForInterval(uint64_t interval_to_calculate_ms) {
  //  We check if it's within the data we have
  uint64_t real_interval = std::min(interval_to_calculate_ms, (accumulated_intervals_ * interval_size_ms_));
  real_interval = std::min(real_interval, (intervals_in_window_ * interval_size_ms_));
  int added_intervals = 0;
  uint64_t total_sum = 0;
  uint32_t next_interval = getNextInterval(current_interval_);
  uint32_t moving_interval = getIntervalForTimeMs(ClockUtils::timePointToMs(clock_->now()) - real_interval);
  do {
    added_intervals++;
    total_sum += samples_[moving_interval];
    moving_interval = getNextInterval(moving_interval);
  } while (moving_interval != next_interval);

  double rate = static_cast<double> (total_sum) / (added_intervals*interval_size_ms_);
  return (rate * 1000 * scale_);
}

uint32_t MovingIntervalRateStat::getIntervalForTimeMs(uint64_t time_ms) {
  double calc = static_cast<double>(time_ms - calculation_start_ms_);
  uint32_t corresponding_interval = ((time_ms - calculation_start_ms_)/interval_size_ms_) % intervals_in_window_;
  return corresponding_interval;
}

uint32_t MovingIntervalRateStat::getNextInterval(uint32_t interval) {
  return (interval + 1) % intervals_in_window_;
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
