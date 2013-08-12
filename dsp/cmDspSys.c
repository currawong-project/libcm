#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmText.h"
#include "cmFileSys.h"
#include "cmSymTbl.h"
#include "cmMidi.h"
#include "cmJson.h"
#include "cmPrefs.h"
#include "cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmAudioSys.h"
#include "cmProcObj.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspStore.h"
#include "cmDspSys.h"
#include "cmDspBuiltIn.h"
#include "cmDspPgm.h"
#include "cmDspPreset.h"
#include "cmDspNet.h"
#include "cmTime.h"

cmDspSysH_t cmDspNullHandle = cmSTATIC_NULL_HANDLE;

#define kDspSysLabelCharCnt (127)
cmChar_t _cmDspSysLabel0[ kDspSysLabelCharCnt + 1 ];
cmChar_t _cmDspSysLabel1[ kDspSysLabelCharCnt + 1 ];

const cmChar_t* cmDspSysPrintLabel( const cmChar_t* label, unsigned i )
{
  snprintf(_cmDspSysLabel0,kDspSysLabelCharCnt,"%s-%i",label,i);

  assert( strlen(_cmDspSysLabel0 ) <= kDspSysLabelCharCnt );

  return _cmDspSysLabel0;
}

const cmChar_t* cmDspSysPrintLabel2( const cmChar_t* label, unsigned i )
{
  snprintf(_cmDspSysLabel1,kDspSysLabelCharCnt,"%s-%i",label,i);

  assert( strlen(_cmDspSysLabel1 ) <= kDspSysLabelCharCnt );
    
  return _cmDspSysLabel1;
}

///============================================================================================
cmDsp_t* _cmDspHandleToPtr( cmDspSysH_t h )
{
  assert(h.h != NULL);
  return (cmDsp_t*)h.h;
}


cmDspInst_t* _cmDspSysInstSymIdToPtr( cmDsp_t* p, unsigned instSymId )
{
  _cmDspInst_t* ip = p->instList;
  for(; ip!=NULL; ip = ip->linkPtr )
    if( ip->instPtr->symId == instSymId )
      return ip->instPtr;

  return NULL;
}


// free a single instance
cmDspRC_t _cmDspInstFree( cmDspCtx_t* ctx,  cmDspInst_t* inst )
{ 
  cmDspRC_t rc = kOkDspRC;

  if( inst->freeFunc != NULL )
    if((rc = inst->freeFunc( ctx, inst, NULL )) != kOkDspRC )
      return rc;

  cmLHeapFree(ctx->lhH,inst);

  return kOkDspRC;
}


cmDspRC_t _cmDspSysFreeInst( cmDsp_t* p, unsigned instId )
{
  cmDspRC_t     rc = kOkDspRC;
  _cmDspInst_t* ip = p->instList;
  _cmDspInst_t* pp = NULL;

  // locate the requested instance
  for(; ip!=NULL; ip=ip->linkPtr)
  {
    if( ip->instPtr->id == instId )
      break;

    pp = ip;
  }

  // verify the instance was located
  if( ip == NULL )
    return cmErrMsg(&p->err,kInstNotFoundDspRC,"The DSP instance %i could not be found.",instId);
  
  // instruct the instance to release its resources
  if( _cmDspInstFree(&p->ctx,ip->instPtr ) != kOkDspRC )
      return cmErrMsg(&p->err,kInstFinalFailDspRC,"An attempt to free DSP instance '%s' id:%i failed.",ip->instPtr->classPtr->labelStr,ip->instPtr->id);

  // remove the instance from the linked list
  if( pp == NULL )
    p->instList = NULL;
  else
    pp->linkPtr = ip->linkPtr;

  return rc;
}


cmDspRC_t _cmDspSysFinalize( cmDsp_t* p )
{
  cmDspRC_t rc = kOkDspRC;

  _cmDspClass_t* cp = p->classList;

  _cmDspSysNetFree(p);
  
  // finalize the classes - no need to free the class ctl
  // recd's because they were allocated in the linked heap
  for(; cp!=NULL; cp=cp->linkPtr)
  {
    if( cp->classPtr->finalClassFunc != NULL )
      if( cp->classPtr->finalClassFunc(&p->ctx,cp->classPtr) != kOkDspRC )
        rc = cmErrMsg(&p->err,kClassFinalFailDspRC,"Class finalization failed on class '%s'.",cp->classPtr->labelStr);
  }

  if( p->ctx.cmProcCtx != NULL )
    if( cmCtxFree(&p->ctx.cmProcCtx) != cmOkRC )
      rc = cmErrMsg(&p->err,kProcFailDspRC,"The proc context finalizatoin failed.");

  cmDspStoreFree(&p->dsH);

  if( cmSymTblIsValid(p->stH) ) 
    cmSymTblDestroy(&p->stH);

  if( cmJsonIsValid(p->jsH) )
    if( cmJsonFinalize(&p->jsH) != kOkJsRC )
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"JSON finalization failed.");

  if( cmLHeapIsValid(p->lhH) )
    cmLHeapDestroy(&p->lhH);

  if( rc == kOkJsRC )
  {
    cmMemFree(p);

    // clear the cmAudioSysCtx_t
    memset(&p->ctx.ctx,0,sizeof(p->ctx.ctx));
  }
  return rc;
}

cmDspRC_t cmDspSysInitialize( cmCtx_t* ctx, cmDspSysH_t* hp, cmUdpNetH_t netH )
{
  unsigned        i;
  cmDspRC_t       rc     = kOkDspRC;

  if((rc = cmDspSysFinalize(hp )) != kOkDspRC )
    return rc;

  cmDsp_t*        p      = cmMemAllocZ( cmDsp_t, 1 );


  cmErrSetup(&p->err,&ctx->rpt,"DSP System");
  //p->ctx.ctx   = asCtx;
  p->cmCtx     = *ctx;
  p->netH      = netH;
  p->pgmIdx    = cmInvalidIdx;

  // create the DSP class  linked heap
  if(cmLHeapIsValid( p->lhH = cmLHeapCreate(1024,ctx)) == false)
    return cmErrMsg(&p->err,kLHeapFailDspRC,"Linked heap intiialization failed.");

  // initialize the DSP class JSON object
  if( cmJsonInitialize(&p->jsH,ctx) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"JSON initialization failed.");
    goto errLabel;
  }

  // intialize the DSP class symbol table
  if( cmSymTblIsValid( p->stH = cmSymTblCreate(cmSymTblNullHandle,1,ctx)) == false )
  {
    rc = cmErrMsg(&p->err,kSymTblFailDspRC,"Symbol table initialization failed.");
    goto errLabel;
  }

  // allocate the DSP system variable storage object
  if( cmDspStoreAlloc(ctx,&p->dsH,10,10) != kOkDspRC )
  {
    rc = cmErrMsg(&p->err,kDspStoreFailDspRC,"DSP store allocation failed.");
    goto errLabel;
  }

  // initialize the proc context
  if( (p->ctx.cmProcCtx = cmCtxAlloc(NULL,&ctx->rpt,p->lhH,p->stH)) == NULL )
  {
    rc = cmErrMsg(&p->err,kProcFailDspRC,"cmProc context initialization failed.");
    goto errLabel;
  }

  // allocate the the preset mgr
  _cmDspPresetAlloc(&p->pm);

  // initialize the networking compenents
  if((rc = _cmDspSysNetAlloc(p)) != kOkDspRC )
    goto errLabel;

  // set the DSP ctx to use the system lHeap and sym. tbl so that the 
  // DSP class instantions use them
  p->ctx.lhH     = p->lhH;
  p->ctx.jsH     = p->jsH;
  p->ctx.stH     = p->stH;
  p->ctx.dsH     = p->dsH;
  p->ctx.rsrcJsH = cmJsonNullHandle;
  p->ctx.rpt     = &ctx->rpt;
  p->ctx.cmCtx   = &p->cmCtx;
  p->ctx.dspH.h  = p;


  _cmDspClass_t*       pp            = NULL;
  cmDspClassConsFunc_t classConsFunc = NULL;

  // Get each DSP class construction function
  for(i=0; (classConsFunc = cmDspClassGetBuiltIn(i))!=NULL; ++i)
  {
    // allocate the class ctl recd
    _cmDspClass_t* cp =  cmLhAllocZ(p->ctx.lhH,_cmDspClass_t,1);

    // call the class constructor func
    cp->classPtr = classConsFunc(&p->ctx);

    // set the class symbol id
    cp->symId    = cmSymTblRegister(p->ctx.stH,cp->classPtr->labelStr,true);
   
    // link in the class ctl recd
    if( pp == NULL )
      p->classList = cp;
    else
      pp->linkPtr = cp;

    pp = cp;
  }

  hp->h = p;

  p->ctx.lhH     = cmLHeapNullHandle;
  p->ctx.jsH     = cmJsonNullHandle;
  p->ctx.stH     = cmSymTblNullHandle;
  
 errLabel:
  if( rc != kOkDspRC )
    _cmDspSysFinalize(p);

  return rc;
}

cmDspRC_t cmDspSysFinalize(  cmDspSysH_t* hp )
{
  cmDspRC_t rc = kOkDspRC;

  if( hp==NULL || cmDspSysIsValid(*hp)==false )
    return kOkDspRC;

  if((rc = cmDspSysUnload(*hp)) != kOkDspRC )
    return rc;

  cmDsp_t* p = _cmDspHandleToPtr(*hp);

  if((rc = _cmDspSysFinalize(p)) == kOkDspRC )
    hp->h = NULL;

  return rc;
}

unsigned  cmDspSysPgmCount( cmDspSysH_t h )
{
  _cmDspSysPgm_t* arr = _cmDspSysPgmArrayBase();
  unsigned        i   = 0;
  while( arr != NULL && arr[i].label != NULL )
    ++i;

  return i;
}

const cmChar_t* cmDspPgmLabel( cmDspSysH_t h, unsigned idx )
{
  if( idx < cmDspSysPgmCount(h) )
  {
    _cmDspSysPgm_t* arr = _cmDspSysPgmArrayBase();
    return arr[idx].label;
  }
  return NULL;
}


bool cmDspSysIsValid( cmDspSysH_t h )
{ return h.h != NULL; }

cmDspRC_t cmDspSysLastRC( cmDspSysH_t h )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return cmErrLastRC(&p->err);
}


// This function is assigns a unique symbol id (cmDspInst_t.symId)
// to all DSP instances whose symId is set to cmInvalidId.
cmDspRC_t _cmDspSysAssignUniqueInstSymId( cmDsp_t* p )
{
  typedef struct class_str
  {
    const cmChar_t* label;
    unsigned        cnt;
    struct class_str* link;
  } class_t;


  cmDspRC_t rc = kOkDspRC;
  class_t*  map = NULL;

  _cmDspInst_t* dip = p->instList;

  for(; dip!=NULL; dip=dip->linkPtr)
  {
    cmDspInst_t* ip = dip->instPtr;
    
    // if this instance does not have a valid symbol id then create one
    if( ip->symId == cmInvalidId )
    {
      // look for the instance class in map[]
      class_t* mp = map;
      for(; mp!=NULL; mp=mp->link)
        if( strcmp(ip->classPtr->labelStr,mp->label) == 0 )
          break;

      // if map[] does not yet have a recd for this class ....
      if( mp == NULL )
      {
        mp = cmMemAllocZ(class_t,1);  // ... then make one
        mp->label = ip->classPtr->labelStr;
        mp->link  = map;
        map = mp;
      }
     
      // generate a new unique symbol
      while( ip->symId==cmInvalidId && mp->cnt != cmInvalidId )
      {
        // increment the instance count for this class
        mp->cnt += 1;

        // form a symbol label
        cmChar_t* idStr = cmTsPrintfP(NULL,"%s-%i",ip->classPtr->labelStr,mp->cnt);

        // register the new symbol
        unsigned symId = cmSymTblRegisterSymbol(p->ctx.stH,idStr); 

        cmMemFree(idStr); // the symbol label is no longer need

        // if the symbol has not yet been used then it must be unique 
        if( _cmDspSysInstSymIdToPtr(p,symId) == NULL )
          ip->symId = symId;
        else
          cmSymTblRemove(p->ctx.stH,symId);  // ... otherwise remove the symbol and try again

      }

      // check for the very unlikely case that no unique symbol could be generated
      if(mp->cnt == cmInvalidId )
        rc = cmErrMsg(&p->err,kSymNotFoundDspRC,"All DSP instances were not assigned a unique symbol id.");

    }
  }

  // free the class list
  class_t* mp = map;
  while( mp != NULL )
  {
    class_t* np = mp->link;
    cmMemFree(mp);
    mp = np;
  }

  return rc;
}

