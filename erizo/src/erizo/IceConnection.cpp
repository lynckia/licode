/*
 * IceConnection.cpp
 */

#include <cstdio>
#include <string>
#include <cstring>
#include <vector>

#include "IceConnection.h"

namespace erizo {

IceConnection::IceConnection(IceConnectionListener* listener, const IceConfig& ice_config) :
  listener_{listener}, ice_state_{INITIAL}, ice_config_{ice_config} {
    for (unsigned int i = 1; i <= ice_config_.ice_components; i++) {
      comp_state_list_[i] = INITIAL;
    }
  }

IceConnection::~IceConnection() {
}

void IceConnection::setIceListener(IceConnectionListener *listener) {
  listener_ = listener;
}

IceConnectionListener* IceConnection::getIceListener() {
  return listener_;
}

std::string IceConnection::getLocalUsername() {
  return ufrag_;
}

std::string IceConnection::getLocalPassword() {
  return upass_;
}

}  // namespace erizo
