#ifndef cmMidiFile_h
#define cmMidiFile_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"MIDI file reader and writer." kw:[midi file]}  
  // MIDI file timing:
  // Messages in the MIDI file are time tagged with a delta offset in 'ticks'
  // from the previous message in the same track.
  // 
  // A 'tick' can be converted to microsends as follows:
  //
  // microsecond per tick = micros per quarter note / ticks per quarter note
  // 
  // MpT = MpQN / TpQN
  // 
  // TpQN is given as a constant in the MIDI file header.
  // MpQN is given as the value of the MIDI file tempo message.
  //
  // See cmMidiFileSeekUSecs() for an example of converting ticks to milliseconds.
  //
  // As part of the file reading process, the status byte of note-on messages 
  // with velocity=0 are is changed to a note-off message. See _cmMidiFileReadChannelMsg().
  //)
  
  //(
  typedef cmHandle_t cmMidiFileH_t;
  typedef unsigned   cmMfRC_t;

  typedef struct
  {
    cmMidiByte_t hr;
    cmMidiByte_t min;
    cmMidiByte_t sec;
    cmMidiByte_t frm;
    cmMidiByte_t sfr;
  } cmMidiSmpte_t;

  typedef struct
  {
    cmMidiByte_t num;
    cmMidiByte_t den;
    cmMidiByte_t metro;
    cmMidiByte_t th2s;
  } cmMidiTimeSig_t;

  typedef struct
  {
    cmMidiByte_t key;
    cmMidiByte_t scale;
  } cmMidiKeySig_t;

  struct cmMidiTrackMsg_str;
  
  typedef struct
  {
    cmMidiByte_t ch;
    cmMidiByte_t d0;
    cmMidiByte_t d1;
    unsigned     durMicros;  // note duration in microseconds (corrected for tempo changes)
    struct cmMidiTrackMsg_str* end; // note-off or pedal-up message
  } cmMidiChMsg_t;


  typedef struct cmMidiTrackMsg_str
  {
    unsigned                   uid;     // uid's are unique among all msg's in the file
    unsigned                   dtick;   // delta ticks between events on this track
    unsigned long long         atick;   // global (all tracks interleaved) accumulated ticks
    unsigned long long         amicro;  // global (all tracks interleaved) accumulated microseconds adjusted for tempo changes
    cmMidiByte_t               status;  // ch msg's have the channel value removed (it is stored in u.chMsgPtr->ch)
    cmMidiByte_t               metaId;  //
    unsigned short             trkIdx;  //  
    unsigned                   byteCnt; // length of data pointed to by u.voidPtr (or any other pointer in the union)
    struct cmMidiTrackMsg_str* link;    // link to next record in this track

    union
    {
      cmMidiByte_t           bVal;
      unsigned               iVal;
      unsigned short         sVal;
      const char*            text;
      const void*            voidPtr;
      const cmMidiSmpte_t*   smptePtr;
      const cmMidiTimeSig_t* timeSigPtr;
      const cmMidiKeySig_t*  keySigPtr;
      const cmMidiChMsg_t*   chMsgPtr;
      const cmMidiByte_t*    sysExPtr;
    } u;
  } cmMidiTrackMsg_t;

#define cmMidiFileIsNoteOn(m)         (cmMidiIsNoteOn((m)->status) && (m)->u.chMsgPtr->d1>0)
#define cmMidiFileIsNoteOff(m)        (cmMidiIsNoteOff((m)->status,(m)->u.chMsgPtr->d1))
  
#define cmMidiFileIsSustainPedalUp(m)     (cmMidiIsSustainPedalUp(    (m)->status,(m)->u.chMsgPtr->d0,(m)->u.chMsgPtr->d1))
#define cmMidiFileIsSustainPedalDown(m)   (cmMidiIsSustainPedalDown(  (m)->status,(m)->u.chMsgPtr->d0,(m)->u.chMsgPtr->d1))
  
