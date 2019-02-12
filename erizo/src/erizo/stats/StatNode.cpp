#include "stats/StatNode.h"

#include <cmath>
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

MovingIntervalRateStat::MovingIntervalRateStat(duration interval_size, uint32_t intervals, double scale,
  std::shared_ptr<Clock> the_clock): interval_size_ms_{ClockUtils::durationToMs(interval_size)},
  intervals_in_window_{intervals}, scale_{scale}, calculation_start_ms_{0}, current_interval_{0},
  accumulated_intervals_{0}, current_window_start_ms_{0}, current_window_end_ms_{0},
  sample_vector_{std::make_shared<std::vector<uint64_t>>(intervals, 0)},
  initialized_{false}, clock_{the_clock} {
}

MovingIntervalRateStat::~MovingIntervalRateStat() {
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
    current_window_start_ms_ = now_ms;
    current_window_end_ms_ = now_ms + accumulated_intervals_ * interval_size_ms_;
  }
  int32_t intervals_to_pass = (now_ms - current_window_end_ms_) / interval_size_ms_;

  if (intervals_to_pass > 0) {
    if (static_cast<uint32_t>(intervals_to_pass) >= intervals_in_window_) {
      sample_vector_->assign(intervals_in_window_, 0);
      uint32_t corresponding_interval = getIntervalForTimeMs(now_ms);
      current_interval_ = corresponding_interval;
      accumulated_intervals_ += intervals_to_pass;
      (*sample_vector_.get())[current_interval_]+= value;
      updateWindowTimes();
      return;
    }
    for (int i = 0; i < intervals_to_pass; i++) {
      current_interval_ = getNextInterval(current_interval_);
      (*sample_vector_.get())[current_interval_] = 0;
      accumulated_intervals_++;
    }
  }

  uint32_t corresponding_interval = getIntervalForTimeMs(now_ms);
  if (corresponding_interval != current_interval_) {
    (*sample_vector_.get())[corresponding_interval] = 0;
    accumulated_intervals_++;
    updateWindowTimes();
    current_interval_ = corresponding_interval;
  }

  (*sample_vector_.get())[current_interval_]+= value;
}

uint64_t MovingIntervalRateStat::value() {
  return calculateRateForInterval(intervals_in_window_ * interval_size_ms_);
}

uint64_t MovingIntervalRateStat::value(duration stat_interval) {
  return calculateRateForInterval(ClockUtils::durationToMs(stat_interval));
}

std::string MovingIntervalRateStat::toString() {
  return std::to_string(value());
}

uint64_t MovingIntervalRateStat::calculateRateForInterval(uint64_t interval_to_calculate_ms) {
  if (!initialized_) {
    return 0;
  }

  uint64_t now_ms = ClockUtils::timePointToMs(clock_->now());
  uint64_t start_of_requested_interval = now_ms - interval_to_calculate_ms;
  uint64_t interval_start_time = std::max(start_of_requested_interval, current_window_start_ms_);
  uint32_t intervals_to_pass = (interval_start_time - current_window_start_ms_) / interval_size_ms_;
  //  We check if it's within the data we have

  if (intervals_to_pass >=  intervals_in_window_) {
    return 0;
  }

  int added_intervals = 0;
  uint64_t total_sum = 0;
  uint32_t moving_interval =  getNextInterval(current_interval_ + intervals_to_pass);
  do {
    added_intervals++;
    total_sum += (*sample_vector_.get())[moving_interval];
    moving_interval = getNextInterval(moving_interval);
  } while (moving_interval != current_interval_);

  double last_value_part_in_interval = static_cast<double>(now_ms - (current_window_end_ms_ - interval_size_ms_))
    /interval_size_ms_;
  double proportional_value = last_value_part_in_interval * (*sample_vector_.get())[current_interval_];
  if (last_value_part_in_interval < 1) {
    total_sum += proportional_value;
  } else {
    total_sum += (*sample_vector_.get())[current_interval_];
    last_value_part_in_interval = 0;
    added_intervals++;
  }
  // Didn't pass enough time to know the rate
  if (now_ms == interval_start_time) {
    return 0;
  }
  double rate = static_cast<double> (total_sum) / (now_ms - interval_start_time);
  return (rate * 1000 * scale_);
}

uint32_t MovingIntervalRateStat::getIntervalForTimeMs(uint64_t time_ms) {
  return ((time_ms - calculation_start_ms_)/interval_size_ms_) % intervals_in_window_;
}

uint32_t MovingIntervalRateStat::getNextInterval(uint32_t interval) {
  return (interval + 1) % intervals_in_window_;
}

void MovingIntervalRateStat::updateWindowTimes() {
  current_window_end_ms_ = calculation_start_ms_ + accumulated_intervals_ * interval_size_ms_;
  current_window_start_ms_ = calculation_start_ms_ +
    (accumulated_intervals_ - std::min(accumulated_intervals_, static_cast<uint64_t>(intervals_in_window_)))
    * interval_size_ms_;
}

MovingAverageStat::MovingAverageStat(uint32_t window_size)
  :sample_vector_{std::make_shared<std::vector<uint64_t>>(window_size, 0)},
  window_size_{window_size}, next_sample_position_{0}, current_average_{0} {
}

MovingAverageStat::~MovingAverageStat() {
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
  return static_cast<uint64_t>(std::round(current_average_));
}

uint64_t MovingAverageStat::value(uint32_t sample_number) {
  return static_cast<uint64_t>(getAverage(sample_number));
}

std::string MovingAverageStat::toString() {
  return std::to_string(value());
}

void MovingAverageStat::add(uint64_t value) {
  if (next_sample_position_ < window_size_) {
    current_average_  = static_cast<double>(current_average_ * next_sample_position_ + value)
      / (next_sample_position_ + 1);
  } else {
    uint64_t old_value = (*sample_vector_.get())[next_sample_position_ % window_size_];
    if (value > old_value) {
      current_average_ = current_average_ + static_cast<double>(value - old_value) / window_size_;
    } else {
      current_average_ = current_average_ - static_cast<double>(old_value - value) / window_size_;
    }
  }

  (*sample_vector_.get())[next_sample_position_ % window_size_] = value;
  next_sample_position_++;
}

double MovingAverageStat::getAverage(uint32_t sample_number) {
  uint64_t current_sample_position = next_sample_position_ - 1;
  //  We won't calculate an average for more than the window size
  sample_number = std::min(sample_number, window_size_);
  //  Check if we have enough samples
  sample_number = std::min(static_cast<uint64_t>(sample_number), current_sample_position);
  uint64_t calculated_sum = 0;
  for (uint32_t i = 0; i < sample_number;  i++) {
    calculated_sum += (*sample_vector_.get())[(current_sample_position - i) % window_size_];
  }
  return static_cast<double>(calculated_sum)/sample_number;
}

}  // namespace erizo
