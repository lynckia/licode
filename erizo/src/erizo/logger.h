/**
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*     http:// www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef ERIZO_SRC_ERIZO_LOGGER_H_
#define ERIZO_SRC_ERIZO_LOGGER_H_

#include <stdlib.h>
#include <stdio.h>

#include <log4cxx/logger.h>
#include <log4cxx/helpers/exception.h>

#include <map>
#include <string>
#include <utility>
#include <type_traits>

class LogContext {
 public:
  LogContext() : context_log_{""} {
  }

  virtual ~LogContext() = default;

  void setLogContext(const std::map<std::string, std::string>& context) {
    context_ = context;
    context_log_ = "";
    for (const auto &item : context) {
      context_log_ += item.first + ": " + item.second + ", ";
    }
  }

  void copyLogContextFrom(const LogContext& log_context) {
    setLogContext(log_context.context_);
  }

  const std::string& printLogContext() const {
    return context_log_;
  }

 private:
  std::string context_log_;
  std::map<std::string, std::string> context_;
};

#define DECLARE_LOGGER() \
static log4cxx::LoggerPtr logger;

#define DEFINE_LOGGER(namespace, logName) \
log4cxx::LoggerPtr namespace::logger = log4cxx::Logger::getLogger(logName);

#define ELOG_MAX_BUFFER_SIZE 10000

#define SPRINTF_ELOG_MSG(buffer, fmt, args...) \
char buffer[ELOG_MAX_BUFFER_SIZE]; \
snprintf(buffer, ELOG_MAX_BUFFER_SIZE, fmt, ##args);

#define ELOG_TRACE2(logger, fmt, args...) \
SPRINTF_ELOG_MSG(__tmp, fmt, ##args); \
LOG4CXX_TRACE(logger, __tmp);

#define ELOG_DEBUG2(logger, fmt, args...) \
SPRINTF_ELOG_MSG(__tmp, fmt, ##args); \
LOG4CXX_DEBUG(logger, __tmp);

#define ELOG_INFO2(logger, fmt, args...) \
SPRINTF_ELOG_MSG(__tmp, fmt, ##args); \
LOG4CXX_INFO(logger, __tmp);

#define ELOG_WARN2(logger, fmt, args...) \
SPRINTF_ELOG_MSG(__tmp, fmt, ##args); \
LOG4CXX_WARN(logger, __tmp);

#define ELOG_ERROR2(logger, fmt, args...) \
SPRINTF_ELOG_MSG(__tmp, fmt, ##args); \
LOG4CXX_ERROR(logger, __tmp);

#define ELOG_FATAL2(logger, fmt, args...) \
SPRINTF_ELOG_MSG(__tmp, fmt, ##args); \
LOG4CXX_FATAL(logger, __tmp);

namespace detail {
// Helper for forwarding correctly the object to be logged
template <typename T>
struct LogElementForwarder {
  T operator()(T t) {
    return t;
  }
};

template <>
struct LogElementForwarder<std::string> {
  template <typename S>
  const char* operator()(const S& t) {
    return t.c_str();
  }
};
}  // namespace detail

#define DEFINE_ELOG_T(name, invoke) \
template <typename Logger, typename... Args> \
inline void name(const Logger&, const char*, Args...) __attribute__((always_inline)); \
\
template <typename Logger, typename... Args> \
void name(const Logger& logger, const char* fmt, Args... args) { \
  invoke(logger, fmt, detail::LogElementForwarder<typename std::decay<Args>::type>{}(args)...); \
} \
\
template <typename Logger> \
inline void name(const Logger&, const char*) __attribute__((always_inline)); \
\
template <typename Logger> \
void name(const Logger& logger, const char* fmt) { \
  invoke(logger, "%s", fmt); \
}

DEFINE_ELOG_T(ELOG_TRACET, ELOG_TRACE2)
DEFINE_ELOG_T(ELOG_DEBUGT, ELOG_DEBUG2)
DEFINE_ELOG_T(ELOG_INFOT, ELOG_INFO2)
DEFINE_ELOG_T(ELOG_WARNT, ELOG_WARN2)
DEFINE_ELOG_T(ELOG_ERRORT, ELOG_ERROR2)
DEFINE_ELOG_T(ELOG_FATALT, ELOG_FATAL2)

// older versions of log4cxx don't support tracing
#ifdef LOG4CXX_TRACE
#define ELOG_TRACE(fmt, args...) \
if (logger->isTraceEnabled()) { \
  ELOG_TRACET(logger, fmt, ##args); \
}
#else
#define ELOG_TRACE(fmt, args...) \
if (logger->isDebugEnabled()) { \
  ELOG_DEBUGT(logger, fmt, ##args); \
}
#endif

#define ELOG_DEBUG(fmt, args...) \
if (logger->isDebugEnabled()) { \
  ELOG_DEBUGT(logger, fmt, ##args); \
}

#define ELOG_INFO(fmt, args...) \
if (logger->isInfoEnabled()) { \
  ELOG_INFOT(logger, fmt, ##args); \
}

#define ELOG_WARN(fmt, args...) \
if (logger->isWarnEnabled()) { \
  ELOG_WARNT(logger, fmt, ##args); \
}

#define ELOG_ERROR(fmt, args...) \
if (logger->isErrorEnabled()) { \
  ELOG_ERRORT(logger, fmt, ##args); \
}

#define ELOG_FATAL(fmt, args...) \
if (logger->isFatalEnabled()) { \
  ELOG_FATALT(logger, fmt, ##args); \
}

#endif  // ERIZO_SRC_ERIZO_LOGGER_H_
