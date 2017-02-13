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
}  // namespace erizo