cmDspRC_t cmDspSysLoad( cmDspSysH_t  h,  cmAudioSysCtx_t* asCtx, unsigned pgmIdx )
{
  cmDspRC_t       rc;
  cmDsp_t*        p           = _cmDspHandleToPtr(h);

  p->pgmIdx = cmInvalidIdx;

  if( pgmIdx >= cmDspSysPgmCount(h) )
    return cmErrMsg(&p->err,kInvalidPgmIdxDspRC,"%i is not a valid program index.",pgmIdx);

  // unload the previously loaded DSP program
  if((rc = cmDspSysUnload(h)) != kOkDspRC )
    return rc;
 
  // assign the cmAudioSysCtx_t context recd
  p->ctx.ctx = asCtx;


  _cmDspSysPgm_t* arr = _cmDspSysPgmArrayBase();


  // form the resource file name
  if((p->rsrcFn  = cmFsMakeFn(cmFsPrefsDir(),arr[pgmIdx].label,"js",NULL)) == NULL )
  {
    rc = cmErrMsg(&p->err,kFileSysFailDspRC,"Resource file name formation failed.");
    goto errLabel;
  }

  // if the pgm specific resource file already exists
  if( cmFsIsFile(p->rsrcFn) )
  {
    // open and parse the resource file
    if( cmJsonInitializeFromFile(&p->ctx.rsrcJsH,p->rsrcFn,&p->cmCtx) != kOkJsRC )
    {
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"Resource file open failed.");
      goto errLabel;    
    }

  }
  else // ... the resource file does not currently exists - create it
  {
    cmJsonNode_t* rootPtr;

    if( cmJsonInitialize(&p->ctx.rsrcJsH,&p->cmCtx) != kOkJsRC )
    {
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"Resource object allocate failed.");
      goto errLabel;
    }

    if((rootPtr = cmJsonCreateObject(p->ctx.rsrcJsH, NULL )) == NULL )
    {
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"Resource object root allocate failed.");
      goto errLabel;
    }

  }
  
  // create a linked heap for use by the DSP instances
  if(cmLHeapIsValid( p->ctx.lhH = cmLHeapCreate(1024,&p->cmCtx)) == false)
    return cmErrMsg(&p->err,kLHeapFailDspRC,"DSP instance linked heap intiialization failed.");

  // initialize a JSON object for use by the DSP instances
  if( cmJsonInitialize(&p->ctx.jsH,&p->cmCtx) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"JSON initialization failed.");
    goto errLabel;
  }   

  // create a symbol table for use by the DSP instance (note that it uses the system sym. tbl. as it's parent)
  if( cmSymTblIsValid( p->ctx.stH = cmSymTblCreate(p->stH, cmDspSys_PARENT_SYM_TBL_BASE_ID, &p->cmCtx)) == false )
  {
    rc = cmErrMsg(&p->err,kSymTblFailDspRC,"DSP instance symbol table initialization failed.");
    goto errLabel;
  }

  // load the preset mgr
  if((rc = _cmDspPresetLoad(&p->pm,&p->cmCtx,&p->err,p->ctx.lhH,p->ctx.stH,arr[pgmIdx].label)) != kOkDspRC )
  {
    rc = cmErrMsg(&p->err,rc,"Preset manager load failed.");
    goto errLabel;
  }

  // setup the DSP network components
  if((rc = _cmDspSysNetPreLoad(p)) != kOkDspRC )
    goto errLabel;

  p->ctx.cycleCnt      = 0;
  p->ctx._disableSymId = cmSymTblRegisterStaticSymbol(p->ctx.stH,"_disable");
  p->ctx._enableSymId  = cmSymTblRegisterStaticSymbol(p->ctx.stH,"_enable");

  void**          vpp = &arr[ pgmIdx ].userPtr;
  cmDspPgmFunc_t  pgm =  arr[ pgmIdx ].loadFunc;
  

  // allocate and connect user defined signal processing network
  if((rc = pgm(h,vpp)) == kOkDspRC )
    if((rc = cmErrLastRC(&p->err)) != kOkDspRC )
    {
      cmErrMsg(&p->err,rc,"User DSP system load failed.");
      goto errLabel;
    }

  p->pgmIdx = pgmIdx;

  // enter sync mode
  if((rc = _cmDspSysNetSync(p)) != kOkDspRC )
  {
    cmErrMsg(&p->err,rc,"DSP system sync failed.");
    goto errLabel;
  }

  rc = _cmDspSysAssignUniqueInstSymId(p);

 errLabel:
  if( rc != kOkDspRC )
    cmDspSysUnload(h);

  return rc;
}

cmDspRC_t cmDspSysUnload( cmDspSysH_t h )
{
  if( cmDspSysIsValid(h) == false )
    return kOkDspRC;

  cmDspRC_t     rc = kOkDspRC;
  cmDsp_t*       p = _cmDspHandleToPtr(h);
  _cmDspInst_t* ip = p->instList;


  // call the DSP network unload function - this function is provided
  // by the client network to release any resources obtained when 
  // the network was created
  if( p->pgmIdx != cmInvalidIdx )
  {
    _cmDspSysPgm_t* arr = _cmDspSysPgmArrayBase();
    void**          vpp = &arr[ p->pgmIdx ].userPtr;
    cmDspPgmFunc_t  pgm =  arr[ p->pgmIdx ].unloadFunc;
  
    if( pgm != NULL )
      if((rc = pgm(h,vpp)) == kOkDspRC )
        if((rc = cmErrLastRC(&p->err)) != kOkDspRC )
          cmErrMsg(&p->err,rc,"User DSP system unload failed.");
  }


  p->pgmIdx    = cmInvalidIdx;

  // unload the networking components
  _cmDspSysNetUnload(p);

  // free the DSP instances
  for(; ip!=NULL; ip=ip->linkPtr)
    if((rc = _cmDspInstFree(&p->ctx,ip->instPtr)) != kOkDspRC )
      rc = cmErrMsg(&p->err,kInstFinalFailDspRC,"An attempt to free DSP instance '%s' id:%i failed.",ip->instPtr->classPtr->labelStr,ip->instPtr->id);

  p->instList = NULL;

  // unload the preset manager
  if((rc = _cmDspPresetUnload(&p->pm,&p->cmCtx)) != kOkDspRC)
    rc = cmErrMsg(&p->err,rc,"Preset manager 'unload' failed.");

  // finalize the pgm specific JSON tree
  if( cmJsonIsValid(p->ctx.rsrcJsH))
  {
    // if the JSON tree has been modified.
    if( cmJsonIsModified(p->ctx.rsrcJsH) )
      if( cmJsonWrite(p->ctx.rsrcJsH, cmJsonRoot(p->ctx.rsrcJsH), p->rsrcFn ) != kOkJsRC )
        rc = cmErrMsg(&p->err,kJsonFailDspRC,"JSON resource file write failed on '%s'.",cmStringNullGuard(p->rsrcFn));
    
    if( cmJsonFinalize(&p->ctx.rsrcJsH) != kOkJsRC )
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"Resource JSON finalization failed.");
  }
  
  // release the JSON file name
  if( p->rsrcFn != NULL )
  {
    cmFsFreeFn(p->rsrcFn); // free the resource file name
    p->rsrcFn = NULL;
  }

  // tell the DSP net system that the pgm has been unloaded
  _cmDspSysNetUnload(p);

  // elimnate the linked heap, system table, and json object used by the DSP instance objects 
  if( cmLHeapIsValid(p->ctx.lhH) )
    cmLHeapDestroy(&p->ctx.lhH);

  if( cmJsonIsValid(p->ctx.jsH) )
    if( cmJsonFinalize(&p->ctx.jsH) != kOkJsRC )
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"DSP instance JSON object finalization failed.");

  if( cmSymTblIsValid(p->ctx.stH) )
    cmSymTblDestroy(&p->ctx.stH);

  return rc;
  
}

cmDspRC_t cmDspSysReset( cmDspSysH_t h )
{
  cmDsp_t*       p = _cmDspHandleToPtr(h);
  cmDspRC_t     rc = kOkDspRC;


  // call reset on each of the instances
  _cmDspInst_t* ip = p->instList;
  for(; ip != NULL; ip = ip->linkPtr )
    if( ip->instPtr->resetFunc != NULL )
    {
      if( ip->instPtr->resetFunc(&p->ctx,ip->instPtr,NULL) != kOkDspRC )
        rc = cmErrMsg(&p->err,kInstResetFailDspRC,"Reset failed on DSP instance '%s' id:%i.",ip->instPtr->classPtr->labelStr,ip->instPtr->id);

      ip->instPtr->flags = 0; 
    }

  // send the _reset symbol to every instance which has the _reset attribute symbol
  if( rc == kOkDspRC )
  {
    cmDspValue_t v;
    unsigned     resetSymId = cmDspSysRegisterStaticSymbol(h,"_reset");
    cmDsvSetSymbol(&v,resetSymId);
    rc =   cmDspSysBroadcastValue(h, resetSymId, &v);
  }
  return rc;
}


// Handle msg's arriving from the UI
cmDspRC_t _cmDspSysHandleUiMsg(cmDsp_t* p, _cmDspInst_t* ip, const void* msgPtr, unsigned msgByteCnt)
{
  cmDspRC_t     rc = kOkDspRC;
  cmDspUiHdr_t* h  = (cmDspUiHdr_t*)msgPtr;
  cmDspEvt_t    e;

  assert( h->uiId  == kUiSelAsId );

  // the only source of msg's (at the time of this writing) is the UI so:
  // Mark the msg as a UI event and set the kUiEchoDspFl if the UI has requested duplex operation.
  e.flags      = kUiDspFl | (cmIsFlag(h->flags,kDuplexDuiFl) ? kUiEchoDspFl : 0);
  e.srcInstPtr = NULL;          // the UI has no source instance
  e.srcVarId   = h->selId;      // set the event srcVarId to the UI selId
  e.dstVarId   = h->instVarId;  // identify the dst inst variable
  e.dstDataPtr = NULL;          // UI msg's don't have custom data (yet)
  e.valuePtr   = &h->value;     // 

  // if the value associated with this msg is a mtx then set
  // its mtx data area pointer to just after the msg header.
  if( cmDsvIsMtx(&h->value) )
    h->value.u.m.u.vp = ((char*)msgPtr) + sizeof(cmDspUiHdr_t);
  else
    rc = cmDsvDeserializeInPlace(&h->value,msgByteCnt-sizeof(cmDspUiHdr_t));

  if( rc != kOkDsvRC )
    cmErrMsg(&p->err,kSerializeUiMsgFailDspRC,"DSP system receive Deserialize failed.");


  // find the dest. DSP instance for this msg
  for(; ip != NULL; ip = ip->linkPtr )
    if( ip->instPtr->id == h->instId )
    {
      // send the msg to the DSP dst. instance
      if( ip->instPtr->recvFunc != NULL )
        if( ip->instPtr->recvFunc(&p->ctx,ip->instPtr,&e) != kOkDspRC )
          rc = cmErrMsg(&p->err,kInstMsgRcvFailDspRC,"Message recv failed on DSP instance '%s' id:%i.",ip->instPtr->classPtr->labelStr,ip->instPtr->id);
          
      break;
    }

  if( ip == NULL )
    rc = cmErrMsg(&p->err,kInstNotFoundDspRC,"No DSP instance with id %i was found during message delivery.",h->instId);

  return rc;
}


