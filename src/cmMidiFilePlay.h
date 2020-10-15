//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef midiFilePlay_h
#define midiFilePlay_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Device indepenent MIDI file player." kw:[midi]}
  
  typedef cmHandle_t cmMfpH_t;
  typedef cmRC_t     cmMfpRC_t;

  typedef void (*cmMfpCallback_t)( void* userCbPtr, unsigned dmicros, const cmMidiTrackMsg_t* msgPtr );

  enum
  {
    kOkMfpRC = cmOkRC,            // 0
    kInvalidHandleMfpRC,          // 1
    kFileOpenFailMfpRC,           // 2 
    kInvalidFileMfpRC,            // 3
    kMemAllocFailMfpRC,           // 4
    kSmpteTickNotImpleMfpRC,      // 5
    kEndOfFileMfpRC,              // 6
    kSmpteTickNotImplMfpRC        // 7
  
  };

  extern cmMfpH_t cmMfpNullHandle;

  cmMfpRC_t cmMfpCreate(     cmMfpH_t* hp, cmMfpCallback_t cbFunc, void* userCbPtr, cmCtx_t* ctx );
  cmMfpRC_t cmMfpDestroy(    cmMfpH_t* hp );
  bool      cmMfpIsValid(    cmMfpH_t h );

  // Load a MIDI file into the player. This MIDI file will be automatically
  // closed when a new file is loaded at a later time or the MIDI file player handle is destroyed.
  cmMfpRC_t cmMfpLoadFile(   cmMfpH_t h, const char* fn );

  // Load a MIDI file into the player using a file owned by the host.
  // This file will NOT be closed when a new file is loaded at a later time
  // or the MIDI file player handle is destroyed.
  cmMfpRC_t cmMfpLoadHandle( cmMfpH_t h, cmMidiFileH_t mfH );

  // Reset the play position of the player to an offset in microseconds from 
  // the beginning of the file.  If there are no message at or after 'offsMicrosecs'
  // then the function will return kEndOfFileMfpRC.
  cmMfpRC_t cmMfpSeek(       cmMfpH_t h, unsigned offsMicrosecs );

  // This is the driving clock call for the player. 'deltaMicroSecs' is the
  // elapsed time in microseconds since the last call to this function.
  // Call to 'cbFunc', as set in by cmMfpCreate() occur from this function.
  cmMfpRC_t cmMfpClock(      cmMfpH_t h, unsigned deltaMicroSecs );

  cmMfpRC_t cmMfpTest( const char* fn, cmCtx_t* ctx );

  cmRC_t cmMfpTest2( const char* midiFn, const char* audioFn, cmCtx_t* ctx );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
