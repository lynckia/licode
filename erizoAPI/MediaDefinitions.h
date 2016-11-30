#ifndef ERIZOAPI_MEDIADEFINITIONS_H_
#define ERIZOAPI_MEDIADEFINITIONS_H_

#include <nan.h>
#include <MediaDefinitions.h>


/*
 * Wrapper class of erizo::MediaSink
 */
class MediaSink : public Nan::ObjectWrap {
 public:
    erizo::MediaSink* msink;
};


/*
 * Wrapper class of erizo::MediaSource
 */
class MediaSource : public Nan::ObjectWrap {
 public:
    erizo::MediaSource* msource;
};

#endif  // ERIZOAPI_MEDIADEFINITIONS_H_
