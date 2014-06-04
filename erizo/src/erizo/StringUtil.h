#ifndef STRINGUTIL_H_
#define STRINGUTIL_H_
#include <string>

#include <vector>

namespace erizo {

    namespace stringutil {

    std::vector<std::string> splitOneOf(const std::string& str,
                                        const std::string& delims,
                                        const size_t maxSplits = 0);

    }

}

#endif // STRINGUTIL_H_
