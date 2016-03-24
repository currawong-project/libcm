#ifndef cmScoreMatchGraphic_h
#define cmScoreMatchGraphic_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkSmgRC = cmOkRC,
    kFileSmgRC,
    kScoreFailSmgRC,
    kMidiFileFailSmgRC
  };
  
  typedef cmRC_t     cmSmgRC_t;
  typedef cmHandle_t cmSmgH_t;

  extern cmSmgH_t cmSmgNullHandle;

  cmSmgRC_t cmScoreMatchGraphicAlloc( cmCtx_t* ctx, cmSmgH_t* hp, const cmChar_t* scoreFn, const cmChar_t* midiFn );
  cmSmgRC_t cmScoreMatchGraphicFree( cmSmgH_t* hp );
  bool      cmScoreMatchGraphicIsValid( cmSmgH_t h );
  cmSmgRC_t cmScoreMatchGraphicInsertMidi( cmSmgH_t h, unsigned midiUid, unsigned midiPitch, unsigned midiVel, unsigned csvScoreEventId );
  cmSmgRC_t cmScoreMatchGraphicWrite( cmSmgH_t h, const cmChar_t* fn );
  
  
#ifdef __cplusplus
}
#endif

#endif
