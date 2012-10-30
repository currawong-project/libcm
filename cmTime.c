#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmTime.h"

void cmTimeGet( cmTimeSpec_t* t )
{ clock_gettime(CLOCK_REALTIME,t); }


// this assumes that the seconds have been normalized to a recent start time
// so as to avoid overflow
unsigned cmTimeElapsedMicros( const cmTimeSpec_t* t0, const cmTimeSpec_t* t1 )
{
  // convert seconds to usecs
  long u0 = t0->tv_sec * 1000000;
  long u1 = t1->tv_sec * 1000000;

  // convert nanoseconds to usec
  u0 += t0->tv_nsec / 1000;
  u1 += t1->tv_nsec / 1000;

  // take diff between t1 and t0
  return u1 - u0;
}
