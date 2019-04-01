#ifndef ERIZOAPI_ASYNCPROMISEWORKER_H_
#define ERIZOAPI_ASYNCPROMISEWORKER_H_

#include <nan.h>

class AsyncPromiseWorker : public Nan::AsyncWorker {
 public:
  explicit AsyncPromiseWorker(Nan::Persistent<v8::Promise::Resolver> *persistent);
  ~AsyncPromiseWorker();
  void Execute() override;
  void HandleOKCallback() override;
  void HandleErrorCallback() override;

 private:
  Nan::Persistent<v8::Promise::Resolver> *_persistent;
};

#endif  // ERIZOAPI_ASYNCPROMISEWORKER_H_

