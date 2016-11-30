#ifndef ERIZOAPI_ONETOMANYTRANSCODER_H_
#define ERIZOAPI_ONETOMANYTRANSCODER_H_

#include <nan.h>
#include <media/OneToManyTranscoder.h>
#include "MediaDefinitions.h"
#include "WebRtcConnection.h"


/*
 * Wrapper class of erizo::OneToManyTranscoder
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class OneToManyTranscoder : public MediaSink {
 public:
  static NAN_MODULE_INIT(Init);
  erizo::OneToManyTranscoder* me;

 private:
  OneToManyTranscoder();
  ~OneToManyTranscoder();

  /*
   * Constructor.
   * Constructs a OneToManyTranscoder
   */
  static NAN_METHOD(New);
  /*
   * Closes the OneToManyTranscoder.
   * The object cannot be used after this call
   */
  static NAN_METHOD(close);
  /*
   * Sets the Publisher
   * Param: the WebRtcConnection of the Publisher
   */
  static NAN_METHOD(setPublisher);
   /*
   * Returns true if OneToManyTranscoder has a publisher
   */
  static NAN_METHOD(hasPublisher);
  /*
   * Sets the subscriber
   * Param1: the WebRtcConnection of the subscriber
   * Param2: an unique Id for the subscriber
   */
  static NAN_METHOD(addSubscriber);
  /*
   * Removes a subscriber given its peer id
   * Param: the peerId
   */
  static NAN_METHOD(removeSubscriber);
};

#endif  // ERIZOAPI_ONETOMANYTRANSCODER_H_
