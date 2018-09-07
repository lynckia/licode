#include "RtpExtentionIdTranslator.h"


using erizo::RTPExtensions;
using erizo::RTPExtensionsMap;
using erizo::RtpExtentionIdTranslator;

void RtpExtentionIdTranslator::UpdateMap() {
  scr_to_dest.clear();
  for (const auto& scr_item : RtpExtentionIdTranslator::src) {
    auto elem = scr_item.second;
    const auto dest_item =
        std::find_if(dest.begin(), dest.end(), [elem](const std::pair<int, RTPExtensions>& pair) {
          return pair.second == elem;
        });
    if (dest_item != dest.end()) {
      scr_to_dest[scr_item.first] = dest_item->first;
    }
  }
}

RTPExtensions RtpExtentionIdTranslator::getSrcExtention(int id) const {
  const auto item = src.find(id);
  return item != src.end()? item->second : UNKNOWN;
}

const std::map<int, int>& RtpExtentionIdTranslator::getScr_to_dest() const {
  return scr_to_dest;
}

int RtpExtentionIdTranslator::translateId(int id) const {
  auto item = scr_to_dest.find(id);
  return item != scr_to_dest.end() ? item->second : unknown;
}

void RtpExtentionIdTranslator::setDestMap(const RTPExtensionsMap& dest) {
  RtpExtentionIdTranslator::dest = dest;
  UpdateMap();
}
void RtpExtentionIdTranslator::setSourceMap(const RTPExtensionsMap& scr) {
  RtpExtentionIdTranslator::src = scr;
  UpdateMap();
}
void RtpExtentionIdTranslator::addScrMapItem(const std::pair<int, RTPExtensions>& item) {
  src.insert(item);
  UpdateMap();
}
void RtpExtentionIdTranslator::addDestMapItem(const std::pair<int, RTPExtensions>& item) {
  dest.insert(item);
  UpdateMap();
}
