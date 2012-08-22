#ifndef MEDIARECEIVER_H
#define MEDIARECEIVER_H

#include <node.h>
#include <MediaDefinitions.h>


/*
 * Wrapper class of erizo::MediaReceiver
 */
class MediaReceiver : public node::ObjectWrap {
 public:

  erizo::MediaReceiver *me;
};

#endif