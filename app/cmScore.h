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
    kEvenScFl  = 0x01,   // This note is marked for evenness measurement
    kDynScFl   = 0x02,   // This note is marked for dynamics measurement
    kTempoScFl = 0x03,   // This note is marked for tempo measurement
    kSkipScFl  = 0x04,   // This isn't a real event (e.g. tied note) skip over it
    kInvalidScFl = 0x08  // This note has a calculated time
  };

  typedef struct
  {
    unsigned     type;         // Event type
    double       secs;         // Time location in seconds 
    double       durSecs;      // Duration in seconds
    unsigned     index;        // index of this event
    cmMidiByte_t pitch;        // MIDI pitch of this note
    unsigned     flags;        // Attribute flags for this event
    unsigned     dynVal;       // Dynamcis value pppp to ffff (1 to 11) for this note.
    unsigned     barNumb;      // bar number of this event
    unsigned     barNoteIdx;   // index of this note in this bar
  } cmScoreEvt_t;

  typedef struct
  {
    double         secs;     // Time of this location
    unsigned       evtCnt;   // Count of events in evtArray[].
    cmScoreEvt_t** evtArray; // Events which occur at this time.
    unsigned       evtIdx;   // Index into the master event array 
                             // (p->array[]) of the first event in this loc.
    unsigned       barNumb;  // Bar number this event is contained by.
                            
  } cmScoreLoc_t;

  typedef void (*cmScCb_t)( void* arg, const void* data, unsigned byteCnt );

  typedef cmRC_t     cmScRC_t;
  typedef cmHandle_t cmScH_t;

  typedef void (*cmScCb_t)( void* arg, const void* data, unsigned byteCnt );
  
  extern cmScH_t cmScNullHandle;

  const cmChar_t* cmScEvtTypeIdToLabel( unsigned id );
  const cmChar_t* cmScDynIdToLabel( unsigned id );


  // Initialize a score object from a CSV File generated from a score spreadsheet.
  cmScRC_t      cmScoreInitialize( cmCtx_t* ctx, cmScH_t* hp, const cmChar_t* fn, cmScCb_t cbFunc, void* cbArg );
  cmScRC_t      cmScoreFinalize(   cmScH_t* hp );

  // Filename of last successfuly loaded score file.
  const cmChar_t* cmScoreFileName( cmScH_t h );

  bool          cmScoreIsValid( cmScH_t h );

  // Access the score data.
  unsigned      cmScoreEvtCount( cmScH_t h );
  cmScoreEvt_t* cmScoreEvt( cmScH_t h, unsigned idx );

  // Access the score location data
  unsigned      cmScoreLocCount( cmScH_t h );
  cmScoreLoc_t* cmScoreLoc( cmScH_t h, unsigned idx );


  cmScRC_t      cmScoreSeqNotify( cmScH_t h );
  
  typedef enum
  {
    kInvalidMsgScId,
    kBeginMsgScId,
    kEventMsgScId,
    kEndMsgScId
  } cmScMsgTypeId_t;

  typedef struct
  {
    cmScMsgTypeId_t typeId;
    cmScoreEvt_t    evt;    // only used when typeId == kEventMsgScId
  } cmScMsg_t;

  cmScRC_t      cmScoreDecode( const void* msg, unsigned msgByteCnt, cmScMsg_t* );

  void          cmScorePrint( cmScH_t h, cmRpt_t* rpt );

  cmScRC_t      cmScoreSyncTimeLine( cmScH_t scH, cmTlH_t tlH, unsigned editDistWndCnt, cmReal_t maxNoteOffsetSecs );

  cmScRC_t      cmScoreSyncTimeLineTest( cmCtx_t* ctx,  const cmChar_t* timeLineJsFn, const cmChar_t* scoreCsvFn );

  void          cmScoreTest( cmCtx_t* ctx, const cmChar_t* fn );

  void          cmScoreFix( cmCtx_t* ctx );

#ifdef __cplusplus
}
#endif

#endif
