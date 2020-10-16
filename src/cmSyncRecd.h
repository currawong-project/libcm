//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmSyncRecd_h
#define cmSyncRecd_h

#ifdef __cplusplus
extern "C" {
#endif
  
  //( { file_desc:"Generate time-alignment data between an audio and MIDI file. See cmDspSyncRecd_t." kw[proc] }

  enum
  {
    kOkSyRC,
    kFileFailSyRC,
    kAudioFileFailSyRC,
    kInvalidOpSyRC
  };

  typedef cmHandle_t cmSyncRecdH_t;
  typedef cmRC_t     cmSyRC_t;
  extern cmSyncRecdH_t cmSyncRecdNullHandle;
  
  cmSyRC_t cmSyncRecdCreate(  cmCtx_t* ctx, cmSyncRecdH_t* hp, const cmChar_t* srFn, const cmChar_t* audioFn, double srate, unsigned chCnt, unsigned bits );
  cmSyRC_t cmSyncRecdOpen(    cmCtx_t* ctx, cmSyncRecdH_t* hp, const cmChar_t* srFn );
  cmSyRC_t cmSyncRecdFinal(   cmSyncRecdH_t* hp );
  bool     cmSyncRecdIsValid( cmSyncRecdH_t h );

  cmSyRC_t cmSyncRecdMidiWrite(  cmSyncRecdH_t h, const cmTimeSpec_t* timestamp, unsigned status, unsigned d0, unsigned d1 );
  cmSyRC_t cmSyncRecdAudioWrite( cmSyncRecdH_t h, const cmTimeSpec_t* timestamp, unsigned smpIdx, const cmSample_t* ch[], unsigned chCnt, unsigned frmCnt );
  

  cmSyRC_t cmSyncRecdTest( cmCtx_t* ctx );
  //)
  
#ifdef __cplusplus
}
#endif

#endif
