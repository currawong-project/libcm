#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmPrefs.h"
#include "cmJson.h"
#include "cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmAudioSys.h"
#include "cmProcObj.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspSys.h"

cmDspRC_t cmDspClassSetup(
    cmDspClass_t*     classPtr,         
    cmDspCtx_t*       ctx,              
    const cmChar_t*   classLabel,       
    cmDspClassFunc_t  finalClassFunc,   
    cmDspConsFunc_t   allocFunc,    
    cmDspFunc_t       freeFunc,
    cmDspFunc_t       resetFunc,
    cmDspFunc_t       execFunc,
    cmDspFunc_t       recvFunc,
    cmDspStoreFunc_t  storeFunc,
    cmDspAttrSymFunc_t sysRecvFunc,
    const cmChar_t*   doc )
{
  classPtr->labelStr       = classLabel;
  classPtr->doc            = doc;

  classPtr->finalClassFunc = finalClassFunc;
  classPtr->allocFunc      = allocFunc;
  classPtr->freeFunc       = freeFunc;
  classPtr->resetFunc      = resetFunc;
  classPtr->execFunc       = execFunc;
  classPtr->recvFunc       = recvFunc;
  classPtr->storeFunc      = storeFunc;
  classPtr->sysRecvFunc    = sysRecvFunc;
  cmErrSetup( &classPtr->err, ctx->rpt, classLabel);

  return kOkDspRC;
}

// default DSP instance system recv function
cmDspRC_t  _cmDspInstSysRecvFunc(   cmDspCtx_t* ctx, struct cmDspInst_str* inst,  unsigned attrSymId, const cmDspValue_t* value )
{
  if( cmDsvIsSymbol(value) )
  {
    unsigned msgSymId = cmDsvSymbol(value);

    if( msgSymId == ctx->_disableSymId )
    {
      inst->flags = cmSetFlag(inst->flags,kDisableExecInstFl );
      //printf("%s disabled\n",cmStringNullGuard(cmDspInstLabel(ctx,inst)));
    }
    else
    {
      if( msgSymId == ctx->_enableSymId )
      {
        inst->flags = cmClrFlag(inst->flags,kDisableExecInstFl );
        //printf("%s enabled\n",cmStringNullGuard(cmDspInstLabel(ctx,inst)));
      }
    }
  }

  if( inst->classPtr->sysRecvFunc != NULL )
    return inst->classPtr->sysRecvFunc(ctx,inst,attrSymId,value);

  return kOkDspRC;
}

void* cmDspInstAllocate(
  cmDspCtx_t*          ctx, 
  cmDspClass_t*        classPtr, 
  const cmDspVarArg_t* argV, 
  unsigned             instByteCnt, 
  unsigned             instSymId, 
  unsigned             instId, 
  unsigned             storeSymId,
  unsigned             va_cnt, 
  va_list              vl )
{
  unsigned i;
  unsigned varCnt         = 0;
  unsigned varDataByteCnt = 0;
  unsigned va_idx         = 0; 
  unsigned reqArgCnt      = 0;

  // check for duplicate variable names
  for(i=0; argV[i].label != NULL; ++i)
    if( argV[i].label != NULL )
    {
      unsigned j = 0;
      for(j=0; argV[j].label != NULL; ++j)
        if( i != j && argV[j].label != NULL &&  strcmp(argV[i].label,argV[j].label) == 0 )
        {
          // two variables have the same name
          unsigned mask = kInDsvFl | kOutDsvFl;

          // we allow two variables to have the same name as long as they are not both inputs or outputs
          if( (argV[j].flags & mask) == (argV[i].flags & mask))
            cmDspClassErr(ctx,classPtr,kVarDuplicateDspRC,"The variable label '%s' is used by multiple variables.",argV[i].label);
        }
    }

  // determine the count and size of the instance variables
  for(i=0; argV[i].label != NULL; ++i,++varCnt)
  {
    unsigned rn    = argV[i].rn;
    unsigned flags = argV[i].flags;

    // audio buffer sample count is determined automatically
    if( cmAllFlags( argV[i].flags, kAudioBufDsvFl | kOutDsvFl ) )
    {
      rn    = ctx->ctx->ss->args.dspFramesPerCycle;
      flags = kSampleDsvFl | kMtxDsvFl;
    }

    // determine the space needed for matrices of known size
    if( cmIsFlag(flags,kMtxDsvFl) && (rn*argV[i].cn) )
    {
      unsigned n = cmDsvByteCount( flags, rn, argV[i].cn );
      varDataByteCnt += n;
    }

    // determine the number of required arguments
    reqArgCnt += cmIsFlag(flags,kReqArgDsvFl) ? 1 : 0;
  }


  unsigned     varRecdByteCnt = varCnt * sizeof(cmDspVar_t);
  unsigned     ttlByteCnt     = instByteCnt + varRecdByteCnt + varDataByteCnt;

  char*        p          = cmLHeapAllocZ(ctx->lhH, ttlByteCnt); // allocate the instance memory
  char*        ep         = p + ttlByteCnt;                      // record the end of the alloc'd memory
  cmDspInst_t* ip         = (cmDspInst_t*)p;                     // set the instance ptr
  cmDspVar_t*  varArray   = (cmDspVar_t*)(p + instByteCnt);      // set the instance variable recd array
  char*        varDataPtr = (char*)(varArray + varCnt);          // set the base of variable data buffer


  // setup the instance record
  ip->classPtr         = classPtr;
  ip->symId            = instSymId;
  ip->id               = instId;
  ip->flags            = kDisableExecInstFl;
  ip->presetGroupSymId = storeSymId;
  ip->varArray         = varArray;
  ip->varCnt           = varCnt;

  ip->freeFunc  = classPtr->freeFunc;
  ip->resetFunc = classPtr->resetFunc;
  ip->execFunc  = classPtr->execFunc;
  ip->recvFunc  = classPtr->recvFunc;
  ip->storeFunc = classPtr->storeFunc;
  ip->sysRecvFunc = _cmDspInstSysRecvFunc;

  if( reqArgCnt > va_cnt )
    cmDspInstErr(ctx,ip,kVarArgParseFailDspRC,"The class requires %i arguments but only %i were given.",reqArgCnt,va_cnt);

  // setup the variable records
  for(i=0; i<varCnt; ++i)
  {
    unsigned flags = argV[i].flags;
    unsigned rn    = argV[i].rn;

    // handle requests for audio output buffers
    if( cmAllFlags(argV[i].flags, kAudioBufDsvFl | kOutDsvFl ) )
    {
      // remove the audio buffer flag and reconstitute as a generic cmDspValue sample mtx type.
      //flags = cmClrFlag( argV[i].flags, kAudioBufDsvFl | kTypeDsvMask );

      flags = kSampleDsvFl | kMtxDsvFl;
      rn    = ctx->ctx->ss->args.dspFramesPerCycle;
    }

    // the constId must be the same as the var index
    assert( i == argV[i].constId );

    bool reqArgFl = cmIsFlag(argV[i].flags,kReqArgDsvFl);
    bool optArgFl = cmIsFlag(argV[i].flags,kOptArgDsvFl);

    // verify that the required arg. exists
    if( reqArgFl &&  i>va_cnt )
      cmDspInstErr(ctx,ip,kVarArgParseFailDspRC,"The required argument '%s' is missing",argV[i].label); 

    ip->varArray[i].flags       = argV[i].flags;    // store the original flag set as the var flags
    ip->varArray[i].constId     = argV[i].constId;
    ip->varArray[i].symId       = cmSymTblRegisterSymbol(ctx->stH,argV[i].label);
    ip->varArray[i].doc         = argV[i].doc;
    ip->varArray[i].value.flags = argV[i].flags & kTypeDsvMask;

    // if this is a required or optional constructor arg. then assign it as the
    // default value for this variable
    if( va_idx < va_cnt && (reqArgFl || optArgFl) )
    {
      cmDspValue_t* vp = &ip->varArray[i].value;

      switch(ip->varArray[i].value.flags)
      {
        case kBoolDsvFl:   cmDsvSetBool(   vp, va_arg(vl,int           )); break;
        case kCharDsvFl:   cmDsvSetChar(   vp, va_arg(vl,int           )); break;
        case kUCharDsvFl:  cmDsvSetUChar(  vp, va_arg(vl,int           )); break;
        case kShortDsvFl:  cmDsvSetShort(  vp, va_arg(vl,int           )); break;
        case kUShortDsvFl: cmDsvSetUShort( vp, va_arg(vl,int           )); break;
        case kLongDsvFl:   cmDsvSetLong(   vp, va_arg(vl,long          )); break;
        case kULongDsvFl:  cmDsvSetULong(  vp, va_arg(vl,unsigned long )); break;
        case kIntDsvFl:    cmDsvSetInt(    vp, va_arg(vl,int           )); break;
        case kUIntDsvFl:   cmDsvSetUInt(   vp, va_arg(vl,unsigned      )); break;
        case kFloatDsvFl:  cmDsvSetFloat(  vp, va_arg(vl,double        )); break;
        case kDoubleDsvFl: cmDsvSetDouble( vp, va_arg(vl,double        )); break;
        case kSampleDsvFl: cmDsvSetSample( vp, va_arg(vl,double        )); break;
        case kRealDsvFl:   cmDsvSetReal(   vp, va_arg(vl,double        )); break;
        case kStrzDsvFl:   cmDsvSetStrz(   vp, va_arg(vl,cmChar_t*     )); break;
        case kSymDsvFl:    cmDsvSetSymbol( vp, va_arg(vl,unsigned      )); break;
        default:
          cmDspInstErr(ctx,ip,kVarArgParseFailDspRC,"A var-args parse error occurred while parsing the variable '%s'.",argV[i].label);
      }

      // set the default value from the initial value
      cmDspValueSet(ctx, ip, i, vp, kSetDefaultDspFl );

      // store the fact that the default value was explicitely set.
      ip->varArray[i].flags = cmSetFlag(ip->varArray[i].flags,kDfltSetDsvFl);

      // track the number of va_list arg's read 
      ++va_idx;
    }

    // assign memory to the matrix types of known size
    if( cmIsFlag(flags,kMtxDsvFl) && (rn*argV[i].cn) )
    {
      cmDsvSetMtx(  &ip->varArray[i].value, flags, varDataPtr, rn, argV[i].cn );
      unsigned n = cmDsvByteCount( flags, rn, argV[i].cn );
      varDataPtr += n;
    }
  }

  assert( varDataPtr == ep );
  
  return p;
}

