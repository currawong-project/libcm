//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.

#ifndef cmSaProc_h
#define cmSaProc_h

#ifdef __cplusplus
extern "C" {
#endif

  

  typedef struct
  {
    cmObj    obj;
    void*    h;
    unsigned mode;
    double   srate;
    unsigned procSmpCnt;
    bool     freeFl;

    double  azimDegrees;
    double  elevDegrees;
    double  dist;
    
  } cmBinEnc;

  cmBinEnc* cmBinEncAlloc( cmCtx* c, cmBinEnc* p, double srate, unsigned procSmpCnt );
  cmRC_t    cmBinEncFree(   cmBinEnc** pp );
  cmRC_t    cmBinEncInit(   cmBinEnc* p, double srate, unsigned procSmpCnt );
  cmRC_t    cmBinEncFinal(  cmBinEnc* p );
  cmRC_t    cmBinEncSetMode(cmBinEnc* p, unsigned mode );
  cmRC_t    cmBinEncSetLoc( cmBinEnc* p, float azimDegrees, float elevDegrees, float dist );
  cmRC_t    cmBinEncExec(   cmBinEnc* p, const cmSample_t* x, cmSample_t* y0, cmSample_t* y1, unsigned xyN );


#ifdef __cplusplus
}
#endif

#endif
