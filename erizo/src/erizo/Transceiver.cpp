
/* 
 * Transceiver.cpp
 */

#include "Transceiver.h"

namespace erizo {
  Transceiver::Transceiver(std::string id) : id_{id} {}
  Transceiver::~Transceiver() {}

  void Transceiver::setMediaStream(std::shared_ptr<MediaStream> stream) {
    stream_ = stream;
  }

  std::shared_ptr<MediaStream> Transceiver::getMediaStream() {
    return stream_;
  }

  void Transceiver::resetMediaStream() {
    stream_.reset();
  }

  bool Transceiver::hasMediaStream() {
    return stream_.get() != nullptr;
  }

  std::string Transceiver::getId() {
    return id_;
  }

  void Transceiver::setId(std::string id) {
    id_ = id;
  }

  bool Transceiver::isSending() {
    return hasMediaStream() ? !stream_->isPublisher() : false;
  }

  bool Transceiver::isReceiving() {
    return hasMediaStream() ? stream_->isPublisher() : false;
  }

  bool Transceiver::isInactive() {
    return !hasMediaStream();
  }
}
