/*
 *CodecFactory.cpp
 */
#include "CodecFactory.h"
#include "VideoCodec.h"

#include <boost/cstdint.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
}

namespace erizo {

  CodecFactory* CodecFactory::getInstance(){
    if (theInstance==0){
      theInstance = new CodecFactory();
      avcodec_register_all();
    }
    return theInstance;
  }
  CodecFactory::CodecFactory(){

  }

}


