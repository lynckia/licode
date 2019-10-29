#ifndef ERIZOAPI_THREADPOOL_H_
#define ERIZOAPI_THREADPOOL_H_

#include <nan.h>
#include <thread/ThreadPool.h>


/*
 * Wrapper class of erizo::ThreadPool
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class ThreadPool : public Nan::ObjectWrap {
 public:
    static NAN_MODULE_INIT(Init);
    std::unique_ptr<erizo::ThreadPool> me;

 private:
    ThreadPool();
    ~ThreadPool();

    /*
     * Constructor.
     * Constructs a ThreadPool
     */
    static NAN_METHOD(New);
    /*
     * Closes the ThreadPool.
     * The object cannot be used after this call
     */
    static NAN_METHOD(close);
    /*
     * Starts all workers in the ThreadPool
     */
    static NAN_METHOD(start);

    static NAN_METHOD(getDurationDistribution);
    static NAN_METHOD(resetStats);

    static Nan::Persistent<v8::Function> constructor;
};

#endif  // ERIZOAPI_THREADPOOL_H_
