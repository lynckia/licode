#ifndef MEDIARECEIVER_H
#define MEDIARECEIVER_H

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

#endif
