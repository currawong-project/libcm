//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmTakeSeqBldr_h
#define cmTakeSeqBldr_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Concatenate multiple overlapping MIDI performances into a single virtual performance by associating score information with the MIDI events." kw:[seq] }
  
  enum
  {
    kOkTsbRC = cmOkRC,
    kJsonFailTsbRC,
    kParseFailTsbRC,
    kTimeLineFailTsbRC,
    kScoreFailTsbRC,
    kInvalidArgTsbRC,
    kMidiFileFailTsbRC,
    kMissingScTrkTsbRC,
    kRenderSeqEmptyTsbRC,
  };

  typedef cmRC_t     cmTsbRC_t;
  typedef cmHandle_t cmTakeSeqBldrH_t;

  extern cmTakeSeqBldrH_t cmTakeSeqBldrNullHandle;

  // Allocate a Sequence Builder.
  cmTsbRC_t cmTakeSeqBldrAlloc(   cmCtx_t* ctx, cmTakeSeqBldrH_t* hp );

  // Allocate and initalize a sequence builder.
  cmTsbRC_t cmTakeSeqBldrAllocFn( cmCtx_t* ctx, cmTakeSeqBldrH_t* hp, const cmChar_t* scoreTrkFn );

  // Free a previously allocated sequence builder.
  cmTsbRC_t cmTakeSeqBldrFree( cmTakeSeqBldrH_t* hp );

  // Validate a sequence builder handle.
  bool      cmTakeSeqBldrIsValid( cmTakeSeqBldrH_t h );

  // Load a score tracking file create by app/cmScoreProc.c:_cmScoreProcGenAssocMain().
  // Note that calling this function loads a time-line object and score object
  // along with the information contained in the score tracking file.
  cmTsbRC_t cmTakeSeqBldrInitialize( cmTakeSeqBldrH_t h, const cmChar_t* scoreTrkFn );


  // Load a group of notes delinated by a time-line marker into the sequence.
  // If notes overlap with existing notes according to their 'scEvtIdx' attribute:
  //   a. If 'overWriteFl' is set then the incoming overlapped notes are enabled 
  //      and the existing overlapped notes are disabled, otherwise the incoming
  //      overlapped notes are disabled and the existing notes remain enabled.
  //   b. The incoming section is time aligned with the first or last existing 
  //      note depending on whether the new section aligns best with the beginning
  //      or ending of the existing notes.
  //
  // If no overlapping notes exist then the incoming section is aligned by estimating
  // the alignment with existing notes using the score alone.
  cmTsbRC_t cmTakeSeqBldrLoadTake(   cmTakeSeqBldrH_t h, unsigned tlMarkUid, bool overwriteFL );
  cmTsbRC_t cmTakeSeqBldrUnloadTake( cmTakeSeqBldrH_t h, unsigned tlMarkUid );
  

  double    cmTakeSeqBldrSampleRate( cmTakeSeqBldrH_t h );

  // Return a handle to the score used by this cmTakeSeqBldr.
  cmScH_t   cmTakeSeqBldrScoreHandle( cmTakeSeqBldrH_t h );

  // Return the count of score-track takes.
  unsigned cmTakeSeqBldrScTrkTakeCount( cmTakeSeqBldrH_t h );

  typedef struct
  {
    unsigned minScEvtIdx;  // first score event index this 'take' contains
    unsigned maxScEvtIdx;  // last score event index this 'take' contains
    unsigned tlMarkerUid;  // the marker Uid associated with this take
  } cmTksbScTrkTake_t;

  // Given an index [0 - cmTakeSeqBldrScTrkTakeCount()) return a cmTksbScTrkTake_t record
  cmTsbRC_t       cmTakeSeqBldrScTrkTake( cmTakeSeqBldrH_t h, unsigned idx, cmTksbScTrkTake_t* ref );

  // Return the text associated with the specified 'take' marker.
  const cmChar_t* cmTakeSeqBldrScTrkTakeText( cmTakeSeqBldrH_t h, unsigned tlMarkerUId );


  // Set score location index to cmInvalidIdx to rewind the player to the beginning of the sequence.
  cmTsbRC_t cmTakeSeqBldrPlaySeekLoc( cmTakeSeqBldrH_t h, unsigned scLocIdx );

  typedef struct
  { 
    unsigned smpIdx;    // NOTE: changes in this structure should be 
    unsigned status;    // might break;
    unsigned d0;        // void _cmDspTakeSeqRendPedalsUp( cmDspCtx_t* ctx, cmDspInst_t* inst )
    unsigned d1;        // because it loads a static array
  } cmTksbEvent_t;

  typedef void (*cmTakeSeqBldrPlayFunc_t)( void* arg, const cmTksbEvent_t* evt );
  
  cmTsbRC_t cmTakeSeqBldrPlayExec( cmTakeSeqBldrH_t h, unsigned deltaSmp, cmTakeSeqBldrPlayFunc_t cbFunc, void* cbArg );
  

  typedef struct
  {
    unsigned      srcId;
    unsigned      scEvtIdx;
    unsigned      rid;
    unsigned      flags;
    unsigned      offsetSmp;
    unsigned      durSmp;
    cmTksbEvent_t evt;
  } cmTksbRend_t;

  void cmTakeSeqBldrRendReset( cmTakeSeqBldrH_t h );
  bool cmTakeSeqBldrRendNext(cmTakeSeqBldrH_t h, cmTksbRend_t* rendRef );
  
  cmTsbRC_t cmTakeSeqBldrRendInfo( cmTakeSeqBldrH_t h, unsigned rid, cmTksbRend_t* r ); 
  cmTsbRC_t cmTakeSeqBldrRendDelete( cmTakeSeqBldrH_t h, unsigned rid );
  cmTsbRC_t cmTakeSeqBldrRendInsert( 
    cmTakeSeqBldrH_t     h, 
    const cmTksbEvent_t* e,
    unsigned             durSmp,
    unsigned*            ridRef );

  cmTsbRC_t cmTakeSeqBldrWrite( cmTakeSeqBldrH_t h, const cmChar_t* fn );
  cmTsbRC_t cmTakeSeqBldrRead(  cmTakeSeqBldrH_t h, const cmChar_t* fn );


  cmTsbRC_t cmTakeSeqBldrTest( cmCtx_t* ctx );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
