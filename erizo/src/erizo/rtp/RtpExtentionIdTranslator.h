#ifndef ERIZO_SRC_ERIZO_RTP_RTPEXTENTIONIDTRANSLATOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTPEXTENTIONIDTRANSLATOR_H_

#include <map>
#include <algorithm>
#include <ostream>
#include "RTPExtensions.h"
#include "RTPExtensionsMap.h"

namespace erizo {

class RtpExtentionIdTranslator {
 public:
  explicit RtpExtentionIdTranslator(RTPExtensionsMap dest = RTPExtensionsMap{})
    : dest{std::move(dest)}
  {}
  explicit RtpExtentionIdTranslator(const std::array<RTPExtensions, 10>& extentions)
    : RtpExtentionIdTranslator(RTPExtensionsMap(extentions))
  {}
  void setDestMap(const RTPExtensionsMap& dest);
  void setSourceMap(const RTPExtensionsMap& scr);
  void addScrMapItem(const std::pair<int, RTPExtensions>& item);
  void addDestMapItem(const std::pair<int, RTPExtensions>& item);

  int translateId(int id) const;
  const RTPExtensionsMap& getSrcMap() const {
    return src;
  }
  const RTPExtensionsMap& getDestMap() const {
    return dest;
  }
  RTPExtensions getSrcExtention(int id) const;
  const std::map<int, int>& getScr_to_dest() const;

  static constexpr uint8_t padding = 0x00;
  static constexpr uint8_t unknown = 0x00;

 protected:
  void UpdateMap();

 private:
  RTPExtensionsMap src;
  RTPExtensionsMap dest;
  std::map<int, int> scr_to_dest;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPEXTENTIONIDTRANSLATOR_H_