// Recv msg's arriving from the audio engine.
cmDspRC_t cmDspSysRcvMsg(    cmDspSysH_t h, cmAudioSysCtx_t* asCtx, const void* msgPtr, unsigned msgByteCnt, unsigned srcNetNodeId )
{
  cmDsp_t*      p  = _cmDspHandleToPtr(h);
  _cmDspInst_t* ip = p->instList;


  if( msgByteCnt == 0 && asCtx->audioRateFl==true)
  {
    cmTimeSpec_t t0,t1;
    cmTimeGet(&t0);

    for(; ip != NULL; ip = ip->linkPtr )
      if( ip->instPtr->execFunc != NULL && cmIsFlag(ip->instPtr->flags,kDisableExecInstFl)==false )
      {
        if( ip->instPtr->execFunc(&p->ctx,ip->instPtr,NULL) != kOkDspRC )
          cmErrMsg(&p->err,kInstExecFailDspRC,"Execution failed on DSP instance '%s' id:%i.",ip->instPtr->classPtr->labelStr,ip->instPtr->id);

        //printf("%i %s\n",p->ctx.cycleCnt,ip->instPtr->classPtr->labelStr);

      }

    cmTimeGet(&t1);
    p->ctx.execDurUsecs = cmTimeElapsedMicros(&t0,&t1);

    ++p->ctx.cycleCnt;
  }
  else
  {
    unsigned* hdr = (unsigned*)msgPtr;

    // the msg selector is the second field in the data packet (following the audio system sub-system id)
    //const unsigned msgTypeSelId = ((const unsigned*)msgPtr)[1];
    

    switch( hdr[1] )
    {
      case kUiSelAsId:
        _cmDspSysHandleUiMsg(p,ip,msgPtr,msgByteCnt);
        break;

      case kNetSyncSelAsId:
        _cmDspSysNetRecv(p, (const cmDspNetMsg_t*)msgPtr, msgByteCnt, srcNetNodeId );
        break;

      case kMidiMsgArraySelAsId:
        {
          cmMidiPacket_t pkt;
          cmDspValue_t   v;

          pkt.cbDataPtr = NULL;
          pkt.devIdx    = hdr[2];
          pkt.portIdx   = hdr[3];
          pkt.msgCnt    = hdr[4];
          pkt.msgArray  = (cmMidiMsg*)(hdr + 5);
          unsigned     midiSymId = cmDspSysRegisterStaticSymbol(h,"_midi");

          v.u.m.u.vp = &pkt;
          cmDspSysBroadcastValue(h, midiSymId, &v);

          
        /*
        // data format for MIDI messages
        { kMidiMsgArraytSelAsId, devIdx, portIdx, msgCnt, msgArray[msgCnt] }
        where each msgArray[] record is a cmMidiMsg record.
        */
        }
        break;

    }
  }

  // report any error - this should stop further calls to the DSP process
  return cmErrLastRC(&p->err);
}

unsigned cmDspSysSyncState( cmDspSysH_t h )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return p->syncState;
}

cmDspRC_t cmDspSysPrintPgm( cmDspSysH_t h, const cmChar_t* outFn )
{
  cmDsp_t*  p = _cmDspHandleToPtr(h);
  cmDspRC_t rc = kOkDspRC;
  cmJsonH_t jsH = cmJsonNullHandle;

  if( cmJsonInitialize(&jsH, &p->cmCtx ) != kOkJsRC )
    return cmErrMsg(&p->err,kJsonFailDspRC,"JSON output object create failed.");


  // create the root object
  cmJsonNode_t* onp = cmJsonCreateObject(jsH,NULL);
  assert( onp != NULL );

  // create the instance array
  cmJsonNode_t* iap = cmJsonInsertPairArray(jsH, cmJsonRoot(jsH), "inst_array" );
  assert( iap != NULL );
  
  // create the connection array
  cmJsonNode_t* cap = cmJsonInsertPairArray(jsH, cmJsonRoot(jsH), "conn_array" );
  assert( cap != NULL );

  _cmDspInst_t* dip = p->instList;

  for(; dip!=NULL; dip=dip->linkPtr)
  {
    cmDspInst_t* ip = dip->instPtr;

    onp   = cmJsonCreateObject(jsH,iap);
    
    if( cmJsonInsertPairs(jsH, onp, 
        "class", kStringTId, ip->classPtr->labelStr,
        "label", kStringTId, cmSymTblLabel(p->ctx.stH,ip->symId),
        "id",    kIntTId,    ip->symId,
        NULL ) != kOkJsRC )
    {
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"JSON DSP instance create failed.");
      goto errLabel;
    }

    unsigned i;
    for(i=0; i<ip->varCnt; ++i)
    {
      const cmDspVar_t* v = ip->varArray + i;

      cmDspCb_t* cp = v->cbList;

      for(; cp!=NULL; cp=cp->linkPtr)
      {
        const cmDspVar_t* dvar = cmDspVarIdToCPtr(cp->dstInstPtr, cp->dstVarId );

        onp   = cmJsonCreateObject(jsH,cap);

        assert(dvar != NULL);

        if(cmJsonInsertPairs(jsH,onp,
            "sid",  kStringTId, cmSymTblLabel(p->ctx.stH,ip->symId),
            "svar", kStringTId, cmSymTblLabel(p->ctx.stH,v->symId),
            "did",  kStringTId, cmSymTblLabel(p->ctx.stH,cp->dstInstPtr->symId),
            "dvar", kStringTId, cmSymTblLabel(p->ctx.stH,dvar->symId),
            NULL ) != kOkJsRC )
        {
          rc = cmErrMsg(&p->err,kJsonFailDspRC,"JSON DSP connect create failed.");
          goto errLabel;
        }
      }
    }
  }

  if( cmJsonWrite(jsH,NULL,outFn) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"JSON file write failed on '%s.",cmStringNullGuard(outFn));
    goto errLabel;
  }

  if( cmJsonFinalize(&jsH) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"JSON tree release failed.");

 errLabel:

  return rc;
}


unsigned        cmDspSysPresetGroupCount( cmDspSysH_t h )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return _cmDspPresetGroupCount(&p->pm); 
}

unsigned        cmDspSysPresetGroupSymId( cmDspSysH_t h, unsigned groupIdx )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return _cmDspPresetGroupSymId(&p->pm,groupIdx); 
}

const cmChar_t* cmDspSysPresetGroupLabel( cmDspSysH_t h, unsigned groupIdx )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return _cmDspPresetGroupLabel(&p->pm,groupIdx); 
}

cmDspRC_t       cmDspSysPresetGroupJsonList(    cmDspSysH_t h, cmJsonH_t* jsHPtr )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return _cmDspPresetGroupJsonList(&p->pm,jsHPtr);
}

unsigned        cmDspSysPresetPresetCount( cmDspSysH_t h, unsigned groupIdx )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return _cmDspPresetPresetCount(&p->pm,groupIdx); 
}

unsigned        cmDspSysPresetPresetSymId( cmDspSysH_t h, unsigned groupIdx, unsigned presetIdx )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return _cmDspPresetPresetSymId(&p->pm,groupIdx,presetIdx); 
}

const cmChar_t* cmDspSysPresetPresetLabel( cmDspSysH_t h, unsigned groupIdx, unsigned presetIdx )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return _cmDspPresetPresetLabel(&p->pm,groupIdx,presetIdx); 
}

cmDspRC_t       cmDspSysPresetPresetJsonList(    cmDspSysH_t h, unsigned groupSymId, cmJsonH_t* jsHPtr )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return _cmDspPresetPresetJsonList(&p->pm,jsHPtr,groupSymId);
}

unsigned       cmDspSysPresetRegisterGroup( cmDspSysH_t h, const cmChar_t* groupLabel )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  assert( groupLabel != NULL );
  unsigned grpSymId;

  if((grpSymId = cmSymTblRegisterSymbol(p->ctx.stH,groupLabel)) == cmInvalidIdx )
    cmErrMsg(&p->err,kSymTblFailDspRC,"Registration of the group label:'%s' failed.",cmStringNullGuard(groupLabel));

  return grpSymId;
}



cmDspRC_t cmDspSysPresetCreate( cmDspSysH_t h, const cmChar_t* groupLabel, const cmChar_t* presetLabel )
{
  cmDspRC_t rc;
  cmDsp_t* p = _cmDspHandleToPtr(h);

  // create a new preset and make it active
  if((rc = _cmDspPresetCreatePreset(&p->pm, groupLabel, presetLabel )) != kOkDspRC )
    return rc;

  assert( p->pm.gp != NULL );
      
  // call store on each of the instances
  _cmDspInst_t* ip = p->instList;
  for(; ip != NULL; ip = ip->linkPtr )
    if( ip->instPtr->symId != cmInvalidId && ip->instPtr->presetGroupSymId == p->pm.gp->symId && ip->instPtr->storeFunc != NULL )
    {

      // create an instance preset and make it active
      if((rc = _cmDspPresetCreateInstance(&p->pm, ip->instPtr->symId )) != kOkDspRC )
        return rc;

      // call the store function on each instance
      if( ip->instPtr->storeFunc(&p->ctx,ip->instPtr,true) != kOkDspRC )
        rc = cmErrMsg(&p->err,kInstStoreFailDspRC,"Save failed on DSP instance '%s' id:%i.",ip->instPtr->classPtr->labelStr,ip->instPtr->id);
    }
  
  return rc;
}

cmDspRC_t cmDspSysPresetRecall( cmDspSysH_t h, const cmChar_t* groupLabel, const cmChar_t* presetLabel )
{
  cmDspRC_t rc;
  cmDsp_t* p = _cmDspHandleToPtr(h);

  // make the given group/preset active
  if((rc = _cmDspPresetRecallPreset(&p->pm, groupLabel, presetLabel )) != kOkDspRC )
    return rc;

  assert( p->pm.gp != NULL );

  // call store on each of the instances
  _cmDspInst_t* ip = p->instList;
  for(; ip != NULL; ip = ip->linkPtr )
    if( ip->instPtr->symId != cmInvalidId && ip->instPtr->storeFunc != NULL && ip->instPtr->presetGroupSymId == p->pm.gp->symId )
    {

      // make the instance active
      if((rc = _cmDspPresetRecallInstance(&p->pm, ip->instPtr->symId )) != kOkDspRC )
      {
        if( rc == kPresetInstNotFoundDspRC ) 
        {
          cmErrWarnMsg(&p->err,kOkDspRC,"Assuming a new instance was added to the preset - continuing with preset load.");
          continue;
        }
      }

      // call the store function on each instance
      if( ip->instPtr->storeFunc(&p->ctx,ip->instPtr,false) != kOkDspRC )
        rc = cmErrMsg(&p->err,kInstStoreFailDspRC,"Restore failed on DSP instance '%s' id:%i.",ip->instPtr->classPtr->labelStr,ip->instPtr->id);
    }
  
  return rc;
}

cmDspRC_t cmDspSysPresetWriteValue( cmDspSysH_t h, unsigned varSymId, const cmDspValue_t* valPtr )
{  
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return _cmDspPresetCreateVar(&p->pm,varSymId,valPtr); 
}

cmDspRC_t cmDspSysPresetReadValue(  cmDspSysH_t h, unsigned varSymId, cmDspValue_t* valPtr )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return _cmDspPresetRecallVar(&p->pm,varSymId,valPtr); 
}


// allocate an instance given the class symbol id
cmDspInst_t* _cmDspSysAllocateInst( cmDsp_t* p, unsigned classSymId, unsigned storeSymId, unsigned instSymId, unsigned va_cnt, va_list vl )
{
  _cmDspClass_t* cp      = p->classList;
  cmDspInst_t*   instPtr = NULL;

  // locate the DSP class
  for(; cp != NULL; cp=cp->linkPtr )
    if( cp->symId == classSymId )
      break;

  // if the DSP class was not found
  if( cp == NULL )
  {
    cmErrMsg(&p->err,kClassNotFoundDspRC,"The DSP class '%s' could not be found.",cmSymTblLabel(p->ctx.stH,classSymId));
    return NULL;
  }
  
  // allocate the instance
  if((instPtr = cp->classPtr->allocFunc( &p->ctx, cp->classPtr, storeSymId, instSymId, p->nextInstId, va_cnt, vl )) == NULL )
  {
    cmErrMsg(&p->err,kAllocInstFailDspRC,"DSP instance allocation failed for class '%s'.",cp->classPtr->labelStr);
    return NULL;
  }

  // assign the symbol attributes
  cmDspInstSymId_t* ip = p->symIdList;
  for(; ip!=NULL; ip=ip->link)
    if( ip->symId != cmInvalidId )
      if( cmDspInstRegisterAttrSym(&p->ctx, instPtr, ip->symId ) != kOkDspRC )
      {
        cmErrMsg(&p->err,kAllocInstFailDspRC,"The DSP instance failed due to attribute symbol registration failure.");
        return NULL;
      }
    
  // allocate the instance ctl recd
  _cmDspInst_t* instCtlPtr = cmLhAllocZ( p->ctx.lhH, _cmDspInst_t, 1 );

  assert( instCtlPtr != NULL );

  // setup the inst ctl recd
  instCtlPtr->instPtr = instPtr;
  instCtlPtr->linkPtr = NULL;

  // link the inst ctl recd onto the end of the instance list
  if( p->instList == NULL )
    p->instList = instCtlPtr;
  else
  {
    _cmDspInst_t* ip = p->instList;
      
    while( ip->linkPtr != NULL  )
      ip = ip->linkPtr;

    ip->linkPtr = instCtlPtr;
  }

  ++p->nextInstId;

  return instPtr;
}


