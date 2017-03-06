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
  sample_vector_{std::make_shared<std::vector<uint64_t>>(intervals, 0)}, initialized_{false}, clock_{the_clock} {
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
  }
  int32_t intervals_to_pass = ((now_ms) -
      (calculation_start_ms_ + accumulated_intervals_* interval_size_ms_)) / interval_size_ms_;
  std::cout << "add: Value " << value << " now_ms " << now_ms << " Intervals to pass "
    << intervals_to_pass << std::endl;

  //  if sample is more than a window ahead from last sample
  //  We clean up and set the new value as the newest
  if (intervals_to_pass > 0) {
    if (intervals_to_pass >= intervals_in_window_) {
      std::cout << "cleaning the whole window " << intervals_to_pass << std::endl;
      sample_vector_->assign(intervals_in_window_, 0);
      uint32_t corresponding_interval = getIntervalForTimeMs(now_ms);
      std::cout << "The interval is " << corresponding_interval << std::endl;
      current_interval_ = (corresponding_interval + intervals_in_window_ - 1) % intervals_in_window_;
      (*sample_vector_.get())[corresponding_interval]+= value;
      accumulated_intervals_ += intervals_to_pass;
      return;
    }
    for (int i = 0; i < intervals_to_pass; i++) {
      current_interval_ = getNextInterval(current_interval_);
      sample_vector_->at(current_interval_) = 0;
      accumulated_intervals_++;
      std::cout << " passing interval " << current_interval_ << " accumulated " << accumulated_intervals_ << std::endl;
    }
  }

  uint32_t corresponding_interval = getIntervalForTimeMs(now_ms);
  if (corresponding_interval != current_interval_) {
    std::cout << "This is a new interval " << corresponding_interval << " value " << value << std::endl;
    sample_vector_->at(corresponding_interval) = 0;
    accumulated_intervals_++;
  }
  current_interval_ = corresponding_interval;
  (*sample_vector_.get())[current_interval_]+= value;
  std::cout << "Adding value " << value << " to interval " << corresponding_interval << " Result "
    << sample_vector_->at(corresponding_interval) << " accumulated intervals " << accumulated_intervals_ << std::endl;
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
  if (!initialized_) {
    return 0;
  }

  uint64_t now_ms = ClockUtils::timePointToMs(clock_->now());
  //  We check if it's within the data we have
  uint32_t available_window = std::min(accumulated_intervals_, static_cast<uint64_t>(intervals_in_window_));
  uint64_t min_requested_ms = std::max((now_ms - interval_to_calculate_ms),
      calculation_start_ms_ + ((accumulated_intervals_ - available_window)*interval_size_ms_));

  int64_t real_interval = (calculation_start_ms_ + (accumulated_intervals_ * interval_size_ms_)) -
    (min_requested_ms);

  if (real_interval < 0) {
    return 0;
  }
  int32_t intervals_to_pass = ((now_ms - real_interval) -
      (calculation_start_ms_ + available_window * interval_size_ms_) * interval_size_ms_) / interval_size_ms_;
  std::cout << "MEAN Intervals to pass " << intervals_to_pass << " real_interval " << real_interval << " accumulated "
   << accumulated_intervals_ << " now ms " << now_ms << " calculation start " << calculation_start_ms_
  << " interval size " << interval_size_ms_ << std::endl;
  intervals_to_pass = intervals_to_pass < 0 ? 0 : intervals_to_pass;  // can be less than zero in first value
  if (intervals_to_pass >= intervals_in_window_) {
    std::cout << " value not in window, returning 0 " << std::endl;
    return 0;
  }
  int added_intervals = 0;
  uint64_t total_sum = 0;
  uint32_t moving_interval =  getIntervalForTimeMs(now_ms - real_interval);
  std::cout << " get interval for time is " << moving_interval << std::endl;
  do {
    added_intervals++;
    total_sum += (*sample_vector_.get())[moving_interval];
    std::cout << " Adding to sum " << (*sample_vector_.get())[moving_interval] << " in pos "
      << moving_interval << std::endl;
    moving_interval = getNextInterval(moving_interval);
  } while (moving_interval != current_interval_);

  // if now is in the middle of an interval
  // add the proportional part of the current interval
  double interval_part = static_cast<double>((now_ms - (calculation_start_ms_ +
          ((accumulated_intervals_ - 1) * interval_size_ms_)))) / interval_size_ms_;
  double proportional_value = interval_part * sample_vector_->at(current_interval_);
  if (interval_part < 1) {
    total_sum += proportional_value;
    std::cout << " Adding proportion " << proportional_value << std::endl;
  } else {
    total_sum += (*sample_vector_.get())[current_interval_];
    interval_part = 0;
    added_intervals++;
    std::cout << " Adding full" << (*sample_vector_.get())[current_interval_] << " pos "
      << current_interval_ << std::endl;
  }
  uint64_t calculated_interval_size_ms = (added_intervals * interval_size_ms_) + interval_part;
  std::cout << " CalculateRate now " << now_ms << " last interval time " <<
    calculation_start_ms_ + (accumulated_intervals_ - 1)  * interval_size_ms_ <<
    " Part of the last interval " << interval_part << " proportion " << proportional_value
    << " proportional value " << proportional_value
    << " total sum " << total_sum << " calculated interval size " << calculated_interval_size_ms
    << std::endl;

  double rate = static_cast<double> (total_sum) / calculated_interval_size_ms;
  std::cout << "THE RATE " << rate << std::endl;
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
      static_cast<double>(value - (*sample_vector_.get())[next_sample_position_ % window_size_])/window_size_;
  }

  (*sample_vector_.get())[next_sample_position_ % window_size_] = value;
  next_sample_position_++;
  if (current_average_ == 0) {
    current_average_ = getAverage(window_size_);
  }
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
