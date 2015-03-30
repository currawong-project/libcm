#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmJson.h"
#include "cmSymTbl.h"
#include "cmPrefs.h"
#include "cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmProcObj.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspUi.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmTime.h"
#include "cmAudioSys.h"

/*
// buffer layout is:
// [ cmDspUiHdr_t <data> ]
// The format of the <data> is determiend by hdr.value.
// Since hdr.value is the last field in the cmDspUiHdr_t record
// the data follows this value.  
cmDspRC_t cmDspMsgSend( 
  cmErr_t* err, 
  unsigned asSubIdx,
  unsigned msgTypeId,
  unsigned selId,
  unsigned flags,
  unsigned instId,
  unsigned instVarId,
  const cmDspValue_t* valPtr,
  cmRC_t (*sendFunc)(void* cbDataPtr, const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt ),
  void* cbDataPtr )
{
  unsigned bufByteCnt  = sizeof(cmDspUiHdr_t);
  unsigned dataByteCnt = 0;

  if( valPtr != NULL )
   dataByteCnt = cmDsvSerialDataByteCount(valPtr);

  bufByteCnt += dataByteCnt;

  char          buf[ bufByteCnt ];
  cmDspUiHdr_t* hdr = (cmDspUiHdr_t*)buf;

  hdr->asSubIdx  = asSubIdx;
  hdr->uiId      = msgTypeId;  // see kXXXSelAsId identifiers in cmAudioSys.h
  hdr->selId     = selId;      // if msgTypeId==kUiSelAsId then see kXXXDuId in cmDspUi.h
  hdr->flags     = flags;
  hdr->instId    = instId;
  hdr->instVarId = instVarId;

  if( valPtr == NULL )
    cmDsvSetNull(&hdr->value);
  else
  {
    // this function relies on the 'hdr.value' field being the last field in the 'hdr'.
    if( cmDsvSerialize( valPtr, &hdr->value, sizeof(cmDspValue_t) + dataByteCnt) != kOkDsvRC )
      return cmErrMsg(err,kSerializeUiMsgFailDspRC,"An attempt to serialize a UI msg failed.");
  }

  const void* vp = buf;
  if( sendFunc(cbDataPtr,&vp,&bufByteCnt,1) != cmOkRC )
    return cmErrMsg(err,kSendToHostFailDspRC,"An attempt to transmit a msg to the host failed.");
  

  return kOkDspRC;
  
}
*/

// This function is passed to cmDspMsgSend() by _cmDspUiMsg().
// It is used to coerce the callback data ptr 'cbDataPtr' into a
// cmAudioSysCtx_str* for calling back into cmDspSys.  It then translates
// the result code from a cmAsRC_t to a cmRC_t.
  cmMsgRC_t _cmDspUiDspToHostCb( void* cbDataPtr, unsigned msgByteCnt, const void* msg)
{
  struct cmAudioSysCtx_str* asCtx  = (struct cmAudioSysCtx_str*)cbDataPtr;
  cmMsgRC_t                 rc     = kOkMsgRC;

  if(asCtx->dspToHostFunc(asCtx, &msg, &msgByteCnt, 1) != kOkAsRC )
    rc = kSendFailMsgRC;

  return rc;
}

cmDspRC_t _cmDspUiMsg(cmDspCtx_t* ctx, unsigned msgTypeId, unsigned selId, unsigned flags, cmDspInst_t* inst, unsigned instVarId, const cmDspValue_t* valPtr )
{
  return cmMsgSend(
    &ctx->cmCtx->err,
    ctx->ctx->asSubIdx,
    msgTypeId,
    selId,
    flags,
    inst==NULL ? cmInvalidId : inst->id,
    instVarId,
    valPtr,
    _cmDspUiDspToHostCb,
    ctx->ctx );
}

