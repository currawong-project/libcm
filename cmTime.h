
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

  // Return the elapsed time (t1 - t0)
  // in microseconds
  unsigned cmTimeElapsedMicros
    ( 
      const 
      cmTimeSpec_t* 
      t0,  //< ptr to start time
      const cmTimeSpec_t* t1 );// ptr to end time


  //)
  //}

#ifdef __cplusplus
}
#endif


#endif

