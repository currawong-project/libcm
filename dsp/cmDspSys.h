#ifndef cmDspSys_h
#define cmDspSys_h

#ifdef __cplusplus
extern "C" {
#endif


  extern cmDspSysH_t cmDspNullHandle;


  // Print labels of the form 'label'-xxx. Where xxx is an integer.
  // The total length of the label must be <=127 or it will be truncated.
  // The returned string is kept in static memory and therefore will be
  // rewritten by the next call to this function.  This makes the function
  // inherently useless if called by multiple threads.
  const cmChar_t* cmDspSysPrintLabel( const cmChar_t* label, unsigned i );
  const cmChar_t* cmDspSysPrintLabel2( const cmChar_t* label, unsigned i );


  //----------------------------------------------------------------------------------------------------
  //  Control Functions
  //

  cmDspRC_t cmDspSysInitialize( cmCtx_t* ctx, cmDspSysH_t* hp, cmUdpNetH_t netH );
  cmDspRC_t cmDspSysFinalize(  cmDspSysH_t* hp );
  
  bool      cmDspSysIsValid(   cmDspSysH_t h );
  cmDspRC_t cmDspSysLastRC(    cmDspSysH_t h );

  unsigned  cmDspSysPgmCount( cmDspSysH_t h );
  const cmChar_t* cmDspPgmLabel( cmDspSysH_t h, unsigned pgmIdx );

  // Load a DSP program. 
  cmDspRC_t cmDspSysLoad(      cmDspSysH_t  h,  cmAudioSysCtx_t* asCtx, unsigned pgmIdx );

  // Unload the previously loaded DSP program
  cmDspRC_t cmDspSysUnload(    cmDspSysH_t h );

  // Call 'reset' on each of the DSP processer instances in the current program.
  cmDspRC_t cmDspSysReset(     cmDspSysH_t  h );

  // Called by the audioSystem to send messages to the DSP system during runtime.
  cmDspRC_t cmDspSysRcvMsg(    cmDspSysH_t  h, cmAudioSysCtx_t* asCtx, const void* msgPtr, unsigned msgByteCnt, unsigned srcNetNodeId );

  enum { kSyncPreDspId, kSyncPendingDspId, kSyncSuccessDspId, kSyncFailDspId };
  unsigned cmDspSysSyncState( cmDspSysH_t h );

  //----------------------------------------------------------------------------------------------------
  // Preset function:
  //

  // A 'preset group' identifies a collection of 'preset's. The group id allows 
  // a particular preset instance to be restored to the same place from which it was formed.
  unsigned        cmDspSysPresetGroupCount(    cmDspSysH_t h );
  unsigned        cmDspSysPresetGroupSymId(    cmDspSysH_t h, unsigned groupIdx );
  const cmChar_t* cmDspSysPresetGroupLabel(    cmDspSysH_t h, unsigned groupIdx );
  cmDspRC_t       cmDspSysPresetGroupJsonList( cmDspSysH_t h, cmJsonH_t* jsHPtr );
  
  // A 'preset' is a collection of stored DSP instances and their variables.  A preset belongs
  // to a group.  A given group may have multiple presets. Each preset represents a saved
  // instance/var state.
  unsigned        cmDspSysPresetPresetCount(   cmDspSysH_t h, unsigned groupIdx );
  unsigned        cmDspSysPresetPresetSymId(   cmDspSysH_t h, unsigned groupIdx, unsigned presetIdx );
  const cmChar_t* cmDspSysPresetPresetLabel(   cmDspSysH_t h, unsigned groupIdx, unsigned presetIdx );
  cmDspRC_t       cmDspSysPresetPresetJsonList(cmDspSysH_t h, unsigned groupSymId, cmJsonH_t* jsHPtr );

  // This function returns a preset group id from a string label.
  unsigned       cmDspSysPresetRegisterGroup( cmDspSysH_t h, const cmChar_t* groupLabel );
      
  // Create a preset named 'presetLabel' in the group named 'groupLabel'.  If the group does not
  // exist it will be created. The creation will fail if the 'presetLabel' already exists.
  // This function calls the 'storeFunc' on every DSP instance belonging to the group
  // identified by 'groupLabel'.
  cmDspRC_t       cmDspSysPresetCreate(     cmDspSysH_t h, const cmChar_t* groupLabel, const cmChar_t* presetLabel );

  // Apply the stored preset named by 'groupLabel' and 'presetLabel'.  
  cmDspRC_t       cmDspSysPresetRecall(     cmDspSysH_t h, const cmChar_t* groupLabel, const cmChar_t* presetLabel );

  // Helper functions used by DSP instances to read and write preset variable values.  These functions
  // are called from inside the user defined DSP instance 'storeFunc'.
  cmDspRC_t       cmDspSysPresetWriteValue( cmDspSysH_t h, unsigned varSymId, const cmDspValue_t* valPtr );
  cmDspRC_t       cmDspSysPresetReadValue(  cmDspSysH_t h, unsigned varSymId, cmDspValue_t* valPtr );

  //----------------------------------------------------------------------------------------------------
  // Generic constructors
  //

  // Allocate single DSP instances.
  cmDspInst_t* cmDspSysAllocInstSV( cmDspSysH_t h, const cmChar_t* classLabelStr, unsigned presetGroupSymId, const cmChar_t* instLabelStr, unsigned va_cnt, va_list vl );
  cmDspInst_t* cmDspSysAllocInstS(  cmDspSysH_t h, const cmChar_t* classLabelStr, unsigned presetGroupSymId, const cmChar_t* instLabelStr, unsigned va_cnt, ... );
  cmDspInst_t* cmDspSysAllocInst(   cmDspSysH_t h, const cmChar_t* classLabelStr,                            const cmChar_t* instLabelStr, unsigned va_cnt, ... );


  typedef const cmChar_t* (*cmDspSysLabelFunc_t)( cmDspSysH_t h, unsigned i );

  // Allocate arrays of DSP instances.  The memory used by the array is allocated from 
  // the cmDspSys linked heap and is therefore automatically garbage collected.
  cmDspInst_t** cmDspSysAllocInstArraySV( cmDspSysH_t h, unsigned cnt, unsigned presetGroupSymId, const cmChar_t* classLabelStr, const cmChar_t* instLabelStr, cmDspSysLabelFunc_t labelFunc, unsigned va_cnt, va_list vl );
  cmDspInst_t** cmDspSysAllocInstArrayS(  cmDspSysH_t h, unsigned cnt, unsigned presetGroupSymId, const cmChar_t* classLabelStr, const cmChar_t* instLabelStr, cmDspSysLabelFunc_t labelFunc, unsigned va_cnt, ... );
  cmDspInst_t** cmDspSysAllocInstArray(   cmDspSysH_t h, unsigned cnt,                            const cmChar_t* classLabelStr, const cmChar_t* instLabelStr, cmDspSysLabelFunc_t labelFunc, unsigned va_cnt, ... );

  cmDspRC_t    cmDspSysNewColumn( cmDspSysH_t h, unsigned colW );

  cmDspRC_t    cmDspSysInsertHorzBorder( cmDspSysH_t h );

  cmDspRC_t    cmDspSysNewPage( cmDspSysH_t h, const cmChar_t* title );


  //----------------------------------------------------------------------------------------------------
  // Connection Functions.
  //


  cmDspRC_t cmDspSysConnectAudio( cmDspSysH_t h, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarLbl, cmDspInst_t* dstInstPtr, const cmChar_t* dstVarLbl );


  cmDspRC_t cmDspSysConnectAudioN11N( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixStr,   unsigned n );
  cmDspRC_t cmDspSysConnectAudio1NN1( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarPrefixStr, cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel,       unsigned n );
  cmDspRC_t cmDspSysConnectAudio1N1N( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarPrefixStr, cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixLabel, unsigned n );
  cmDspRC_t cmDspSysConnectAudioN1N1( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel,       unsigned n );
  cmDspRC_t cmDspSysConnectAudio11N1( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstPArray[],const cmChar_t* dstVarPrefixStr,   unsigned n );
  cmDspRC_t cmDspSysConnectAudio111N( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarLabel,       unsigned n );

  // Connect srcInstArray[ map[i] ] to dstInst.in[i] - map[dstCnt] therefore contains dstCnt elements
  cmDspRC_t cmDspSysConnectAudioN11NM( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel,  unsigned srcCnt,   cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixStr,   unsigned dstCnt, const unsigned* map );


  cmDspRC_t cmDspSysInstallCb(     cmDspSysH_t h, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarLabel, cmDspInst_t* dstInstPtr, const cmChar_t* dstVarLabel, void* dstCbDataPtr);

  cmDspRC_t cmDspSysInstallCbN11N( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixStr,   unsigned n );
  cmDspRC_t cmDspSysInstallCb1NN1( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarPrefixStr, cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel,       unsigned n );
  cmDspRC_t cmDspSysInstallCb1N1N( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarPrefixStr, cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixLabel, unsigned n );
  cmDspRC_t cmDspSysInstallCbN1N1( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel,       unsigned n );
  cmDspRC_t cmDspSysInstallCb11N1( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel,       unsigned n );
  cmDspRC_t cmDspSysInstallCb111N( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixLabel, unsigned n );
  cmDspRC_t cmDspSysInstallCbN111( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarLabel,       unsigned n );
  cmDspRC_t cmDspSysInstallCb1N11( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarPrefixStr, cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarLabel,       unsigned n );

  cmDspRC_t cmDspSysInstallCb1N1NM( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarPrefixStr, unsigned srcCnt, cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixLabel, unsigned dstCnt, const unsigned* map );
  cmDspRC_t cmDspSysInstallCb1NN1M( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarPrefixStr, unsigned srcCnt, cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel,       unsigned dstCnt, const unsigned* map );
  cmDspRC_t cmDspSysInstallCb1NN1M2(cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarPrefixStr, unsigned srcCnt, const unsigned* map, cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel, unsigned dstCnt );

  cmDspRC_t cmDspSysInstallNetCb(     cmDspSysH_t h, cmDspInst_t* srcInstPtr,  const cmChar_t* srcVarLabel,      const cmChar_t* dstNetNodeLabel, const cmChar_t* dstInstLabel, const cmChar_t* dstVarLabel  );
  cmDspRC_t cmDspSysInstallNetCb1N1N( cmDspSysH_t h, cmDspInst_t* srcInstPtr,  const cmChar_t* srcVarPrefixStr,  const cmChar_t* dstNetNodeLabel, const cmChar_t* dstInstLabel, const cmChar_t* dstVarPrefixStr, unsigned dstOffs, unsigned n  );
  cmDspRC_t cmDspSysInstallNetCb1N1NM(cmDspSysH_t h, cmDspInst_t* srcInstPtr,  const cmChar_t* srcVarPrefixStr,  unsigned srcCnt, const cmChar_t* dstNetNodeLabel, const cmChar_t* dstInstLabel, const cmChar_t* dstVarPrefixStr,  unsigned dstCnt, const unsigned* map ); 

  //----------------------------------------------------------------------------------------------------
  // cmDspSysH_t Accessor Functions
  //

  double          cmDspSysSampleRate( cmDspSysH_t h );
  cmJsonH_t       cmDspSysPgmRsrcHandle( cmDspSysH_t h );
  cmSymTblH_t     cmDspSysSymbolTable( cmDspSysH_t h );

  unsigned        cmDspSysRegisterStaticSymbol( cmDspSysH_t h, const cmChar_t* symLabel );
  unsigned        cmDspSysRegisterSymbol( cmDspSysH_t h, const cmChar_t* symLabel );

  cmCtx_t*        cmDspSysPgmCtx( cmDspSysH_t h );
  cmLHeapH_t      cmDspSysLHeap( cmDspSysH_t h );

  unsigned        cmDspSysNetNodeId(        cmDspSysH_t h );
  const cmChar_t* cmDspSysNetNodeLabel(     cmDspSysH_t h );
  const cmChar_t* cmDspSysNetNodeIdToLabel( cmDspSysH_t h, unsigned netNodeId );
  unsigned        cmDspSysNetNodeLabelToId( cmDspSysH_t h, const cmChar_t* netNodeLabel );

  //----------------------------------------------------------------------------------------------------
  // Attribute Symbol assignment and value broadcasing
  //

  // Each DSP instance can be tagged with an arbitrary number of 'attribute' symbols.
  // The primary use of an attribute symbol is to identify targets of 
  // messages sent via the cmDspSysBroadcastValue() call.

  // This function declares an active attribute symbol which will be assigned to all DSP
  // instances which are created after the function is called and before
  // cmDspSysRemoveInstAttrSymbol().  Multiple attribute symbols may be made active
  // in this way - in which case all active attribute symbols will be assigned to any
  // DSP instances created during their life time.
  // Return symId or cmInvalidId on failure.  
  unsigned    cmDspSysRegisterInstAttrSymbol(    cmDspSysH_t h, unsigned symId );
  unsigned    cmDspSysRegisterInstAttrSymbolStr( cmDspSysH_t h, const cmChar_t* symLabel );

  // Assign a instance attribute symbol to a DSP instance.
  cmDspRC_t   cmDspSysAssignInstAttrSymbol( cmDspSysH_t h, cmDspInst_t* inst, unsigned symId );
  // Return cmInvalidId on error otherwise returns the symbol id associated with symLabel.
  unsigned    cmDspSysAssignInstAttrSymbolStr( cmDspSysH_t h, cmDspInst_t* inst, const cmChar_t* symLabel );

  // Remove a previously registered attribute symbol.
  cmDspRC_t   cmDspSysRemoveInstAttrSymbol(      cmDspSysH_t h, unsigned symId );
  cmDspRC_t   cmDspSysRemoveInstAttrSymbolStr(   cmDspSysH_t h, const cmChar_t* symLabel );

  // Send a value to every DSP instance tagged with the attribute symbol 
  // 'instAttrSymId'.
  // Set instAttrSymId to cmInvalidId to send to all instances
  cmDspRC_t   cmDspSysBroadcastValue( cmDspSysH_t h, unsigned instAttrSymId, const cmDspValue_t* valuePtr );


  //----------------------------------------------------------------------------------------------------
  // Specialized Constructors:
  //

  // align: kRightAlignDuiId, kLeftAlignDuiId, kCenterAlignDuiId
  cmDspInst_t* cmDspSysAllocLabel( cmDspSysH_t h, const cmChar_t* label, unsigned alignId );

  cmDspInst_t*  cmDspSysAllocScalar(     cmDspSysH_t h,                                                                       const cmChar_t* label, cmReal_t min, cmReal_t max, cmReal_t step, cmReal_t val );
  cmDspInst_t*  cmDspSysAllocScalarP(    cmDspSysH_t h,               unsigned presetGroupSymId, const cmChar_t* prefixLabel, const cmChar_t* label, cmReal_t min, cmReal_t max, cmReal_t step, cmReal_t val );
  cmDspInst_t*  cmDspSysAllocScalarRsrc( cmDspSysH_t h,                                                                       const cmChar_t* label, cmReal_t min, cmReal_t max, cmReal_t step, const cmChar_t* rsrcPath );
  cmDspInst_t** cmDspSysAllocScalarA(    cmDspSysH_t h, unsigned cnt, unsigned presetGroupSymId, const cmChar_t* prefixLabel, const cmChar_t* label, cmReal_t min, cmReal_t max, cmReal_t step, cmReal_t val );

  cmDspInst_t* cmDspSysAllocButton(     cmDspSysH_t h, const cmChar_t* label, cmReal_t val );
  cmDspInst_t* cmDspSysAllocButtonP(    cmDspSysH_t h, const cmChar_t* prefixLabel, const cmChar_t* label, cmReal_t val );
  cmDspInst_t* cmDspSysAllocButtonRsrc( cmDspSysH_t h, const cmChar_t* label, const cmChar_t* rsrcPath );

  cmDspInst_t* cmDspSysAllocCheck(     cmDspSysH_t h, const cmChar_t* label, cmReal_t val );
  cmDspInst_t* cmDspSysAllocCheckP(    cmDspSysH_t h, unsigned presetGroupSymId, const cmChar_t* prefixLabel, const cmChar_t* label, cmReal_t val );
  cmDspInst_t* cmDspSysAllocCheckRsrc( cmDspSysH_t h, const cmChar_t* label, const cmChar_t* rsrcPath );

  // If 'fn' is NULL then the DSP rsrc tree is used to locate the resource.
  // The resource named by 'rsrcLabel' may be any place in the resource tree.
  cmDspInst_t* cmDspSysAllocMsgList(  cmDspSysH_t h, const cmChar_t* fn, const cmChar_t* rsrcLabel, unsigned initSelIdx );
  cmDspInst_t* cmDspSysAllocMsgListP( cmDspSysH_t h, unsigned presetGroupSymId, const cmChar_t* preLabel, const cmChar_t* label, const cmChar_t* fn, const cmChar_t* rsrcLabel, unsigned initSelIdx );

  cmDspInst_t*  cmDspSysAllocAudioIn(  cmDspSysH_t h, unsigned chIdx, cmReal_t gain );
  // Set chMapRsrcLabel to NULL to use the rsrc named 'audioInMap'.
  // *retChCntPtr is set to the count of audio channels created (and the count of channels in the map).
  cmDspInst_t** cmDspSysAllocAudioInAR( cmDspSysH_t h, const cmChar_t* chMapRsrcLabel, cmReal_t gain, unsigned* retChCntPtr );

  cmDspInst_t* cmDspSysAllocAudioOut(  cmDspSysH_t h, unsigned chIdx, cmReal_t gain );

  // Set chMapRsrcLabel to NULL to use the rsrc named 'audioOutMap'.
  // *retChCntPtr is set to the count of audio channels created (and the count of channels in the map).
  cmDspInst_t** cmDspSysAllocAudioOutAR( cmDspSysH_t h, const cmChar_t* chMapRsrcLabel, cmReal_t gain, unsigned* retChCntPtr );

  //----------------------------------------------------------------------------------------------------
  // Read resource values from the program resource file.
  //

  cmDspRC_t cmDspRsrcBoolV(   cmDspSysH_t h, bool*      vp, va_list vl );
  cmDspRC_t cmDspRsrcIntV(    cmDspSysH_t h, int*       vp, va_list vl );
  cmDspRC_t cmDspRsrcUIntV(   cmDspSysH_t h, unsigned*  vp, va_list vl );
  cmDspRC_t cmDspRsrcDblV(    cmDspSysH_t h, double*    vp, va_list vl );
  cmDspRC_t cmDspRsrcRealV(   cmDspSysH_t h, cmReal_t*  vp, va_list vl );
  cmDspRC_t cmDspRsrcStringV( cmDspSysH_t h, const cmChar_t** vp, va_list vl );

  cmDspRC_t cmDspRsrcArrayCountV( cmDspSysH_t h, unsigned* np, va_list vl );
  cmDspRC_t cmDspRsrcBoolArrayV(  cmDspSysH_t h, unsigned* np, bool**      vpp, va_list vl );
  cmDspRC_t cmDspRsrcIntArrayV(   cmDspSysH_t h, unsigned* np, int**       vpp, va_list vl );
  cmDspRC_t cmDspRsrcUIntArrayV(  cmDspSysH_t h, unsigned* np, unsigned**  vpp, va_list vl );
  cmDspRC_t cmDspRsrcDblArrayV(   cmDspSysH_t h, unsigned* np, double**    vpp, va_list vl );
  cmDspRC_t cmDspRsrcRealArrayV(  cmDspSysH_t h, unsigned* np, cmReal_t**  vpp, va_list vl );
  cmDspRC_t cmDspRsrcStringArrayV(cmDspSysH_t h, unsigned* np, const cmChar_t*** vpp, va_list vl );

  cmDspRC_t cmDspRsrcInt(    cmDspSysH_t h, int*       vp, ... );
  cmDspRC_t cmDspRsrcBool(   cmDspSysH_t h, bool*      vp, ... );
  cmDspRC_t cmDspRsrcUInt(   cmDspSysH_t h, unsigned*  vp, ... );
  cmDspRC_t cmDspRsrcDbl(    cmDspSysH_t h, double*    vp, ... );
  cmDspRC_t cmDspRsrcReal(   cmDspSysH_t h, cmReal_t*  vp, ... );
  cmDspRC_t cmDspRsrcString( cmDspSysH_t h, const cmChar_t** vp, ... );

  cmDspRC_t cmDspRsrcArrayCount(  cmDspSysH_t h, unsigned* np, ... );
  cmDspRC_t cmDspRsrcBoolArray(   cmDspSysH_t h, unsigned* np, bool**      vpp, ... );
  cmDspRC_t cmDspRsrcIntArray(    cmDspSysH_t h, unsigned* np, int**       vpp, ... );
  cmDspRC_t cmDspRsrcUIntArray(   cmDspSysH_t h, unsigned* np, unsigned**  vpp, ... );
  cmDspRC_t cmDspRsrcDblArray(    cmDspSysH_t h, unsigned* np, double**    vpp, ... );
  cmDspRC_t cmDspRsrcRealArray(   cmDspSysH_t h, unsigned* np, cmReal_t**  vpp, ... );
  cmDspRC_t cmDspRsrcStringArray( cmDspSysH_t h, unsigned* np, const cmChar_t*** vpp, ... );
 

  cmDspRC_t cmDspRsrcWriteStringV( cmDspSysH_t h, const cmChar_t* v, va_list vl );

  cmDspRC_t cmDspRsrcWriteString(  cmDspSysH_t h, const cmChar_t* v, ... );


#ifdef __cplusplus
  }
#endif

#endif
