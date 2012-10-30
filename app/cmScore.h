#ifndef cmScore_h
#define cmScore_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkScRC = cmOkRC,
    kCsvFailScRC,
    kSyntaxErrScRC,
    kInvalidIdxScRC,
    kTimeLineFailScRC
  
  };

  enum
  {
    kInvalidEvtScId = 0,
    kTimeSigEvtScId,
    kKeySigEvtScId,
    kTempoEvtScId,
    kTrackEvtScId,
    kTextEvtScId,
    kEOTrackEvtScId,
    kCopyEvtScId,
    kBlankEvtScId,
    kBarEvtScId,
    kPgmEvtScId,
    kCtlEvtScId,
    kNonEvtScId
  };

  enum
  {
    kEvenScFl  = 0x01,  // This note is marked for evenness measurement
    kDynScFl   = 0x02,  // This note is marked for dynamics measurement
    kTempoScFl = 0x03,  // This note is marked for tempo measurement
    kSkipScFl  = 0x04   // this isn't a real event (e.g. tied note) skip over it
  };

  typedef struct
  {
    unsigned     type;         // Event type
    double       dsecs;        // 
    cmMidiByte_t pitch;        // MIDI pitch of this note
    unsigned     flags;        // Attribute flags for this event
    unsigned     dynVal;       // Dynamcis value pppp to ffff (1 to 11) for this note.
    unsigned     barNumb;      // bar number of this event
    unsigned     barNoteIdx;   // index of this note in this bar
  } cmScoreEvt_t;


  typedef cmRC_t     cmScRC_t;
  typedef cmHandle_t cmScH_t;
  
  extern cmScH_t cmScNullHandle;

  // Initialize a score object from a CSV File generated from a score spreadsheet.
  cmScRC_t      cmScoreInitialize( cmCtx_t* ctx, cmScH_t* hp, const cmChar_t* fn );
  cmScRC_t      cmScoreFinalize( cmScH_t* hp );

  bool          cmScoreIsValid( cmScH_t h );

  // Access the score data.
  unsigned      cmScoreEvtCount( cmScH_t h );
  cmScoreEvt_t* cmScoreEvt( cmScH_t h, unsigned idx );

  void          cmScorePrint( cmScH_t h, cmRpt_t* rpt );

  cmScRC_t      cmScoreSyncTimeLine( cmScH_t scH, cmTlH_t tlH, unsigned editDistWndCnt, cmReal_t maxNoteOffsetSecs );

  cmScRC_t      cmScoreSyncTimeLineTest( cmCtx_t* ctx,  const cmChar_t* timeLineJsFn, const cmChar_t* scoreCsvFn );

  void           cmScoreTest( cmCtx_t* ctx, const cmChar_t* fn );

#ifdef __cplusplus
}
#endif

#endif