#ifdef OS_X
va_list  _cmDspParseArgV( cmDspVarArg_t* a, va_list vl )
#else
void  _cmDspParseArgV( cmDspVarArg_t* a, va_list vl )
#endif
{
  a->label   = va_arg(vl,const char*);
  a->constId = va_arg(vl,unsigned);
  a->rn      = va_arg(vl,unsigned);
  a->cn      = va_arg(vl,unsigned);
  a->flags   = va_arg(vl,unsigned);
  a->doc     = va_arg(vl,const char*);  
#ifdef OS_X
  return vl;
#endif
}

void* cmDspInstAllocateV(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned instByteCnt, unsigned instSymId, unsigned instId, unsigned storeSymId, unsigned va_cnt, va_list vl0, ... )
{
  va_list vl1,vl2;
  va_start(vl1,vl0);
  va_copy(vl2,vl1);

  unsigned argCnt = 0;
  int      repeatCnt;

  while( (repeatCnt = va_arg(vl1,int)) != 0 )
  {
    cmDspVarArg_t a;

    // do some value checking to possibly catch problems with the var_args format
    assert( repeatCnt > 0 && repeatCnt < 255 );
    
    argCnt += repeatCnt;

#ifdef OS_X
    vl1 = 
#endif
      _cmDspParseArgV(&a,vl1);
  }
  
  cmDspVarArg_t aa[ argCnt+1 ];
  unsigned j=0;
  while( (repeatCnt = va_arg(vl2,int)) != 0 )
  {
    cmDspVarArg_t a;
    
#ifdef OS_X
    vl2 = 
#endif
      _cmDspParseArgV(&a,vl2);
    
    cmDspArgSetupN(ctx,aa,argCnt,j,repeatCnt,a.label,a.constId,a.rn, a.cn, a.flags, a.doc ); 
    j += repeatCnt;
    
  }
  cmDspArgSetupNull(aa + argCnt);

  va_end(vl1);

  return cmDspInstAllocate(ctx,classPtr,aa,instByteCnt,instSymId,instId,storeSymId,va_cnt,vl0);
}

cmDspInstSymId_t* _cmDspInstFindAttrSymId( cmDspInst_t* inst, unsigned symId )
{
  cmDspInstSymId_t* ip = inst->symIdList;
  for(; ip != NULL; ip=ip->link)
    if( ip->symId == symId )
      return ip;
  return NULL;
}

bool        cmDspInstHasAttrSym(      cmDspInst_t* inst, unsigned attrSymId )
{ return _cmDspInstFindAttrSymId(inst,attrSymId) != NULL; }


cmDspRC_t   cmDspInstRegisterAttrSym( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned symId )
{
  cmDspInstSymId_t* ip;

  // if this symbol is already registered
  if((ip = _cmDspInstFindAttrSymId(inst,symId)) != NULL )
    return cmDspInstErr(ctx,inst,kOkDspRC,"The symbol '%s' is already registered with the instance.",cmStringNullGuard(cmSymTblLabel(ctx->stH,symId)));

  // try to find an unused symbol
  ip = inst->symIdList;
  for(; ip != NULL; ip=ip->link)
    if( ip->symId == cmInvalidId )
      break;

  // if no unused symbols were found then allocate a new one
  if( ip == NULL )
  {
    ip              = cmLhAllocZ(ctx->lhH, cmDspInstSymId_t, 1 );
    ip->link        = inst->symIdList;
    inst->symIdList = ip;

  }

  ip->symId       = symId;

  return kOkDspRC;
}