#define cmMidiFileIsSostenutoPedalUp(m)   (cmMidiIsSostenutoPedalUp(  (m)->status,(m)->u.chMsgPtr->d0,(m)->u.chMsgPtr->d1))
#define cmMidiFileIsSostenutoPedalDown(m) (cmMidiIsSostenutoPedalDown((m)->status,(m)->u.chMsgPtr->d0,(m)->u.chMsgPtr->d1))
  
  enum
  {
    kOkMfRC = cmOkRC,    //  0
    kFileFailMfRC,       //  1
    kNotAMidiFileMfRC,   //  2
    kMemAllocFailMfRC,   //  3
    kFileCorruptMfRC,    //  4
    kMissingEoxMfRC,     //  5 
    kUnknownMetaIdMfRC,  //  6
    kInvalidHandleMfRC,  //  7 
    kMissingNoteOffMfRC, //  8
    kInvalidStatusMfRC,  //  9
    kSustainPedalMfRC,   // 10
    kSostenutoPedalMfRC, // 11
    kLargeDeltaTickMfRC, // 12 (a large delta tick value was filtered)
    kUidNotFoundMfRC,    // 13
    kUidNotANoteMsgMfRC, // 14
    kInvalidTrkIndexMfRC // 15
  };

  extern cmMidiFileH_t cmMidiFileNullHandle;

  cmMfRC_t              cmMidiFileOpen( cmCtx_t* ctx, cmMidiFileH_t* h, const char* fn );
  cmMfRC_t              cmMidiFileCreate( cmCtx_t* ctx, cmMidiFileH_t* hp, unsigned trkN, unsigned ticksPerQN );
  cmMfRC_t              cmMidiFileClose( cmMidiFileH_t* hp );

  cmMfRC_t              cmMidiFileWrite( cmMidiFileH_t h, const char* fn );

  bool                  cmMidiFileIsValid( cmMidiFileH_t h );

  // Returns track count or kInvalidCnt if 'h' is invalid.
  unsigned              cmMidiFileTrackCount( cmMidiFileH_t h );

  // Return midi file format id (0,1,2) or kInvalidId if 'h' is invalid.
  unsigned              cmMidiFileType( cmMidiFileH_t h );

  // Returns ticks per quarter note or kInvalidMidiByte if 'h' is
  // invalid or 0 if file uses SMPTE ticks per frame time base.
  unsigned              cmMidiFileTicksPerQN( cmMidiFileH_t h );

  // The file name used in an earlier call to midiFileOpen() or NULL if this 
  // midi file did not originate from an actual file.
  const char*           cmMidiFileName( cmMidiFileH_t h );

  // Returns SMPTE ticks per frame or kInvalidMidiByte if 'h' is
  // invalid or 0 if file uses ticks per quarter note time base.
  cmMidiByte_t          cmMidiFileTicksPerSmpteFrame( cmMidiFileH_t h );

  // Returns SMPTE format or kInvalidMidiByte if 'h' is invalid or 0
  // if file uses ticks per quarter note time base.
  cmMidiByte_t          cmMidiFileSmpteFormatId( cmMidiFileH_t h );

  // Returns count of records in track 'trackIdx' or kInvalidCnt if 'h' is invalid.
  unsigned              cmMidiFileTrackMsgCount( cmMidiFileH_t h, unsigned trackIdx );

  // Returns base of record chain from track 'trackIdx' or NULL if 'h' is invalid.
  const cmMidiTrackMsg_t* cmMidiFileTrackMsg( cmMidiFileH_t h, unsigned trackIdx );

  // Returns the total count of records in the midi file and the
  // number in the array returned by cmMidiFileMsgArray(). 
  // Return kInvalidCnt if 'h' is invalid.
  unsigned              cmMidiFileMsgCount( cmMidiFileH_t h );

  // Returns a pointer to the base of an array of pointers to all records
  // in the file sorted in ascending time order. 
  // Returns NULL if 'h' is invalid.
  const cmMidiTrackMsg_t** cmMidiFileMsgArray( cmMidiFileH_t h );

  // Set the velocity of a note-on/off msg identified by 'uid'.
  cmMfRC_t             cmMidiFileSetVelocity( cmMidiFileH_t h, unsigned uid, cmMidiByte_t vel );

  // Insert a MIDI message relative to the reference msg identified by 'uid'.
  // If dtick is positive/negative then the new msg is inserted after/before the reference msg.  
  cmMfRC_t             cmMidiFileInsertMsg( cmMidiFileH_t h, unsigned uid, int dtick, cmMidiByte_t ch, cmMidiByte_t status, cmMidiByte_t d0, cmMidiByte_t d1 );

  //
  // Insert a new cmMidiTrackMsg_t into the MIDI file on the specified track.
  //
  // Only the following fields need be set in 'msg'.
  //   atick    - used to position the msg in the track
  //   status   - this field is always set (Note that channel information must stripped from the status byte and included in the channel msg data)
  //   metaId   - this field is optional depending on the msg type
  //   byteCnt  - used to allocate storage for the data element in 'cmMidiTrackMsg_t.u'
  //   u        - the message data
  //
  cmMfRC_t             cmMidiFileInsertTrackMsg(     cmMidiFileH_t h, unsigned trkIdx, const cmMidiTrackMsg_t* msg );
  cmMfRC_t             cmMidiFileInsertTrackChMsg(   cmMidiFileH_t h, unsigned trkIdx, unsigned atick, cmMidiByte_t status, cmMidiByte_t d0, cmMidiByte_t d1 );
  cmMfRC_t             cmMidFileInsertTrackTempoMsg( cmMidiFileH_t h, unsigned trkIdx, unsigned atick, unsigned bpm );
  
  // Return a pointer to the first msg at or after 'usecsOffs' or kInvalidIdx if no
  // msg exists after 'usecsOffs'.  Note that 'usecOffs' is an offset from the beginning
  // of the file.
  // On return *'msgUsecsPtr' is set to the actual time of the msg. 
  // (which will be equal to or greater than 'usecsOffs').
  unsigned              cmMidiFileSeekUsecs( cmMidiFileH_t h, unsigned long long usecsOffs, unsigned* msgUsecsPtr, unsigned* newMicrosPerTickPtr );

  double                cmMidiFileDurSecs( cmMidiFileH_t h );

  // Calculate Note Duration 
  void                  cmMidiFileCalcNoteDurations( cmMidiFileH_t h );

  // Set the delay prior to the first non-zero msg.
  void                  cmMidiFileSetDelay( cmMidiFileH_t h, unsigned ticks );

  // This function packs a track msg into a single  consecutive 
  // block of memory buf[ bufByteCnt ]. Call cmMidiFilePackTracMsgBufByteCount()
  // to get the required buffer length for any given cmMidiTrackMsg_t instance.
  cmMidiTrackMsg_t*     cmMidiFilePackTrackMsg( const cmMidiTrackMsg_t* m, void* buf, unsigned bufByteCnt );
  unsigned              cmMidiFilePackTrackMsgBufByteCount( const cmMidiTrackMsg_t* m );

  void                  cmMidiFilePrintMsgs( cmMidiFileH_t h, cmRpt_t* rpt );
  void                  cmMidiFilePrintTrack( cmMidiFileH_t h, unsigned trkIdx, cmRpt_t* rpt );

  typedef struct
  {
    unsigned           uid;
    unsigned long long amicro;
    unsigned           density;
    
  } cmMidiFileDensity_t;
  cmMidiFileDensity_t* cmMidiFileNoteTimeDensity( cmMidiFileH_t h, unsigned* cntRef );

  // Generate a piano-roll plot description file which can be displayed with cmXScore.m
  cmMfRC_t             cmMidiFileGenPlotFile( cmCtx_t* ctx, const cmChar_t* midiFn, const cmChar_t* outFn );

  void                  cmMidiFileTest( const char* fn, cmCtx_t* ctx );


  
  //)
  
#ifdef __cplusplus
}
#endif
 
#endif
