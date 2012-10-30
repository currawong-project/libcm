#ifndef cmTimeLine_h
#define cmTimeLine_h

#ifdef __cplusplus
extern "C" {
#endif


  typedef cmHandle_t cmTlH_t;

  typedef cmRC_t cmTlRC_t;

  enum
  {
    kOkTlRC = cmOkRC,
    kLHeapFailTlRC,
    kParseFailTlRC,
    kJsonFailTlRC,
    kDuplNameTlRC,
    kRefNotFoundTlRC,
    kAudioFileFailTlRC,
    kMidiFileFailTlRC,
    kTypeCvtFailTlRC,
    kUnknownRecdTypeTlRC,
    kFinalizeFailTlRC,
    kInvalidSeqIdTlRC
  };

  typedef enum
  {
    kMidiFileTlId  = 0x01,
    kMidiEvtTlId   = 0x02,
    kAudioFileTlId = 0x03,
    kAudioEvtTlId  = 0x04,
    kMarkerTlId    = 0x05
  } cmTlObjTypeId_t;

  enum
  {
    kReservedTlFl = 0x01,
    kNoWriteTlFl  = 0x02  // do not write this object in cmTimeLineWrite()
  };

  typedef void (*cmTlCb_t)( void* arg, const void* data, unsigned byteCnt );

  typedef struct cmTlObj_str
  {
    void*               reserved;    // pt's to _cmTlObj_t 
    unsigned            seqId;       // sequence this object is assigned to
    const cmChar_t*     name;        // text name of this object
    unsigned            uid;         // generated unique id for this object
    cmTlObjTypeId_t     typeId;      // type of the object
    struct cmTlObj_str* ref;         // time reference object
    int                 begSmpIdx;   // start time of this object as an offset from the start time of the reference object
    unsigned            durSmpCnt;   // duration of this object
    int                 seqSmpIdx;   // absolute start time of this object within the sequence
    const cmChar_t*     text;        // points to text assoc'd with this node (file name for audio/midi file, marker text)
    unsigned            flags;       // see kXXXTlFl 
    void*               userDataPtr; // user customizable data pointer 
  } cmTlObj_t;

  typedef struct
  {
    cmTlObj_t     obj;
    cmMidiFileH_t h;
    unsigned      noteOnCnt;
    cmChar_t*     fn;
  } cmTlMidiFile_t;

  typedef struct
  {
    cmTlObj_t               obj;
    unsigned                midiFileObjId;
    const cmMidiTrackMsg_t* msg;       // w/ dticks converted to microsecs
  } cmTlMidiEvt_t;


  typedef struct
  {
    cmTlObj_t         obj;
    cmAudioFileH_t    h;
    cmAudioFileInfo_t info;
    cmChar_t*         fn;
  } cmTlAudioFile_t;

  typedef struct
  {
    cmTlObj_t       obj;
    cmAudioFileH_t  h;
    unsigned        smpIdx;
    unsigned        smpCnt;
    cmChar_t*       text;
  } cmTlAudioEvt_t;

  typedef struct
  {
    cmTlObj_t       obj;
    const cmChar_t* text;
  } cmTlMarker_t;

  extern cmTlH_t cmTimeLineNullHandle;

  // 
  cmTlRC_t   cmTimeLineInitialize( cmCtx_t* ctx, cmTlH_t* hp, cmTlCb_t cbFunc, void* cbArg );
  cmTlRC_t   cmTimeLineInitializeFromFile( cmCtx_t* ctx, cmTlH_t* hp, cmTlCb_t cbFunc, void* cbArg, const cmChar_t* fn );

  cmTlRC_t   cmTimeLineFinalize( cmTlH_t* hp );

  bool       cmTimeLineIsValid( cmTlH_t h );
  double     cmTimeLineSampleRate( cmTlH_t h );

  // Return the object following 'p' assigned to 'seqId'.
  // If 'p' is NULL then return the first object assigned to seqId.
  // If 'seqId' is set to cmInvalidId then return the next object on any seq.
  // If no objects follow 'p' on the specified sequence then return NULL.
  cmTlObj_t* cmTimeLineNextObj( cmTlH_t h, cmTlObj_t* p, unsigned seqId );

  // Same as cmTimeLineNextObj() but returns next object whose type matches 'typeId'.
  cmTlObj_t* cmTimeLineNextTypeObj( cmTlH_t h, cmTlObj_t* p, unsigned seqId, unsigned typeId );

  cmTlMidiFile_t*  cmTlNextMidiFileObjPtr(  cmTlH_t h, cmTlObj_t* op, unsigned seqId );
  cmTlAudioFile_t* cmTlNextAudioFileObjPtr( cmTlH_t h, cmTlObj_t* op, unsigned seqId );
  cmTlMidiEvt_t*   cmTlNextMidiEvtObjPtr(   cmTlH_t h, cmTlObj_t* op, unsigned seqId );
  cmTlAudioEvt_t*  cmTlNextAudioEvtObjPtr(  cmTlH_t h, cmTlObj_t* op, unsigned seqId );
  cmTlMarker_t*    cmTlNextMarkerObjPtr(    cmTlH_t h, cmTlObj_t* op, unsigned seqId );


  // Cast a genereic cmTlObj_t pointer to a specificy type.
  cmTlMidiFile_t*  cmTimeLineMidiFileObjPtr(  cmTlH_t h, cmTlObj_t* op );
  cmTlAudioFile_t* cmTimeLineAudioFileObjPtr( cmTlH_t h, cmTlObj_t* op );
  cmTlMidiEvt_t*   cmTimeLineMidiEvtObjPtr(   cmTlH_t h, cmTlObj_t* op );
  cmTlAudioEvt_t*  cmTimeLineAudioEvtObjPtr(  cmTlH_t h, cmTlObj_t* op );
  cmTlMarker_t*    cmTimeLineMarkerObjPtr(    cmTlH_t h, cmTlObj_t* op );

  // Same as cmTimeLineXXXObjPtr() but does not generate an error when
  // 'op' does not point to the correct type. These function quietly
  // return NULL if the requested type does not match.
  cmTlMidiFile_t*  cmTlMidiFileObjPtr(  cmTlH_t h, cmTlObj_t* op );
  cmTlAudioFile_t* cmTlAudioFileObjPtr( cmTlH_t h, cmTlObj_t* op );
  cmTlMidiEvt_t*   cmTlMidiEvtObjPtr(   cmTlH_t h, cmTlObj_t* op );
  cmTlAudioEvt_t*  cmTlAudioEvtObjPtr(  cmTlH_t h, cmTlObj_t* op );
  cmTlMarker_t*    cmTlMarkerObjPtr(    cmTlH_t h, cmTlObj_t* op );

  cmTlAudioFile_t* cmTimeLineFindAudioFile( cmTlH_t h, const cmChar_t* fn );
  cmTlMidiFile_t*  cmTimeLineFindMidiFile( cmTlH_t h, const cmChar_t* fn );

  // 'typeId' = kAudioFileTlId, kMidiFileTId, kMarkerTlId.
  // 'nameStr' and 'refObjNameStr' may be NULL.
  cmTlRC_t cmTimeLineInsert( 
    cmTlH_t         h, 
    const cmChar_t* nameStr, 
    unsigned        typeId, 
    const cmChar_t* fn_or_markerStr, 
    int             begSmpIdx, 
    unsigned        durSmpCnt, 
    const cmChar_t* refObjNameStr, 
    unsigned        seqId ); 
  
  // See src/data/tl0.json for an example JSON file.
  cmTlRC_t cmTimeLineReadJson(  cmTlH_t h, const cmChar_t* ifn );

  // Return a count of sequences contained within this timeline.
  unsigned cmTimeLineSeqCount( cmTlH_t h );

  // Make notifications for all records belonging to the sequence.
  cmTlRC_t cmTimeLineSeqNotify( cmTlH_t h, unsigned seqId );

  cmTlRC_t cmTimeLineWrite( cmTlH_t h, const cmChar_t* fn );

  cmTlRC_t cmTimeLinePrint( cmTlH_t h, cmRpt_t* rpt );
  cmTlRC_t cmTimeLinePrintFn( cmCtx_t* ctx, const cmChar_t* fn, cmRpt_t* rpt );

  cmTlRC_t cmTimeLineTest( cmCtx_t* ctx, const cmChar_t* jsFn  );

  // The time-line notifies listeners of initialization and finalization
  // events via calling a cmTlCbFunc_t function.  The argument to this 
  // function is a serialized cmTlUiMsg_t.  The recipient of the callback
  // can extract information from this message using cmTimeLineDecode()
  // to form a cmTlUiMsg_t record. Note that all pointers internal to the
  // cmTlUiMsg_t point into the message buffer itself.
  
  // id's used to indicate the type of a serialized object
  typedef enum
  {
    kInvalidMsgTlId,
    kInitMsgTlId,      // A new time-line object is begin intialized.  
    kFinalMsgTlId,     // A time-line object is being finalized.
    kDoneMsgTlId,      // All the objects assoc'd with a time line seq-notify have been sent.
    kInsertMsgTlId,    // A time-line object was inserted.
  } cmTlUiMsgTypeId_t;

  typedef struct
  {
    cmTlUiMsgTypeId_t       msgId;         // See cmTlUiMsgTypeId_t.
    unsigned                objId;         // Set to cmTlObj_t.uid
    unsigned                parentObjId;   // cmTlObj_t.uid of the object this object's begSmpIdx is set relative to.
    unsigned                seqId;         // 
    cmTlObjTypeId_t         typeId;        // 
    int                     begSmpIdx;     // Time relative to parent.
    unsigned                durSmpCnt;     // Duration of the object.
    const char*             label;         // Object label (points to memory inside the serialized msg.)
    double                  srate;         // Only valid with kInitMsgTlId.
    unsigned                seqCnt;        // Only valid with kInitMsgTlId.
    const cmMidiTrackMsg_t* midiTrkMsg;    // Only valid for typeId  == kMidiEvtTlId. Internal pointers refer to memory inside the serialzed msg. buffer.
    unsigned                midiFileObjId; // Only valid for typeId  == kMidiEvtTlId
    const char*             textStr;       // filename for kAudioFileTlId and kMidiFileTlId, marker text for kMarkerTlId
  } cmTlUiMsg_t;

  // Decode a serialized cmTlObj_t as passed to the cmTlCb_t listener
  // callback function.
  cmTlRC_t cmTimeLineDecode( const void* msg, unsigned msgByteCnt, cmTlUiMsg_t* uiMsg );

#ifdef __cplusplus
}
#endif

#endif
