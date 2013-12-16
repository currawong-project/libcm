
//{ { label:cmTime
//    kw: [ time ] }
//
//( 
// This interface is used to read the systems high resolution timer and 
// calculate elapsed time.
//)


#ifndef cmTime_h   
#define cmTime_h

#ifdef __cplusplus
extern "C" {
#endif

  //(

  typedef  struct timespec cmTimeSpec_t;

  /* 
     get the time 
   */
  void cmTimeGet( cmTimeSpec_t* t );

  // Return the elapsed time (t1 - t0) in microseconds
  // t1 is assumed to be at a later time than t0.
  unsigned cmTimeElapsedMicros( const cmTimeSpec_t*  t0, const cmTimeSpec_t* t1 );

  
  // Same as cmTimeElapsedMicros() but the times are not assumed to be ordered.
  // The function therefore begins by swapping t1 and t0 if t0 is after t1.
  unsigned cmTimeAbsElapsedMicros( const cmTimeSpec_t*  t0, const cmTimeSpec_t* t1 ); 

  //)
  //}

#ifdef __cplusplus
}
#endif


#endif