// allocate a an instance from the class label - this function calls _cmDspSysAllocateInst()
cmDspInst_t* cmDspSysAllocInstSV( cmDspSysH_t h, const cmChar_t* classLabelStr, unsigned storeSymId, const cmChar_t* instLabelStr, unsigned va_cnt, va_list vl)
{ 
  cmDsp_t*     p          = _cmDspHandleToPtr(h);
  unsigned     instSymId  = cmInvalidId;
  cmDspInst_t* newInstPtr;
  unsigned     classSymId;

  // get the class symbold
  if((classSymId = cmSymTblId(p->ctx.stH,classLabelStr)) == cmInvalidId )
  {
    cmErrMsg(&p->err,kSymNotFoundDspRC,"The symbol id associated with the class '%s' could not be found.",classLabelStr);
    return NULL;
  }

  if( instLabelStr != NULL )
  {
    // get the symbold id assoc'd with the instance label
    if((instSymId = cmSymTblRegisterSymbol(p->ctx.stH,instLabelStr)) == cmInvalidId )
    {
      cmErrMsg(&p->err,kSymTblFailDspRC,"The DSP instance symbol '%s' failed to register with the symbol table.",instLabelStr);
      return NULL;
    }

    // do not allow instance labels to be re-used
    if( _cmDspSysInstSymIdToPtr(p,instSymId) != NULL )
    {
      cmErrMsg(&p->err,kDuplInstSymIdDspRC,"The DSP instance label '%s' was reused (class:'%s').",cmStringNullGuard(instLabelStr),cmStringNullGuard(classLabelStr));
      return NULL;
    }
  }

  // allocate the instance
  newInstPtr = _cmDspSysAllocateInst(p, classSymId, storeSymId, instSymId, va_cnt, vl );
  return newInstPtr;
}


cmDspInst_t* cmDspSysAllocInstS( cmDspSysH_t h, const cmChar_t* classLabelStr, unsigned storeSymId, const cmChar_t* instLabelStr, unsigned va_cnt, ... )
{
  cmDspInst_t* p;
  va_list vl;
  va_start(vl,va_cnt);
  p =  cmDspSysAllocInstSV(h, classLabelStr, storeSymId, instLabelStr, va_cnt, vl);
  va_end(vl);
  return p;
}

cmDspInst_t* cmDspSysAllocInst( cmDspSysH_t h, const cmChar_t* classLabelStr, const cmChar_t* instLabelStr, unsigned va_cnt, ... )
{
  cmDspInst_t* p;
  va_list vl;
  va_start(vl,va_cnt);
  p =  cmDspSysAllocInstSV(h, classLabelStr, cmInvalidId, instLabelStr, va_cnt, vl);
  va_end(vl);
  return p;
}

cmDspInst_t** cmDspSysAllocInstArraySV( cmDspSysH_t h, unsigned cnt, unsigned presetGroupSymId, const cmChar_t* classLabelStr, const cmChar_t* instLabelStr, cmDspSysLabelFunc_t labelFunc, unsigned va_cnt, va_list vl )
{
  cmDsp_t*      p = _cmDspHandleToPtr(h);
  cmDspInst_t** a = cmLhAllocZ( p->lhH, cmDspInst_t*, cnt );
  unsigned      i;

  for(i=0; i<cnt; ++i)
  {
    const cmChar_t* label = instLabelStr;
    va_list         vl1;

    va_copy(vl1,vl);
    
    if( labelFunc != NULL )
      label = labelFunc(h,i);
    else
      if( instLabelStr != NULL )
        label = cmDspSysPrintLabel(instLabelStr,i);

    a[i] = cmDspSysAllocInstSV(h,classLabelStr,presetGroupSymId,label,va_cnt,vl1);
  }

  return a;
}

cmDspInst_t** cmDspSysAllocInstArrayS(  cmDspSysH_t h, unsigned cnt, unsigned presetGroupSymId, const cmChar_t* classLabelStr, const cmChar_t* instLabelStr, cmDspSysLabelFunc_t labelFunc, unsigned va_cnt, ... )
{
  va_list vl;
  va_start(vl,va_cnt);
  cmDspInst_t** a = cmDspSysAllocInstArraySV(h,cnt,presetGroupSymId,classLabelStr,instLabelStr,labelFunc,va_cnt,vl);
  va_end(vl);
  return a;
}

cmDspInst_t** cmDspSysAllocInstArray(   cmDspSysH_t h, unsigned cnt, const cmChar_t* classLabelStr, const cmChar_t* instLabelStr, cmDspSysLabelFunc_t labelFunc, unsigned va_cnt, ... )
{
  va_list vl;
  va_start(vl,va_cnt);
  cmDspInst_t** a = cmDspSysAllocInstArraySV(h,cnt,cmInvalidId,classLabelStr,instLabelStr,labelFunc,va_cnt,vl);
  va_end(vl);
  return a;
}

cmDspRC_t cmDspSysNewColumn( cmDspSysH_t h, unsigned colW )
{ return cmDspUiNewColumn(&_cmDspHandleToPtr(h)->ctx,colW); }

cmDspRC_t cmDspSysInsertHorzBorder( cmDspSysH_t h )
{ return cmDspUiInsertHorzBorder(&_cmDspHandleToPtr(h)->ctx); }

cmDspRC_t    cmDspSysNewPage( cmDspSysH_t h, const cmChar_t* title )
{ return cmDspUiNewPage(&_cmDspHandleToPtr(h)->ctx,title); }


cmDspRC_t _cmDspSysConnAudio( cmDsp_t* p, cmDspInst_t* srcInstPtr, unsigned srcVarSymId, cmDspInst_t* dstInstPtr, unsigned dstVarSymId )
{
  cmDspVar_t* srcVarPtr;
  cmDspVar_t* dstVarPtr;
  //cmSample_t* srcDataPtr;

  if( srcInstPtr == NULL )
    return cmErrMsg(&p->err,kInstNotFoundDspRC,"The audio connection process was passed a NULL source processor instance pointer for connection to '%s'.",cmStringNullGuard(dstInstPtr->classPtr->labelStr));

  if( dstInstPtr == NULL )
    return cmErrMsg(&p->err,kInstNotFoundDspRC,"The audio connection process was passed a NULL destination processor instance pointer for connection from '%s'.",cmStringNullGuard(srcInstPtr->classPtr->labelStr));

  // locate the src audio variable
  if((srcVarPtr = cmDspVarSymbolToPtr( &p->ctx, srcInstPtr, srcVarSymId, kOutDsvFl )) == NULL )
    return cmErrMsg(&p->err,kVarNotFoundDspRC,"Audio connection failed. The source variable '%s' could not be found in DSP instance '%s' id=%i.", cmStringNullGuard(cmSymTblLabel(p->ctx.stH,srcVarSymId)), cmStringNullGuard(srcInstPtr->classPtr->labelStr),srcInstPtr->id);
    
  // validate the src variable type
  if( cmIsFlag(srcVarPtr->flags,kAudioBufDsvFl) == false )
    return cmErrMsg(&p->err,kVarTypeErrDspRC,"Audio connection failed. The source variable '%s' in '%s' id=%i was not an audio variable.", cmStringNullGuard(cmSymTblLabel(p->ctx.stH,srcVarSymId)), cmStringNullGuard(srcInstPtr->classPtr->labelStr),srcInstPtr->id);

  // locate the dst audio variable
  if((dstVarPtr = cmDspVarSymbolToPtr( &p->ctx, dstInstPtr, dstVarSymId, kInDsvFl )) == NULL )
    return cmErrMsg(&p->err,kVarNotFoundDspRC,"Audio connection failed. The destination variable '%s' could not be found in DSP instance '%s' id=%i.", cmStringNullGuard(cmSymTblLabel(p->ctx.stH,dstVarSymId)), cmStringNullGuard(dstInstPtr->classPtr->labelStr),dstInstPtr->id);

  // validate the dst variable type
  if( cmIsFlag(dstVarPtr->flags,kAudioBufDsvFl) == false )
    return cmErrMsg(&p->err,kVarTypeErrDspRC,"Audio connection failed. The destination variable '%s' in '%s' id=%i was not an audio variable.", cmStringNullGuard(cmSymTblLabel(p->ctx.stH,dstVarSymId)), cmStringNullGuard(dstInstPtr->classPtr->labelStr),dstInstPtr->id);

  // get a pointer to the source sample buffer
  //if( (srcDataPtr = cmDsvSampleMtx( &srcVarPtr->value )) == NULL )
  //  return cmErrMsg(&p->err,kVarNotValidDspRC,"Audio connection failed. The audio source variable '%s' in '%s' id=%i has not been allocated.",cmSymTblLabel(p->ctx.stH,srcVarSymId), srcInstPtr->classPtr->labelStr,srcInstPtr->id);

  // set the destination sample buffer to point to the source sample buffer.
  //cmDsvSetSampleMtx(  &dstVarPtr->value, srcDataPtr, cmDsvRows(&srcVarPtr->value), cmDsvCols(&srcVarPtr->value));

  cmDsvSetProxy( &dstVarPtr->value, &srcVarPtr->value );

  return kOkDspRC;
}

cmDspRC_t cmDspSysConnectAudio( cmDspSysH_t h, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarLbl, cmDspInst_t* dstInstPtr, const cmChar_t* dstVarLbl )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  unsigned srcSymId, dstSymId;

  if( srcInstPtr == NULL && dstInstPtr == NULL )
    return cmErrMsg(&p->err,kInstNotFoundDspRC,"The audio connection process was passed a NULL source and destination processor instance.");

  if( srcInstPtr == NULL )
    return cmErrMsg(&p->err,kInstNotFoundDspRC,"The audio connection process was passed a NULL source processor instance pointer for connection to '%s'.",cmStringNullGuard(dstInstPtr->classPtr->labelStr));

  if( dstInstPtr == NULL )
    return cmErrMsg(&p->err,kInstNotFoundDspRC,"The audio connection process was passed a NULL destination processor instance pointer for connection from '%s'.",cmStringNullGuard(srcInstPtr->classPtr->labelStr));

  if((srcSymId = cmSymTblId(p->ctx.stH,srcVarLbl)) == cmInvalidId )
    return cmErrMsg(&p->err,kSymNotFoundDspRC,"Audio connection failed. The source variable symbol '%s' for source DSP instance '%s' id=%i could not be found.",cmStringNullGuard(srcVarLbl),cmStringNullGuard(srcInstPtr->classPtr->labelStr),srcInstPtr->id);

  if((dstSymId = cmSymTblId(p->ctx.stH,dstVarLbl)) == cmInvalidId )
    return cmErrMsg(&p->err,kSymNotFoundDspRC,"Audio connection failed. The destination variable symbol '%s' for source DSP instance '%s' id=%i could not be found.",cmStringNullGuard(dstVarLbl),cmStringNullGuard(dstInstPtr->classPtr->labelStr),dstInstPtr->id);

  return _cmDspSysConnAudio(p,srcInstPtr,srcSymId,dstInstPtr,dstSymId);
}

