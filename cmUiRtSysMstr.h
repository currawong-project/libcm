#ifndef cmUiRtSysMstr_h
#define cmUiRtSysMstr_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Application side API for communicating with the UI audio master controls and meters." kw:[rtsys]}
  
  enum
  {
    kOkAmRC = cmOkRC,
    kUiFailAmRC,
    kPanelNotFoundAmRC
  };

  typedef cmHandle_t cmUiRtMstrH_t;
  typedef cmRC_t     cmAmRC_t;

  extern cmUiRtMstrH_t cmUiRtMstrNullHandle;

  cmAmRC_t cmUiRtSysMstrAlloc( cmCtx_t* ctx, cmUiRtMstrH_t* hp, cmUiH_t uiH, cmRtSysH_t asH, unsigned appId );
  cmAmRC_t cmUiRtSysMstrFree(  cmUiRtMstrH_t* hp );

  bool     cmUiRtSysMstrIsValid( cmUiRtMstrH_t h );

  cmAmRC_t cmUiRtSysMstrInitialize( cmUiRtMstrH_t h, const cmRtSysCtx_t* c, const cmChar_t* inDevLabel, const cmChar_t* outDevLabel );

  // Receive UI events.
  cmUiRC_t cmUiRtSysMstrOnUiEvent( cmUiRtMstrH_t h, const cmUiDriverArg_t* a );

  // Receive UI status events
  cmUiRC_t cmUiRtSysMstrOnStatusEvent( cmUiRtMstrH_t h, const cmRtSysStatus_t* m, const double* iMeterArray, const double* oMeterArray );

  // Clear the status indicators.
  void cmUiRtSysMstrClearStatus( cmUiRtMstrH_t h );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
