#ifndef cmRbm_h
#define cmRbm_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Restricted Bolzmann Machine object." kw:[model] }
enum
{
  kOkRbmRC = cmOkRC,
  kInvalidParmRbmRC,
  kStackFailRbmRC,
  kMonitorWrFailRbmRC
};

  typedef cmHandle_t cmRbmH_t;
  typedef cmRC_t     cmRbmRC_t;

  extern cmRbmH_t cmRbmNullHandle;

  cmRbmRC_t cmRbmAllocate( cmCtx_t* ctx, cmRbmH_t* hp, const unsigned nodesPerLayer[], unsigned layerCnt );
  cmRbmRC_t cmRbmFree( cmRbmH_t* hp );
  bool      cmRbmIsValid( cmRbmH_t h );

  cmRbmRC_t cmRbmConfigure( cmRbmH_t h, const unsigned nodesPerLayerV[], unsigned layerCnt );  

  typedef struct
  {
    double maxX;
    double minX;
    double initW;  
    double eta;       // learning rate
    double holdOutFrac; 
    unsigned epochCnt;
    unsigned batchCnt;
    double momentum;
  } cmRbmTrainParms_t;

  void cmRbmBinaryTest(cmCtx_t* ctx);

  //)
  
#ifdef __cplusplus
}
#endif

#endif
