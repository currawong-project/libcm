#ifndef cmDspNet_h
#define cmDspNet_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"'snap' distributed host UDP networking implementation." kw:[snap]}
  
#define cmDspSys_PARENT_SYM_TBL_BASE_ID 10000
#define cmDspSys_AsSubIdx_Zero (0)

  typedef struct _cmDspClass_str
  {
    cmDspClass_t*           classPtr;
    unsigned                symId;
    struct _cmDspClass_str* linkPtr;
  } _cmDspClass_t;

  typedef struct _cmDspInst_str
  {
    cmDspInst_t*           instPtr;
    struct _cmDspInst_str* linkPtr;
  } _cmDspInst_t;

  typedef struct
  {
    unsigned id;
    bool     helloFl;  // recv'd a sync 'hello' msg from this node
    bool     reqDoneFl;// recv'd a synd 'requests done' msg from this node
    bool     doneFl;   // recv'd a sync 'done' msg from this node
    bool     localFl;  // this is the local node
  } _cmDspNetNode_t;


  // connections from a DSP instance on this machine - this list exists on the src machine only
  // to a DSP instance on another machine
  typedef struct _cmDspSrcConn_str
  {
    unsigned              srcId;        // address provided by this machine
    unsigned              dstId;        // address provided by the remote machine
    unsigned              dstNetNodeId;
    cmChar_t*             dstInstLabel;
    cmChar_t*             dstVarLabel;
    struct _cmDspSrcConn_str* link;  
  } _cmDspSrcConn_t;

  // Connection proxies for connection destinations - this list exists on the dst machine only
  typedef struct _cmDspDstConn_str
  {
    unsigned                  dstId;    // address provided by this machine (same as dstId on other machine)
    unsigned                  srcNetNodeId; // net node Id of the source - srcId is unique to this machine
    unsigned                  srcId;    // address provided by remote machine (same as srcId on other machine)
    cmDspInst_t*              dstInst;  // connection destination target instance/var
    unsigned                  dstVarId; //        
    struct _cmDspDstConn_str* link;     //
  } _cmDspDstConn_t;


  typedef struct
  {
    cmErr_t             err;
    cmCtx_t             cmCtx;
    cmDspCtx_t          ctx;
    cmLHeapH_t          lhH;      // DSP system lHeap used for system memory (DSP instance memory uses ctx->lhH so that it can be removed independent of the DSP system memory)
    cmSymTblH_t         stH;      // DSP system symbol table (holds class based symbols) (DSP instances use ctx->stH)
    cmDspStoreH_t       dsH;      // DSP system global variable storate table
    cmJsonH_t           jsH;      // DSP json for use by the system 
    const cmChar_t*     rsrcFn;   // name of the JSON file containing resource specific resource      
    _cmDspClass_t*      classList;
    _cmDspInst_t*       instList;
    cmDspPresetMgr_t    pm;
    unsigned            nextInstId;
    unsigned            pgmIdx;
    cmSeH_t             serialPortH;


    cmUdpNetH_t           netH;          //
    cmThreadH_t           thH;           //
    unsigned              nextDstId;     //
    unsigned              netNodeCnt;    //
    _cmDspNetNode_t*      netNodeArray;  // netNodeArray[ netNodeCnt ]
    _cmDspSrcConn_t*      srcConnList;   // linked list of all dstConn recds
    _cmDspDstConn_t*      dstConnList;   // linked list of all srcConn recds
    _cmDspSrcConn_t**     srcConnMap;    // srcConnMap[srcConnMapCnt] array of all srcConn recd ptr's mapped to srcId
    unsigned              srcConnMapCnt; // count of records in srcConnMap[]
    _cmDspDstConn_t**     dstConnMap;    // dstConnMap[dstConnMapCnt] array of all dstConn recd ptr's mapped to dstId
    unsigned              dstConnMapCnt; // count of record in dstConnMap[]
    bool                  netDoneSentFl; // true when this node has broadcast it's 'done' msg
    unsigned              netVerbosity; 
    unsigned              sendWaitMs;
    unsigned              syncState; // see kSyncXXXDspId
    cmDspInstSymId_t*     symIdList; // sym id's which will be assigned to each new instance
  } cmDsp_t;


  // called by cmDspSysInstallNetCb()
  _cmDspSrcConn_t*  _cmDspSysNetCreateSrcConn( cmDsp_t* p, unsigned dstNetNodeId, const cmChar_t* dstInstLabel, const cmChar_t* dstVarLabel );

  // called by cmDspSysInitialize()
  cmDspRC_t  _cmDspSysNetAlloc( cmDsp_t* p );

  // called by cmDspSysFinalize()
  cmDspRC_t  _cmDspSysNetFree( cmDsp_t* p );

  // called by cmDspSysLoad()
  cmDspRC_t  _cmDspSysNetPreLoad( cmDsp_t* p );

  // called by cmDspSysUnload() 
  cmDspRC_t  _cmDspSysNetUnload( cmDsp_t* p );

  // Call this function to enter 'sync' mode.
  cmDspRC_t _cmDspSysNetSync( cmDsp_t* p );

  // Called from cmAudDsp.c:_cmAdUdpNetCallback() to to send an incoming msg to the DSP system.
  cmDspRC_t _cmDspSysNetRecv( cmDsp_t* p, const cmDspNetMsg_t* msg, unsigned msgByteCnt, unsigned remoteNetNodeId );


  cmDspRC_t _cmDspSysNetSendEvent( cmDspSysH_t h, unsigned dstNetNodeId,  unsigned dstId, const cmDspEvt_t* evt );

  //)
  
#ifdef __cplusplus
   }
#endif

#endif
