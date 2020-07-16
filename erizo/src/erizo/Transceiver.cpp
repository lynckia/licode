
/*
 * Transceiver.cpp
 */

#include "Transceiver.h"

namespace erizo {
  Transceiver::Transceiver(std::string id) : id_{id} {}
  Transceiver::~Transceiver() {}

  void Transceiver::setReceiver(std::shared_ptr<MediaStream> stream) {
    receiver_ = stream;
  }

  void Transceiver::resetReceiver() {
    receiver_.reset();
  }

  std::shared_ptr<MediaStream> Transceiver::getReceiver() {
    return receiver_;
  }

  void Transceiver::setSender(std::shared_ptr<MediaStream> stream) {
    sender_ = stream;
  }

  void Transceiver::resetSender() {
    sender_.reset();
  }

  std::shared_ptr<MediaStream> Transceiver::getSender() {
    return sender_;
  }

  bool Transceiver::hasSender() {
    return sender_.get() != nullptr;
  }

  bool Transceiver::hasReceiver() {
    return receiver_.get() != nullptr;
  }

  std::string Transceiver::getId() {
    return id_;
  }

  void Transceiver::setId(std::string id) {
    id_ = id;
  }

  bool Transceiver::isInactive() {
    return !hasSender() && !hasReceiver();
  }
}  // namespace erizo
