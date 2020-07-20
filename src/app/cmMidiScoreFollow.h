#ifndef cmMidiScoreFollow_h
#define cmMidiScoreFollow_h


#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkMsfRC = cmOkRC,
    kFailMsfRC
  };

  typedef cmRC_t cmMsfRC_t;
  

  cmMsfRC_t cmMidiScoreFollowMain(
    cmCtx_t* ctx,
    const cmChar_t* scoreCsvFn,      // score CSV file as generated from cmXScoreTest().
    const cmChar_t* midiFn,          // MIDI file to track
    const cmChar_t* matchRptOutFn,   // Score follow status report 
    const cmChar_t* matchSvgOutFn,   // Score follow graphic report
    const cmChar_t* midiOutFn,       // (optional) midiFn with apply sostenuto and velocities from the score to the MIDI file
    const cmChar_t* tlBarOutFn       // (optional) bar positions sutiable for use in a cmTimeLine description file.
                                  );
  
#ifdef __cplusplus
}
#endif


#endif
