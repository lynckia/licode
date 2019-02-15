#ifndef ERIZOAPI_ASYNCPROMISEWORKER_H_
#define ERIZOAPI_ASYNCPROMISEWORKER_H_

#include <nan.h>

class AsyncPromiseWorker : public Nan::AsyncWorker {
 public:
  explicit AsyncPromiseWorker(Nan::Persistent<v8::Promise::Resolver> *persistent);
  ~AsyncPromiseWorker();
  virtual void Execute() = 0;
  void HandleOKCallback();

 private:
  Nan::Persistent<v8::Promise::Resolver> *_persistent;
};

#endif  // ERIZOAPI_ASYNCPROMISEWORKER_H_
