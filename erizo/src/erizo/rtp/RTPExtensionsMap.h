#ifndef ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONSMAP_H_
#define ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONSMAP_H_

#include <map>
#include <array>
#include "RTPExtensions.h"

namespace erizo {

class RTPExtensionsMap : public std::map<int, RTPExtensions> {
 public:
  RTPExtensionsMap() = default;
  explicit RTPExtensionsMap(const std::map<int, RTPExtensions>& other);
  explicit RTPExtensionsMap(const std::array<RTPExtensions, 10>& extentions);
  std::array<RTPExtensions, 10> toArray() const;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONSMAP_H_
