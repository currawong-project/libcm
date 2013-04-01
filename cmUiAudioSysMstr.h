#ifndef cmUiAudioSysMstr_h
#define cmUiAudioSysMstr_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkAmRC = cmOkRC,
    kUiFailAmRC,
    kPanelNotFoundAmRC
  };

  typedef cmHandle_t cmUiASMstrH_t;
  typedef cmRC_t     cmAmRC_t;

  extern cmUiASMstrH_t cmUiASMstrNullHandle;

  cmAmRC_t cmUiAudioSysMstrAlloc( cmCtx_t* ctx, cmUiASMstrH_t* hp, cmUiH_t uiH, cmAudioSysH_t asH, unsigned appId );
  cmAmRC_t cmUiAudioSysMstrFree(  cmUiASMstrH_t* hp );

  bool     cmUiAudioSysMstrIsValid( cmUiASMstrH_t h );

  cmAmRC_t cmUiAudioSysMstrInitialize( cmUiASMstrH_t h, const cmAudioSysSsInitMsg_t* m, const cmChar_t* inDevLabel, const cmChar_t* outDevLabel );

  // Receive UI events.
  cmUiRC_t cmUiAudioSysMstrOnUiEvent( cmUiASMstrH_t h, const cmUiDriverArg_t* a );

  // Receive UI status events
  cmUiRC_t cmUiAudioSysMstrOnStatusEvent( cmUiASMstrH_t h, const cmAudioSysStatus_t* m, const double* iMeterArray, const double* oMeterArray );

#ifdef __cplusplus
}
#endif

#endif
