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
    kTimeLineFailScRC,
    kInvalidDynRefCntScRC
  
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

  // Flags used by cmScoreEvt_t.flags
  enum
  {
    kEvenScFl    = 0x01,        // This note is marked for evenness measurement
    kDynScFl     = 0x02,        // This note is marked for dynamics measurement
    kTempoScFl   = 0x04,        // This note is marked for tempo measurement
    kSkipScFl    = 0x08,        // This isn't a real event (e.g. tied note) skip over it
    kGraceScFl   = 0x10,        // This is a grace note
    kInvalidScFl = 0x20         // This note has a calculated time
  };


  // Id's used by cmScoreSet_t.varId and as indexes into
  // cmScoreSection_t.vars[].
  enum
  {
    kEvenVarScId,
    kDynVarScId,
    kTempoVarScId,
    kScVarCnt
  };

  struct cmScoreLoc_str;
  struct cmScoreSet_str;

  // The score can be divided into arbitrary non-overlapping sections.
  typedef struct
  {
    const cmChar_t*         label;             // section label
    unsigned                index;             // index of this record in the internal section array
    struct cmScoreLoc_str*  locPtr;            // location where this section starts
    unsigned                begEvtIndex;       // score element index where this section starts    
    unsigned                setCnt;            // Count of elements in setArray[]
    struct cmScoreSet_str** setArray;          // Ptrs to sets which are applied to this section.
    double                  vars[ kScVarCnt ]; // Set to DBL_MAX by default.
  } cmScoreSection_t;

  typedef struct
  {
    unsigned     type;         // Event type
    double       secs;         // Time location in seconds 
    double       durSecs;      // Duration in seconds
    unsigned     index;        // Index of this event in the event array.
    unsigned     locIdx;       // Index of the location containing this event
    cmMidiByte_t pitch;        // MIDI pitch of this note
    unsigned     flags;        // Attribute flags for this event
    unsigned     dynVal;       // Dynamcis value pppp to ffff (1 to 11) for this note.
    double       frac;         // Note's time value for tempo and non-grace evenness notes.
    unsigned     barNumb;      // Bar id of the measure containing this event.
    unsigned     barNoteIdx;   // Index of this note in this bar
    unsigned     csvRowNumb;   // File row number (not index) from which this record originated
    unsigned     perfSmpIdx;   // Time this event was performed or cmInvalidIdx if the event was not performed.
    unsigned     perfVel;      // Velocity of the performed note or 0 if the note was not performed.
    unsigned     perfDynLvl;   // Index into dynamic level ref. array assoc'd with perfVel  
  } cmScoreEvt_t;

  typedef struct cmScoreSet_str
  {
    unsigned               varId;      // See kXXXVarScId flags above
    cmScoreEvt_t**         eleArray;   // Events that make up this set in time order
    unsigned               eleCnt;     // 
    bool                   doneFl;
    double                 value;
    struct cmScoreSet_str* llink;      // cmScoreLoc_t setList link
  } cmScoreSet_t;
  

  // All events which are simultaneous are collected into a single
  // cmScoreLoc_t record.
  typedef struct cmScoreLoc_str
  {    
    unsigned          index;         // index of this location record
    double            secs;          // Time of this location
    unsigned          evtCnt;        // Count of events in evtArray[].
    cmScoreEvt_t**    evtArray;      // Events which occur at this time.
    unsigned          barNumb;       // Bar number this event is contained by.                            
    cmScoreSet_t*     setList;       // Set's which end on this time location (linked through cmScoreSet_t.llink)
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
  // The dynRefArray[dynRefCnt] and cbFunc(cbArg) are optional if these 
  // features are not used.
  // If provided the dynRefArray[] is copied into an internal array.
  // The physical array passed here therefore does not need to remain valid.
  // Set 'srate' to zero if the score will not be used to perform measurement calculations.
  cmScRC_t      cmScoreInitialize( cmCtx_t* ctx, cmScH_t* hp, const cmChar_t* fn, double srate, const unsigned* dynRefArray, unsigned dynRefCnt, cmScCb_t cbFunc, void* cbArg );
  cmScRC_t      cmScoreFinalize(   cmScH_t* hp );

  // Filename of last successfuly loaded score file.
  const cmChar_t* cmScoreFileName( cmScH_t h );
  
  // Sample rate as set in cmScoreInitialize()
  double          cmScoreSampleRate( cmScH_t h );

  // Validate the score handle
  bool          cmScoreIsValid( cmScH_t h );

  // Access the score data.
  unsigned      cmScoreEvtCount( cmScH_t h );
  cmScoreEvt_t* cmScoreEvt( cmScH_t h, unsigned idx );

  // Access section records
  unsigned      cmScoreSectionCount( cmScH_t h );
  cmScoreSection_t* cmScoreSection( cmScH_t h, unsigned idx );

  // Access the score location data
  unsigned      cmScoreLocCount( cmScH_t h );
  cmScoreLoc_t* cmScoreLoc( cmScH_t h, unsigned idx );
  void          cmScorePrintLoc( cmScH_t h );

  // Return the count of sets.
  unsigned      cmScoreSetCount( cmScH_t h );

  // Make callbacks for all events in the score. The callbacks
  // contain cmScMsg_t records serialized as a byte stream.
  // Use cmScoreDecode() to convert the byte string to a
  // cmScMsg_t record.
  cmScRC_t      cmScoreSeqNotify( cmScH_t h );

  void          cmScoreClearPerfInfo( cmScH_t h );

  // Assign 'smpIdx' and 'vel'  to the event matching 'pitch' at 'locIdx'
  // but do not trigger any variable calculations. Return true if as a
  // result of this call all events assigned to 'locIdx' have been received
  // otherwise return false.
  bool          cmScoreSetPerfEvent(  cmScH_t h, unsigned locIdx, unsigned smpIdx, unsigned pitch, unsigned vel );

  // Assign 'smpIdx' and 'vel'  to the event matching 'pitch' at 'locIdx'
  // but and trigger any variable calculations which may happen on, or before, 'locIdx'.
  void          cmScoreExecPerfEvent( cmScH_t h, unsigned locIdx, unsigned smpIdx, unsigned pitch, unsigned vel );

  // Assign 'value' to the section at, or before, 'locIdx'.
  void          cmScoreSetPerfValue(  cmScH_t h, unsigned locIdx, unsigned varId, double value );
  
  // Set the performed dynamic level of a score event.
  void          cmScoreSetPerfDynLevel( cmScH_t h, unsigned evtIdx, unsigned dynLvl );

  typedef enum
  {
    kInvalidMsgScId,
    kBeginMsgScId,
    kEventMsgScId,
    kSectionMsgScId,
    kEndMsgScId,
    kVarMsgScId,
    kDynMsgScId
  } cmScMsgTypeId_t;

  typedef struct
  {
    unsigned varId;  // see kXXXVarScId from cmScoreSet_t.varId
    double   value;   // value of a variable
  } cmScMeas_t;

  typedef struct
  {
    unsigned evtIdx;
    unsigned dynLvl;
  } cmScDyn_t;

  typedef struct
  {
    cmScMsgTypeId_t typeId;
    union
    {
      cmScoreEvt_t     evt;      // only used when typeId == kEventMsgScId
      cmScMeas_t       meas;     // only used when typeId == kVarMsgScId 
      cmScoreSection_t sect;     // only used when typeId == kSectionMsgScId
      cmScDyn_t        dyn;      // only used when typeId == kDynLvlMsgScId
    } u;
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