cmDspRC_t   cmDspInstRemoveAttrSym(   cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned symId )
{
  cmDspInstSymId_t* ip;

  // if this symbol is already registered
  if((ip = _cmDspInstFindAttrSymId(inst,symId)) == NULL )
    return cmDspInstErr(ctx,inst,kOkDspRC,"The symbol '%s' is could not be found to be removed from the instance.",cmStringNullGuard(cmSymTblLabel(ctx->stH,symId)));

  // mark a symbol recd as inactive by setting the symbol id to cmInalidId
  ip->symId = cmInvalidId;
  return kOkDspRC;
}


cmDspRC_t _cmDspClassErrV( cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned instId, cmDspRC_t rc, const cmChar_t* fmt, va_list vl )
{
  va_list vl2;
  va_copy(vl2,vl);
  unsigned n = vsnprintf(NULL,0,fmt,vl2)+1;

  cmChar_t buf[n+1];
  vsnprintf(buf,n,fmt,vl);
  if( instId == cmInvalidId )
    cmErrMsg( &classPtr->err, rc, "%s DSP Class:%s",buf, classPtr->labelStr);
  else
    cmErrMsg( &classPtr->err, rc, "%s DSP Class:%s Inst:%i",buf, classPtr->labelStr,instId);

  return rc;
}

cmDspRC_t   cmDspClassErr( cmDspCtx_t* ctx, cmDspClass_t* classPtr, cmDspRC_t rc, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc = _cmDspClassErrV(ctx,classPtr,cmInvalidId,rc,fmt,vl);
  va_end(vl);
  return rc;
}

cmDspRC_t cmDspInstErr( cmDspCtx_t* ctx, cmDspInst_t* inst, cmDspRC_t rc, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc = _cmDspClassErrV(ctx,inst->classPtr,inst->id,rc,fmt,vl);
  va_end(vl);
  return rc;
}

void   cmDspArgSetup(  
  cmDspCtx_t*     ctx, 
  cmDspVarArg_t*  arg, 
  const cmChar_t* labelPrefix, 
  unsigned        labelId, 
  unsigned        constId, 
  unsigned        rn, 
  unsigned        cn, 
  unsigned        flags, 
  const cmChar_t* docStr )
{
  unsigned        labelIdCharCnt = (unsigned)floor( labelId==0 ? 1.0 : log10(abs(labelId)) + 1.0 );
  int             labelCharCnt   = strlen(labelPrefix) + 1 + labelIdCharCnt + 1;
  cmChar_t        label[ labelCharCnt + 1 ];
  const cmChar_t* lp;

  if( labelId == cmInvalidId )
    lp = labelPrefix;
  else
  {
    lp = label;
    label[labelCharCnt]        = 0;
    snprintf(label,labelCharCnt,"%s-%i",labelPrefix,labelId);    
  }

  // use the symbol table to hold the label string
  arg->label   = cmSymTblLabel(ctx->stH,cmSymTblRegisterSymbol(ctx->stH,lp));
  arg->constId = constId;
  arg->rn      = rn;
  arg->cn      = cn;
  arg->flags   = flags;
  arg->doc     = cmLhAllocStr(ctx->lhH,docStr);;
}

unsigned cmDspArgCopy(   cmDspVarArg_t* argArray, unsigned argN, unsigned dstIdx, const cmDspVarArg_t* s, unsigned sn )
{
  assert( dstIdx + sn <= argN );
  unsigned i;
  for(i=0; i<sn; ++i)
    argArray[dstIdx+i] = s[i];
  return dstIdx + i;
}

unsigned   cmDspArgSetupN( 
  cmDspCtx_t*     ctx, 
  cmDspVarArg_t*  arg, 
  unsigned        argN, 
  unsigned        dstIdx,
  unsigned        cnt,
  const cmChar_t* labelPrefix, 
  unsigned        baseConstId, 
  unsigned        rn, 
  unsigned        cn, 
  unsigned        flags, 
  const cmChar_t* staticDocStr )
{
  assert( dstIdx + cnt <= argN );
  unsigned i;
  for(i=0; i<cnt; ++i)
    cmDspArgSetup(ctx,arg + dstIdx + i,labelPrefix, cnt==1 ? cmInvalidId : i, baseConstId + i, rn, cn, flags, staticDocStr );
  return dstIdx + i;
}

void   cmDspArgSetupNull( cmDspVarArg_t* arg )
{ memset(arg,0,sizeof(*arg)); }


const cmChar_t*   cmDspInstLabel( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  const char* noInstLbl = "<no_inst_lbl>";
  const char* lbl = noInstLbl;

  if( inst->symId == cmInvalidId )
    return lbl;

  if((lbl = cmSymTblLabel(ctx->stH,inst->symId)) == NULL )
    lbl = noInstLbl;

  return lbl;
}


cmDspRC_t _cmDspVarPtr( const cmDspInst_t* inst, unsigned varId, cmDspVar_t** retVarPtrPtr )
{
  if( varId >= inst->varCnt )
  {
    *retVarPtrPtr = NULL;
    return cmErrMsg(&inst->classPtr->err,kVarNotFoundDspRC,"The variable id %i is invalid for DSP instance '%s' (id:%i) .", varId, inst->classPtr->labelStr, inst->id );
  }

  assert( inst->varArray[varId].constId == varId );

  *retVarPtrPtr = inst->varArray + varId;

  return kOkDspRC;
}

cmDspRC_t _cmDspVarAndValuePtr( cmDspInst_t* inst, unsigned varId, cmDspVar_t** varPtrPtr, cmDspValue_t** valPtrPtr )
{
  cmDspRC_t   rc;

  if((rc = _cmDspVarPtr(inst,varId,varPtrPtr)) != kOkDspRC )
    *valPtrPtr = NULL;
  else
    *valPtrPtr = cmDsvValuePtr(&(*varPtrPtr)->value);

  return rc;
}

cmDspRC_t _cmDspVarValuePtr( cmDspInst_t* inst, unsigned varId, cmDspValue_t** retValPtrPtr )
{ 
  cmDspVar_t* varPtr = NULL;
  cmDspRC_t   rc;

  if((rc = _cmDspVarPtr(inst,varId,&varPtr)) != kOkDspRC )
    *retValPtrPtr = NULL;
  else
    *retValPtrPtr = cmDsvValuePtr(&varPtr->value);

  return rc;
}

