#ifndef cmDspKr_h
#define cmDspKr_h

#ifdef __cplusplus
extern "C" {
#endif

  struct cmDspClass_str* cmKrClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmTimeLineClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmScoreClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmMidiFilePlayClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmScFolClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmScModClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmGSwitchClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmScaleRangeClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmActiveMeasClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmAmSyncClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmNanoMapClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmRecdPlayClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmGoertzelClassCons( cmDspCtx_t* ctx );

#ifdef __cplusplus
}
#endif


#endif
