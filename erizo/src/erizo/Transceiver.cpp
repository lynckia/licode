
/*
 * Transceiver.cpp
 */

#include "Transceiver.h"

namespace erizo {
  Transceiver::Transceiver(std::string id, std::string kind) : id_{id}, kind_{kind}, added_to_sdp_{false} {}
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

  bool Transceiver::hadSenderBefore() {
    return !ssrc_.empty();
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
