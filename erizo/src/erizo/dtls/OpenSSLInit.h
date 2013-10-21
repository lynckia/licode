#if !defined(RESIP_OPENSSLINIT_H)
#define RESIP_OPENSSLINIT_H 

#if defined(HAVE_CONFIG_H)
  #include "config.h"
#endif

//#include "rutil/Mutex.h"
//#include "rutil/RWMutex.h"
//#include "rutil/Data.h"
#include <cassert>
#include <vector>
#include <boost/thread/mutex.hpp>

// This will not be built or installed if USE_SSL is not defined; if you are 
// building against a source tree, and including this, and getting linker 
// errors, the source tree was probably built with this flag off. Either stop
// including this file, or re-build the source tree with SSL enabled.

struct CRYPTO_dynlock_value
{
      boost::mutex* mutex;
};

extern "C"
{
  void resip_OpenSSLInit_lockingFunction(int mode, int n, const char* file, int line);
  unsigned long resip_OpenSSLInit_threadIdFunction();
  CRYPTO_dynlock_value* resip_OpenSSLInit_dynCreateFunction(char* file, int line);
  void resip_OpenSSLInit_dynDestroyFunction(CRYPTO_dynlock_value*, const char* file, int line);
  void resip_OpenSSLInit_dynLockFunction(int mode, struct CRYPTO_dynlock_value*, const char* file, int line);
}

namespace resip
{

class OpenSSLInit
{
   public:
      static bool init();
   private:
	   OpenSSLInit();
	   ~OpenSSLInit();
      friend void ::resip_OpenSSLInit_lockingFunction(int mode, int n, const char* file, int line);
      friend unsigned long ::resip_OpenSSLInit_threadIdFunction();
      friend CRYPTO_dynlock_value* ::resip_OpenSSLInit_dynCreateFunction(char* file, int line);
      friend void ::resip_OpenSSLInit_dynDestroyFunction(CRYPTO_dynlock_value*, const char* file, int line);
      friend void ::resip_OpenSSLInit_dynLockFunction(int mode, struct CRYPTO_dynlock_value*, const char* file, int line);

      //static Mutex* mMutexes;
      static boost::mutex* mMutexes;
      static volatile bool mInitialized;
};
static bool invokeOpenSSLInit = OpenSSLInit::init();

}

#endif