cmDspRC_t _cmDspVarDefaultPtr( cmDspInst_t* inst, unsigned varId, cmDspValue_t** retValPtrPtr )
{ 
  cmDspVar_t* varPtr = NULL;
  cmDspRC_t   rc;

  if((rc = _cmDspVarPtr(inst,varId,&varPtr)) != kOkDspRC )
    *retValPtrPtr = NULL;
  else
    *retValPtrPtr = &varPtr->dflt;

  return rc;
}

void _cmDspSendEvt( cmDspCtx_t* ctx, cmDspInst_t* srcInstPtr, unsigned srcVarId, const cmDspVar_t* varPtr )
{
  cmDspEvt_t  e;
  cmDspCb_t*  cbp = varPtr->cbList;

  if( cbp == NULL )
    return;

  e.flags      = kDfltEvtDspFlags;  
  e.srcInstPtr = srcInstPtr;
  e.srcVarId   = srcVarId;
  e.valuePtr   = &varPtr->value;

  for(; cbp!=NULL; cbp=cbp->linkPtr)
  {
    e.dstVarId   = cbp->dstVarId;
    e.dstDataPtr = cbp->dstDataPtr;

    if( cbp->dstInstPtr->recvFunc!=NULL)
      cbp->dstInstPtr->recvFunc(ctx,cbp->dstInstPtr,&e);
  }
}

cmDspVar_t* cmDspVarSymbolToPtr( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varSymId, unsigned flags )
{
  unsigned i;
  for(i=0; i<inst->varCnt; ++i)
    if( inst->varArray[i].symId == varSymId )
    {
      if( flags==0 || cmAllFlags(inst->varArray[i].flags,flags) )
        return inst->varArray + i;
    }

  cmDspInstErr(ctx,inst,kVarNotFoundDspRC,"The variable '%s' could not be found in the instance '%s' (id:%i).",
    cmSymTblLabel( ctx->stH, varSymId), inst->classPtr->labelStr, inst->id );

  return NULL;
}

const cmDspVar_t* cmDspVarIdToCPtr( const cmDspInst_t* inst, unsigned varId )
{
  cmDspVar_t* varPtr;

  if( _cmDspVarPtr(inst, varId, &varPtr) != kOkDspRC )
    return NULL;

  return varPtr;
}

cmDspVar_t* cmDspVarIdToPtr( cmDspInst_t* inst, unsigned varId )
{
  cmDspVar_t* varPtr;

  if( _cmDspVarPtr(inst, varId, &varPtr) != kOkDspRC )
    return NULL;

  return varPtr;
}

const cmChar_t*   cmDspVarLabel( cmDspCtx_t* ctx, const cmDspInst_t* inst, unsigned varId )
{
  const cmDspVar_t* varPtr;
  if(( varPtr = cmDspVarIdToCPtr(inst,varId) ) == NULL )
    return NULL;

  return cmSymTblLabel( ctx->stH, varPtr->symId);
}

cmDspRC_t   cmDspOutputEvent( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId )
{
  cmDspRC_t rc;
  cmDspVar_t* varPtr;

  if((rc = _cmDspVarPtr(inst, varId, &varPtr )) != kOkDspRC )
    return rc;

  _cmDspSendEvt(ctx, inst, varId, varPtr );

  return rc;
}

