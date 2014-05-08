#include "StringUtil.h"

namespace erizo {
  namespace stringutil {
    std::vector<std::string> splitOneOf(const std::string& str,
                                        const std::string& delims,
                                        const size_t maxSplits) {
      std::string remaining(str);
      std::vector<std::string> result;
      size_t splits = 0, pos;

      while (((maxSplits == 0) || (splits < maxSplits)) &&
             ((pos = remaining.find_first_of(delims)) != std::string::npos)) {
        result.push_back(remaining.substr(0, pos));
        remaining = remaining.substr(pos + 1);
        splits++;
      }

      if (remaining.length() > 0)
        result.push_back(remaining);

      return result;
    }
  }
}
