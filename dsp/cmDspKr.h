#ifndef cmDspKr_h
#define cmDspKr_h

#ifdef __cplusplus
extern "C" {
#endif

  struct cmDspClass_str* cmKrClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmTimeLineClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmMidiFilePlayClassCons( cmDspCtx_t* ctx );

#ifdef __cplusplus
}
#endif


#endif
