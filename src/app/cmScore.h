//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmScore_h
#define cmScore_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Object for managing musical score data." kw:[score]}
  
  enum
  {
    kOkScRC = cmOkRC,
    kCsvFailScRC,
    kSyntaxErrScRC,
    kInvalidIdxScRC,
    kTimeLineFailScRC,
    kInvalidDynRefCntScRC,
    kMidiFileFailScRC,
    kPedalInvalidScRC,
    kFileFailScRC
  };

  enum
  {
    kInvalidEvtScId = 0,
    kTimeSigEvtScId,
    kKeySigEvtScId,
    kTempoEvtScId,
    kTrackEvtScId,
    kTextEvtScId,
    kNameEvtScId,
    kEOTrackEvtScId,
    kCopyEvtScId,
    kBlankEvtScId,
    kBarEvtScId,
    kPgmEvtScId,
    kCtlEvtScId,
    kNonEvtScId,
    kPedalEvtScId
  };

  // Flags used by cmScoreEvt_t.flags
  enum
  {
    kEvenScFl    = 0x001,        // This note is marked for evenness measurement
    kDynScFl     = 0x002,        // This note is marked for dynamics measurement
    kTempoScFl   = 0x004,        // This note is marked for tempo measurement
    kSkipScFl    = 0x008,        // This isn't a real event (e.g. tied note) skip over it
    kGraceScFl   = 0x010,        // This is a grace note
    kInvalidScFl = 0x020,        // This note has a calculated time
    kPedalDnScFl   = 0x040,        // This is a pedal down event (pitch holds the pedal id and durSecs holds the time the pedal will remain down.)
    kPedalUpScFl   = 0x080         // This is a pedal up event (pitch holds the pedal id)
  };


  // Id's used by cmScoreSet_t.varId and as indexes into
  // cmScoreSection_t.vars[].
  enum
  {
    kInvalidVarScId, // 0
    kEvenVarScId,    // 1
    kDynVarScId,     // 2
    kTempoVarScId,   // 3
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
    cmMidiByte_t pitch;        // MIDI pitch of this note or the MIDI pedal id of pedal down/up msg (64=sustain 65=sostenuto 66=soft)
    cmMidiByte_t vel;          // MIDI velocity of this note
    unsigned     flags;        // Attribute flags for this event
    unsigned     dynVal;       // Dynamcis value pppp to ffff (1 to 11) for this note.
    double       frac;         // Note's time value for tempo and non-grace evenness notes.
    unsigned     barNumb;      // Bar id of the measure containing this event.
    unsigned     barNoteIdx;   // Index of this note in this bar
    unsigned     csvRowNumb;   // File row number (not index) from which this record originated
    unsigned     perfSmpIdx;   // Time this event was performed or cmInvalidIdx if the event was not performed.
    unsigned     perfVel;      // Velocity of the performed note or 0 if the note was not performed.
    unsigned     perfDynLvl;   // Index into dynamic level ref. array assoc'd with perfVel  
    unsigned     line;         // Line number of this event in the score file.
    unsigned     csvEventId;   // EventId from CSV 'evt' column.
  } cmScoreEvt_t;

  // A 'set' is a collection of events that are grouped in time and all marked with a given attribute.
  // (e.g. eveness, tempo, dynamcs ... )
  typedef struct cmScoreSet_str
  {
    unsigned               varId;      // See kXXXVarScId flags above
    cmScoreEvt_t**         eleArray;   // Events that make up this set in time order
    unsigned               eleCnt;     // 
    cmScoreSection_t**     sectArray;  // Sections this set will be applied to
    unsigned               sectCnt;    // 
    unsigned*              symArray;   // symArray[sectCnt] - symbol name of all variables represented by this set (e.g '1a-e', '1b-e', '2-t', etc)
    unsigned*              costSymArray; // costSymArray[sectCnt] - same as symbols in symArray[] with 'c' prepended to front
    bool                   doneFl;
    double                 value;
    struct cmScoreSet_str* llink;      // cmScoreLoc_t setList link
  } cmScoreSet_t;

  typedef enum
  {
    kInvalidScMId,
    kRecdBegScMId,
    kRecdEndScMId,
    kFadeScMId,
    kPlayBegScMId,
    kPlayEndScMId  
  } cmMarkScMId_t;

  // score markers
  typedef struct cmScoreMarker_str
  {
    cmMarkScMId_t             markTypeId;  // marker type
    unsigned                  labelSymId;  // marker label
    struct cmScoreLoc_str*    scoreLocPtr; // score location of the marker
    unsigned                  csvRowIdx;   // score CSV file line assoc'd w/ this marker
    struct cmScoreMarker_str* link;        // cmScoreLoc_t.markList links
  } cmScoreMarker_t;

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
    cmScoreMarker_t*  markList;      // List of markers assigned to this location
  } cmScoreLoc_t;

  typedef void (*cmScCb_t)( void* arg, const void* data, unsigned byteCnt );

  typedef cmRC_t     cmScRC_t;
  typedef cmHandle_t cmScH_t;
  
  extern cmScH_t cmScNullHandle;

  const cmChar_t* cmScEvtTypeIdToLabel( unsigned id );
  const cmChar_t* cmScDynIdToLabel( unsigned id );
  const cmChar_t* cmScStatusToOpString( unsigned id );

  // Initialize a score object from a CSV File generated from a score spreadsheet.
  // The dynRefArray[dynRefCnt] and cbFunc(cbArg) are optional if these 
  // features are not used.
  // If provided the dynRefArray[] is copied into an internal array.
  // The physical array passed here therefore does not need to remain valid.
  // Set 'srate' to zero if the score will not be used to perform measurement calculations.
  // The symbol table is only necessary if valid symbols are to be assigned to the cmScoreSet_t.symArray[].
  cmScRC_t      cmScoreInitialize( cmCtx_t* ctx, cmScH_t* hp, const cmChar_t* fn, double srate, const unsigned* dynRefArray, unsigned dynRefCnt, cmScCb_t cbFunc, void* cbArg, cmSymTblH_t stH );
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

  // Given a bar number return the associated 'bar' event record.
  const cmScoreEvt_t* cmScoreBarEvt( cmScH_t h, unsigned barNumb );

  // Given a csvEventId return the associated event
  const cmScoreEvt_t* cmScoreIdToEvt( cmScH_t h, unsigned csvEventId );

  // Access section records
  unsigned      cmScoreSectionCount( cmScH_t h );
  cmScoreSection_t* cmScoreSection( cmScH_t h, unsigned idx );

  // Access the score location data
  unsigned      cmScoreLocCount( cmScH_t h );
  cmScoreLoc_t* cmScoreLoc( cmScH_t h, unsigned idx );
  void          cmScorePrintLoc( cmScH_t h );

  // Return the location associated with a given score event.
  cmScoreLoc_t* cmScoreEvtLoc( cmScH_t h, const cmScoreEvt_t* evt );

  // Return the count of sets.
  unsigned      cmScoreSetCount( cmScH_t h );

  unsigned      cmScoreMarkerLabelCount( cmScH_t h );
  unsigned      cmScoreMarkerLabelSymbolId( cmScH_t h, unsigned idx );
  const cmScoreMarker_t* cmScoreMarker( cmScH_t h, cmMarkScMId_t markMId, unsigned labelSymId );

  // Make callbacks for all events in the score. The callbacks
  // contain cmScMsg_t records serialized as a byte stream.
  // Use cmScoreDecode() to convert the byte string to a
  // cmScMsg_t record.
  cmScRC_t      cmScoreSeqNotify( cmScH_t h );
  cmScRC_t      cmScoreSeqNotifyCb( cmScH_t h, cmScCb_t cbFunc, void* cbArg );

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
  void          cmScorePrintSets( cmScH_t h, cmRpt_t* rpt );

  // Generate a new score file from a MIDI file.
  cmScRC_t      cmScoreFileFromMidi( cmCtx_t* ctx, const cmChar_t* midiFn, const cmChar_t* scoreFn );

  // Print open the score file 'fn' and report the contents.  This function
  // simply wraps calls to cmScoreInitialize() and cmScorePrint().
  void          cmScoreReport( cmCtx_t* ctx, const cmChar_t* fn, const cmChar_t* outFn );

  void          cmScoreTest( cmCtx_t* ctx, const cmChar_t* fn );
    
  //)
  
#ifdef __cplusplus
}
#endif

#endif
