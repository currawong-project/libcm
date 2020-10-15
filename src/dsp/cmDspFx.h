#ifndef cmDspFx_h
#define cmDspFx_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Large collection of real-time audio processing dataflow class descriptions originally developed for 'fluxo'." kw:[snap fluxo] }
  
  struct cmDspClass_str* cmDelayClassCons(      cmDspCtx_t* ctx );
  struct cmDspClass_str* cmPShiftClassCons(     cmDspCtx_t* ctx );
  struct cmDspClass_str* cmLoopRecdClassCons(   cmDspCtx_t* ctx );
  struct cmDspClass_str* cmRectifyClassCons(    cmDspCtx_t* ctx );
  struct cmDspClass_str* cmGateDetectClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmAutoGainClassCons(   cmDspCtx_t* ctx );
  struct cmDspClass_str* cmEnvFollowClassCons(  cmDspCtx_t* ctx );
  struct cmDspClass_str* cmXfaderClassCons(     cmDspCtx_t* ctx );
  struct cmDspClass_str* cmChCfgClassCons(      cmDspCtx_t* ctx );
  struct cmDspClass_str* cmChordDetectClassCons(cmDspCtx_t* ctx );
  struct cmDspClass_str* cmFaderClassCons(      cmDspCtx_t* ctx );
  struct cmDspClass_str* cmNoteSelectClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmNetNoteSelectClassCons(cmDspCtx_t* ctx );
  struct cmDspClass_str* cmCombFiltClassCons(   cmDspCtx_t* ctx );
  struct cmDspClass_str* cmScalarOpClassCons(   cmDspCtx_t* ctx );
  struct cmDspClass_str* cmGroupSelClassCons(   cmDspCtx_t* ctx );
  struct cmDspClass_str* cmAudioNofMClassCons(  cmDspCtx_t* ctx );
  struct cmDspClass_str* cmRingModClassCons(    cmDspCtx_t* ctx );
  struct cmDspClass_str* cmMsgDelayClassCons(   cmDspCtx_t* ctx );
  struct cmDspClass_str* cmLineClassCons(       cmDspCtx_t* ctx );
  struct cmDspClass_str* cmAdsrClassCons(       cmDspCtx_t* ctx );
  struct cmDspClass_str* cmCompressorClassCons( cmDspCtx_t* ctx );
  struct cmDspClass_str* cmBiQuadEqClassCons(   cmDspCtx_t* ctx );
  struct cmDspClass_str* cmDistDsClassCons(     cmDspCtx_t* ctx );
  struct cmDspClass_str* cmDbToLinClassCons(    cmDspCtx_t* ctx );
  struct cmDspClass_str* cmMtDelayClassCons(    cmDspCtx_t* ctx );
  struct cmDspClass_str* cmNofMClassCons(       cmDspCtx_t* ctx );
  struct cmDspClass_str* cm1ofNClassCons(       cmDspCtx_t* ctx );
  struct cmDspClass_str* cm1UpClassCons(        cmDspCtx_t* ctx );
  struct cmDspClass_str* cmGateToSymClassCons(  cmDspCtx_t* ctx );
  struct cmDspClass_str* cmPortToSymClassCons(  cmDspCtx_t* ctx );
  struct cmDspClass_str* cmIntToSymClassCons(   cmDspCtx_t* ctx );  
  struct cmDspClass_str* cmRouterClassCons(     cmDspCtx_t* ctx );
  struct cmDspClass_str* cmAvailChClassCons(    cmDspCtx_t* ctx );
  struct cmDspClass_str* cmPresetClassCons(     cmDspCtx_t* ctx );
  struct cmDspClass_str* cmBcastSymClassCons(   cmDspCtx_t* ctx );
  struct cmDspClass_str* cmSegLineClassCons(    cmDspCtx_t* ctx );

  //)
  
#ifdef __cplusplus
}
#endif



#endif
