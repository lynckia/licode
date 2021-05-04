
/*
 * Transceiver.cpp
 */

#include "Transceiver.h"

namespace erizo {
  Transceiver::Transceiver(std::string id, std::string kind)
    : id_{id}, kind_{kind}, added_to_sdp_{false}, stopped_{false} {}
  Transceiver::Transceiver(uint32_t id, std::string kind)
    : id_{std::to_string(id)}, kind_{kind}, added_to_sdp_{false}, stopped_{false} {}
  Transceiver::~Transceiver() {}

  void Transceiver::setReceiver(std::shared_ptr<MediaStream> stream) {
    receiver_ = stream;
    receiver_label_ = stream->getLabel();
  }

  void Transceiver::resetReceiver() {
    receiver_.reset();
  }

  std::shared_ptr<MediaStream> Transceiver::getReceiver() {
    return receiver_;
  }

  std::string Transceiver::getReceiverLabel() {
    return receiver_label_;
  }

  void Transceiver::setSender(std::shared_ptr<MediaStream> stream) {
    sender_ = stream;
    if (kind_ == "video") {
      ssrc_ = std::to_string(stream->getVideoSinkSSRC());
    } else {
      ssrc_ = std::to_string(stream->getAudioSinkSSRC());
    }
    sender_label_ = stream->getLabel();
  }

  void Transceiver::resetSender() {
    sender_.reset();
    ssrc_ = "";
  }

  std::shared_ptr<MediaStream> Transceiver::getSender() {
    return sender_;
  }

  std::string Transceiver::getSenderLabel() {
    return sender_label_;
  }

  bool Transceiver::hasSender() {
    return sender_.get() != nullptr;
  }

  void Transceiver::stop() {
    if (isInactive()) {
      stopped_ = true;
    }
  }

  bool Transceiver::isStopped() {
    return stopped_;
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

  void Transceiver::setId(uint32_t id) {
    id_ = std::to_string(id);
  }

  bool Transceiver::isInactive() {
    return !hasSender() && !hasReceiver();
  }

  void Transceiver::setAsAddedToSdp() {
    added_to_sdp_ = true;
  }

  bool Transceiver::hasBeenAddedToSdp() {
    return added_to_sdp_;
  }

  std::string Transceiver::getKind() {
    return kind_;
  }

  std::string Transceiver::getSsrc() {
    return ssrc_;
  }
}  // namespace erizo
