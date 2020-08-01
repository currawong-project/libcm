#ifndef cmMidi_h
#define cmMidi_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"MIDI utility constants and functions." kw:[midi]}
  
  enum
  {
    kMidiChCnt           = 16,
    kInvalidMidiByte     = 128,
    kMidiNoteCnt         = kInvalidMidiByte,
    kMidiCtlCnt          = kInvalidMidiByte,
    kMidiPgmCnt          = kInvalidMidiByte,
    kInvalidMidiPitch    = kInvalidMidiByte,
    kInvalidMidiVelocity = kInvalidMidiByte,
    kInvalidMidiCtl      = kInvalidMidiByte,
    kInvalidMidiPgm      = kInvalidMidiByte,
    kMidiSciPitchCharCnt = 5  // A#-1
  };


  // MIDI status bytes
  enum
  {
    kInvalidStatusMdId = 0x00,
    kNoteOffMdId       = 0x80,
    kNoteOnMdId        = 0x90,
    kPolyPresMdId      = 0xa0,
    kCtlMdId           = 0xb0,
    kPgmMdId           = 0xc0,
    kChPresMdId        = 0xd0,
    kPbendMdId         = 0xe0,
    kSysExMdId         = 0xf0,

    kSysComMtcMdId     = 0xf1,
    kSysComSppMdId     = 0xf2,
    kSysComSelMdId     = 0xf3,
    kSysComUndef0MdId  = 0xf4,
    kSysComUndef1MdId  = 0xf5,
    kSysComTuneMdId    = 0xf6,
    kSysComEoxMdId     = 0xf7,

    kSysRtClockMdId  = 0xf8,
    kSysRtUndef0MdId = 0xf9,
    kSysRtStartMdId  = 0xfa,
    kSysRtContMdId   = 0xfb,
    kSysRtStopMdId   = 0xfc,
    kSysRtUndef1MdId = 0xfd,
    kSysRtSenseMdId  = 0xfe,
    kSysRtResetMdId  = 0xff,
    kMetaStId        = 0xff,

    kSeqNumbMdId     = 0x00,
    kTextMdId        = 0x01,
    kCopyMdId        = 0x02,
    kTrkNameMdId     = 0x03,
    kInstrNameMdId   = 0x04,
    kLyricsMdId      = 0x05,
    kMarkerMdId      = 0x06,
    kCuePointMdId    = 0x07,
    kMidiChMdId      = 0x20,
    kEndOfTrkMdId    = 0x2f,
    kTempoMdId       = 0x51,
    kSmpteMdId       = 0x54,
    kTimeSigMdId     = 0x58,
    kKeySigMdId      = 0x59,
    kSeqSpecMdId     = 0x7f,
    kInvalidMetaMdId = 0x80,

    kSustainCtlMdId    = 0x40,
    kPortamentoCtlMdId = 0x41,
    kSostenutoCtlMdId  = 0x42,
    kSoftPedalCtlMdId  = 0x43,
    kLegatoCtlMdId     = 0x44
  
  };


  typedef unsigned char cmMidiByte_t;

  //===============================================================================================
  // Utility Functions
  //

#define cmMidiIsStatus( s )   (kNoteOffMdId <= (s) /*&& ((unsigned)(s)) <= kSysRtResetMdId*/ )
#define cmMidiIsChStatus( s ) (kNoteOffMdId <= (s) && (s) <  kSysExMdId)

#define cmMidiIsNoteOn( s )      ( kNoteOnMdId <= (s) && (s) < (kNoteOnMdId + kMidiChCnt) )
#define cmMidiIsNoteOff( s, d1 ) ( (cmMidiIsNoteOn(s) && (d1)==0) || (kNoteOffMdId <= (s) && (s) < (kNoteOffMdId + kMidiChCnt)) )
#define cmMidiIsCtl( s )         ( kCtlMdId <= (s) && (s) < (kCtlMdId + kMidiChCnt) )

#define cmMidiIsSustainPedal(     s, d0 )    ( kCtlMdId <= (s) && (s) <= (kCtlMdId + kMidiChCnt) && (d0)== kSustainCtlMdId )
#define cmMidiIsSustainPedalDown( s, d0, d1) ( cmMidiIsSustainPedal(s,d0) && (d1)>=64 )
#define cmMidiIsSustainPedalUp(   s, d0, d1) ( cmMidiIsSustainPedal(s,d0) && (d1)<64 )
  
