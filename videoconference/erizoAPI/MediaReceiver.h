#ifndef MEDIARECEIVER_H
#define MEDIARECEIVER_H

#include <node.h>
#include <MediaDefinitions.h>


class MediaReceiver : public node::ObjectWrap {
 public:

  erizo::MediaReceiver *me;
};

#endif