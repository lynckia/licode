/*
 * Stats.cpp
 *
 */

#include "Stats.h"
#include <sstream>
namespace erizo {
 
  std::string Stats::getString(){
    std::ostringstream theString;
    theString << "{";
    for (std::map<std::string, unsigned int>::iterator it=theStats_.begin(); it!=theStats_.end(); ++it){
      theString << "\"" << it->first << "\":\"" << it->second << "\",\n";
    }
    theString << "}";
    return theString.str(); 
  }

}
