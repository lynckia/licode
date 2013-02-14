#ifndef MEDIARECEIVER_H
#define MEDIARECEIVER_H

#include <node.h>
#include <MediaDefinitions.h>


/*
 * Wrapper class of erizo::MediaReceiver
 */
class MediaSink : public node::ObjectWrap {
 public:

  erizo::MediaSink *me;
};

/*
 * Wrapper class of erizo::MediaSource
 */
class MediaSource : public node::ObjectWrap {
 public:

  erizo::MediaSource *me;
};

#endif
