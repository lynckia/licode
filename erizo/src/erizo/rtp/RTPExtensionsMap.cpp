#include "RTPExtensionsMap.h"

using erizo::RTPExtensions;
using erizo::RTPExtensionsMap;

RTPExtensionsMap::RTPExtensionsMap(const std::map<int, RTPExtensions>& other)
    : map{other}
{}

RTPExtensionsMap::RTPExtensionsMap(const std::array<RTPExtensions, 10>& extentions) {
  for (size_t i = 0; i < extentions.size(); ++i) {
    if (extentions[i] != UNKNOWN) {
      insert(std::make_pair(i, extentions[i]));
    }
  }
}

std::array<RTPExtensions, 10> RTPExtensionsMap::toArray() const {
  std::array<RTPExtensions, 10> result{UNKNOWN};
  for (const auto item : *this) {
    result[item.first] = item.second;
  }
  return result;
}
