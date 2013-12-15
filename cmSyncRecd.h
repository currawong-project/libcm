#ifndef cmSyncRecd_h
#define cmSyncRecd_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkSrRC,
    kFileFailSrRC,
    kAudioFileFailSrRC
  };

  typedef cmHandle_t cmSyncRecdH_t;
  typedef cmRC_t     cmSrRC_t;
  extern cmSyncRecdH_t cmSyncRecdNullHandle;
  
  cmSrRC_t cmSyncRecdCreate(  cmCtx_t* ctx, cmSyncRecdH_t* hp, const cmChar_t* srFn, const cmChar_t* audioFn, double srate, unsigned chCnt, unsigned bits );
  cmSrRC_t cmSyncRecdOpen(    cmCtx_t* ctx, cmSyncRecdH_t* hp, const cmChar_t* srFn );
  cmSrRC_t cmSyncRecdFinal(   cmSyncRecdH_t* hp );
  bool     cmSyncRecdIsValid( cmSyncRecdH_t h );

  cmSrRC_t cmSyncRecdMidiWrite(  cmSyncRecdH_t h, const cmTimeSpec_t* timestamp, unsigned status, unsigned d0, unsigned d1 );
  cmSrRC_t cmSyncRecdAudioWrite( cmSyncRecdH_t h, const cmTimeSpec_t* timestamp, unsigned smpIdx, const cmSample_t* ch[], unsigned chCnt, unsigned frmCnt );
  

#ifdef __cplusplus
}
#endif

#endif
