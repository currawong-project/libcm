//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmMidiScoreFollow_h
#define cmMidiScoreFollow_h


#ifdef __cplusplus
extern "C" {
#endif
  
  //( { file_desc:"Score follow a MIDI files." kw[score] }
  //
  // This function uses a CSV score file generated from cmXScoreTest() to score follow a MIDI file.
  // Output filesL
  // - MIDI file with velocities from the score applied to the associated notes in the MIDI file.
  // - A text file, for use with cmTimeLine, which describes the bar positions as absolute times into the score.
  // - An SVG file which shows the score match results over time for each note in the score.
  // - A report file which lists the score match status over time.
  
  

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
  //)
  
#ifdef __cplusplus
}
#endif


#endif
