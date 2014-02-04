#ifndef cmProc5_h
#define cmProc5_h

#ifdef __cplusplus
extern "C" {
#endif


  //=======================================================================================================================
  // Goertzel Filter
  //

  typedef struct
  {
    double s0;
    double s1;
    double s2;
    double coeff;
    double hz;
  } cmGoertzelCh;

  struct cmShiftBuf_str;

  typedef struct cmGoertzel_str
  {
    cmObj                  obj;
    cmGoertzelCh*          ch;
    unsigned               chCnt;
    double                 srate;
    struct cmShiftBuf_str* shb;
    cmSample_t*            wnd;
  } cmGoertzel;

  cmGoertzel* cmGoertzelAlloc( cmCtx* c, cmGoertzel* p, double srate, const double* fcHzV, unsigned chCnt, unsigned procSmpCnt, unsigned hopSmpCnt, unsigned wndSmpCnt );
  cmRC_t cmGoertzelFree( cmGoertzel** pp );
  cmRC_t cmGoertzelInit( cmGoertzel* p, double srate, const double* fcHzV, unsigned chCnt, unsigned procSmpCnt, unsigned hopSmpCnt, unsigned wndSmpCnt );
  cmRC_t cmGoertzelFinal( cmGoertzel* p );
  cmRC_t cmGoertzelSetFcHz( cmGoertzel* p, unsigned chIdx, double hz );
  cmRC_t cmGoertzelExec( cmGoertzel* p, const cmSample_t* in, unsigned procSmpCnt,  double* outV, unsigned chCnt );



#ifdef __cplusplus
}
#endif

#endif
