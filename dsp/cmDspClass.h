#ifndef cmDspClass_h
#define cmDspClass_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned   cmDspRC_t;

  enum
  {
    kOkDspRC = cmOkRC,   // 0
    kMemAllocFailDspRC,  // 1
    kLHeapFailDspRC,     // 2
    kJsonFailDspRC,      // 3
    kFileSysFailDspRC,   // 4
    kSymTblFailDspRC,    // 5
    kThreadFailDspRC,    // 6
    kNetFailDspRC,       // 7
    kCsvFailDspRC,       // 8
    kDspStoreFailDspRC,

    kProcFailDspRC,

    kAllocInstFailDspRC,
    kClassFinalFailDspRC,
    kInstFinalFailDspRC,
    kInstResetFailDspRC,
    kInstExecFailDspRC,
    kInstStoreFailDspRC,
    kInstRecallFailDspRC,
    kInstMsgRcvFailDspRC,
    kNetSendAllocFailDspRC,

    kClassNotFoundDspRC,
    kInstNotFoundDspRC,
    kDuplInstSymIdDspRC,
    kVarNotFoundDspRC,
    kSrcVarNotFoundDspRC,
    kRsrcNotFoundDspRC,
    kSymNotFoundDspRC,
    kNetNodeNotFoundDspRC,

    kDuplPresetInstDspRC,
    kDuplPresetVarDspRC,
    kPresetGrpNotFoundDspRC,
    kPresetPreNotFoundDspRC,
    kPresetInstNotFoundDspRC,
    kPresetVarNotFoundDspRC,

    kVarDuplicateDspRC,
    kVarTypeErrDspRC,
    kVarNotValidDspRC,
    kVarArgParseFailDspRC,

    kInstCbInstallFailDspRC,
    kConnectFailDspRC,

    kInstSetFlagsFailDspRC,

    kSerializeUiMsgFailDspRC,

    kSendToHostFailDspRC,
    kUiEleCreateFailDspRC,

    kInvalidArgDspRC,
    kInvalidStateDspRC,
    kSubSysFailDspRC,

    kFileWriteFailDspRC,
    kFileReadFailDspRC,
    kFileOpenFailDspRC,
    kFileCloseFailDspRC,

    kInvalidPgmIdxDspRC,
    kPgmCfgFailDspRC
  };


  struct cmDspClass_str;
  struct cmDspInst_str;

  enum
  {
    kUiDspFl     = 0x01,  // set in cmDspSysHandleUiMsg() to indicate that a msg originated from a UI control.
    kUiEchoDspFl = 0x02,  // set in cmDspSysHandleUiMsg() to indicate that the value set from a UI control should sent back to it by the DSP engine.
    kDfltEvtDspFlags = 0
  };

  // DSP event record
  typedef struct
  {
    unsigned              flags;      // See kXXDspFl
    struct cmDspInst_str* srcInstPtr; // source instance of the event or NULL if the system generated the event
    unsigned              srcVarId;   // source selector id used to identify this event (See Note 1 below.)
    unsigned              dstVarId;   // dest. selector id used to identify the target of the event
    void*                 dstDataPtr; // dest. supplied custom callback data pointer
    const cmDspValue_t*   valuePtr;   // event message data    
  } cmDspEvt_t;
  