// buffer layout is:
// [ cmDspUiHdr_t <data> ]
// The format of the <data> is determiend by hdr.value.
// Since hdr.value is the last field in the cmDspUiHdr_t record
// the data follows this value.  
/*
cmDspRC_t _cmDspUiMsg(cmDspCtx_t* ctx, unsigned msgTypeId, unsigned selId, unsigned flags, cmDspInst_t* inst, unsigned instVarId, const cmDspValue_t* valPtr )
{
  unsigned bufByteCnt  = sizeof(cmDspUiHdr_t);
  unsigned dataByteCnt = 0;

  if( valPtr != NULL )
   dataByteCnt = cmDsvSerialDataByteCount(valPtr);

  bufByteCnt += dataByteCnt;

  char          buf[ bufByteCnt ];
  cmDspUiHdr_t* hdr = (cmDspUiHdr_t*)buf;

  hdr->asSubIdx  = ctx->ctx->asSubIdx;
  hdr->uiId      = msgTypeId;  // see kXXXSelAsId identifiers in cmAudioSys.h
  hdr->selId     = selId;      // if msgTypeId==kUiSelAsId then see kXXXDuId in cmDspUi.h
  hdr->flags     = flags;
  hdr->instId    = inst==NULL ? cmInvalidId : inst->id;
  hdr->instVarId = instVarId;

  if( valPtr == NULL )
    cmDsvSetNull(&hdr->value);
  else
  {
    // this function relies on the 'hdr.value' field being the last field in the 'hdr'.
    if( cmDsvSerialize( valPtr, &hdr->value, sizeof(cmDspValue_t) + dataByteCnt) != kOkDsvRC )
    {
      if( inst == NULL )
        return cmErrMsg(&ctx->cmCtx->err,kSerializeUiMsgFailDspRC,"An attempt to serialize a UI msg failed.");
      else
        return cmDspInstErr(ctx,inst,kSerializeUiMsgFailDspRC,"An attempt to serialize a msg for '%s' failed.",inst->classPtr->labelStr);
    }
  }

  const void* vp = buf;
  if( ctx->ctx->dspToHostFunc(ctx->ctx,&vp,&bufByteCnt,1) != kOkAsRC )
  {
    if( inst == NULL )
      return cmErrMsg(&ctx->cmCtx->err,kSendToHostFailDspRC,"An attempt to transmit a msg to the host failed.");
    else
      return cmDspInstErr(ctx,inst,kSendToHostFailDspRC,"An attempt to transmit a msg to the host failed.");
  }

  return kOkDspRC;
}
*/


cmDspRC_t _cmDspUiUseInstSymbolAsLabel( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned lblVarId, const cmChar_t* ctlTypeStr )
{
  if( inst->symId != cmInvalidId )
  {
    cmDspValue_t v;

    // use the instance symbol as the default UI control label
    const cmChar_t* label = cmDspDefaultStrcz(inst,lblVarId);
    
    if( label == NULL )
      label = cmSymTblLabel(ctx->stH,inst->symId);

    if(label != NULL )
    {
      cmDsvSetStrcz(&v,label);
      if( _cmDspUiMsg(ctx, kUiSelAsId, kValueDuiId, 0, inst, lblVarId, &v ) != kOkDspRC )
        return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"%s label UI elment create failed.", ctlTypeStr);
    }
  }

  return kOkDspRC;
}

cmDspRC_t  cmDspSendValueToAudioSys( cmDspCtx_t* ctx, unsigned msgTypeId, unsigned selId, unsigned valId, const cmDspValue_t* valPtr )
{ return _cmDspUiMsg(ctx, msgTypeId, selId, 0, NULL, valId, valPtr ); }

cmDspRC_t   cmDspUiConsolePrint( cmDspCtx_t* ctx, cmChar_t* text )
{
  cmDspValue_t v;
  cmDsvSetStrz(&v,text);
  return _cmDspUiMsg( ctx, kUiSelAsId, kPrintDuiId, 0, NULL, cmInvalidId, &v );
}

cmDspRC_t   cmDspUiSendValue( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned varId, const cmDspValue_t* valPtr )
{ return _cmDspUiMsg(ctx, kUiSelAsId, kValueDuiId, 0, inst, varId, valPtr ); }

cmDspRC_t   cmDspUiSendVar( cmDspCtx_t* ctx, cmDspInst_t* inst, cmDspVar_t* var )
{ return _cmDspUiMsg(ctx, kUiSelAsId, kValueDuiId, 0, inst, var->constId, &var->value ); }

