//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.

#ifndef cmDspPresetMgr_h
#define cmDspPresetMgr_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"'snap' unit state store/recall implementation." kw:[snap] }
  
  typedef struct _cmDspPreVar_str
  {
    unsigned                 symId;
    cmDspValue_t             value;
    struct _cmDspPreVar_str* link;
  } _cmDspPresetVar_t;

  typedef struct _cmDspPresetInst_str
  {
    unsigned                     symId;
    struct _cmDspPresetInst_str* link;
    _cmDspPresetVar_t*           list;
  } _cmDspPresetInst_t;

  typedef struct _cmDspPresetPre_str
  {
    unsigned                    symId;
    struct _cmDspPresetPre_str* link;
    _cmDspPresetInst_t*         list;
    _cmDspPresetInst_t*         ip;
  } _cmDspPresetPre_t;

  typedef struct _cmDspPresetGrp_str
  {
    unsigned                    symId;
    struct _cmDspPresetGrp_str* link;
    _cmDspPresetPre_t*          list;
    _cmDspPresetPre_t*          pp;
  } _cmDspPresetGrp_t;

  typedef struct 
  {
    cmErr_t*           err;
    cmLHeapH_t         lhH;
    cmSymTblH_t        stH;    
    _cmDspPresetGrp_t* list;
    _cmDspPresetGrp_t* gp;
    const cmChar_t*    dfltPathJsFn;
    const cmChar_t*    dfltPathCsvFn;
  } cmDspPresetMgr_t;

  void      _cmDspPresetAlloc(  cmDspPresetMgr_t* p );
  cmDspRC_t _cmDspPresetLoad(   cmDspPresetMgr_t* p, cmCtx_t* ctx, cmErr_t* err, cmLHeapH_t lhH, cmSymTblH_t stH, const cmChar_t* fnPrefixStr );
  cmDspRC_t _cmDspPresetUnload( cmDspPresetMgr_t* p, cmCtx_t* ctx );

  cmDspRC_t _cmDspPresetRead(     cmDspPresetMgr_t* p, cmCtx_t* ctx, const cmChar_t* fn );
  cmDspRC_t _cmDspPresetWrite(    cmDspPresetMgr_t* p, cmCtx_t* ctx, const cmChar_t* fn );
  cmDspRC_t _cmDspPresetWriteCsv( cmDspPresetMgr_t* p, cmCtx_t* ctx, const cmChar_t* fn );

  unsigned        _cmDspPresetGroupCount( cmDspPresetMgr_t* p );
  unsigned        _cmDspPresetGroupSymId( cmDspPresetMgr_t* p, unsigned groupIdx );
  const cmChar_t* _cmDspPresetGroupLabel( cmDspPresetMgr_t* p, unsigned groupIdx );
  cmDspRC_t       _cmDspPresetGroupJsonList(  cmDspPresetMgr_t* p, cmJsonH_t* jsHPtr );
  
  unsigned        _cmDspPresetPresetCount( cmDspPresetMgr_t* p, unsigned groupIdx );
  unsigned        _cmDspPresetPresetSymId( cmDspPresetMgr_t* p, unsigned groupIdx, unsigned presetIdx );
  const cmChar_t* _cmDspPresetPresetLabel( cmDspPresetMgr_t* p, unsigned groupIdx, unsigned presetIdx );
  cmDspRC_t       _cmDspPresetPresetJsonList( cmDspPresetMgr_t* p, cmJsonH_t* jsHPtr, unsigned groupSymId );    

  cmDspRC_t _cmDspPresetCreatePreset(    cmDspPresetMgr_t* p, const cmChar_t* groupLabel, const cmChar_t* presetLabel );
  cmDspRC_t _cmDspPresetCreateInstance(  cmDspPresetMgr_t* p, unsigned instSymId );
  cmDspRC_t _cmDspPresetCreateVar(       cmDspPresetMgr_t* p, unsigned varSymId, const cmDspValue_t* valPtr );
  
  cmDspRC_t _cmDspPresetRecallPreset(    cmDspPresetMgr_t* p, const cmChar_t* groupLabel, const cmChar_t* presetLabel );
  cmDspRC_t _cmDspPresetRecallInstance(  cmDspPresetMgr_t* p, unsigned instSymId );
  cmDspRC_t _cmDspPresetRecallVar(       cmDspPresetMgr_t* p, unsigned varSymId, cmDspValue_t* valPtr );
  
  //)
  
#ifdef __cplusplus
}
#endif

#endif
