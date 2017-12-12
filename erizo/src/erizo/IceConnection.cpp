/*
 * IceConnection.cpp
 */

#include <cstdio>
#include <string>
#include <cstring>
#include <vector>

#include "IceConnection.h"

namespace erizo {

DEFINE_LOGGER(IceConnection, "IceConnection")

IceConnection::IceConnection(IceConnectionListener* listener, const IceConfig& ice_config) :
  listener_{listener}, ice_state_{INITIAL}, ice_config_{ice_config} {
    for (unsigned int i = 1; i <= ice_config_.ice_components; i++) {
      comp_state_list_[i] = INITIAL;
    }
  }

IceConnection::~IceConnection() {
  this->listener_ = nullptr;
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


IceState IceConnection::checkIceState() {
  return ice_state_;
}

std::string IceConnection::iceStateToString(IceState state) const {
  switch (state) {
    case IceState::INITIAL:             return "initial";
    case IceState::FINISHED:            return "finished";
    case IceState::FAILED:              return "failed";
    case IceState::READY:               return "ready";
    case IceState::CANDIDATES_RECEIVED: return "cand_received";
  }
  return "unknown";
}

void IceConnection::updateIceState(IceState state) {
  if (state <= ice_state_) {
    if (state != IceState::READY) {
      const auto toLog_str = toLog();
      const auto iceState_str = iceStateToString(ice_state_);
      const auto new_iceState_str = iceStateToString(state);
      ELOG_WARN("%s message: unexpected ice state transition, iceState: %s,  newIceState: %s",
              toLog_str.c_str(), iceState_str.c_str(), new_iceState_str.c_str());
    }
    return;
  }

  const auto toLog_str = toLog();
  const auto iceState_str = iceStateToString(ice_state_);
  const auto new_iceState_str = iceStateToString(state);
  ELOG_INFO("%s message: iceState transition, ice_config_.transport_name: %s, iceState: %s, newIceState: %s, this: %p",
             toLog_str.c_str(), ice_config_.transport_name.c_str(),
             iceState_str.c_str(), new_iceState_str.c_str(), this);
  this->ice_state_ = state;
  switch (ice_state_) {
    case IceState::FINISHED:
      return;
    case IceState::FAILED:
      ELOG_WARN("%s message: Ice Failed", toLog_str.c_str());
      break;

    case IceState::READY:
    case IceState::CANDIDATES_RECEIVED:
      break;
    default:
      break;
  }

  // Important: send this outside our state lock.  Otherwise, serious risk of deadlock.
  if (this->listener_ != NULL)
    this->listener_->updateIceState(state, this);
}

}  // namespace erizo
