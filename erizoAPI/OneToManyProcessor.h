#ifndef ERIZOAPI_ONETOMANYPROCESSOR_H_
#define ERIZOAPI_ONETOMANYPROCESSOR_H_

#include <nan.h>
#include <OneToManyProcessor.h>
#include <MediaStream.h>
#include "MediaDefinitions.h"
#include "MediaStream.h"
#include "ExternalInput.h"
#include "ExternalOutput.h"


/*
 * Wrapper class of erizo::OneToManyProcessor
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class OneToManyProcessor : public MediaSink {
 public:
    static NAN_MODULE_INIT(Init);
    std::shared_ptr<erizo::OneToManyProcessor> me;

 private:
    OneToManyProcessor();
    ~OneToManyProcessor();

    /*
     * Constructor.
     * Constructs a OneToManyProcessor
     */
    static NAN_METHOD(New);
    /*
     * Closes the OneToManyProcessor.
     * The object cannot be used after this call
     */
    static NAN_METHOD(close);
    /*
     * Sets the Publisher
     * Param: the WebRtcConnection of the Publisher
     */
    static NAN_METHOD(setPublisher);
    /*
     * Adds an ExternalOutput
     * Param: The ExternalOutput
     */
    static NAN_METHOD(addExternalOutput);
    /*
     * Sets an External Publisher
     * Param: the ExternalInput of the Publisher
     */
    static NAN_METHOD(setExternalPublisher);
    /*
     * Gets the Publisher state
     * Param: none
     */
    static NAN_METHOD(getPublisherState);
    /*
     * Returns true if OneToManyProcessor has a publisher
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

    static Nan::Persistent<v8::Function> constructor;
};

#endif  // ERIZOAPI_ONETOMANYPROCESSOR_H_
