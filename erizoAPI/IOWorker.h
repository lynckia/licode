#ifndef ERIZOAPI_IOWORKER_H_
#define ERIZOAPI_IOWORKER_H_

#include <nan.h>
#include <thread/IOWorker.h>


/*
 * Wrapper class of erizo::IOWorker
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class IOWorker : public Nan::ObjectWrap {
 public:
    static NAN_MODULE_INIT(Init);
    std::unique_ptr<erizo::IOWorker> me;

 private:
    IOWorker();
    ~IOWorker();

    /*
     * Constructor.
     * Constructs a IOWorker
     */
    static NAN_METHOD(New);
    /*
     * Closes the IOWorker.
     * The object cannot be used after this call
     */
    static NAN_METHOD(close);
    /*
     * Starts all workers in the IOWorker
     */
    static NAN_METHOD(start);

    static Nan::Persistent<v8::Function> constructor;
};

#endif  // ERIZOAPI_IOWORKER_H_
