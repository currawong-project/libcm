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
    kEvenScFl    = 0x01,        // This note is marked for evenness measurement
    kDynScFl     = 0x02,        // This note is marked for dynamics measurement
    kTempoScFl   = 0x04,        // This note is marked for tempo measurement
    kSkipScFl    = 0x08,        // This isn't a real event (e.g. tied note) skip over it
    kInvalidScFl = 0x10         // This note has a calculated time
  };

  struct cmScoreLoc_str;

  // The score can be divided into arbitrary non-overlapping sections.
  typedef struct
  {
    const cmChar_t*        label;      // section label
    struct cmScoreLoc_str* locPtr;     // location where this section starts
    unsigned               begIndex;   // score element index where this section starts
    double                 evenCoeff;  // 
    double                 dynCoeff;   //
    double                 tempoCeoff; //
  } cmScoreSection_t;

  typedef struct
  {
    unsigned     type;         // Event type
    double       secs;         // Time location in seconds 
    double       durSecs;      // Duration in seconds
    unsigned     index;        // Index of this event in the event array.
    cmMidiByte_t pitch;        // MIDI pitch of this note
    unsigned     flags;        // Attribute flags for this event
    unsigned     dynVal;       // Dynamcis value pppp to ffff (1 to 11) for this note.
    unsigned     barNumb;      // Bar id of the measure containing this event.
    unsigned     barNoteIdx;   // Index of this note in this bar
    unsigned     csvRowNumb;   // File row number (not index) from which this record originated
    unsigned     perfSmpIdx;   // Time this event was performed or cmInvalidIdx if the event was not performed.
    unsigned     perfVel;      // Velocity of the performed note or 0 if the note was not performed.
  } cmScoreEvt_t;

  typedef struct cmScoreSet_str
  {
    unsigned           typeFl;     // See kXXXScFl flags above
    cmScoreEvt_t**     eleArray;   // Events that make up this set in time order
    unsigned           eleCnt;     // 
    cmScoreSection_t** sectArray;  // Array of pointers to sections to apply this set to
    unsigned           sectCnt;    //
    struct cmScoreSet_str* link;   // cmScoreLoc_t setList link
  } cmScoreSet_t;
  

  // All events which are simultaneous are collected into a single
  // cmScoreLoc_t record.
  typedef struct cmScoreLoc_str
  {
    double            secs;          // Time of this location
    unsigned          evtCnt;        // Count of events in evtArray[].
    cmScoreEvt_t**    evtArray;      // Events which occur at this time.
    unsigned          barNumb;       // Bar number this event is contained by.                            
    cmScoreSet_t*     setList;       // Set's which end on this time location
    cmScoreSection_t* begSectPtr;    // NULL if this location does not start a section
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

  // Validate the score handle
  bool          cmScoreIsValid( cmScH_t h );

  // Access the score data.
  unsigned      cmScoreEvtCount( cmScH_t h );
  cmScoreEvt_t* cmScoreEvt( cmScH_t h, unsigned idx );

  // Access the score location data
  unsigned      cmScoreLocCount( cmScH_t h );
  cmScoreLoc_t* cmScoreLoc( cmScH_t h, unsigned idx );
  void          cmScorePrintLoc( cmScH_t h );

  // Make callbacks for all events in the score. The callbacks
  // contain cmScMsg_t records serialized as a byte stream.
  // Use cmScoreDecode() to convert the byte string to a
  // cmScMsg_t record.
  cmScRC_t      cmScoreSeqNotify( cmScH_t h );

  void          cmScoreClearPerfInfo( cmScH_t h );
  void          cmScoreSetPerfEvent( cmScH_t h, unsigned locIdx, unsigned smpIdx, unsigned pitch, unsigned vel );
  
  
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

  // Decode a serialized cmScMsg_t from a byte stream as passed to the 
  // cmScCb_t function.
  cmScRC_t      cmScoreDecode( const void* msg, unsigned msgByteCnt, cmScMsg_t* );

  void          cmScorePrint( cmScH_t h, cmRpt_t* rpt );

  void          cmScoreTest( cmCtx_t* ctx, const cmChar_t* fn );


#ifdef __cplusplus
}
#endif

#endif