cmDspRC_t   cmDspVarPresetWrite(  cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId )
{
  cmDspRC_t         rc;
  const cmDspVar_t* varPtr;

  if((varPtr = cmDspVarIdToCPtr(inst,varId)) == NULL )
    return cmDspInstErr(ctx,inst,kVarNotFoundDspRC,"Var with id %i not found during preset store.");

  if((rc = cmDspSysPresetWriteValue(ctx->dspH, varPtr->symId, &varPtr->value )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kSubSysFailDspRC,"Var preset storage failed on var:'%s'.",cmStringNullGuard(cmSymTblLabel(ctx->stH,varPtr->symId)));

  return rc;
}

cmDspRC_t   cmDspVarPresetRead( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId )
{
  cmDspRC_t   rc;
  cmDspVar_t* varPtr;
  cmDspValue_t value;

  if((varPtr = cmDspVarIdToPtr(inst,varId)) == NULL )
    return cmDspInstErr(ctx,inst,kVarNotFoundDspRC,"Var with id %i not found during preset recall.");

  if((rc = cmDspSysPresetReadValue(ctx->dspH, varPtr->symId, &value )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kSubSysFailDspRC,"Var preset recall failed on var:'%s'.",cmStringNullGuard(cmSymTblLabel(ctx->stH,varPtr->symId)));

  return cmDspValueSet(ctx,inst,varId,&value,0);
}

cmDspRC_t   cmDspVarPresetRdWr(     cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, bool storeFl )
{
  cmDspRC_t rc;

  if( storeFl )
    rc = cmDspVarPresetWrite(ctx,inst,varId);
  else
    rc = cmDspVarPresetRead(ctx,inst,varId);

  return rc;
}

double      cmDspSampleRate(      cmDspCtx_t* ctx )
{ return ctx->ctx->ss->args.srate; }

unsigned    cmDspSamplesPerCycle( cmDspCtx_t * ctx )
{ return ctx->ctx->ss->args.dspFramesPerCycle; }

unsigned cmDspTypeFlags(const cmDspInst_t* inst, unsigned varId )
{
  cmDspVar_t* varPtr;
 
  unsigned flags;
  if( _cmDspVarPtr(inst,varId,&varPtr) != kOkDspRC )
    return 0;

  cmDspValue_t* valPtr = cmDsvValuePtr(&varPtr->value);

  if( valPtr->flags != 0 )
    flags = valPtr->flags;
  else
    flags = varPtr->flags;

  return  flags & kTypeDsvMask;
}
   
bool     cmDspIsBool(   const cmDspInst_t* inst, unsigned varId )
{ return cmIsFlag(cmDspTypeFlags(inst,varId),kBoolDsvFl); }

bool     cmDspIsInt(    const cmDspInst_t* inst, unsigned varId )
{ return cmIsFlag(cmDspTypeFlags(inst,varId),kIntDsvFl); }

bool     cmDspIsUInt(   const cmDspInst_t* inst, unsigned varId )
{ return cmIsFlag(cmDspTypeFlags(inst,varId),kUIntDsvFl); }

bool     cmDspIsDouble( const cmDspInst_t* inst, unsigned varId )
{ return cmIsFlag(cmDspTypeFlags(inst,varId),kDoubleDsvFl); }

bool     cmDspIsStrz(   const cmDspInst_t* inst, unsigned varId )
{ return cmIsFlag(cmDspTypeFlags(inst,varId),kStrzDsvFl); }

bool     cmDspIsSymbol( const cmDspInst_t* inst, unsigned varId )
{ return cmIsFlag(cmDspTypeFlags(inst,varId),kSymDsvFl); }

bool     cmDspIsJson(   const cmDspInst_t* inst, unsigned varId )
{ return cmIsFlag(cmDspTypeFlags(inst,varId),kJsonDsvFl); }

bool  cmDspBool( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetBool(vp); 
}

int    cmDspInt( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetInt(vp); 
}


unsigned  cmDspUInt( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetUInt(vp); 
}

double cmDspDouble( cmDspInst_t* inst, unsigned varId )
{ 
  cmDspValue_t* vp;
  if(_cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetDouble(vp); 
}

cmSample_t cmDspSample( cmDspInst_t* inst, unsigned varId )
{ 
  cmDspValue_t* vp;
  if(_cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetSample(vp); 
}

cmReal_t cmDspReal( cmDspInst_t* inst, unsigned varId )
{ 
  cmDspValue_t* vp;
  if(_cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetReal(vp); 
}

const cmChar_t* cmDspStrcz( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return NULL;

  return cmDsvStrcz(vp); 
}

unsigned cmDspSymbol( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return cmInvalidId;

  return cmDsvSymbol(vp); 
}

cmJsonNode_t* cmDspJson( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return NULL;

  return cmDsvJson(vp); 
}

bool  cmDspDefaultBool( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarDefaultPtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetBool(vp); 
}

int    cmDspDefaultInt( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarDefaultPtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetInt(vp); 
}


unsigned  cmDspDefaultUInt( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarDefaultPtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetUInt(vp); 
}

double cmDspDefaultDouble( cmDspInst_t* inst, unsigned varId )
{ 
  cmDspValue_t* vp;
  if(_cmDspVarDefaultPtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetDouble(vp); 
}

cmSample_t cmDspDefaultSample( cmDspInst_t* inst, unsigned varId )
{ 
  cmDspValue_t* vp;
  if(_cmDspVarDefaultPtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetSample(vp); 
}

cmReal_t cmDspDefaultReal( cmDspInst_t* inst, unsigned varId )
{ 
  cmDspValue_t* vp;
  if(_cmDspVarDefaultPtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvGetReal(vp); 
}

const cmChar_t* cmDspDefaultStrcz( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarDefaultPtr(inst,varId,&vp) != kOkDspRC )
    return NULL;

  return cmDsvGetStrcz(vp); 
}

unsigned cmDspDefaultSymbol( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarDefaultPtr(inst,varId,&vp) != kOkDspRC )
    return cmInvalidId;

  return cmDsvGetSymbol(vp); 
}

cmJsonNode_t* cmDspDefaultJson( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarDefaultPtr(inst,varId,&vp) != kOkDspRC )
    return NULL;

  return cmDsvGetJson(vp); 
}


cmDspRC_t   cmDspValueSet( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmDspValue_t* svp, unsigned flags )
{
  cmDspRC_t     rc;
  cmDspVar_t*   varPtr;

  // get a pointer to the var recd
  if((rc = _cmDspVarPtr(inst,varId,&varPtr)) != kOkDspRC )
    return rc;

  // get a pointer to the target value.
  cmDspValue_t* dvp     = cmIsFlag(flags,kSetDefaultDspFl) ? &varPtr->dflt : cmDsvValuePtr(&varPtr->value);

  // store a pointer to the dst string memory in case we need to delete it later
  cmChar_t* rlsPtr  = cmAllFlags(dvp->flags,kStrzDsvFl | kDynDsvFl) ? dvp->u.z : NULL;

  // set the dest type to the type shared with the source
  // ( this assumes that the dst may have multiple types but the source has one type)
  unsigned typeFlags = (varPtr->flags & svp->flags) & kTypeDsvMask;

  // if svp->flags is set to kNullDsvFl then use just the dst type
  if( typeFlags == 0 )
    typeFlags = varPtr->flags & kTypeDsvMask;
  
  // convert the source to the var type
  switch(typeFlags)
  {
    case kBoolDsvFl:   
      cmDsvSetBool(  dvp, cmDsvGetBool(svp));   
      break;

    case kIntDsvFl:    
      cmDsvSetInt(   dvp, cmDsvGetInt(svp));    
      break;

    case kUIntDsvFl:   
      cmDsvSetUInt(  dvp, cmDsvGetUInt(svp));   
      break;

    case kDoubleDsvFl: 
      cmDsvSetDouble(dvp, cmDsvGetDouble(svp)); 
      break;

    case kSampleDsvFl:
      cmDsvSetSample(dvp, cmDsvGetSample(svp));
      break;

    case kRealDsvFl:
      cmDsvSetReal(dvp, cmDsvGetReal(svp));
      break;

    case kSymDsvFl:
      cmDsvSetSymbol(dvp, cmDsvGetSymbol(svp));
      break;

    case kJsonDsvFl:   
      cmDsvSetJson(  dvp, cmDsvJson(svp));      
      break;

    case kStrzDsvFl:   
      {
        const cmChar_t* sp;
        
        if( cmDsvIsSymbol(svp) )
          sp = cmSymTblLabel(ctx->stH,cmDsvSymbol(svp));
        else
          sp = cmDsvGetStrcz(svp);

        // don't copy over myself
        if( sp == dvp->u.z )
          rlsPtr = NULL;
        else
        {
          // if the source == NULL then set the dst to NULL
          // (NULL should always be a legitimate value)
          if( sp == NULL )
            cmDsvSetStrcz(dvp,sp);
          else
          {          
            // if the source should not be copied into internal memory
            if( cmIsFlag(flags,kNoAllocDspFl) )
              cmDsvSetStrcz(dvp,sp);
            else
            {
              // allocate memory to hold the new string
              unsigned  n  = strlen(sp)+1;
              cmChar_t* dp = cmLhResizeN(ctx->lhH,cmChar_t,rlsPtr,n);
              strncpy(dp,sp,n);
              cmDsvSetStrz(dvp, dp); 
              dvp->flags = cmSetFlag(dvp->flags,kDynDsvFl); 
              rlsPtr     = NULL;
            }
          }
        }
      }
      break;

    default:
      { assert(0); }
  }

  // if the dst contained a dynamically alloc'd string prior to being 
  // set with a new value - then release the memory used by the original
  // string here
  if( rlsPtr != NULL )
    cmLHeapFree(ctx->lhH,rlsPtr);

  // notify listeners of the change of value
  if( cmIsFlag(flags,kNoSendDspFl) == false )
    _cmDspSendEvt( ctx, inst, varId, varPtr );

  // notify the UI of the change of value
  if( cmIsFlag(varPtr->flags,kUiDsvFl) && cmIsFlag(flags,kNoUpdateUiDspFl)==false )
  {
    cmDspUiSendVar( ctx, inst, varPtr );
  }

  return rc;
}

cmDspRC_t   cmDspApplyDefault( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId )
{
  cmDspRC_t     rc;
  cmDspVar_t*   varPtr;

  // get a pointer to the var recd
  if((rc = _cmDspVarPtr(inst,varId,&varPtr)) != kOkDspRC )
    return rc;

  if( cmIsFlag(varPtr->flags,kDfltSetDsvFl) )
  {
    // unless the var has kSendDfltDsvFl set we don't transmit the default values
    unsigned flags = cmIsFlag(varPtr->flags,kSendDfltDsvFl) ? 0 : kNoSendDspFl;

    return cmDspValueSet(ctx, inst, varId, &varPtr->dflt, flags );
  }

  return kOkDspRC;
}

cmDspRC_t   cmDspApplyAllDefaults( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;
  for(i=0; i<inst->varCnt; ++i)
    if((rc = cmDspApplyDefault(ctx,inst,inst->varArray[i].constId)) != kOkDspRC )
      return rc;

  return rc;
}

cmDspRC_t   cmDspSetDefault( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmDspValue_t* valPtr )
{
  cmDspRC_t rc;
  cmDspVar_t* varPtr;

  // get a pointer to the var recd
  if((rc = _cmDspVarPtr(inst,varId,&varPtr)) != kOkDspRC )
      return rc;

  if(( cmDspValueSet( ctx, inst, varId, valPtr, kSetDefaultDspFl )) == kOkDspRC )
  {
    varPtr->flags = cmSetFlag(varPtr->flags, kDfltSetDsvFl);
  }

  return rc;
}

cmDspRC_t   _cmDspDefaultSet( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmDspValue_t* valPtr, bool nonInitValIsEqualFl )
{
  cmDspRC_t     rc;
  cmDspVar_t*   varPtr;

  // get a pointer to the var recd
  if((rc = _cmDspVarPtr(inst,varId,&varPtr)) != kOkDspRC )
    return rc;

  // if the dflt value was previuosly set and the default value is 
  // not equal to the non-intialized value for this var then the default value
  // has already been set to a legal value
  if( cmIsFlag(varPtr->flags,kDfltSetDsvFl) && nonInitValIsEqualFl==false )
    return kOkDspRC;
 
  return cmDspSetDefault(ctx,inst,varId,valPtr);
}

cmDspRC_t   cmDspSetDefaultBool( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, bool nonInitVal, bool val )
{
  cmDspValue_t v;
  double dv = cmDspDefaultBool(inst,varId);
  cmDsvSetBool(&v,val);
  return _cmDspDefaultSet(ctx,inst,varId,&v,dv==nonInitVal);
}

cmDspRC_t   cmDspSetDefaultInt( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, int nonInitVal, int val )
{
  cmDspValue_t v;
  int dv = cmDspDefaultInt(inst,varId);
  cmDsvSetInt(&v,val);
  return _cmDspDefaultSet(ctx,inst,varId,&v,dv==nonInitVal);
}

cmDspRC_t   cmDspSetDefaultUInt( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned nonInitVal, unsigned val )
{
  cmDspValue_t v;
  unsigned dv = cmDspDefaultUInt(inst,varId);
  cmDsvSetUInt(&v,val);
  return _cmDspDefaultSet(ctx,inst,varId,&v,dv==nonInitVal);
}

cmDspRC_t   cmDspSetDefaultDouble( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, double nonInitVal, double val )
{
  cmDspValue_t v;
  double dv = cmDspDefaultDouble(inst,varId);
  cmDsvSetDouble(&v,val);
  return _cmDspDefaultSet(ctx,inst,varId,&v,dv==nonInitVal);
}

cmDspRC_t   cmDspSetDefaultSample( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, cmSample_t nonInitVal, cmSample_t val )
{
  cmDspValue_t v;
  cmSample_t dv = cmDspDefaultSample(inst,varId);
  cmDsvSetSample(&v,val);
  return _cmDspDefaultSet(ctx,inst,varId,&v,dv==nonInitVal);
}

cmDspRC_t   cmDspSetDefaultReal( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, cmReal_t nonInitVal, cmReal_t val )
{
  cmDspValue_t v;
  cmReal_t dv = cmDspDefaultReal(inst,varId);
  cmDsvSetReal(&v,val);
  return _cmDspDefaultSet(ctx,inst,varId,&v,dv==nonInitVal);
}

cmDspRC_t   cmDspSetDefaultSymbol( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned val )
{
  cmDspValue_t v;
  unsigned dv = cmDspDefaultSymbol(inst,varId);
  cmDsvSetSymbol(&v,val);
  return _cmDspDefaultSet(ctx,inst,varId,&v,dv==cmInvalidId);
}

cmDspRC_t   cmDspSetDefaultStrcz( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmChar_t* nonInitVal, const cmChar_t* val )
{
  cmDspValue_t v;
  const cmChar_t* dv= cmDspDefaultStrcz(inst,varId);
  cmDsvSetStrcz(&v,val);
  bool fl = false;  // assume the default and non-init value are not equal

  // if the pointers are the same then the strings must match
  if( dv == nonInitVal )
    fl = true;
  else
    // if both strings exist - then test if they are equal
    if( dv!=NULL && nonInitVal!=NULL)
      fl = strcmp(dv,nonInitVal)==0;

  return _cmDspDefaultSet(ctx,inst,varId,&v,fl);
}

cmDspRC_t   cmDspSetDefaultJson( cmDspCtx_t*  ctx, cmDspInst_t* inst, unsigned varId, cmJsonNode_t*   nonInitVal, cmJsonNode_t*   val )
{
  cmDspValue_t v;
  cmJsonNode_t* dv= cmDspDefaultJson(inst,varId);
  cmDsvSetJson(&v,val);
  return _cmDspDefaultSet(ctx,inst,varId,&v,dv==nonInitVal);
}


cmDspRC_t  cmDspSetBool(    cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, bool val )
{ 
  cmDspValue_t v;
  cmDsvSetBool(&v,val);
  return cmDspValueSet(ctx, inst, varId, &v, 0);
}

cmDspRC_t  cmDspSetInt(    cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, int val )
{ 
  cmDspValue_t v;
  cmDsvSetInt(&v,val);
  return cmDspValueSet(ctx, inst, varId, &v, 0);
}

cmDspRC_t  cmDspSetUInt(    cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned val )
{ 
  cmDspValue_t v;
  cmDsvSetUInt(&v,val);
  return cmDspValueSet(ctx, inst, varId, &v, 0);
}

cmDspRC_t  cmDspSetDouble(    cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, double val )
{ 
  cmDspValue_t v;
  cmDsvSetDouble(&v,val);
  return cmDspValueSet(ctx, inst, varId, &v, 0);
}

cmDspRC_t  cmDspSetSample(    cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, cmSample_t val )
{ 
  cmDspValue_t v;
  cmDsvSetSample(&v,val);
  return cmDspValueSet(ctx, inst, varId, &v, 0);
}

cmDspRC_t  cmDspSetReal(    cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, cmReal_t val )
{ 
  cmDspValue_t v;
  cmDsvSetReal(&v,val);
  return cmDspValueSet(ctx, inst, varId, &v, 0);
}

cmDspRC_t  cmDspSetStrcz(    cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmChar_t* val )
{ 
  cmDspValue_t v;
  cmDsvSetStrcz(&v,val);
  return cmDspValueSet(ctx, inst, varId, &v, 0);
}

cmDspRC_t   cmDspSetSymbol( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned val )
{
  cmDspValue_t v;
  cmDsvSetSymbol(&v,val);
  return cmDspValueSet(ctx, inst, varId, &v, 0);
}

cmDspRC_t  cmDspSetJson(    cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId,  cmJsonNode_t* val )
{ 
  cmDspValue_t v;
  cmDsvSetJson(&v,val);
  return cmDspValueSet(ctx, inst, varId, &v, 0);
}

cmDspRC_t   cmDspSetEvent(         cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  unsigned flags = kUpdateUiDspFl;

  // We will assume that all values should be reflected to the UI - unless
  // this event is known to have been generated by the UI (evt->kUiDspFl is set)
  // and evt->kUiEchoDspFl is not set. 
  
  if( cmIsFlag(evt->flags,kUiDspFl)==true && cmIsFlag(evt->flags,kUiEchoDspFl)==false )
    flags = kNoUpdateUiDspFl;

  // Note: If the event target variable is not a UI variable
  // (i.e. cmDspVar_t.kUiDsvFl is not set) then calling cmDspValueSet()
  // with the kUpdateUiDspFl set has no effect.

  return cmDspValueSet(ctx,inst,evt->dstVarId, evt->valuePtr, flags); 
} 


cmDspRC_t   cmDspSetEventUi(         cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ return cmDspValueSet(ctx,inst,evt->dstVarId, evt->valuePtr,  kUpdateUiDspFl); } 

cmDspRC_t   cmDspSetEventUiId(       cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt, unsigned varId )
{
  cmDspEvt_t e;
  cmDspEvtCopy(&e,evt);
  e.dstVarId = varId;
  evt = &e;
  return cmDspSetEventUi(ctx,inst,evt);
}

unsigned    cmDspVarRows( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if(_cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvRows(vp);  
}

unsigned    cmDspVarCols( cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
  if( _cmDspVarValuePtr(inst,varId,&vp) != kOkDspRC )
    return 0;

  return cmDsvCols(vp);  
}

bool        cmDspIsAudioInputConnected( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* vp;
 
  if(  _cmDspVarValuePtr(inst,varId,&vp ) != kOkDspRC )
    return false;

  if( cmAllFlags(vp->flags, kSampleDsvFl | kMtxDsvFl ))
    return true; 

  return false;
}

cmDspRC_t  cmDspZeroAudioBuf( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId )
{
  cmDspValue_t* valPtr;
  cmDspVar_t*   varPtr;
  cmDspRC_t     rc;

  if((rc  = _cmDspVarAndValuePtr(inst,varId,&varPtr,&valPtr)) != kOkDspRC )
    return rc;

  bool fl0 = cmAllFlags(valPtr->flags, kSampleDsvFl | kMtxDsvFl );
  bool fl1 = cmIsFlag(varPtr->flags,kOutDsvFl);
  
  if( fl0==false || fl1==false )
    return cmDspInstErr(ctx,inst,kVarTypeErrDspRC,"The variable '%s' of DSP instance %s (id:%i) is not an output audio buffer.", cmDspVarLabel(ctx,inst,varId),inst->classPtr->labelStr,inst->id);

  memset(cmDsvSampleMtx(valPtr), 0, cmDsvEleCount(valPtr)*sizeof(cmSample_t) );

  return rc;
}

cmSample_t* cmDspAudioBuf( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned chIdx )
{
  cmDspValue_t* vp;
  cmDspVar_t*   varPtr;
  cmDspRC_t     rc;

  if((rc =  _cmDspVarAndValuePtr(inst,varId,&varPtr,&vp )) != kOkDspRC )
    return NULL;

  if( !cmAllFlags(vp->flags, kSampleDsvFl | kMtxDsvFl ))
  {

    // this is an unconnected audio input
    if( cmAllFlags(varPtr->flags,kInDsvFl | kAudioBufDsvFl ) )
      return NULL;

    cmDspInstErr(ctx,inst,kVarTypeErrDspRC,"The variable '%s' of DSP instance '%s' (id:%i) is not an audio buffer.", cmDspVarLabel(ctx,inst,varId),inst->classPtr->labelStr,inst->id);
    return NULL;
  }

  if( chIdx >= cmDsvCols(vp) )
  {
    cmDspInstErr(ctx,inst,kVarTypeErrDspRC,"Channel index %i of audio buffer variable '%s' in  DSP instance '%s' (id:%i) is outside the channel count (%i).", 
      chIdx,cmDspVarLabel(ctx,inst,varId),inst->classPtr->labelStr,inst->id,cmDsvCols(vp));
  }
  
  return cmDsvSampleMtx(vp) + (chIdx * cmDsvRows(vp));
}

unsigned cmDspAudioBufSmpCount( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, unsigned chIdx )
{
  cmDspValue_t* vp;
  cmDspRC_t     rc;

  if((rc  = _cmDspVarValuePtr(inst,varId,&vp)) != kOkDspRC )
    return 0;

  if( !cmAllFlags(vp->flags, kSampleDsvFl | kMtxDsvFl ))
  {
    cmDspInstErr(ctx,inst,kVarTypeErrDspRC,"The variable '%s' of DSP instance '%s' (id:%i) is not an audio buffer or is an unconnected input audio buffer.", cmDspVarLabel(ctx,inst,varId),inst->classPtr->labelStr,inst->id);
    return 0;
  }
  
  return cmDsvRows(vp);
}

cmDspRC_t   cmDspInstInstallCb( cmDspCtx_t* ctx, cmDspInst_t* srcInstPtr, unsigned srcVarSymId, cmDspInst_t* dstInstPtr, unsigned dstVarSymId, void* dstDataPtr)
{
  cmDspVar_t* srcVarPtr;
  cmDspVar_t* dstVarPtr;

  // get the src and dst var ptrs
  if((srcVarPtr = cmDspVarSymbolToPtr(ctx,srcInstPtr,srcVarSymId,kOutDsvFl)) == NULL || (dstVarPtr = cmDspVarSymbolToPtr(ctx,dstInstPtr,dstVarSymId,kInDsvFl)) == NULL )
    return cmDspInstErr(ctx,srcInstPtr,kInstCbInstallFailDspRC,"Callback installation failed for connection from '%s' (id:%i) var:'%s' to  '%s' (id:%i) var:'%s'.",
      srcInstPtr->classPtr->labelStr, srcInstPtr->id, cmSymTblLabel(ctx->stH,srcVarSymId), dstInstPtr->classPtr->labelStr, dstInstPtr->id, cmSymTblLabel(ctx->stH,dstVarSymId)  );

  // check for strict type conversion - this will fail for vars which allow multiple types
  // (e.g. numbers and strings)
  if( cmDsvCanConvertFlags(dstVarPtr->value.flags,srcVarPtr->value.flags) == false )
  {
    // if all the flags in the src are set in the dst then the match is ok
    // (this will cover dst types which admit multiple types)
    if( cmAllFlags(dstVarPtr->flags & kTypeDsvMask,srcVarPtr->flags & kTypeDsvMask) == false )
       cmDspInstErr(ctx,srcInstPtr,kOkDspRC,"Data types may not be compatible for connection to '%s' id:%i var:'%s' from '%s'.",dstInstPtr->classPtr->labelStr, dstInstPtr->id, cmSymTblLabel(ctx->stH,dstVarSymId), cmSymTblLabel(ctx->stH,srcVarSymId));
  }

  if( cmIsFlag(srcVarPtr->flags, kAudioBufDsvFl) )
    cmDspInstErr(ctx,srcInstPtr,kOkDspRC,"An audio source is being used as an event callback connection.");

  if( cmIsFlag(dstVarPtr->flags, kAudioBufDsvFl ) )
    cmDspInstErr(ctx,dstInstPtr,kOkDspRC,"An audio destination is the target of a callback connection.");

  cmDspCb_t* r = cmLhAllocZ( ctx->lhH, cmDspCb_t, 1 );
  r->srcVarSymId = srcVarSymId;
  r->dstInstPtr  = dstInstPtr;
  r->dstVarId    = dstVarPtr->constId;
  r->dstDataPtr  = dstDataPtr;
  r->linkPtr     = NULL;

  cmDspCb_t* cp = srcVarPtr->cbList;
  cmDspCb_t* pp = NULL;

  for(; cp != NULL; cp = cp->linkPtr )
    pp = cp;

  if( pp == NULL )
    srcVarPtr->cbList = r;
  else
    pp->linkPtr = r;

  return kOkDspRC;  
}

cmDspRC_t   cmDspInstallCb( cmDspCtx_t* ctx, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarLabel, cmDspInst_t* dstInstPtr, const cmChar_t* dstVarLabel, void* dstCbDataPtr)
{ 
  unsigned srcVarSymId,dstVarSymId;

  if( srcInstPtr == NULL && dstInstPtr == NULL )
    return cmErrMsg(&ctx->cmCtx->err,kInstNotFoundDspRC,"The callback installation was passed a NULL source and destination processor instance pointer.");

  if( srcInstPtr == NULL )
    return cmErrMsg(&ctx->cmCtx->err,kInstNotFoundDspRC,"The callback installation was passed a NULL source processor instance pointer for connection to '%s'.",cmStringNullGuard(dstInstPtr->classPtr->labelStr));

  if( dstInstPtr == NULL )
    return cmErrMsg(&ctx->cmCtx->err,kInstNotFoundDspRC,"The callback installation was passed a NULL destination processor instance pointer for connection from '%s'.",cmStringNullGuard(srcInstPtr->classPtr->labelStr));

  if((srcVarSymId = cmSymTblId( ctx->stH, srcVarLabel )) == cmInvalidId )
    return cmDspInstErr(ctx,srcInstPtr,kSrcVarNotFoundDspRC,"Connection failed. The source variable '%s' could not found in '%s' (id:%i) for registration by '%s' (id:%i).",
      srcVarLabel,srcInstPtr->classPtr->labelStr,srcInstPtr->id,dstInstPtr->classPtr->labelStr,dstInstPtr->id);

  if((dstVarSymId = cmSymTblId( ctx->stH, dstVarLabel )) == cmInvalidId )
    return cmDspInstErr(ctx,dstInstPtr,kSrcVarNotFoundDspRC,"Connection failed. The destination variable '%s' for the DSP instance '%s' (id:%i).",
      dstVarLabel,dstInstPtr->classPtr->labelStr,dstInstPtr->id);
  

  return cmDspInstInstallCb(ctx, srcInstPtr, srcVarSymId, dstInstPtr, dstVarSymId, dstCbDataPtr); 
}

cmDspRC_t   cmDspInstRemoveCb( cmDspCtx_t* ctx, cmDspInst_t* srcInstPtr, unsigned srcVarSymId, cmDspInst_t* dstInstPtr, unsigned dstVarId )
{
  cmDspVar_t* varPtr;

  if((varPtr = cmDspVarSymbolToPtr(ctx,srcInstPtr,srcVarSymId,kOutDsvFl)) == NULL )
    return cmDspInstErr(ctx,srcInstPtr,kInstCbInstallFailDspRC,"Callback removal failed for instance '%s' (id:%i).",
      srcInstPtr->classPtr->labelStr, srcInstPtr->id );


  cmDspCb_t* cp = varPtr->cbList;
  cmDspCb_t* pp = NULL;

  for(; cp != NULL; cp = cp->linkPtr )
  {
    if( cp->srcVarSymId == srcVarSymId && cp->dstInstPtr == dstInstPtr && cp->dstVarId == dstVarId )
      break;

    pp = cp;
  }

  if( pp == NULL )
    varPtr->cbList = cp->linkPtr;
  else
    pp->linkPtr = cp->linkPtr;

  return kOkDspRC;  

}

cmDspRC_t   cmDspRemoveCb( cmDspCtx_t* ctx, cmDspInst_t* srcInstPtr, const cmChar_t* srcVarLabel, cmDspInst_t* dstInstPtr, unsigned dstVarId)
{
  unsigned srcVarSymId;

  if((srcVarSymId = cmSymTblId( ctx->stH, srcVarLabel )) == cmInvalidId )
    return cmDspInstErr(ctx,srcInstPtr,kSrcVarNotFoundDspRC,"The variable '%s' could not found in '%s' (id:%i) do de-register for '%s' (id:%i).",
      srcVarLabel,srcInstPtr->classPtr->labelStr,srcInstPtr->id,dstInstPtr->classPtr->labelStr,dstInstPtr->id);

  return cmDspInstRemoveCb(ctx, srcInstPtr, srcVarSymId, dstInstPtr, dstVarId ); 
}

cmDspRC_t   cmDspInstVarSetFlags( cmDspCtx_t* ctx, cmDspInst_t* instPtr, unsigned varId, unsigned flags )
{
  cmDspVar_t* varPtr;

  if(_cmDspVarPtr(instPtr,  varId, &varPtr ) != kOkDspRC )
    return cmDspInstErr(ctx,instPtr,kInstSetFlagsFailDspRC,"Set flags failed for DSP instance %s (id:%i).",instPtr->classPtr->labelStr,instPtr->id);

  varPtr->flags = cmSetFlag(varPtr->flags,flags);

  return kOkDspRC;
}