#define cmDspEvtCopy( e0, e1 ) (*(e0)) = (*(e1))

  // Note 1: When the event was generated from a UI msg then:
  //   a. The kUiDspFl is set and the kUiEchoDspFl if the UI requested duplex operation.
  //   b. srcVarId is set with the cmDspUiHdr_t selId value of the UI msg.

  typedef struct cmDspClass_str* (*cmDspClassConsFunc_t)(cmDspCtx_t* ctx);
  typedef cmDspRC_t              (*cmDspClassFunc_t)( cmDspCtx_t* ctx, struct cmDspClass_str* classPtr );
  typedef struct cmDspInst_str*  (*cmDspConsFunc_t)(  cmDspCtx_t* ctx, struct cmDspClass_str* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl );
  typedef cmDspRC_t              (*cmDspFunc_t)(      cmDspCtx_t* ctx, struct cmDspInst_str*  thisPtr,  const cmDspEvt_t* evtPtr );
  typedef cmDspRC_t              (*cmDspStoreFunc_t)( cmDspCtx_t* ctx, struct cmDspInst_str*  thisPtr,  bool Fl );
  typedef cmDspRC_t              (*cmDspAttrSymFunc_t)(cmDspCtx_t* ctx, struct cmDspInst_str*  thisPtr, unsigned attrSymId, const cmDspValue_t* value );

  // DSP class record
  typedef struct cmDspClass_str
  {
    cmErr_t          err;
    const cmChar_t*  labelStr;
    const cmChar_t*  doc;
    cmDspClassFunc_t finalClassFunc; // finalize the class   
    cmDspConsFunc_t  allocFunc;      // allocate an instance
    cmDspFunc_t      freeFunc;       // free an instance
    cmDspFunc_t      resetFunc;      //
    cmDspFunc_t      execFunc;       //
    cmDspFunc_t      recvFunc;       // 
    cmDspStoreFunc_t storeFunc;      // store the state of this instance
    cmDspAttrSymFunc_t sysRecvFunc;  // attr/value recv function (see cmDspSysBroadcastValue()
  } cmDspClass_t;


  // record used to maintain an event target callback chain
  typedef struct cmDspInstCb_str
  {
    unsigned                 srcVarSymId;  // src var symbol id
    struct cmDspInst_str*    dstInstPtr;   // target instance
    unsigned                 dstVarId;     // target instance sel id
    void*                    dstDataPtr;   // target instance custom data
    struct cmDspInstCb_str*  linkPtr;      // chain link
  } cmDspCb_t;


  // record used to maintain instance variables
  typedef struct 
  {
    unsigned        flags;    // see xxxDsvFl in cmDspValue.h for possible flag values
    unsigned        constId;  // constant id provided by the instance
    unsigned        symId;    // symbol id assoc'd with this var's label
    cmDspValue_t    value;    // current value of this variable
    cmDspValue_t    dflt;     // default value for this variable
    cmDspCb_t*      cbList;   // event targets registered with this instance
    const cmChar_t* doc;      // document string
  } cmDspVar_t;


  typedef struct
  {
    const char*      label;
    unsigned         constId;
    unsigned         rn;
    unsigned         cn;
    unsigned         flags;
    const cmChar_t*  doc;
  } cmDspVarArg_t;

  typedef struct cmDspInstSymId_str
  {
    unsigned                   symId;  // set to cmInvalidId if the recd is not active
    struct cmDspInstSymId_str* link;
  } cmDspInstSymId_t;


  enum
  {
    kDisableExecInstFl = 0x01
  };

  typedef struct cmDspInst_str
  {
    struct cmDspClass_str* classPtr;  // ptr to class for this instance
    unsigned               symId;     // optional unique instance label symbol id
    unsigned               id;        // id is unique among all insts
    unsigned               flags;     // See kXXXInstFl above
    cmDspVar_t*            varArray;  //
    unsigned               varCnt;    //
    cmDspFunc_t            freeFunc;  // free an instance
    cmDspFunc_t            resetFunc; // reset a instance to its default values
    cmDspFunc_t            execFunc;  // DSP clock tick
    cmDspFunc_t            recvFunc;  // recv an event
    cmDspStoreFunc_t       storeFunc; //
    cmDspAttrSymFunc_t     sysRecvFunc;
    unsigned               presetGroupSymId;// 
    cmDspInstSymId_t*      symIdList; // 

  } cmDspInst_t;


  cmDspRC_t           cmDspClassSetup( 
    cmDspClass_t*     classPtr,         // class to setup
    cmDspCtx_t*       ctx,              //
    const cmChar_t*   classLabel,       // 
    cmDspClassFunc_t  finalClassFunc,   // finalize the class   
    cmDspConsFunc_t   allocFunc,        // allocate an instance
    cmDspFunc_t       freeFunc,         // free an instance
    cmDspFunc_t       resetFunc,
    cmDspFunc_t       execFunc,
    cmDspFunc_t       recvFunc,
    cmDspStoreFunc_t  storeFunc,
    cmDspAttrSymFunc_t sysRecvFunc,
    const cmChar_t*   doc
                                       );

  void* cmDspInstAllocate(cmDspCtx_t* ctx, cmDspClass_t* classPtr, const cmDspVarArg_t* argV, unsigned instByteCnt, unsigned instSymId, unsigned instId, unsigned storeSymId, unsigned vargCnt, va_list vl );

