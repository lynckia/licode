#ifndef ERIZOAPI_IOTHREADPOOL_H_
#define ERIZOAPI_IOTHREADPOOL_H_

#include <nan.h>
#include <thread/IOThreadPool.h>


/*
 * Wrapper class of erizo::IOThreadPool
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class IOThreadPool : public Nan::ObjectWrap {
 public:
    static NAN_MODULE_INIT(Init);
    std::unique_ptr<erizo::IOThreadPool> me;

 private:
    IOThreadPool();
    ~IOThreadPool();

    /*
     * Constructor.
     * Constructs a IOThreadPool
     */
    static NAN_METHOD(New);
    /*
     * Closes the IOThreadPool.
     * The object cannot be used after this call
     */
    static NAN_METHOD(close);
    /*
     * Starts all workers in the IOThreadPool
     */
    static NAN_METHOD(start);

    static Nan::Persistent<v8::Function> constructor;
};

#endif  // ERIZOAPI_IOTHREADPOOL_H_
