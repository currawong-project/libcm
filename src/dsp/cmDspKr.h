//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmDspKr_h
#define cmDspKr_h

#ifdef __cplusplus
extern "C" {
#endif

  struct cmDspClass_str* cmKrClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmKr2ClassCons( cmDspCtx_t* ctx );
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
  struct cmDspClass_str* cmPicadaeClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmRecdPlayClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmGoertzelClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmSyncRecdClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmTakeSeqBldrClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmTakeSeqRendClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmReflectCalcClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmEchoCancelClassCons( cmDspCtx_t* ctx );

#ifdef __cplusplus
}
#endif


#endif