cmDspRC_t cmDspSysConnectAudioN11N( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixStr,   unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    const cmChar_t* dstVarStr = dstVarPrefixStr;
    if( n >= 2 )
      dstVarStr = cmDspSysPrintLabel(dstVarPrefixStr,i);

    if((rc = cmDspSysConnectAudio( h, srcInstArray[i], srcVarLabel, dstInstPtr, dstVarStr )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysConnectAudio1NN1( cmDspSysH_t h, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarPrefixStr, cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel, unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    const cmChar_t* label = srcVarPrefixStr;

    if( n >= 2 )
      label = cmDspSysPrintLabel(srcVarPrefixStr,i);

    if((rc = cmDspSysConnectAudio( h, srcInstPtr, label, dstInstArray[i], dstVarLabel )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysConnectAudio1N1N( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarPrefixStr, cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixStr, unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned  i;

  for(i=0; i<n; ++i)
  {
    const cmChar_t* label0 = cmDspSysPrintLabel(srcVarPrefixStr,i);
    const cmChar_t* label1 = cmDspSysPrintLabel2(dstVarPrefixStr,i);
    if((rc = cmDspSysConnectAudio( h, srcInstPtr, label0, dstInstPtr, label1 )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysConnectAudioN1N1( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel, cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel,       unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned  i;

  for(i=0; i<n; ++i)
  {
    if((rc = cmDspSysConnectAudio( h, srcInstArray[i], srcVarLabel, dstInstArray[i],  dstVarLabel )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysConnectAudio11N1( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstArray[],const cmChar_t* dstVarLabel,   unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    if((rc = cmDspSysConnectAudio( h, srcInstPtr, srcVarLabel, dstInstArray[i], dstVarLabel )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysConnectAudio111N( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstPtr, const cmChar_t* dstVarLabel,   unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    const cmChar_t* label1 = cmDspSysPrintLabel(dstVarLabel,i);
    const cmChar_t* label  = n>1 ? label1 : dstVarLabel;

    if((rc = cmDspSysConnectAudio( h, srcInstPtr, srcVarLabel, dstInstPtr, label )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysConnectAudioN11NM( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel, unsigned srcCnt,    cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixStr,   unsigned dstCnt, const unsigned* map )
{
  cmDsp_t*  p  = _cmDspHandleToPtr(h);
  cmDspRC_t rc = kOkDspRC;
  unsigned  i;

  for(i=0; i<dstCnt; ++i)
  {
    const cmChar_t* dstVarStr = dstVarPrefixStr;
    if( dstCnt >= 2 )
      dstVarStr = cmDspSysPrintLabel(dstVarPrefixStr,i);
    
    if( map[i] >= srcCnt )
      return cmErrMsg(&p->err,kInstNotFoundDspRC,"The source instance map connection index %i is of range %i.",i,srcCnt);

    if((rc = cmDspSysConnectAudio( h, srcInstArray[ map[i] ], srcVarLabel, dstInstPtr, dstVarStr )) != kOkDspRC )
      break;
  }
  return rc;
}


cmDspRC_t   cmDspSysInstallCb(     cmDspSysH_t h, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarLabel, cmDspInst_t* dstInstPtr, const cmChar_t* dstVarLabel, void* dstCbDataPtr)
{
    cmDsp_t* p = _cmDspHandleToPtr(h);
    return cmDspInstallCb(&p->ctx,srcInstPtr,srcVarLabel,dstInstPtr,dstVarLabel,dstCbDataPtr);
}

cmDspRC_t cmDspSysInstallCbN11N( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixStr,   unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    const cmChar_t* dstVarStr = dstVarPrefixStr;
    if( n >= 2 )
      dstVarStr = cmDspSysPrintLabel(dstVarPrefixStr,i);

    if((rc = cmDspSysInstallCb( h, srcInstArray[i], srcVarLabel, dstInstPtr, dstVarStr, NULL )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysInstallCb1NN1( cmDspSysH_t h, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarPrefixStr, cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel, unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    const cmChar_t* srcVarStr = srcVarPrefixStr;
    if( n >= 2 )
      srcVarStr = cmDspSysPrintLabel(srcVarPrefixStr,i);

    if((rc = cmDspSysInstallCb( h, srcInstPtr, srcVarStr, dstInstArray[i], dstVarLabel, NULL )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysInstallCb1N1N( cmDspSysH_t h, cmDspInst_t* srcInstPtr,  const cmChar_t* srcVarPrefixStr, cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixStr, unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned  i;

  for(i=0; i<n; ++i)
  {
    const cmChar_t* label0 = srcVarPrefixStr;
    const cmChar_t* label1 = dstVarPrefixStr;

    if( n >= 2 )
    {
      label0 = cmDspSysPrintLabel(srcVarPrefixStr,i);
      label1 = cmDspSysPrintLabel2(dstVarPrefixStr,i);
    }

    if((rc = cmDspSysInstallCb( h, srcInstPtr, label0, dstInstPtr, label1, NULL )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysInstallCbN1N1( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel, cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel,       unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned  i;

  for(i=0; i<n; ++i)
  {
    if((rc = cmDspSysInstallCb( h, srcInstArray[i], srcVarLabel, dstInstArray[i],  dstVarLabel, NULL )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysInstallCb11N1( cmDspSysH_t h, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarLabel,   cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel, unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    if((rc = cmDspSysInstallCb( h, srcInstPtr, srcVarLabel, dstInstArray[i], dstVarLabel, NULL )) != kOkDspRC )
      break;
  }
  return rc;
}


cmDspRC_t cmDspSysInstallCb111N( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarLabel,     cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixLabel, unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    const cmChar_t* dstVarStr = dstVarPrefixLabel;
    if( n >= 2 )
      dstVarStr = cmDspSysPrintLabel(dstVarPrefixLabel,i);

    if((rc = cmDspSysInstallCb( h, srcInstPtr, srcVarLabel, dstInstPtr, dstVarStr, NULL )) != kOkDspRC )
      break;
  }
  return rc;
}


cmDspRC_t cmDspSysInstallCbN111( cmDspSysH_t h, cmDspInst_t* srcInstArray[], const cmChar_t* srcVarLabel, cmDspInst_t* dstInstPtr, const cmChar_t* dstVarLabel,       unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned  i;

  for(i=0; i<n; ++i)
  {
    if((rc = cmDspSysInstallCb( h, srcInstArray[i], srcVarLabel, dstInstPtr,  dstVarLabel, NULL )) != kOkDspRC )
      break;
  }
  return rc;
}


cmDspRC_t cmDspSysInstallCb1N11( cmDspSysH_t h, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarPrefixLabel, cmDspInst_t* dstInstPtr, const cmChar_t* dstVarLabel, unsigned n )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned  i;

  for(i=0; i<n; ++i)
  {
    const cmChar_t* srcVarStr = srcVarPrefixLabel;
    if( n >= 2 )
      srcVarStr = cmDspSysPrintLabel(srcVarPrefixLabel,i);

    if((rc = cmDspSysInstallCb( h, srcInstPtr, srcVarStr, dstInstPtr,  dstVarLabel, NULL )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysInstallCb1N1NM( cmDspSysH_t h, cmDspInst_t* srcInstPtr,     const cmChar_t* srcVarPrefixStr, unsigned srcCnt, cmDspInst_t* dstInstPtr,     const cmChar_t* dstVarPrefixStr, unsigned dstCnt, const unsigned* map )
{
  cmDsp_t*  p  = _cmDspHandleToPtr(h);
  cmDspRC_t rc = kOkDspRC;
  unsigned  i;

  for(i=0; i<dstCnt; ++i)
  {
    const cmChar_t* label0 = srcVarPrefixStr;
    const cmChar_t* label1 = dstVarPrefixStr;

    if( map[i] >= srcCnt )
    {
      if( map[i] >= srcCnt )
        return cmErrMsg(&p->err,kInstNotFoundDspRC,"The source instance map connection index %i is of range %i.",i,srcCnt);
    }

    if( srcCnt >= 2 )
      label0 = cmDspSysPrintLabel(srcVarPrefixStr,map[i]);

    if( dstCnt >= 2 )
      label1 = cmDspSysPrintLabel2(dstVarPrefixStr,i);


    if((rc = cmDspSysInstallCb( h, srcInstPtr, label0, dstInstPtr, label1, NULL )) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysInstallCb1NN1M( cmDspSysH_t h, cmDspInst_t* srcInstPtr,  const cmChar_t* srcVarPrefixStr, unsigned srcCnt, cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel, unsigned dstCnt, const unsigned* map )
{
  cmDsp_t*  p  = _cmDspHandleToPtr(h);
  cmDspRC_t rc = kOkDspRC;
  unsigned  i;

  for(i=0; i<dstCnt; ++i)
  {
    if( map[i] >= srcCnt )
      return cmErrMsg(&p->err,kInstNotFoundDspRC,"The source instance map connection index %i is of range %i.",i,srcCnt);

    const cmChar_t* srcVarStr = srcVarPrefixStr;

    if( srcCnt >= 2 )
      srcVarStr = cmDspSysPrintLabel(srcVarPrefixStr,map[i]);

    if((rc = cmDspSysInstallCb( h, srcInstPtr, srcVarStr, dstInstArray[i], dstVarLabel, NULL )) != kOkDspRC )
      break;
  }
  return rc;

}

cmDspRC_t cmDspSysInstallCb1NN1M2( cmDspSysH_t h, cmDspInst_t* srcInstPtr,  const cmChar_t* srcVarPrefixStr, unsigned srcCnt, const unsigned* map, cmDspInst_t* dstInstArray[], const cmChar_t* dstVarLabel, unsigned dstCnt )
{
  cmDsp_t*  p  = _cmDspHandleToPtr(h);
  cmDspRC_t rc = kOkDspRC;
  unsigned  i;

  for(i=0; i<srcCnt; ++i)
  {
    if( map[i] >= dstCnt )
      return cmErrMsg(&p->err,kInstNotFoundDspRC,"The dest. instance map connection index %i is of range %i.",map[i],dstCnt);

    const cmChar_t* srcVarStr = srcVarPrefixStr;

    if( srcCnt >= 2 )
      srcVarStr = cmDspSysPrintLabel(srcVarPrefixStr,i);

    if((rc = cmDspSysInstallCb( h, srcInstPtr, srcVarStr, dstInstArray[ map[i] ], dstVarLabel, NULL )) != kOkDspRC )
      break;
  }
  return rc;

}


double       cmDspSysSampleRate( cmDspSysH_t h )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return p->ctx.ctx->ss->args.srate;
}

cmJsonH_t    cmDspSysPgmRsrcHandle( cmDspSysH_t h )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return p->ctx.rsrcJsH;
}

cmSymTblH_t  cmDspSysSymbolTable( cmDspSysH_t h )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return p->ctx.stH;  
}

unsigned     cmDspSysRegisterStaticSymbol( cmDspSysH_t h, const cmChar_t* symLabel )
{ return cmSymTblRegisterStaticSymbol( cmDspSysSymbolTable(h), symLabel ); }

unsigned     cmDspSysRegisterSymbol( cmDspSysH_t h, const cmChar_t* symLabel )
{ return cmSymTblRegisterSymbol( cmDspSysSymbolTable(h), symLabel); }

cmCtx_t*     cmDspSysPgmCtx( cmDspSysH_t h )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return &p->cmCtx;  
}

cmLHeapH_t   cmDspSysLHeap( cmDspSysH_t h )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return p->lhH;  
}

unsigned     cmDspSysNetNodeId( cmDspSysH_t h )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h); 
  return cmUdpNetLocalNodeId(p->netH);
}

const cmChar_t* cmDspSysNetNodeLabel( cmDspSysH_t h )
{
  cmDsp_t* p = _cmDspHandleToPtr(h); 
  return cmUdpNetLocalNodeLabel(p->netH);
}

const cmChar_t* cmDspSysNetNodeIdToLabel( cmDspSysH_t h, unsigned netNodeId )
{
  cmDsp_t* p = _cmDspHandleToPtr(h); 
  return cmUdpNetNodeIdToLabel(p->netH,netNodeId);
}

unsigned        cmDspSysNetNodeLabelToId( cmDspSysH_t h, const cmChar_t* netNodeLabel )
{
  cmDsp_t* p = _cmDspHandleToPtr(h); 
  return cmUdpNetNodeLabelToId(p->netH,netNodeLabel);
}


cmDspInstSymId_t* _cmDspSysFindInstSymId( cmDsp_t* p, unsigned symId )
{
  cmDspInstSymId_t* ip = p->symIdList;
  for(; ip != NULL; ip=ip->link)
    if( ip->symId == symId )
      return ip;
  return NULL;
}

unsigned   cmDspSysRegisterInstAttrSymbol(    cmDspSysH_t h, unsigned symId )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  cmDspInstSymId_t* ip;

  // if this symbol is already registered
  if((ip = _cmDspSysFindInstSymId(p,symId)) != NULL )
    return ip->symId;

  // try to find an unused symbol
  ip = p->symIdList;
  for(; ip != NULL; ip=ip->link)
    if( ip->symId == cmInvalidId )
      break;

  // if no unused symbols were found then allocate a new one
  if( ip == NULL )
  {
    ip           = cmLhAllocZ(p->ctx.lhH, cmDspInstSymId_t, 1 );
    ip->link     = p->symIdList;
    p->symIdList = ip;
  }

  ip->symId       = symId;

  return ip->symId;
}

unsigned    cmDspSysRegisterInstAttrSymbolStr( cmDspSysH_t h, const cmChar_t* symLabel )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h);
  unsigned symId;

  if((symId = cmSymTblRegisterSymbol(p->ctx.stH,symLabel)) == cmInvalidId )
    return cmErrMsg(&p->err,kSymTblFailDspRC,"The instance attribute symbol '%s' could not be registered.",cmStringNullGuard(symLabel));

  return cmDspSysRegisterInstAttrSymbol(h,symId);
}

cmDspRC_t    cmDspSysAssignInstAttrSymbol( cmDspSysH_t h, cmDspInst_t* inst, unsigned symId )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);
  return cmDspInstRegisterAttrSym(&p->ctx, inst, symId) != kOkDspRC;
}

unsigned    cmDspSysAssignInstAttrSymbolStr( cmDspSysH_t h, cmDspInst_t* inst, const cmChar_t* symLabel )
{  
  unsigned symId = cmDspSysRegisterSymbol(h,symLabel);

  if( cmDspSysAssignInstAttrSymbol( h, inst, symId) != kOkDspRC )
    return cmInvalidId;

  return symId;
}

cmDspRC_t   cmDspSysRemoveInstAttrSymbol(      cmDspSysH_t h, unsigned symId )
{
  cmDsp_t* p = _cmDspHandleToPtr(h);

  cmDspInstSymId_t* ip;

  // if this symbol is already registered
  if((ip = _cmDspSysFindInstSymId(p,symId)) == NULL )
    return cmErrMsg(&p->err,kOkDspRC,"The instance attribute symbol '%s' is could not be found to be removed.",cmStringNullGuard(cmSymTblLabel(p->ctx.stH,symId)));

  // mark a symbol recd as inactive by setting the symbol id to cmInalidId
  ip->symId = cmInvalidId;
  return kOkDspRC;
}

cmDspRC_t   cmDspSysRemoveInstAttrSymbolStr(   cmDspSysH_t h, const cmChar_t* symLabel )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h);
  unsigned symId;
  if( (symId = cmSymTblId(p->ctx.stH, symLabel )) == cmInvalidId )
    return cmErrMsg(&p->err,kSymNotFoundDspRC,"The instance attribute symbol '%s' does not exist and therefore cannot be removed.",cmStringNullGuard(symLabel));

  return cmDspSysRemoveInstAttrSymbol(h, symId );
}

cmDspRC_t   cmDspSysBroadcastValue( cmDspSysH_t h, unsigned instAttrSymId, const cmDspValue_t* valuePtr )
{
    cmDsp_t* p = _cmDspHandleToPtr(h);
   _cmDspInst_t* ip = p->instList;

   for(; ip != NULL; ip = ip->linkPtr )
     if( (instAttrSymId==cmInvalidId) || cmDspInstHasAttrSym(ip->instPtr,instAttrSymId)) 
       ip->instPtr->sysRecvFunc(&p->ctx,ip->instPtr,instAttrSymId,valuePtr);
     
   return kOkDspRC;
}


cmDspInst_t* cmDspSysAllocLabel( cmDspSysH_t h, const cmChar_t* label, unsigned alignId )
{
  return cmDspSysAllocInst(h,"Label", NULL, 2, label, alignId);
}

cmDspRC_t _cmDspGetRsrcReal( cmDspSysH_t h, const cmChar_t* label, const cmChar_t* rsrcPath, cmReal_t* valPtr )
{
  const cmChar_t* errLabelPtr = NULL;

  if( cmJsonPathValues( cmDspSysPgmRsrcHandle(h),NULL,NULL,&errLabelPtr, rsrcPath, kRealTId, valPtr, NULL ) != kOkJsRC )
  {
    cmDsp_t* p = _cmDspHandleToPtr(h);
    return cmErrMsg(&p->err,kRsrcNotFoundDspRC,"DSP instance allocation failed for '%s' and the initial value resource path '%s'.", cmStringNullGuard(label),cmStringNullGuard(rsrcPath));    
  }

  return kOkDspRC;
}

cmDspInst_t* _cmDspSysAllocScalar(     cmDspSysH_t h, unsigned presetGrpSymId, const cmChar_t* instLabel, cmReal_t min, cmReal_t max, cmReal_t step, cmReal_t val, const cmChar_t* label )
{
  cmDspInst_t* inst;
  if((inst = cmDspSysAllocInstS(h,"Scalar", presetGrpSymId, instLabel, 6, kNumberDuiId, min, max, step, val, label )) == NULL)
  {
    cmDsp_t* p = _cmDspHandleToPtr(h);
    cmErrMsg(&p->err,kAllocInstFailDspRC,"Scalar UI control allocation failed for '%s'.", cmStringNullGuard(instLabel));
  }

  return inst;
}

cmDspInst_t* cmDspSysAllocScalar(     cmDspSysH_t h, const cmChar_t* label, cmReal_t min, cmReal_t max, cmReal_t step, cmReal_t val )
{ return _cmDspSysAllocScalar(h,cmInvalidId,label,min,max,step,val,label); }


cmChar_t*  _cmDspSysFormLabel( cmDspSysH_t h, unsigned presetGrpSymId, const cmChar_t* prefixLabel, const cmChar_t* label )
{ 
  cmDsp_t* p = _cmDspHandleToPtr(h);

  if( prefixLabel == NULL )
  {
    if(( prefixLabel = cmSymTblLabel(p->ctx.stH,presetGrpSymId)) == NULL )
      cmErrMsg(&p->err,kInvalidArgDspRC,"The prefix label was not given for the DSP instance '%s'.",cmStringNullGuard(label));
  }

  if( label == NULL )
    cmErrMsg(&p->err,kInvalidArgDspRC,"NULL was passed where a DSP instance label was expected.");

  return cmTsPrintfH(cmDspSysLHeap(h),"%s-%s",cmStringNullGuard(prefixLabel),cmStringNullGuard(label));  
}

cmDspInst_t* cmDspSysAllocScalarP(     cmDspSysH_t h, unsigned presetGrpSymId, const cmChar_t* prefixLabel, const cmChar_t* label, cmReal_t min, cmReal_t max, cmReal_t step, cmReal_t val )
{
  return _cmDspSysAllocScalar(h,presetGrpSymId,_cmDspSysFormLabel(h,presetGrpSymId,prefixLabel,label),min,max,step,val,label);
}

cmDspInst_t* cmDspSysAllocScalarRsrc( cmDspSysH_t h, const cmChar_t* label, cmReal_t min, cmReal_t max, cmReal_t step, const cmChar_t* rsrcPath )
{
  cmReal_t val;

  if(_cmDspGetRsrcReal(h,label,rsrcPath,&val ) != kOkDspRC )
    return NULL;

  return cmDspSysAllocScalar(h,label,min,max,step,val);
}

cmDspInst_t** cmDspSysAllocScalarA(    cmDspSysH_t h, unsigned cnt, unsigned presetGroupSymId, const cmChar_t* prefixLabel, const cmChar_t* label, cmReal_t min, cmReal_t max, cmReal_t step, cmReal_t val )
{ 
  cmChar_t* lbl = cmTsPrintfH( cmDspSysLHeap(h), "%s-%s",prefixLabel,label);
  return cmDspSysAllocInstArrayS( h, cnt, presetGroupSymId, "Scalar", lbl, NULL, 5, kNumberDuiId, min, max, step, val); 
}

cmDspInst_t* _cmDspSysAllocButton(     cmDspSysH_t h, unsigned btnTypeId, unsigned presetGroupSymId, const cmChar_t* instLabel, const cmChar_t* label, cmReal_t val, unsigned valSymId )
{
  cmDspInst_t* inst;
  if((inst = cmDspSysAllocInstS( h, "Button", presetGroupSymId,  instLabel, 4, btnTypeId, val, valSymId, label)) == NULL )
  {
    cmDsp_t* p = _cmDspHandleToPtr(h);
    cmErrMsg(&p->err,kAllocInstFailDspRC,"Button UI control allocation failed for '%s'.", cmStringNullGuard(label));
  }
  return inst;
}

cmDspInst_t* cmDspSysAllocButtonP(    cmDspSysH_t h, const cmChar_t* prefixLabel, const cmChar_t* label, cmReal_t val )
{
  return cmDspSysAllocButton(h,_cmDspSysFormLabel(h,cmInvalidId,prefixLabel,label),val);
}

cmDspInst_t* _cmDspSysAllocButtonRsrc( cmDspSysH_t h, unsigned typeId, unsigned presetGroupSymId, const cmChar_t* label, const cmChar_t* rsrcPath )
{
  cmReal_t val;

  if(_cmDspGetRsrcReal(h,label,rsrcPath,&val) != kOkDspRC )
    return NULL;

  return _cmDspSysAllocButton(h,typeId,presetGroupSymId,label,label,val,cmInvalidId);
}

cmDspInst_t* cmDspSysAllocButton(     cmDspSysH_t h, const cmChar_t* label, cmReal_t val )
{ return _cmDspSysAllocButton(h,kButtonDuiId,cmInvalidId,label,label,val,cmInvalidId); }

cmDspInst_t* cmDspSysAllocButtonRsrc( cmDspSysH_t h, const cmChar_t* label, const cmChar_t* rsrcPath )
{ return _cmDspSysAllocButtonRsrc( h,kButtonDuiId,cmInvalidId,label,rsrcPath); }

cmDspInst_t* _cmDspSysAllocCheck(     cmDspSysH_t h, unsigned presetGroupSymId, const cmChar_t* instLabel, const cmChar_t* label, cmReal_t val )
{ return _cmDspSysAllocButton(h,kCheckDuiId,presetGroupSymId,instLabel,label,val,cmInvalidId); }

cmDspInst_t* cmDspSysAllocCheck(     cmDspSysH_t h, const cmChar_t* label, cmReal_t val )
{ return  _cmDspSysAllocCheck(h,cmInvalidId,label,label,val); }

cmDspInst_t* cmDspSysAllocCheckP(    cmDspSysH_t h, unsigned presetGroupSymId, const cmChar_t* prefixLabel, const cmChar_t* label, cmReal_t val )
{ return _cmDspSysAllocCheck(h,presetGroupSymId,_cmDspSysFormLabel(h,presetGroupSymId,prefixLabel,label),label,val); }

cmDspInst_t* cmDspSysAllocCheckRsrc( cmDspSysH_t h, const cmChar_t* label, const cmChar_t* rsrcPath )
{ return _cmDspSysAllocButtonRsrc( h,kCheckDuiId, cmInvalidId, label,rsrcPath); }

cmDspInst_t* cmDspSysAllocMsgList( cmDspSysH_t h, const cmChar_t* fn, const cmChar_t* rsrcLabel, unsigned initSelIdx )
{ return cmDspSysAllocInst( h, "MsgList", NULL,  3, rsrcLabel, fn, initSelIdx); }

cmDspInst_t* cmDspSysAllocMsgListP( cmDspSysH_t h, unsigned presetGroupSymId, const cmChar_t* preLabel, const cmChar_t* label, const cmChar_t* fn, const cmChar_t* rsrcLabel, unsigned initSelIdx )
{ 
  cmDspInst_t* inst;
  cmChar_t* lbl;
  if((inst = cmDspSysAllocInstS( h, "MsgList", presetGroupSymId,  lbl = _cmDspSysFormLabel(h,presetGroupSymId,preLabel,label), 3, rsrcLabel, fn, initSelIdx)) == NULL )
  {
    cmDsp_t* p = _cmDspHandleToPtr(h);
    cmErrMsg(&p->err,kAllocInstFailDspRC,"Msg List UI control allocation failed for '%s'.", cmStringNullGuard(lbl));
  }
  return inst;


}


cmDspInst_t* cmDspSysAllocAudioIn( cmDspSysH_t h, unsigned chIdx, cmReal_t gain )
{ return cmDspSysAllocInst( h, "AudioIn",     NULL, 2, chIdx, gain ); }

const cmJsonNode_t*  _cmDspSysFindAudioChMap( cmDspSysH_t h, const cmChar_t* rsrcLabel )
{
  cmDsp_t*  p   = _cmDspHandleToPtr(h);
  cmJsonH_t jsH = cmDspSysPgmRsrcHandle(h);
  assert( cmJsonIsValid(jsH) );

  const cmJsonNode_t* np = NULL;
  if(( np = cmJsonFindValue( jsH, rsrcLabel, cmJsonRoot(jsH), kArrayTId )) == NULL )
  {
    cmErrMsg(&p->err,kRsrcNotFoundDspRC,"The audio channel map resource '%s' was not found.", cmStringNullGuard(rsrcLabel));
    return NULL;
  }

  unsigned chCnt = cmJsonChildCount(np);

  if( chCnt == 0 )
  {
    cmErrMsg(&p->err,kInvalidArgDspRC,"The audio channel map resource '%s' is empty.",cmStringNullGuard(rsrcLabel));
    return NULL;
  }

  return np;
}

cmDspRC_t _cmDspSysReadAudioChMap( cmDspSysH_t h, const cmChar_t* rsrcLabel, const cmJsonNode_t* mapNodePtr, unsigned* map, unsigned mapCnt )
{
  cmDsp_t*  p   = _cmDspHandleToPtr(h);
  unsigned  i;

  for(i=0; i<mapCnt; ++i)
  {
    const cmJsonNode_t* np;

    if((np = cmJsonArrayElementC(mapNodePtr,i)) == NULL )
      return cmErrMsg(&p->err,kInvalidArgDspRC,"The audio map element at index %i could not be accessed in the audio map:'%s'.",i,cmStringNullGuard(rsrcLabel));

    if( cmJsonUIntValue(np,map + i ) != kOkJsRC )
      return cmErrMsg(&p->err,kInvalidArgDspRC,"The audio map element at index %i could not be read as an integer in the audio map:'%s'..",i,cmStringNullGuard(rsrcLabel));
    
  }

  return kOkDspRC;
}

cmDspInst_t** _cmDspSysAllocAudioRsrc( cmDspSysH_t h, const cmChar_t* classLabel, const cmChar_t* rsrcLabel, cmReal_t gain, unsigned* retChCntPtr )
{ 
  cmDsp_t*  p   = _cmDspHandleToPtr(h);
  const cmJsonNode_t* np;

  if( retChCntPtr != NULL )
    *retChCntPtr = 0;

  // 
  if( rsrcLabel==NULL )
    rsrcLabel = "audioInMap";

  // find the map resource
  if((np = _cmDspSysFindAudioChMap(h,rsrcLabel)) == NULL )
    return NULL;

  unsigned      chCnt = cmJsonChildCount(np);
  cmDspInst_t** a     = cmLhAllocZ( p->lhH, cmDspInst_t*, chCnt );
  unsigned      chMap[ chCnt ];
  
  // fill in the channel map - maps virtual channels to physical channels
  if( _cmDspSysReadAudioChMap(h,rsrcLabel,np,chMap,chCnt) != kOkDspRC )
    return NULL;

  unsigned i;
  for(i=0; i<chCnt; ++i)
    if((a[i] = cmDspSysAllocInst( h, classLabel,  NULL, 2, chMap[i], gain )) == NULL )
      break;
  
  if( i==chCnt )
  {

    if( retChCntPtr != NULL )
      *retChCntPtr = chCnt;

    return a;
  }

  return NULL;
}

cmDspInst_t** cmDspSysAllocAudioInAR( cmDspSysH_t h, const cmChar_t* rsrcLabel, cmReal_t gain, unsigned* retChCntPtr )
{ return _cmDspSysAllocAudioRsrc(h,"AudioIn",rsrcLabel,gain,retChCntPtr); }


cmDspInst_t* cmDspSysAllocAudioOut(cmDspSysH_t h, unsigned chIdx, cmReal_t gain )
{ return cmDspSysAllocInst( h, "AudioOut", NULL, 2, chIdx, gain ); }

cmDspInst_t** cmDspSysAllocAudioOutAR( cmDspSysH_t h, const cmChar_t* rsrcLabel, cmReal_t gain, unsigned* retChCntPtr )
{ return _cmDspSysAllocAudioRsrc(h,"AudioOut",rsrcLabel,gain,retChCntPtr); }

cmDspRC_t cmDspSysInstallNetCb( cmDspSysH_t h, cmDspInst_t* srcInstPtr,  const cmChar_t* srcVarLabel,  const cmChar_t* dstNetNodeLabel, const cmChar_t* dstInstLabel, const cmChar_t* dstVarLabel  )
{
  cmDsp_t*         p            = _cmDspHandleToPtr(h);
  cmDspRC_t        rc           = kOkDspRC;
  cmDspInst_t*     netSendInstPtr;  
  unsigned         dstNetNodeId;
  _cmDspSrcConn_t* srcConnPtr   = NULL;

  // get the dest. network node id
  if( (dstNetNodeId = cmUdpNetNodeLabelToId(p->netH, dstNetNodeLabel )) == cmInvalidId )
    return cmErrMsg(&p->err,kNetNodeNotFoundDspRC,"The destination network node label '%s' was not found.",cmStringNullGuard(dstNetNodeLabel));

  if((srcConnPtr = _cmDspSysNetCreateSrcConn( p, dstNetNodeId, dstInstLabel, dstVarLabel)) == NULL )
    return rc;

  // allocate a network sender
  if((netSendInstPtr = cmDspSysAllocInst(h, "NetSend", NULL, 1, srcConnPtr )) == NULL )
    return cmErrMsg(&p->err,kNetSendAllocFailDspRC,"The network sender DSP instance allocation failed.");
  
  return cmDspSysInstallCb(h, srcInstPtr, srcVarLabel, netSendInstPtr, "in", NULL );
}

cmDspRC_t cmDspSysInstallNetCb1N1N( cmDspSysH_t h, cmDspInst_t* srcInstPtr,  const cmChar_t* srcVarPrefixStr,  const cmChar_t* dstNetNodeLabel, const cmChar_t* dstInstLabel, const cmChar_t* dstVarPrefixStr, unsigned dstOffs, unsigned n  )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    
    const cmChar_t* label0 = srcVarPrefixStr; 
    const cmChar_t* label1 = dstVarPrefixStr; 

    if( n >= 2 )
    {
      label0 = cmDspSysPrintLabel( srcVarPrefixStr,i);
      label1 = cmDspSysPrintLabel2(dstVarPrefixStr,dstOffs + i);
    }

    if((rc =  cmDspSysInstallNetCb(h,srcInstPtr,label0,dstNetNodeLabel,dstInstLabel,label1)) != kOkDspRC )
      break;
  }
  return rc;
}

cmDspRC_t cmDspSysInstallNetCb1N1NM(cmDspSysH_t h, cmDspInst_t* srcInstPtr,  const cmChar_t* srcVarPrefixStr,  unsigned srcCnt, const cmChar_t* dstNetNodeLabel, const cmChar_t* dstInstLabel, const cmChar_t* dstVarPrefixStr,  unsigned dstCnt, const unsigned* map )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<dstCnt; ++i)
  {
    assert( map[i] < srcCnt );

    const cmChar_t* label0 = cmDspSysPrintLabel( srcVarPrefixStr, map[i] );
    const cmChar_t* label1 = cmDspSysPrintLabel2(dstVarPrefixStr, i );

    if((rc =  cmDspSysInstallNetCb(h,srcInstPtr,label0,dstNetNodeLabel,dstInstLabel,label1)) != kOkDspRC )
      break;    
  }
  return rc;
}


cmDspRC_t _cmDspRsrcPath( cmDspSysH_t h, cmChar_t** pathPtr, va_list vl )
{
  unsigned  i;
  cmDspRC_t rc        = kOkDspRC;;
  cmDsp_t*  p         = _cmDspHandleToPtr(h);
  cmChar_t* path      = NULL;
  unsigned  maxArgCnt = 10;

  *pathPtr = NULL;

  for(i=0; i<maxArgCnt; ++i )
  {    
    cmChar_t* str;

    if((str = va_arg(vl,cmChar_t* )) == NULL )
      break;
    
    if( path != NULL )
      path = cmTextAppendSS(path,"/");
    path = cmTextAppendSS(path,str);    
  }

  if( i >= maxArgCnt )
  {
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"A resource path, beginning with '%25s', does not contain a terminating NULL.", cmStringNullGuard(path) );
    goto errLabel;
  }

  // duplicate the string onto the program linked heap
  *pathPtr = cmLhAllocStr( cmDspSysLHeap(h), path);

 errLabel:
  cmMemFree(path);

  return rc;
}

cmDspRC_t _cmDspRsrcWritePath( cmDspSysH_t h, cmChar_t** pathStrRef, cmChar_t** varStrRef, va_list vl )
{
  unsigned  i;
  cmDspRC_t rc        = kOkDspRC;;
  cmDsp_t*  p         = _cmDspHandleToPtr(h);
  cmChar_t* path      = NULL;
  unsigned  maxArgCnt = 10;
  cmChar_t* prevStr   = NULL;

  *pathStrRef = NULL;
  *varStrRef  = NULL;
 
  for(i=0; i<maxArgCnt; ++i )
  {
    cmChar_t* str;

    if((str = va_arg(vl,cmChar_t* )) == NULL )
      break;
    
    if( prevStr != NULL )
    {
      if( path != NULL )
        path = cmTextAppendSS(path,"/");

      path = cmTextAppendSS(path,prevStr);    
    }

    prevStr = str;    
  }

  if( i >= maxArgCnt )
  {
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"A resource path, beginning with '%25s', does not contain a terminating NULL.", cmStringNullGuard(path) );
    goto errLabel;
  }

  // duplicate the string onto the program linked heap
  if( path != NULL )
    *pathStrRef = cmLhAllocStr( cmDspSysLHeap(h), path);

  *varStrRef  = prevStr;

 errLabel:
  cmMemFree(path);

  return rc;

}

cmDspRC_t cmDspRsrcBoolV(    cmDspSysH_t h, bool* vp, va_list vl )
{
  assert(vp != NULL);
  cmDsp_t*  p    = _cmDspHandleToPtr(h);
  cmChar_t* path = NULL;
  cmDspRC_t rc;

  if((rc = _cmDspRsrcPath(h,&path,vl)) != kOkDspRC )
    return rc;

  if((rc = cmJsonPathToBool( cmDspSysPgmRsrcHandle(h), NULL, NULL, path, vp )) != kOkJsRC )
    rc =  cmErrMsg(&p->err,kJsonFailDspRC,"'bool' resource not found at path:'%s'.",cmStringNullGuard(path));

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;
}

cmDspRC_t cmDspRsrcIntV(    cmDspSysH_t h, int* vp, va_list vl )
{
  assert(vp != NULL);
  cmDsp_t*  p    = _cmDspHandleToPtr(h);
  cmChar_t* path = NULL;
  cmDspRC_t rc;

  if((rc = _cmDspRsrcPath(h,&path,vl)) != kOkDspRC )
    return rc;

  if((rc = cmJsonPathToInt( cmDspSysPgmRsrcHandle(h), NULL, NULL, path, vp )) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"'int' resource not found at path:'%s'.",cmStringNullGuard(path));

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;  
}

cmDspRC_t cmDspRsrcUIntV(   cmDspSysH_t h, unsigned* vp, va_list vl )
{
  assert(vp != NULL);
  cmDsp_t*  p    = _cmDspHandleToPtr(h);
  cmChar_t* path = NULL;
  cmDspRC_t rc;

  if((rc = _cmDspRsrcPath(h,&path,vl)) != kOkDspRC )
    return rc;

  if((rc = cmJsonPathToUInt( cmDspSysPgmRsrcHandle(h), NULL, NULL, path, vp )) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"'uint' resource not found at path:'%s'.",cmStringNullGuard(path));

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;  
}

cmDspRC_t cmDspRsrcDblV(   cmDspSysH_t h, double* vp, va_list vl )
{
  assert(vp != NULL);
  cmDsp_t*  p    = _cmDspHandleToPtr(h);
  cmChar_t* path = NULL;
  cmDspRC_t rc;

  if((rc = _cmDspRsrcPath(h,&path,vl)) != kOkDspRC )
    return rc;

  if((rc = cmJsonPathToReal( cmDspSysPgmRsrcHandle(h), NULL, NULL, path, vp )) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"'real' resource not found at path:'%s'.",cmStringNullGuard(path));

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;  
}

cmDspRC_t cmDspRsrcRealV(   cmDspSysH_t h, cmReal_t* vp, va_list vl )
{
  assert(vp != NULL);
  cmDsp_t*  p    = _cmDspHandleToPtr(h);
  cmChar_t* path = NULL;
  cmDspRC_t rc;
  double    dval;

  if((rc = _cmDspRsrcPath(h,&path,vl)) != kOkDspRC )
    return rc;

  if((rc = cmJsonPathToReal( cmDspSysPgmRsrcHandle(h), NULL, NULL, path, &dval )) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"'real' resource not found at path:'%s'.",cmStringNullGuard(path));
  else
    *vp = dval;

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;  
}

cmDspRC_t cmDspRsrcStringV( cmDspSysH_t h, const cmChar_t** vp, va_list vl )
{
  assert(vp != NULL);
  cmDsp_t*  p    = _cmDspHandleToPtr(h);
  cmChar_t* path = NULL;
  cmDspRC_t rc;

  if((rc = _cmDspRsrcPath(h,&path,vl)) != kOkDspRC )
    return rc;

  if((rc = cmJsonPathToString( cmDspSysPgmRsrcHandle(h), NULL, NULL, path, vp )) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"'string' resource not found at path:'%s'.",cmStringNullGuard(path));

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;  
}

cmDspRC_t cmDspRsrcArrayCountV( cmDspSysH_t h, unsigned *n, va_list vl )
{
  assert(n != NULL );
  cmDsp_t*      p    = _cmDspHandleToPtr(h);
  cmChar_t*     path = NULL;
  cmJsonNode_t* np   = NULL;
  cmDspRC_t     rc;

  if((rc = _cmDspRsrcPath(h,&path,vl)) != kOkDspRC )
    return rc;

  if((rc = cmJsonPathToArray( cmDspSysPgmRsrcHandle(h), NULL, NULL, path, &np )) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"'array' resource not found at path:'%s'.",cmStringNullGuard(path));
  else
    *n = cmJsonChildCount(np);

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;  

}

cmDspRC_t _cmDspRsrcArrayV( cmDspSysH_t h, unsigned* np, cmJsonNode_t** npp, cmChar_t** pathPtrPtr, va_list vl )
{
  cmDsp_t*      p       = _cmDspHandleToPtr(h);
  cmChar_t*     path    = NULL;
  cmJsonH_t     jsH     = cmDspSysPgmRsrcHandle(h);
  cmDspRC_t     rc;

  if((rc = _cmDspRsrcPath(h,&path,vl)) != kOkDspRC )
    return rc;

  if((rc = cmJsonPathToArray( jsH, NULL, NULL, path, npp )) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"'array' resource not found at path:'%s'.",cmStringNullGuard(path));
  else
  {
    if((*np = cmJsonChildCount(*npp)) != 0 )
      *pathPtrPtr = path;

  }

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;
}

cmDspRC_t cmDspRsrcBoolArrayV(   cmDspSysH_t h, unsigned* np, bool** vpp, va_list vl )
{
  assert(vpp!=NULL);
  cmDsp_t*      p       = _cmDspHandleToPtr(h);
  cmJsonNode_t* nodePtr = NULL;
  unsigned      n       = 0;
  cmChar_t*     path    = NULL;
  unsigned      i;
  cmDspRC_t     rc;

  if(( rc = _cmDspRsrcArrayV(h,&n,&nodePtr,&path,vl)) != kOkDspRC )
    return rc;
  
  bool* vp = *vpp;
  vp = cmLhResizeN(p->ctx.lhH,bool,vp,n);

  for(i=0; i<n && rc == kOkDspRC; ++i)
    if( cmJsonBoolValue( cmJsonArrayElementC(nodePtr,i), vp + i ) != kOkJsRC )
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"Element at  index '%i' in resource '%s' is invalid.",i,cmStringNullGuard(path)); 

  *vpp = vp;
  *np  = n;

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;
}

cmDspRC_t cmDspRsrcIntArrayV(   cmDspSysH_t h, unsigned* np, int** vpp, va_list vl )
{
  assert(vpp!=NULL);
  cmDsp_t*      p       = _cmDspHandleToPtr(h);
  cmJsonNode_t* nodePtr = NULL;
  unsigned      n       = 0;
  cmChar_t*     path    = NULL;
  unsigned      i;
  cmDspRC_t     rc;

  if(( rc = _cmDspRsrcArrayV(h,&n,&nodePtr,&path,vl)) != kOkDspRC )
    return rc;
  
  int* vp = *vpp;
  vp = cmLhResizeN(p->ctx.lhH,int,vp,n);

  for(i=0; i<n && rc == kOkDspRC; ++i)
    if( cmJsonIntValue( cmJsonArrayElementC(nodePtr,i), vp + i ) != kOkJsRC )
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"Element at  index '%i' in resource '%s' is invalid.",i,cmStringNullGuard(path)); 

  *vpp = vp;
  *np  = n;

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;
}

cmDspRC_t cmDspRsrcUIntArrayV(  cmDspSysH_t h, unsigned* np, unsigned** vpp, va_list vl )
{
  assert(vpp!=NULL);
  cmDsp_t*      p       = _cmDspHandleToPtr(h);
  cmJsonNode_t* nodePtr = NULL;
  unsigned      n       = 0;
  cmChar_t*     path    = NULL;
  unsigned      i;
  cmDspRC_t     rc;

  if(( rc = _cmDspRsrcArrayV(h,&n,&nodePtr,&path,vl)) != kOkDspRC )
    return rc;
  
  unsigned* vp = *vpp;
  vp = cmLhResizeN(p->ctx.lhH,unsigned,vp,n);

  for(i=0; i<n && rc == kOkDspRC; ++i)
    if( cmJsonUIntValue( cmJsonArrayElementC(nodePtr,i), vp + i ) != kOkJsRC )
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"Element at  index '%i' in resource '%s' is invalid.",i,cmStringNullGuard(path)); 

  *vpp = vp;
  *np  = n;

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;
}

cmDspRC_t cmDspRsrcDblArrayV(  cmDspSysH_t h, unsigned* np, double** vpp, va_list vl )
{
  assert(vpp!=NULL);
  cmDsp_t*      p       = _cmDspHandleToPtr(h);
  cmJsonNode_t* nodePtr = NULL;
  unsigned      n       = 0;
  cmChar_t*     path    = NULL;
  unsigned      i;
  cmDspRC_t     rc;

  if(( rc = _cmDspRsrcArrayV(h,&n,&nodePtr,&path,vl)) != kOkDspRC )
    return rc;
  
  double* vp = *vpp;
  vp = cmLhResizeN(p->ctx.lhH,double,vp,n);

  for(i=0; i<n && rc == kOkDspRC; ++i)
    if( cmJsonRealValue( cmJsonArrayElementC(nodePtr,i), vp + i ) != kOkJsRC )
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"Element at  index '%i' in resource '%s' is invalid.",i,cmStringNullGuard(path)); 

  *vpp = vp;
  *np  = n;

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;
}

cmDspRC_t cmDspRsrcRealArrayV(  cmDspSysH_t h, unsigned* np, cmReal_t** vpp, va_list vl )
{
  assert(vpp!=NULL);
  cmDsp_t*      p       = _cmDspHandleToPtr(h);
  cmJsonNode_t* nodePtr = NULL;
  unsigned      n       = 0;
  cmChar_t*     path    = NULL;
  unsigned      i;
  cmDspRC_t     rc;

  if(( rc = _cmDspRsrcArrayV(h,&n,&nodePtr,&path,vl)) != kOkDspRC )
    return rc;
  
  cmReal_t* vp = *vpp;
  vp = cmLhResizeN(p->ctx.lhH,cmReal_t,vp,n);

  for(i=0; i<n && rc == kOkDspRC; ++i)
  {
    double v;
    if( cmJsonRealValue( cmJsonArrayElementC(nodePtr,i), &v ) != kOkJsRC )
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"Element at  index '%i' in resource '%s' is invalid.",i,cmStringNullGuard(path)); 
    
    vp[i] = v;
  }

  *vpp = vp;
  *np  = n;

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;
}

cmDspRC_t cmDspRsrcStringArrayV(cmDspSysH_t h, unsigned* np, const cmChar_t*** vpp, va_list vl )
{
  assert(vpp!=NULL);
  cmDsp_t*      p       = _cmDspHandleToPtr(h);
  cmJsonNode_t* nodePtr = NULL;
  unsigned      n       = 0;
  cmChar_t*     path    = NULL;
  unsigned      i;
  cmDspRC_t     rc;

  if(( rc = _cmDspRsrcArrayV(h,&n,&nodePtr,&path,vl)) != kOkDspRC )
    return rc;
  
  const cmChar_t** vp = *vpp;
  vp = cmLhResizeN(p->ctx.lhH,const cmChar_t*,vp,n);

  for(i=0; i<n && rc == kOkDspRC; ++i)
    if( cmJsonStringValue( cmJsonArrayElementC(nodePtr,i), vp + i ) != kOkJsRC )
      rc = cmErrMsg(&p->err,kJsonFailDspRC,"Element at  index '%i' in resource '%s' is invalid.",i,cmStringNullGuard(path)); 

  *vpp = vp;
  *np  = n;

  cmLhFree(cmDspSysLHeap(h),path);

  return rc;
}

cmDspRC_t cmDspRsrcInt(    cmDspSysH_t h, int* vp, ... )
{
  va_list vl;
  va_start(vl,vp);
  cmDspRC_t rc = cmDspRsrcIntV(h,vp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcUInt(   cmDspSysH_t h, unsigned* vp, ... )
{
  va_list vl;
  va_start(vl,vp);
  cmDspRC_t rc = cmDspRsrcUIntV(h,vp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcDbl(   cmDspSysH_t h, double* vp, ... )
{
  va_list vl;
  va_start(vl,vp);
  cmDspRC_t rc = cmDspRsrcDblV(h,vp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcReal(   cmDspSysH_t h, cmReal_t*  vp, ... )
{
  va_list vl;
  va_start(vl,vp);
  cmDspRC_t rc = cmDspRsrcRealV(h,vp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcString( cmDspSysH_t h, const cmChar_t** vp, ... )
{
  va_list vl;
  va_start(vl,vp);
  cmDspRC_t rc = cmDspRsrcStringV(h,vp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcArrayCount( cmDspSysH_t h, unsigned *np, ... )
{
  va_list vl;
  va_start(vl,np);
  cmDspRC_t rc = cmDspRsrcArrayCountV(h,np,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcBoolArray(    cmDspSysH_t h, unsigned* np, bool** vpp, ... )
{
  va_list vl;
  va_start(vl,vpp);
  cmDspRC_t rc = cmDspRsrcBoolArrayV(h,np,vpp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcIntArray(    cmDspSysH_t h, unsigned* np, int** vpp, ... )
{
  va_list vl;
  va_start(vl,vpp);
  cmDspRC_t rc = cmDspRsrcIntArrayV(h,np,vpp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcUIntArray(   cmDspSysH_t h, unsigned* np, unsigned** vpp, ... )
{
  va_list vl;
  va_start(vl,vpp);
  cmDspRC_t rc = cmDspRsrcUIntArrayV(h,np,vpp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcDblArray(   cmDspSysH_t h, unsigned* np, double** vpp, ... )
{
  va_list vl;
  va_start(vl,vpp);
  cmDspRC_t rc = cmDspRsrcDblArrayV(h,np,vpp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcRealArray(   cmDspSysH_t h, unsigned* np, cmReal_t**  vpp, ... )
{
  va_list vl;
  va_start(vl,vpp);
  cmDspRC_t rc = cmDspRsrcRealArrayV(h,np,vpp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspRsrcStringArray( cmDspSysH_t h, unsigned* np, const cmChar_t*** vpp, ... )
{
  va_list vl;
  va_start(vl,vpp);
  cmDspRC_t rc = cmDspRsrcStringArrayV(h,np,vpp,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t _cmDspWritePathAndParent( cmDspSysH_t h, cmChar_t** pathRef, cmChar_t** varLabelRef, cmJsonNode_t** parentRef, va_list vl )
{
  cmDsp_t*      p                = _cmDspHandleToPtr(h);
  cmJsonH_t     jsH              = cmDspSysPgmRsrcHandle(h);
  cmDspRC_t     rc;

  if((rc = _cmDspRsrcWritePath(h,pathRef,varLabelRef,vl)) != kOkDspRC )
    goto errLabel;

  if( *pathRef == NULL )
    *parentRef = cmJsonRoot(jsH);
  else
    *parentRef = cmJsonFindPathValue( jsH, *pathRef, cmJsonRoot(jsH), kPairTId );
    
  if( *parentRef == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"The parent object '%s' for variable '%s' could not be found.",cmStringNullGuard(*pathRef),cmStringNullGuard(*varLabelRef)); 
    goto errLabel;
  }

 errLabel:
  return rc;
}

cmDspRC_t cmDspRsrcWriteStringV( cmDspSysH_t h, const cmChar_t*  v, va_list vl )
{
  cmDsp_t*      p                = _cmDspHandleToPtr(h);
  cmJsonH_t     jsH              = cmDspSysPgmRsrcHandle(h);
  cmChar_t*     varStr           = NULL;
  cmChar_t*     pathStr          = NULL;
  cmJsonNode_t* parentObjNodePtr = NULL;
  cmDspRC_t     rc;

  if((rc = _cmDspWritePathAndParent(h, &pathStr, &varStr, &parentObjNodePtr,  vl )) != kOkDspRC )
    goto errLabel;

  if( cmJsonInsertOrReplacePairString( jsH, parentObjNodePtr, varStr, kStringTId, v ) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailDspRC,"Write 'string' resource value failed for path '%s' and variable '%s'",cmStringNullGuard(pathStr),cmStringNullGuard(varStr)); 

 errLabel:
  if( pathStr != NULL )
    cmLhFree(cmDspSysLHeap(h),pathStr);

  return rc;    
}

cmDspRC_t cmDspRsrcWriteString( cmDspSysH_t h, const cmChar_t* v, ... )
{
  va_list vl;
  va_start(vl,v);
  cmDspRC_t rc = cmDspRsrcWriteStringV(h,v,vl);
  va_end(vl);
  return rc;
}
