/*
 * Srtpchannel.h
 */

#ifndef ERIZO_SRC_ERIZO_SRTPCHANNEL_H_
#define ERIZO_SRC_ERIZO_SRTPCHANNEL_H_

#include <netinet/in.h>
#include <srtp2/srtp.h>
#include <boost/thread/mutex.hpp>

#include <string>

#include "rtp/RtpHeaders.h"
#include "./logger.h"

namespace erizo {

/**
 * A SRTP data Channel.
 * Represents a SRTP Channel with keys for protecting and unprotecting RTP and RTCP data.
 */
class SrtpChannel {
  DECLARE_LOGGER();
  static bool initialized;
  static boost::mutex sessionMutex_;

 public:
  /**
   * The constructor. At this point the class is only initialized but it still needs the Key pair.
   */
  SrtpChannel();
  virtual ~SrtpChannel();
  /**
   * Protects RTP Data
   * @param buffer Pointer to the buffer with the data. The protected data is returned here
   * @param len Pointer to the length of the data. The length is returned here
   * @return 0 or an error code
   */
  int protectRtp(char* buffer, int *len);
  /**
   * Unprotects RTP Data
   * @param buffer Pointer to the buffer with the data. The unprotected data is returned here
   * @param len Pointer to the length of the data. The length is returned here
   * @return 0 or an error code
   */
  int unprotectRtp(char* buffer, int *len);
  /**
   * Protects RTCP Data
   * @param buffer Pointer to the buffer with the data. The protected data is returned here
   * @param len Pointer to the length of the data. The length is returned here
   * @return 0 or an error code
   */
  int protectRtcp(char* buffer, int *len);
  /**
   * Unprotects RTCP Data
   * @param buffer Pointer to the buffer with the data. The unprotected data is returned here
   * @param len Pointer to the length of the data. The length is returned here
   * @return 0 or an error code
   */
  int unprotectRtcp(char* buffer, int *len);
  /**
   * Sets a key pair for the RTP channel
   * @param sendingKey The key for protecting data
   * @param receivingKey The key for unprotecting data
   * @return true if everything is ok
   */
  bool setRtpParams(const std::string &sendingKey, const std::string &receivingKey);
  /**
   * Sets a key pair for the RTCP channel
   * @param sendingKey The key for protecting data
   * @param receivingKey The key for unprotecting data
   * @return true if everything is ok
   */
  bool setRtcpParams(const std::string &sendingKey, const std::string &receivingKey);

 private:
  enum TransmissionType {
    SENDING, RECEIVING
  };

  bool configureSrtpSession(srtp_t *session, const std::string &key, enum TransmissionType type);

  bool active_;
  srtp_t send_session_;
  srtp_t receive_session_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_SRTPCHANNEL_H_
