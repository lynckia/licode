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
  }
  uint32_t corresponding_interval = getIntervalForTimeMs(now_ms);
  std::cout << "corresponding interval " << corresponding_interval << " next_interval " << current_interval_ <<
    "total intervals " << intervals_in_window_ << std::endl;
  if (corresponding_interval > current_interval_ + 1) {
    for (int i = current_interval_; i < corresponding_interval; i = getNextInterval(i)) {
      std::cout << "setting 0 to " << i << std::endl;
      samples_[i] = 0;
      accumulated_intervals_++;
    }
  }
  std::cout << "setting next_interval to " << corresponding_interval << " and adding value " << value << std::endl;
  current_interval_ = corresponding_interval;
  samples_[current_interval_]+= value;
  accumulated_intervals_++;
  std::cout << " samples_[ " << current_interval_ << "] is " << samples_[current_interval_] << std::endl;
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
  // calculate where the interval fits, take the proportional part of the samples of the interval
  // sum up and divide the rest
  //  We check if it's within the data we have
  //
  std::cout << "Calculate Rate for Interval " << interval_to_calculate_ms << " calculation_start_ms_ "
    << calculation_start_ms_ << " interval size " << interval_size_ms_ << " current_interval_ "
    << current_interval_ << std::endl;
  uint64_t real_interval = std::min(interval_to_calculate_ms, (accumulated_intervals_ * interval_size_ms_));
  real_interval = std::min(real_interval, (intervals_in_window_ * interval_size_ms_));
  uint64_t now_ms = ClockUtils::timePointToMs(clock_->now());
  int added_intervals = 0;
  uint64_t total_sum = 0;
  std::cout << " real_interval " << real_interval << " so calculating from " << now_ms - real_interval <<
    " to " << now_ms << std::endl;
  for (int i = getIntervalForTimeMs(now_ms - real_interval); i <= current_interval_; i = getNextInterval(i)) {
    std::cout << "adding position " << i << " value " << samples_[i] << std::endl;
    added_intervals++;
    total_sum += samples_[i];
  }
  std::cout << "finally " << total_sum << " added intervals " << added_intervals << std::endl;

  double rate = static_cast<double> (total_sum) / (added_intervals*interval_size_ms_);
  std::cout << "Rate " << rate << std::endl;
  return (rate * 1000 * scale_);
}

uint32_t MovingIntervalRateStat::getIntervalForTimeMs(uint64_t time_ms) {
  double calc = static_cast<double>(time_ms - calculation_start_ms_);
  uint32_t corresponding_interval = ((time_ms - calculation_start_ms_)/interval_size_ms_) % intervals_in_window_;
  std::cout << "time " << time_ms << " corresponding interval " << corresponding_interval << " calc "
    << calc << std::endl;
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
