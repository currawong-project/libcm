//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmJson.h"
#include "dsp/cmDspValue.h"
#include "cmMsgProtocol.h"

// buffer layout is:
// [ cmDspUiHdr_t <data> ]
// The format of the <data> is determiend by hdr.value.
// Since hdr.value is the last field in the cmDspUiHdr_t record
// the data follows this value.  
cmMsgRC_t cmMsgSend( 
  cmErr_t* err, 
  unsigned asSubIdx,
  unsigned msgTypeId,
  unsigned selId,
  unsigned flags,
  unsigned instId,
  unsigned instVarId,
  const cmDspValue_t* valPtr,
  cmMsgSendFuncPtr_t sendFunc,
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
      return cmErrMsg(err,kSerializeFailMsgRC,"An attempt to serialize a UI msg failed.");
  }

  const void* vp = buf;
  if( sendFunc(cbDataPtr,bufByteCnt,vp) != cmOkRC )
    return cmErrMsg(err,kSendFailMsgRC,"An attempt to transmit a msg to the host failed.");
  

  return kOkMsgRC;
  
}

// Return the unsigned value at the specified byte offset into the msg buffer.
cmMsgRC_t  _cmMsgPeekAtUInt( const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned offset, unsigned* retValPtr )
{
  unsigned i,j,k;
  
  for(k=0,i=0; i<segCnt; ++i)
    for(j=0; j<msgByteCntArray[i]; ++j,++k)
      if( k == offset )
        break;
  
  if( i == segCnt )
    return kDecodeFailMsgRC;

  *retValPtr = *((unsigned*)((char*)(msgArray[i]) + j));
  return kOkMsgRC;    
}

cmMsgRC_t cmMsgPeekAsSubIndex( const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr )
{ 
  cmDspUiHdr_t h;
  unsigned offset = ((char*)(&h.asSubIdx)) - ((char*)&h);
  return _cmMsgPeekAtUInt(msgArray,msgByteCntArray,segCnt,offset,retValPtr);
}

cmMsgRC_t cmMsgPeekMsgTypeId(  const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr )
{ 
  cmDspUiHdr_t h;
  unsigned offset = ((char*)(&h.uiId)) - ((char*)&h);
  return _cmMsgPeekAtUInt(msgArray,msgByteCntArray,segCnt,offset,retValPtr);
}

cmMsgRC_t cmMsgPeekSelId(      const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr )
{ 
  cmDspUiHdr_t h;
  unsigned offset = ((char*)(&h.selId)) - ((char*)&h);
  return _cmMsgPeekAtUInt(msgArray,msgByteCntArray,segCnt,offset,retValPtr);
}

cmMsgRC_t cmMsgPeekFlags(      const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr )
{ 
  cmDspUiHdr_t h;
  unsigned offset = ((char*)(&h.flags)) - ((char*)&h);
  return _cmMsgPeekAtUInt(msgArray,msgByteCntArray,segCnt,offset,retValPtr);
}

cmMsgRC_t cmMsgPeekInstId(     const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr )
{ 
  cmDspUiHdr_t h;
  unsigned offset = ((char*)(&h.instId)) - ((char*)&h);
  return _cmMsgPeekAtUInt(msgArray,msgByteCntArray,segCnt,offset,retValPtr);
}

cmMsgRC_t cmMsgPeekInstVarId(  const void* msgArray[], unsigned msgByteCntArray[], unsigned segCnt, unsigned* retValPtr )
{ 
  cmDspUiHdr_t h;
  unsigned offset = ((char*)(&h.instVarId)) - ((char*)&h);
  return _cmMsgPeekAtUInt(msgArray,msgByteCntArray,segCnt,offset,retValPtr);
}

