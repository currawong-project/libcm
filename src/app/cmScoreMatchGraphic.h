#ifndef cmScoreMatchGraphic_h
#define cmScoreMatchGraphic_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Implements the functionality of cmMidiScoreFollowMain()." kw[score] }

  enum
  {
    kOkSmgRC = cmOkRC,
    kFileSmgRC,
    kScoreFailSmgRC,
    kMidiFileFailSmgRC,
    kOpFailSmgRC,
    kMatchFailSmgRC
  };
  
  typedef cmRC_t     cmSmgRC_t;
  typedef cmHandle_t cmSmgH_t;

  extern cmSmgH_t cmSmgNullHandle;

  cmSmgRC_t cmScoreMatchGraphicAlloc( cmCtx_t* ctx, cmSmgH_t* hp, const cmChar_t* scoreFn, const cmChar_t* midiFn );
  cmSmgRC_t cmScoreMatchGraphicFree( cmSmgH_t* hp );
  bool      cmScoreMatchGraphicIsValid( cmSmgH_t h );
  cmSmgRC_t cmScoreMatchGraphicInsertMidi( cmSmgH_t h, unsigned midiUid, unsigned midiPitch, unsigned midiVel, unsigned csvScoreEventId );
  cmSmgRC_t cmScoreMatchGraphicWrite( cmSmgH_t h, const cmChar_t* fn );

  // Generate a set of markers for use in a cmTimeLine file which forms a marked area
  // beginning at each bar line and ends at the end of the file.
  cmSmgRC_t cmScoreMatchGraphicGenTimeLineBars( cmSmgH_t h, const cmChar_t* fn, unsigned srate );

  // Update the MIDI file velocity values and insert pedal events
  // from from score into MIDI file and then write the updated MIDI
  // file to 'newMidiFn'.
  cmSmgRC_t cmScoreMatchGraphicUpdateMidiFromScore( cmCtx_t* ctx, cmSmgH_t h, const cmChar_t* newMidiFn );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
