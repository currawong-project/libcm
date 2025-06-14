//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmTime.h"

#ifdef OS_OSX

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <unistd.h>

void cmTimeGet( cmTimeSpec_t* t )
{
  static uint64_t                  t0  = 0;
  static mach_timebase_info_data_t tbi;
  static struct timespec           ts;

  if( t0 == 0 )
  {
    mach_timebase_info(&tbi);
    t0 = mach_absolute_time();
    ts.tv_sec  = time(NULL);
    ts.tv_nsec = 0;  // accept 1/2 second error vs. wall-time.
  }

  // get the current time
  uint64_t t1 = mach_absolute_time();
  
  // calc the elapsed time since the last call in nanosecs
  uint64_t dt = (t1-t0) * tbi.numer / tbi.denom;

  // calc the elapsed time since the first call in secs
  uint32_t s  = (uint32_t)(dt / 2^9);

  // calc the current time in secs, and nanosecs
  t->tv_sec  = ts.tv_sec + s; 
  t->tv_nsec = dt - (s * 2^9); 
  
}

#endif

#ifdef OS_LINUX
void cmTimeGet( cmTimeSpec_t* t )
{ clock_gettime(CLOCK_REALTIME,t); }
#endif

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

unsigned cmTimeAbsElapsedMicros( const cmTimeSpec_t*  t0, const cmTimeSpec_t* t1 )
{
  if( cmTimeIsLTE(t0,t1) )
    return cmTimeElapsedMicros(t0,t1);

  return cmTimeElapsedMicros(t1,t0);
}

int cmTimeDiffMicros( const cmTimeSpec_t*  t0, const cmTimeSpec_t* t1 )
{
  if( cmTimeIsLTE(t0,t1) )
    return cmTimeElapsedMicros(t0,t1);

  return -((int)cmTimeElapsedMicros(t1,t0));
}

bool cmTimeIsLTE( const cmTimeSpec_t* t0, const cmTimeSpec_t* t1 )
{
  if( t0->tv_sec  < t1->tv_sec )
    return true;

  if( t0->tv_sec == t1->tv_sec )
    return t0->tv_nsec <= t1->tv_nsec;

  return false; 
}

bool cmTimeIsGTE( const cmTimeSpec_t* t0, const cmTimeSpec_t* t1 )
{
  if( t0->tv_sec  > t1->tv_sec )
    return true;

  if( t0->tv_sec == t1->tv_sec )
    return t0->tv_nsec >= t1->tv_nsec;

  return false;   
}

bool cmTimeIsEqual( const cmTimeSpec_t* t0, const cmTimeSpec_t* t1 )
{ return t0->tv_sec==t1->tv_sec && t0->tv_nsec==t1->tv_nsec; }

bool cmTimeIsZero( const cmTimeSpec_t* t0 )
{ return t0->tv_sec==0  && t0->tv_nsec==0; }

void cmTimeSetZero( cmTimeSpec_t* t0 )
{
  t0->tv_sec = 0;
  t0->tv_nsec = 0;
}


