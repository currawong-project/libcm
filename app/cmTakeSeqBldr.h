#ifndef cmTakeSeqBldr_h
#define cmTakeSeqBldr_h

#ifdef __cplusplus
extern "C" {
#endif


  enum
  {
    kOkTsbRC = cmOkRC,
    kJsonFailTsbRC,
    kParseFailTsbRC,
    kTimeLineFailTsbRC,
    kScoreFailTsbRC,
    kInvalidArgTsbRC,
    kMidiFileFailTsbRC
  };

  typedef cmRC_t     cmTsbRC_t;
  typedef cmHandle_t cmTakeSeqBldrH_t;

  extern cmTakeSeqBldrH_t cmTakeSeqBldrNullHandle;

  cmTsbRC_t cmTakeSeqBldrAlloc(   cmCtx_t* ctx, cmTakeSeqBldrH_t* hp );
  cmTsbRC_t cmTakeSeqBldrAllocFn( cmCtx_t* ctx, cmTakeSeqBldrH_t* hp, const cmChar_t* scoreTrkFn );

  cmTsbRC_t cmTakeSeqBldrFree( cmTakeSeqBldrH_t* hp );

  bool      cmTakeSeqBldrIsValid( cmTakeSeqBldrH_t h );

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
  
  // Fill in missing notes from the score. 
  cmTsbRC_t cmTakeSeqBldrInsertScoreNotes( cmTakeSeqBldrH_t h, unsigned begScEvtIdx, unsigned endScEvtId );
  cmTsbRC_t cmTakeSeqBldrRemoveScoreNotes( cmTakeSeqBldrH_t h, unsigned begScEvtIdx, unsigned endScEvtId );

  //enum { kMarkTsbFl = 0x01, kTlNoteTsbFl=0x02, kScoreNoteTsbFl = 0x04, kPedalTsbFl = 0x08 };
  cmTsbRC_t cmTakeSeqBldrSelectEnable( cmTakeSeqBldrH_t h, unsigned flags, unsigned id, bool selectFl );
  cmTsbRC_t cmTakeSeqBldrEnableNote( cmTakeSeqBldrH_t h, unsigned ssqId, bool enableFl );

  cmTsbRC_t cmTakeSeqBldrMoveNote(   cmTakeSeqBldrH_t h, unsigned ssqId, int deltaSmpIdx );

  cmTsbRC_t cmTakeSeqBldrWriteMidiFile( cmTakeSeqBldrH_t h, const char* fn );

  cmTsbRC_t cmTakeSeqBldrTest( cmCtx_t* ctx );

#ifdef __cplusplus
}
#endif

#endif