cmDspRC_t cmDspUiScalarCreate( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned ctlDuiId, unsigned minVarId, unsigned maxVarId, unsigned stpVarId, unsigned valVarId, unsigned lblVarId )
{
  cmDspRC_t    rc;
  unsigned     arr[] = { minVarId, maxVarId, stpVarId, valVarId, lblVarId  };
  cmDspValue_t v;
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);
  cmDsvSetUIntMtx(&v,arr,vn,1);

  // tell the UI to create a slider control
  if((rc = _cmDspUiMsg( ctx, kUiSelAsId, ctlDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"Scalar UI element create failed.");

  // use instance symbol as default label
  if((rc = _cmDspUiUseInstSymbolAsLabel(ctx, inst, lblVarId, "Scalar")) != kOkDspRC )
    return rc;

  // Set the kUiDsvFl on the variables used for the min/max/def/val for this scalar
  // Setting this flag will cause their values to be sent to the UI whenever they change.
  cmDspInstVarSetFlags( ctx, inst, minVarId, kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, maxVarId, kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, stpVarId, kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, valVarId, kUiDsvFl );
  return rc;
}

cmDspRC_t  cmDspUiTextCreate(   cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned valVarId, unsigned lblVarId )
{
  cmDspRC_t    rc;
  unsigned     arr[] = { valVarId, lblVarId  };
  cmDspValue_t v;
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);
  cmDsvSetUIntMtx(&v,arr,vn,1);

  // tell the UI to create a text control
  if((rc = _cmDspUiMsg( ctx, kUiSelAsId, kTextDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"Text UI element create failed.");

  // use instance symbol as default label
  if((rc = _cmDspUiUseInstSymbolAsLabel(ctx, inst, lblVarId, "Text")) != kOkDspRC )
    return rc;

  // Set the kUiDsvFl on the variables used for the min/max/def/val for this scalar
  // Setting this flag will cause their values to be sent to the UI whenever they change.
  cmDspInstVarSetFlags( ctx, inst, valVarId, kUiDsvFl );
  return rc;
}


cmDspRC_t   cmDspUiFnameCreate(  cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned valVarId, unsigned patVarId, unsigned dirVarId )
{
  cmDspRC_t    rc    = kOkDspRC;
  unsigned     arr[] = { valVarId, patVarId, dirVarId };
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);
  cmDspValue_t v;
  unsigned     i;
  cmDsvSetUIntMtx(&v,arr,vn,1);
  
  if((rc = _cmDspUiMsg(ctx, kUiSelAsId, kFnameDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"File/Directory chooser UI element create failed.");

  // set the kDsvUiFl in the variabes which update the UI
  for(i=0; i<vn; ++i)
    cmDspInstVarSetFlags( ctx, inst, arr[i], kUiDsvFl );  

  return rc;
}

cmDspRC_t cmDspUiMsgListCreate( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned height, unsigned listVarId, unsigned selVarId )
{
  cmDspRC_t    rc    = kOkDspRC;
  unsigned     arr[] = { height, selVarId, listVarId };
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);
  cmDspValue_t v;
  unsigned     i;
  cmDsvSetUIntMtx(&v,arr,vn,1);
  
  if((rc = _cmDspUiMsg(ctx, kUiSelAsId, kMsgListDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"Msg List UI element create failed.");

  // set the kDsvUiFl in the variabes which update the UI
  for(i=1; i<vn; ++i)
    cmDspInstVarSetFlags( ctx, inst, arr[i], kUiDsvFl );  

  return rc;
}

cmDspRC_t cmDspUiMeterCreate( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned minVarId, unsigned maxVarId, unsigned valVarId, unsigned lblVarId )
{
  cmDspRC_t    rc;
  unsigned     arr[] = { minVarId, maxVarId, valVarId, lblVarId  };
  cmDspValue_t v;
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);
  cmDsvSetUIntMtx(&v,arr,vn,1);

  // tell the UI to create a meter control
  if((rc = _cmDspUiMsg( ctx, kUiSelAsId, kMeterDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"Meter UI element create failed.");

  // use instance symbol as default label
  if((rc =  _cmDspUiUseInstSymbolAsLabel(ctx, inst, lblVarId, "Meter")) != kOkDspRC )
    return rc;

  // Set the kUiDsvFl on the variables used for the min/max/val for this meter
  // Setting this flag will cause their values to be sent to the UI whenever they change.
  cmDspInstVarSetFlags( ctx, inst, minVarId, kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, maxVarId, kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, valVarId, kUiDsvFl );

  return rc;
}

cmDspRC_t  cmDspUiButtonCreate( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned typeDuiId, unsigned outVarId, unsigned lblVarId )
{
  cmDspRC_t    rc;
  unsigned     arr[] = { outVarId, lblVarId  };
  cmDspValue_t v;
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);

  cmDsvSetUIntMtx(&v, arr, vn, 1);

  // tell the UI to create a button control
  if((rc = _cmDspUiMsg( ctx, kUiSelAsId, typeDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"Button UI element create failed.");

  // use instance symbol as default label
  if((rc =  _cmDspUiUseInstSymbolAsLabel(ctx, inst, lblVarId, "Button" )) != kOkDspRC )
    return rc;

  
  // Set the kUiDsvFl on the variables used for the val for this button
  // Setting this flag will cause their values to be sent to the UI whenever they change.
  cmDspInstVarSetFlags( ctx, inst, outVarId, kUiDsvFl );
  
  return rc;
}

cmDspRC_t  cmDspUiLabelCreate(  cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned lblVarId, unsigned alignVarId )
{
  cmDspRC_t    rc;
  unsigned     arr[] = { lblVarId, alignVarId  };
  cmDspValue_t v;
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);

  cmDsvSetUIntMtx(&v, arr, vn, 1);

  // tell the UI to create a button control
  if((rc = _cmDspUiMsg( ctx, kUiSelAsId, kLabelDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"Button UI element create failed.");

  // use instance symbol as default label
  if((rc =  _cmDspUiUseInstSymbolAsLabel(ctx, inst, lblVarId, "Label" )) != kOkDspRC )
    return rc;

  
  // Set the kUiDsvFl on the variables used for the val for this button
  // Setting this flag will cause their values to be sent to the UI whenever they change.
  cmDspInstVarSetFlags( ctx, inst, lblVarId,   kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, alignVarId, kUiDsvFl );

  return rc;
}

cmDspRC_t  cmDspUiTimeLineCreate(   cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned tlFileVarId, unsigned audPathVarId, unsigned selVarId, unsigned cursVarId )
{
  cmDspRC_t    rc;
  unsigned     arr[] = { tlFileVarId, audPathVarId, selVarId, cursVarId  };
  cmDspValue_t v;
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);
  cmDsvSetUIntMtx(&v,arr,vn,1);

  // tell the UI to create a time-line control
  if((rc = _cmDspUiMsg( ctx, kUiSelAsId, kTimeLineDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"Time Line UI element create failed.");

  // use instance symbol as default label
  //if((rc = _cmDspUiUseInstSymbolAsLabel(ctx, inst, lblVarId, "TimeLine")) != kOkDspRC )
  //  return rc;

  // Set the kUiDsvFl on the variables used for the min/max/def/val for this scalar
  // Setting this flag will cause their values to be sent to the UI whenever they change.
  cmDspInstVarSetFlags( ctx, inst, tlFileVarId,  kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, audPathVarId, kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, selVarId,     kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, cursVarId,     kUiDsvFl );
  return rc;
}

cmDspRC_t  cmDspUiScoreCreate( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned scFileVarId, unsigned selVarId, unsigned smpIdxVarId, unsigned pitchVarId, unsigned velVarId, unsigned locIdxVarId, unsigned evtIdxVarId, unsigned dynVarId, unsigned valTypeVarId, unsigned valueVarId )
{
  cmDspRC_t    rc;
  unsigned     arr[] = { scFileVarId, selVarId, smpIdxVarId, pitchVarId, velVarId, locIdxVarId, evtIdxVarId, dynVarId, valTypeVarId, valueVarId };
  cmDspValue_t v;
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);
  cmDsvSetUIntMtx(&v,arr,vn,1);

  // tell the UI to create a score control
  if((rc = _cmDspUiMsg( ctx, kUiSelAsId, kScoreDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"Score UI element create failed.");

  // Set the kUiDsvFl on the variables used for the min/max/def/val for this scalar
  // Setting this flag will cause their values to be sent to the UI whenever they change.
  cmDspInstVarSetFlags( ctx, inst, scFileVarId,  kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, selVarId,     kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, smpIdxVarId,  kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, pitchVarId,   kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, velVarId,     kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, locIdxVarId,  kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, evtIdxVarId,  kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, dynVarId,     kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, valTypeVarId, kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, valueVarId,   kUiDsvFl );

  return rc;
}

cmDspRC_t  cmDspUiTakeSeqBldrCreate( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned fileNameVarId, unsigned ptrVarId, unsigned selVarId, unsigned refreshVarId )
{
  cmDspRC_t    rc;
  unsigned     arr[] = { fileNameVarId, ptrVarId, selVarId, refreshVarId  };
  cmDspValue_t v;
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);
  cmDsvSetUIntMtx(&v,arr,vn,1);

  // tell the UI to create a sequence builder  control
  if((rc = _cmDspUiMsg( ctx, kUiSelAsId, kTakeSeqBldrDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"Take Sequence Builder UI element create failed.");

  // Setting kUiDsvFl will cause variable values to be sent to the UI whenever they change.
  cmDspInstVarSetFlags( ctx, inst, fileNameVarId,  kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, ptrVarId,       kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, selVarId,       kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, refreshVarId,   kUiDsvFl );

  return rc;
}


cmDspRC_t  cmDspUiTakeSeqRendCreate( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned ptrVarId, unsigned refreshVarId, unsigned selVarId )
{
  cmDspRC_t    rc;
  unsigned     arr[] = { ptrVarId, refreshVarId, selVarId  };
  cmDspValue_t v;
  unsigned     vn    = sizeof(arr)/sizeof(arr[0]);
  cmDsvSetUIntMtx(&v,arr,vn,1);

  // tell the UI to create a sequence render  control
  if((rc = _cmDspUiMsg( ctx, kUiSelAsId, kTakeSeqRendDuiId, 0, inst, cmInvalidId, &v )) != kOkDspRC )
    return cmDspInstErr(ctx,inst,kUiEleCreateFailDspRC,"Take Sequence Render UI element create failed.");

  // Setting kUiDsvFl will cause variable values to be sent to the UI whenever they change.
  cmDspInstVarSetFlags( ctx, inst, ptrVarId,       kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, refreshVarId,   kUiDsvFl );
  cmDspInstVarSetFlags( ctx, inst, selVarId,       kUiDsvFl );

  return rc;
}


cmDspRC_t  cmDspUiNewColumn(    cmDspCtx_t* ctx, unsigned colW )
{
  cmDspRC_t rc = kOkDspRC;
  cmDspValue_t val;
  cmDsvSetUInt(&val,colW);
  if((rc = _cmDspUiMsg(ctx, kUiSelAsId, kColumnDuiId, 0, NULL, cmInvalidId, &val )) != kOkDspRC )
    return cmErrMsg(&ctx->cmCtx->err,kUiEleCreateFailDspRC,"New UI column request failed.");

  return rc;
}

cmDspRC_t  cmDspUiInsertHorzBorder(    cmDspCtx_t* ctx )
{
  cmDspRC_t rc = kOkDspRC;
  if((rc = _cmDspUiMsg(ctx, kUiSelAsId, kHBorderDuiId, 0, NULL, cmInvalidId, NULL )) != kOkDspRC )
    return cmErrMsg(&ctx->cmCtx->err,kUiEleCreateFailDspRC,"Horizontal border request failed.");

  return rc;
}

cmDspRC_t  cmDspUiNewPage(          cmDspCtx_t* ctx, const cmChar_t* title )
{
  cmDspRC_t rc = kOkDspRC;
  cmDspValue_t v;
  
  cmDsvSetStrcz(&v,title==NULL ? "Controls" : title );

  if((rc = _cmDspUiMsg(ctx, kUiSelAsId, kPageDuiId, 0, NULL, cmInvalidId, &v )) != kOkDspRC )
    return cmErrMsg(&ctx->cmCtx->err,kUiEleCreateFailDspRC,"New page request failed.");

  return rc;
}
