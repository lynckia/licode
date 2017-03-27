/*
Copyright (c) 2016 Erik Rigtorp <erik@rigtorp.se>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#ifndef ERIZO_SRC_ERIZO_LIB_TOKENBUCKET_H_
#define ERIZO_SRC_ERIZO_LIB_TOKENBUCKET_H_

#include <atomic>
#include <memory>

#include "ClockUtils.h"

namespace erizo {

class TokenBucket {
 public:
  explicit TokenBucket(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<SteadyClock>());

  TokenBucket(const uint64_t rate, const uint64_t burst_size,
              std::shared_ptr<erizo::Clock> the_clock = std::make_shared<SteadyClock>());

  TokenBucket(const TokenBucket &other);

  TokenBucket& operator=(const TokenBucket &other);

  void reset(const uint64_t rate, const uint64_t burst_size);

  bool consume(const uint64_t tokens);

 private:
  std::atomic<uint64_t> time_;
  std::atomic<uint64_t> time_per_token_;
  std::atomic<uint64_t> time_per_burst_;

  std::shared_ptr<erizo::Clock> clock_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_LIB_TOKENBUCKET_H_
