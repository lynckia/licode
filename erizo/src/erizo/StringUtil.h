#ifndef ERIZO_SRC_ERIZO_STRINGUTIL_H_
#define ERIZO_SRC_ERIZO_STRINGUTIL_H_

#include <string>
#include <vector>

namespace erizo {

namespace stringutil {

std::vector<std::string> splitOneOf(const std::string& str,
                                    const std::string& delims,
                                    const size_t maxSplits = 0);

}  // namespace stringutil

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_STRINGUTIL_H_
