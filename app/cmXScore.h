#ifndef cmXScore_h
#define cmXScore_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
  kOkXsRC = cmOkRC,
    kXmlFailXsRC,
    kLHeapFailXsRC,
    kSyntaxErrorXsRC
    };

  typedef cmRC_t     cmXsRC_t;
  typedef cmHandle_t cmXsH_t;

  extern cmXsH_t cmXsNullHandle;

  // Prepare the MusicXML file:
  //
  // 1) Convert XML to UTF-8:
  //       a. Change: encoding='UTF-16' to encoding='UTF-8'
  //       b. Emacs C-x <RET> f utf-8 <RET>
  //
  // 2) Replace "DoletSibelius Unknown Symbol Index" with "DoletSibelius unknownSymIdx"
  

  cmXsRC_t cmXScoreInitialize( cmCtx_t* ctx, cmXsH_t* hp, const cmChar_t* xmlFn );
  cmXsRC_t cmXScoreFinalize( cmXsH_t* hp );

  bool     cmXScoreIsValid( cmXsH_t h );

  void     cmXScoreReport( cmXsH_t h, cmRpt_t* rpt );

  cmXsRC_t cmXScoreTest( cmCtx_t* ctx, const cmChar_t* fn );
  
#ifdef __cplusplus
}
#endif

#endif
