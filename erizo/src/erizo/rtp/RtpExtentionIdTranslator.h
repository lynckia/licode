//
// Created by bandyer on 03/09/18.
//

#ifndef ERIZO_SRC_ERIZO_RTP_RTPEXTENTIONIDTRANSLATOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTPEXTENTIONIDTRANSLATOR_H_

#include <map>
#include <algorithm>
#include <ostream>
#include "RTPExtensions.h"

namespace erizo {

class RTPExtensionsMap : public std::map<int, RTPExtensions> {
 public:
  RTPExtensionsMap() = default;
  explicit RTPExtensionsMap(const std::map<int, RTPExtensions>& other) : map{other} {}
  explicit RTPExtensionsMap(const std::array<RTPExtensions, 10>& extentions) {
    for (size_t i = 0; i < extentions.size(); ++i) {
      if (extentions[i] != UNKNOWN) {
        insert(std::make_pair(i, extentions[i]));
      }
    }
  }
  std::array<RTPExtensions, 10> toArray() const {
    std::array<RTPExtensions, 10> result{UNKNOWN};
    for (const auto item : *this) {
      result[item.first] = item.second;
    }
    return result;
  }
  friend std::ostream& operator<<(std::ostream& os, const RTPExtensionsMap& map) {
    os << "{ ";
    for (const auto& item : map) {
      os << "{ " << item.first << ", " << item.second << "} ";
    }
    os << "}";
    return os;
  }
};

class RtpExtentionIdTranslator {
 public:
  explicit RtpExtentionIdTranslator(RTPExtensionsMap dest = RTPExtensionsMap{})
    : dest{std::move(dest)}
  {}
  explicit RtpExtentionIdTranslator(const std::array<RTPExtensions, 10>& extentions)
    : RtpExtentionIdTranslator(RTPExtensionsMap(extentions))
  {}
  void setDestMap(const RTPExtensionsMap& dest) {
    RtpExtentionIdTranslator::dest = dest;
    UpdateMap();
  }
  void setSourceMap(const RTPExtensionsMap& scr) {
    RtpExtentionIdTranslator::src = scr;
    UpdateMap();
  }
  void addScrMapItem(const std::pair<int, RTPExtensions>& item) {
    src.insert(item);
    UpdateMap();
  }
  void addDestMapItem(const std::pair<int, RTPExtensions>& item) {
    dest.insert(item);
    UpdateMap();
  }

  int translateId(int id) const {
    auto item = scr_to_dest.find(id);
    return item != scr_to_dest.end() ? item->second : padding;
  }
  const RTPExtensionsMap& getSrcMap() const {
    return src;
  }
  const RTPExtensionsMap& getDestMap() const {
    return dest;
  }
  RTPExtensions getSrcExtention(int id) const {
    const auto item = src.find(id);
    return item != src.end()? item->second : UNKNOWN;
  }
  friend std::ostream& operator<<(std::ostream& os, const RtpExtentionIdTranslator& translator) {
    os << "src: " << translator.src << "\ndest: " << translator.dest << "\nscr_to_dest: {";
    for (const auto& item : translator.scr_to_dest) {
      os << "{ " << item.first << ", " << item.second << "}";
    }
    os << "}";
    return os;
  }
  static constexpr uint8_t padding = 0x00;

 protected:
  void UpdateMap() {
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

 private:
  RTPExtensionsMap src;
  RTPExtensionsMap dest;
  std::map<int, int> scr_to_dest;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPEXTENTIONIDTRANSLATOR_H_