#define cmMidiIsSostenutoPedal(     s, d0 )    ( kCtlMdId <= (s) && (s) <= (kCtlMdId + kMidiChCnt) && (d0)== kSostenutoCtlMdId )
#define cmMidiIsSostenutoPedalDown( s, d0, d1) ( cmMidiIsSostenutoPedal(s,d0) && (d1)>=64 )
#define cmMidiIsSostenutoPedalUp(   s, d0, d1) ( cmMidiIsSostenutoPedal(s,d0) && (d1)<64 )

#define cmMidiIsPedal(     s, d0 )      ( kCtlMdId <= (s) && (s) <= (kCtlMdId + kMidiChCnt) && (d0)>=kSustainCtlMdId && (d0)<=kLegatoCtlMdId )
#define cmMidiIsPedalDown( s, d0, d1 )  ( cmMidiIsPedal(s,d0) && (d1)>=64 )
#define cmMidiIsPedalUp(   s, d0, d1 )  ( cmMidiIsPedal(s,d0) && (d1)<64  )


  const char*   cmMidiStatusToLabel(     cmMidiByte_t status );
  const char*   cmMidiMetaStatusToLabel( cmMidiByte_t metaStatus );
  const char*   cmMidiPedalLabel(        cmMidiByte_t d0 );

  // Returns kInvalidMidiByte if status is not a valid status byte
  cmMidiByte_t  cmMidiStatusToByteCount( cmMidiByte_t status );

  unsigned      cmMidiTo14Bits( cmMidiByte_t d0, cmMidiByte_t d1 );
  void          cmMidiSplit14Bits( unsigned v, cmMidiByte_t* d0Ref, cmMidiByte_t* d1Ref );
  int           cmMidiToPbend(  cmMidiByte_t d0, cmMidiByte_t d1 );
  void          cmMidiSplitPbend( int v, cmMidiByte_t* d0Ref, cmMidiByte_t* d1Ref ); 

  //===============================================================================================
  // MIDI Communication data types
  //

  typedef struct 
  {
    //unsigned     deltaUs; // time since last MIDI msg in microseconds
    cmTimeSpec_t timeStamp;
    cmMidiByte_t status;  // midi status byte
    cmMidiByte_t d0;      // midi data byte 0
    cmMidiByte_t d1;      // midi data byte 1
    cmMidiByte_t pad;
  } cmMidiMsg;

  typedef struct
  {
    void*         cbDataPtr; // application supplied reference value from mdParserCreate()
    unsigned      devIdx;    // the device the msg originated from
    unsigned      portIdx;   // the port index on the source device
    cmMidiMsg*    msgArray;  // pointer to an array of 'msgCnt' mdMsg records or NULL if sysExMsg is non-NULL
    cmMidiByte_t* sysExMsg;  // pointer to a sys-ex msg or NULL if msgArray is non-NULL (see note below)
    unsigned      msgCnt;    // count of mdMsg records or sys-ex bytes
  } cmMidiPacket_t;

  // Notes: If the sys-ex message can be contained in a single msg then
  // then the first msg byte is kSysExMdId and the last is kSysComEoxMdId.
  // If the sys-ex message is broken into multiple pieces then only the
  // first will begin with kSysExMdId and the last will end with kSysComEoxMdId.


  // If label is NULL or labelCharCnt==0 then a pointer to an internal static
  // buffer is returned. If label[] is given the it
  // should have at least 5 (kMidiPitchCharCnt) char's (including the terminating zero).
  // If 'pitch' is outside of the range 0-127 then a blank string is returned.
  const char*     cmMidiToSciPitch( cmMidiByte_t pitch, char* label, unsigned labelCharCnt );

  // Convert a scientific pitch to MIDI pitch.  acc == 1 == sharp, acc == -1 == flat.
  // The pitch character must be in the range 'A' to 'G'. Upper or lower case is valid.
  // Return kInvalidMidiPitch if the arguments are not valid. 
  cmMidiByte_t    cmSciPitchToMidiPitch( cmChar_t pitch, int acc, int octave );


  // Scientific pitch string: [A-Ga-g][#b][#] where  # may be -1 to 9.
  // Return kInvalidMidiPitch if sciPtichStr does not contain a valid 
  // scientific pitch string. This function will convert C-1 to G9 to 
  // valid MIDI pitch values 0 to 127.  Scientific pitch strings outside
  // of this range will be returned as kInvalidMidiPitch.   
  cmMidiByte_t    cmSciPitchToMidi( const char* sciPitchStr );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
