#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "DtlsTimer.h"
#include <sys/time.h>
#include <iostream>

using namespace dtls;
using namespace std;

DtlsTimer::DtlsTimer(unsigned int seq)
{
   mValid=true;
}

DtlsTimer::~DtlsTimer() 
{
}

void
DtlsTimer::fire() 
{
   if(mValid)
   {
      expired();
   }
   else
   {
      //memory mangement is overly tricky and possibly wrong...deleted by target
      //if valid is the contract. weak pointers would help.
      delete this;
   }
}

void
DtlsTimerContext::fire(DtlsTimer *timer)
{
   timer->fire();
}

long long getTimeMS() {
  struct timeval start;
  gettimeofday(&start, NULL);
  long timeMs = ((start.tv_sec) * 1000 + start.tv_usec/1000.0);
  return timeMs;
}

void
TestTimerContext::addTimer(DtlsTimer *timer, unsigned int lifetime)
{
   if(mTimer)
      delete mTimer;

   mTimer=timer;

   long timeMs = getTimeMS();

   mExpiryTime=timeMs+lifetime;

}

long long
TestTimerContext::getRemainingTime()
{
   long long timeMs = getTimeMS();

   if(mTimer)
   {
      if(mExpiryTime<timeMs)
         return(0);

      return(mExpiryTime-timeMs);
   }
   else
   {
      #if defined(WIN32) && !defined(__GNUC__)
         return 18446744073709551615ui64;
      #else
         return 18446744073709551615ULL;
      #endif
   }
}

void
TestTimerContext::updateTimer()
{
   long long timeMs = getTimeMS();

   if(mTimer) 
   {
      if(mExpiryTime<timeMs)
      {
         DtlsTimer *tmpTimer=mTimer;
         mTimer=0;

         fire(tmpTimer);
      }
   }
}


/* ====================================================================

 Copyright (c) 2007-2008, Eric Rescorla and Derek MacDonald 
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are 
 met:
 
 1. Redistributions of source code must retain the above copyright 
    notice, this list of conditions and the following disclaimer. 
 
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution. 
 
 3. None of the contributors names may be used to endorse or promote 
    products derived from this software without specific prior written 
    permission. 
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ==================================================================== */
