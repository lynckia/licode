#ifndef ERIZO_SRC_ERIZO_STATS_STATNODE_H_
#define ERIZO_SRC_ERIZO_STATS_STATNODE_H_

#include <string>
#include <map>
#include <memory>

#include "lib/Clock.h"

namespace erizo {

class StatNode {
 public:
  StatNode() {}
  virtual ~StatNode() {}

  virtual StatNode& operator[](std::string key);

  virtual StatNode& operator[](uint64_t key) { return (*this)[std::to_string(key)]; }

  template <typename Node>
  void insertStat(std::string key, Node&& stat) {  // NOLINT
    // forward ensures that Node type is passed to make_shared(). It would otherwise pass StatNode.
    if (node_map_.find(key) != node_map_.end()) {
      node_map_.erase(key);
    }
    node_map_.insert(std::make_pair(key, std::make_shared<Node>(std::forward<Node>(stat))));
  }

  virtual bool hasChild(std::string name) { return node_map_.find(name) != node_map_.end(); }

  virtual bool hasChild(uint64_t value) { return hasChild(std::to_string(value)); }

  virtual StatNode& operator+=(uint64_t value) { return *this; }

  virtual StatNode operator++(int value) { return StatNode{}; }

  virtual uint64_t value() { return 0; }

  virtual std::string toString();

 private:
  bool is_node_;
  std::map<std::string, std::shared_ptr<StatNode>> node_map_;
};

class StringStat : public StatNode {
 public:
  StringStat() : text_{} {}
  explicit StringStat(std::string text) : text_{text} {}
  explicit StringStat(const StringStat &string_stat) : text_{string_stat.text_} {}
  virtual ~StringStat() {}

  StatNode& operator=(std::string text);

  StatNode operator++(int value) override { return StringStat{text_}; }

  StatNode& operator+=(uint64_t value) override { return *this; }

  std::string toString() override;

  uint64_t value() override { return 0; }

 private:
  std::string text_;
};

class CumulativeStat : public StatNode {
 public:
  CumulativeStat() : total_{0} {}
  explicit CumulativeStat(uint64_t initial) : total_{initial} {}
  virtual ~CumulativeStat() {}

  StatNode& operator=(uint64_t initial);

  StatNode operator++(int value) override;

  StatNode& operator+=(uint64_t value) override;

  std::string toString() override { return std::to_string(total_); }

  uint64_t value() override { return total_; }

 private:
  uint64_t total_;
};

class RateStat : public StatNode {
 public:
  RateStat(duration period, double scale,
                     std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());
  ~RateStat() {}

  StatNode operator++(int value) override;

  StatNode& operator+=(uint64_t value) override;

  uint64_t value() override;

  std::string toString() override;

 private:
  void add(uint64_t value);
  void checkPeriod();

 private:
  duration period_;
  double scale_;
  time_point calculation_start_;
  uint64_t last_;
  uint64_t total_;
  uint64_t current_period_total_;
  uint64_t last_period_calculated_rate_;
  std::shared_ptr<Clock> clock_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_STATS_STATNODE_H_