#define cmDspInstAlloc(T,ctx,classPtr,args,symId,id,storeSymId,vargCnt,vl) (T*)cmDspInstAllocate(ctx,classPtr,args,sizeof(T),symId,id,storeSymId,vargCnt,vl)

  void* cmDspInstAllocateV(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned instByteCnt, unsigned instSymId, unsigned instId, unsigned storeSymId, unsigned vargCnt, va_list vl0, ... );
#define cmDspInstAllocV(T,ctx,classPtr,symId,id,storeSymId,va_cnt,vl, ... ) (T*)cmDspInstAllocateV(ctx,classPtr,sizeof(T),symId,id,storeSymId,va_cnt,vl, __VA_ARGS__ )

  bool        cmDspInstHasAttrSym(      cmDspInst_t* inst, unsigned attrSymId );
  cmDspRC_t   cmDspInstRegisterAttrSym( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned attrSymId );
  cmDspRC_t   cmDspInstRemoveAttrSym(   cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned attrSymId );

  cmDspRC_t   cmDspClassErr( cmDspCtx_t* ctx, cmDspClass_t* classPtr, cmDspRC_t rc, const cmChar_t* fmt, ... );
  cmDspRC_t   cmDspInstErr(  cmDspCtx_t* ctx, cmDspInst_t*  inst,     cmDspRC_t rc, const cmChar_t* fmt, ... );

  // Helper functions for setting up enumerated cmDspVarArg_t records.
  void     cmDspArgSetup(  cmDspCtx_t* ctx, cmDspVarArg_t* arg, const cmChar_t* labelPrefix, unsigned labelId, unsigned constId, unsigned rn, unsigned cn, unsigned flags, const cmChar_t* docStr ); 
  unsigned cmDspArgCopy(   cmDspVarArg_t* argArray, unsigned argN, unsigned dstIdx, const cmDspVarArg_t* s, unsigned sn );
  unsigned cmDspArgSetupN( cmDspCtx_t* ctx, cmDspVarArg_t* arg, unsigned argN, unsigned dstIdx, unsigned cnt, const cmChar_t* labelPrefix, unsigned baseConstId, unsigned rn, unsigned cn, unsigned flags, const cmChar_t* staticDocStr ); 
  void     cmDspArgSetupNull( cmDspVarArg_t* arg );

  const cmChar_t*   cmDspInstLabel( cmDspCtx_t* ctx, cmDspInst_t* inst ); 


  // Given and instance and a variable symbol id return a pointer to the variable.
  cmDspVar_t* cmDspVarSymbolToPtr( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varSymId, unsigned flags );

  // Given an instance and a var id return a pointer to the variable.
  const cmDspVar_t* cmDspVarIdToCPtr( const cmDspInst_t* inst, unsigned varId );
  cmDspVar_t*       cmDspVarIdToPtr( cmDspInst_t* inst, unsigned varId );


  const cmChar_t*   cmDspVarLabel( cmDspCtx_t* ctx, const cmDspInst_t* inst, unsigned varId );

  // Make callbacks for the specified instance var.
  cmDspRC_t   cmDspOutputEvent( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId );

  cmDspRC_t   cmDspVarPresetWrite(  cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId );
  cmDspRC_t   cmDspVarPresetRead(   cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId );
  cmDspRC_t   cmDspVarPresetRdWr(     cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, bool storeFl );

  double      cmDspSampleRate(      cmDspCtx_t* ctx );
  unsigned    cmDspSamplesPerCycle( cmDspCtx_t* ctx );


  // Query the type of a variable
  unsigned cmDspTypeFlags(const cmDspInst_t* inst, unsigned varId );   
  bool     cmDspIsBool(   const cmDspInst_t* inst, unsigned varId );
  bool     cmDspIsInt(    const cmDspInst_t* inst, unsigned varId );
  bool     cmDspIsUInt(   const cmDspInst_t* inst, unsigned varId );
  bool     cmDspIsDouble( const cmDspInst_t* inst, unsigned varId );
  bool     cmDspIsStrz(   const cmDspInst_t* inst, unsigned varId );
  bool     cmDspIsSymbol( const cmDspInst_t* inst, unsigned varId );
  bool     cmDspIsJson(   const cmDspInst_t* inst, unsigned varId );


  // Get the value of an instance variable.
  bool            cmDspBool(   cmDspInst_t* inst, unsigned varId );
  int             cmDspInt(    cmDspInst_t* inst, unsigned varId );
  unsigned        cmDspUInt(   cmDspInst_t* inst, unsigned varId );
  double          cmDspDouble( cmDspInst_t* inst, unsigned varId );
  cmSample_t      cmDspSample( cmDspInst_t* inst, unsigned varId );
  cmReal_t        cmDspReal(   cmDspInst_t* inst, unsigned varId );
  const cmChar_t* cmDspStrcz(   cmDspInst_t* inst, unsigned varId );
  unsigned        cmDspSymbol( cmDspInst_t* inst, unsigned varId );
  cmJsonNode_t*   cmDspJson(   cmDspInst_t* inst, unsigned varId );

  // Get the default value of an instance variable.
  bool            cmDspDefaultBool(   cmDspInst_t* inst, unsigned varId );
  int             cmDspDefaultInt(    cmDspInst_t* inst, unsigned varId );
  unsigned        cmDspDefaultUInt(   cmDspInst_t* inst, unsigned varId );
  double          cmDspDefaultDouble( cmDspInst_t* inst, unsigned varId );
  cmSample_t      cmDspDefaultSample( cmDspInst_t* inst, unsigned varId );
  cmReal_t        cmDspDefaultReal(   cmDspInst_t* inst, unsigned varId );
  const cmChar_t* cmDspDefaultStrcz(   cmDspInst_t* inst, unsigned varId );
  unsigned        cmDspDefaultSymbol( cmDspInst_t* inst, unsigned varId );
  cmJsonNode_t*   cmDspDefaultJson(   cmDspInst_t* inst, unsigned varId );


  // Possible values for cmDspSetXXX()
  enum
  {
    kUpdateUiDspFl   = 0x00,  // 
    kNoUpdateUiDspFl = 0x01,  // don't callback the UI
    kNoAllocDspFl    = 0x02,  // the caller is handling memory mgmt for the incoming value don't allocate space for it internally
    kSetDefaultDspFl = 0x04,  // set the var default value rather than the current value
    kNoSendDspFl     = 0x08   // do not transmit the value to connected objects
  };

  // Instance variable setter functions:
  //
  // All forms of the setter assign a value to of the specified instance variable and 
  // then make the callbacks attached to the variable. Note that callbacks to the UI
  // can be prevented by setting the kNoUpdateUiDspFl in the call to cmDspSet<type>X().
  //
  //
  // All setters are implemented in terms of the cmDspValueSet()
  //

  // Convert the source value (svp) to the type of the target var (inst->varArray[varId])
  // and assign it to the var value. All the cmDspSetXXX() functions work in terms of this function.
  // See the kXXXDspFl flags above for possible 'flag' values.
  cmDspRC_t   cmDspValueSet(         cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmDspValue_t* vp, unsigned flags );

  // Assign the default value to the current value (if the the specified variable has a valid default value).
  cmDspRC_t   cmDspApplyDefault(     cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId );

  // Apply the default value to the current value for all variables which has a valid default value.
  // This is 
  cmDspRC_t   cmDspApplyAllDefaults( cmDspCtx_t* ctx, cmDspInst_t* inst );

  // Set the default value of a variable.
  cmDspRC_t   cmDspSetDefault( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmDspValue_t* valPtr );

  // Set the default variable of a variable to 'val'.
  // The call is ignored (and the default value is not set) if the default
  // value was previously set to a vaue which is not equal to 'nonInitVal'.
  cmDspRC_t   cmDspSetDefaultBool(   cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, bool            nonInitVal, bool            val );
  cmDspRC_t   cmDspSetDefaultInt(    cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, int             nonInitVal, int             val );
  cmDspRC_t   cmDspSetDefaultUInt(   cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned        nonInitVal, unsigned        val );
  cmDspRC_t   cmDspSetDefaultDouble( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, double          nonInitVal, double          val );
  cmDspRC_t   cmDspSetDefaultSample( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, cmSample_t      nonInitVal, cmSample_t      val );
  cmDspRC_t   cmDspSetDefaultReal(   cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, cmReal_t        nonInitVal, cmReal_t        val );
  cmDspRC_t   cmDspSetDefaultStrcz(  cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmChar_t* nonInitVal, const cmChar_t* val );
  cmDspRC_t   cmDspSetDefaultSymbol( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, /*unsigned cmInvalidId*/    unsigned        val );
  cmDspRC_t   cmDspSetDefaultJson(   cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, cmJsonNode_t*   nonInitVal, cmJsonNode_t*   val );

  // Set the value of a variable and make any attached callbacks.
  // If the kUiDsvFl is set on the variable then the UI is also informed of the new value.
  // If the source ('val') type does not match the variable type then if possible the
  // source is converted to the dest (variable) type.
  // This function is implemented in terms of cmDspValueSet().
  cmDspRC_t   cmDspSetBool(          cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, bool            val );
  cmDspRC_t   cmDspSetInt(           cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, int             val );
  cmDspRC_t   cmDspSetUInt(          cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned        val );
  cmDspRC_t   cmDspSetDouble(        cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, double          val );
  cmDspRC_t   cmDspSetSample(        cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, cmSample_t      val );
  cmDspRC_t   cmDspSetReal(          cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, cmReal_t        val );
  cmDspRC_t   cmDspSetStrcz(         cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmChar_t* val );
  cmDspRC_t   cmDspSetSymbol(        cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned        val );
  cmDspRC_t   cmDspSetJson(          cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, cmJsonNode_t*   val );

  // Set the value of a variable to the *evt->valuePtr and make any attached callbacks.
  // If the kUiDsvFl is set and kNoUiEchoDspFl is not set in evt->flags then UI is informed of the new value.
  // If the source (*evt->valuePtr) type does not match the variable type then if possible the
  // source is converted to the dest (variable) type.
  // This function is implemented in terms of cmDspValueSet().
  cmDspRC_t   cmDspSetEvent(         cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt );

  // Same as cmDspSetEvent() but the event is automatically echoed to the UI.
  cmDspRC_t   cmDspSetEventUi(         cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt );
  // Same as cmDspSetEventUi() but change the event target variable to 'varId'.
  cmDspRC_t   cmDspSetEventUiId(       cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt, unsigned varId );

  // Get the row and col count of a matrix valued instance variable.
  unsigned    cmDspVarRows(      cmDspInst_t* inst, unsigned varId );
  unsigned    cmDspVarCols(      cmDspInst_t* inst, unsigned varId );

  // Audio buffer helper functions
  bool        cmDspIsAudioInputConnected( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId );
  cmDspRC_t   cmDspZeroAudioBuf(     cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId );
  cmSample_t* cmDspAudioBuf(         cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned chIdx );
  unsigned    cmDspAudioBufSmpCount( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned chIdx );

  // Register dspInstPtr to receive a callback when srcInstPtr generates an event identified by srcVarId.
  cmDspRC_t   cmDspInstInstallCb( cmDspCtx_t* ctx, cmDspInst_t* srcInstPtr, unsigned        srcVarId,    cmDspInst_t* dstInstPtr, unsigned        dstVarSymId, void* dstCbDataPtr);
  cmDspRC_t   cmDspInstallCb(     cmDspCtx_t* ctx, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarLabel, cmDspInst_t* dstInstPtr, const cmChar_t* dstVarLabel, void* dstCbDataPtr);

  // Uninstall a previous registred instance variable callback function.
  cmDspRC_t   cmDspInstRemoveCb(  cmDspCtx_t* ctx, cmDspInst_t* srcInstPtr, unsigned srcVarId,           cmDspInst_t* dstInstPtr, unsigned dstVarId );
  cmDspRC_t   cmDspRemoveCb(      cmDspCtx_t* ctx, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarLabel, cmDspInst_t* dstInstPtr, unsigned dstVarId );

  // 
  cmDspRC_t   cmDspInstVarSetFlags( cmDspCtx_t* ctx, cmDspInst_t* instPtr, unsigned varId, unsigned flags );

  // Used to transmit messages to the audio system.
  cmDspRC_t  cmDspSendValueToAudioSys( cmDspCtx_t* ctx, unsigned msgTypeId, unsigned selId, unsigned valId, const cmDspValue_t* valPtr ); 

  // The following functions are used to send message to the UI and are 
  // implemented in cmDspUi.c.  They are declared here because they are 
  // visible to the cmDspInst functions which use them but are not available
  // to the host UI.  The other values defined and declared in cmDspUi.h are
  // common to both the host UI and the DSP instances.
  cmDspRC_t  cmDspUiConsolePrint(      cmDspCtx_t* ctx, cmChar_t* text );
  cmDspRC_t  cmDspUiSendValue(         cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmDspValue_t* valPtr ); 
  cmDspRC_t  cmDspUiSendVar(           cmDspCtx_t* ctx, cmDspInst_t* inst, cmDspVar_t* var );
  cmDspRC_t  cmDspUiScalarCreate(      cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned typeDuiId, unsigned minVarId,  unsigned maxVarId,  unsigned stpVarId, unsigned valVarId, unsigned lblVarId );
  cmDspRC_t  cmDspUiTextCreate(        cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned valVarId, unsigned lblVarId );
  cmDspRC_t  cmDspUiMeterCreate(       cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned minVarId,  unsigned maxVarId,  unsigned valVarId,  unsigned lblVarId );
  cmDspRC_t  cmDspUiButtonCreate(      cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned typeDuiId, unsigned outVarId, unsigned lblVarId );
  cmDspRC_t  cmDspUiLabelCreate(       cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned lblVarId,  unsigned alignVarId );
  cmDspRC_t  cmDspUiTimeLineCreate(    cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned tlFileVarId,  unsigned audPathVarId, unsigned selVarId, unsigned cursVarId );
  cmDspRC_t  cmDspUiScoreCreate(       cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned scFileVarId,  unsigned selVarId, unsigned smpIdxVarId, unsigned pitchVarId, unsigned velVarId, unsigned locIdxVarIdx, unsigned evtIdxVarIdx, unsigned dynLvlVarIdx, unsigned valTypeVarIdx, unsigned valueVarIdx );
  cmDspRC_t  cmDspUiTakeSeqBldrCreate( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned fileNameVarId );

  cmDspRC_t  cmDspUiNewColumn(        cmDspCtx_t* ctx, unsigned colW );
  cmDspRC_t  cmDspUiInsertHorzBorder( cmDspCtx_t* ctx );
  cmDspRC_t  cmDspUiNewPage(          cmDspCtx_t* ctx, const cmChar_t* title );

  enum { kFnameFnDspFl=0x00, kFnameDirDspFl=0x01 };
  cmDspRC_t   cmDspUiFnameCreate(  cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned valVarId, unsigned patVarId, unsigned dirVarId );
  cmDspRC_t   cmDspUiMsgListCreate(cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned height, unsigned listVarId, unsigned selVarId );

 

#ifdef __cplusplus
}
#endif

#endif
