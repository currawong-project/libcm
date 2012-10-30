#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmAudioPort.h"
#include "cmAudioBuf.h"

enum
{
  kInIdx = 0;
  kOutIdx = 1;
  kIoCnt = 2;
};

typedef struct
{
  cmApAudioPacket_t pkt;        //
  char*             buf;        // buf[bufByteCnt]
  unsigned          bufByteCnt; //
} cmAudioBufDevBuf_t;

typedef struct
{
  cmAudioBufSubDev_t* subDevArray;
  unsigned            subDevCnt;
  unsigned            readyCnt;    
} cmAudioBufCycle_t;

typedef struct
{
  unsigned           chCnt;       // device channel count
  cmAudioBufCycle_t* cycleArray;  // subBufArray[ subBufCnt ] 
  unsigned           cycleCnt;    // set by cmAudioBufSetup().subBufCnt
  unsigned           faultCnt;    // count of overruns (input) underruns (output)
  unsigned           iCycleIdx;   // in: next buf to rcv on abUpdate() out: rcv on abGet()
  unsigned           oCycleIdx;   // in: next buf to send on abGet()   out: send on abUpdate()
  unsigned           fullCnt;     // in: cnt of bufs ready for abGet() out: cnt of bufs ready for abUpdate()  
} cmAudioBufIO_t;

typedef struct
{
  cmAudioBufIO_t ioArray[kIoCnt];
} cmAudioBufDev_t;

typedef struct
{
  cmErr_t          err;
  unsigned         devCnt;
  cmAudioBufDev_t* devArray;       // devArray[ devCnt ]
  unsigned         zeroBufByteCnt; // max of all possible buf sizes for all devices
  char*            zeroBuf;        // zeroBuf[ zeroBufByteCnt ]
  unsigned         updateCnt;
  unsigned         nextSeqId;
} cmAudioBuf_t;

cmAudioBuf_t _cmBa;


cmBaRC_t cmAudioBufInit( cmCtx_t* ctx, unsigned devCnt )
{
  cmBaRC_t rc = kOkBaRC;

  cmErrSetup(_cmBa.err,&ctx->rpt,"Audio Buf");

  _cmBa.devArray       = cmMemAllocZ(cmAudioBufDev_t,devCnt);
  _cmBa.devCnt         = devCnt;
  _cmBa.zeroBufByteCnt = 0;
  _cmBa.zeroBuf        = NULL;
  _cmBa.updateCnt      = 0;
  _cmBa.nextSeqId      = 0;
  return rc;
}

cmBaRC_t _cmAudioBufIoFree( cmAudioBufIO_t* iop )
{
  unsigned i;
  for(i=0; i<iop->subBufCnt; ++i)
    cmMemPtrFree(&iop->subBufArray[i].buf);

  cmMemPtrFree(&iop->subBufArray);

  return kOkBaRC;
} 

cmBaRC_t cmAudioBufFinal()
{
  cmBaRC_t rc = kOkBaRC;
  unsigned i,j;
  for(i=0; i<_cmBa.devCnt; ++i)
    for(j=0; j<kIoCnt; ++j)
      _cmAudioBufIoFree( _cmBa.devArray[i].ioArray + j );

  cmMemPtrFree(&_cmBa.devArray);
  _cmBa.devCnt = 0;
  cmMemPtrFree(&_cmBa.zeroBuf);
  _cmBa.zeroBufByteCnt = 0;
}


cmBaRC_t cmAudioBufSetup(
  unsigned devIdx,
  unsigned cycleCnt,
  unsigned inSubDevCnt,
  unsigned inChCnt,
  unsigned inFrameCnt,
  unsigned outChCnt,
  unsigned outFrameCnt,
  unsigned outSubDevCnt )
{
  assert(devIdx < _cmBa.devCnt );
  cmAudioBufDev_t* dp = _cmBa.devArray + devIdx;
  unsigned i,j;
  for(i=0; i<kIoCnt; ++i)
  {
    cmAudioBufIO_t* iop = dp->ioArray + i;

    _cmAudioBufIoFree(iop);
    
    iop->subBufArray = cmMemAllocZ(cmAudioBufSubBuf_t,subBufCnt);
    iop->subBufCnt   = 0;
    
    unsigned maxSampleWidthByteCnt = 4;

    // max size of any buffer arriving via cmAudioBufUpdate() for this device/direction
    unsigned bufByteCnt            = frameCnt*chCnt*maxSampleWidthByteCnt;

    // initialize the sub-buf array for this device/direction
    for(j=0; j<subBufCnt; ++j)
    {
      iop->subBufArray[j].buf        = cmMemAllocZ(char,bufByteCnt);
      iop->subBufArray[j].bufByteCnt = bufByteCnt;
    }

    // track the largest buffer size and make _cmBa.zeroBuf[] that size
    if( bufByteCnt > _cmBa.zeroBufByteCnt )
    {
      cmMemResizeZ(char,_cmBa.zeroBuf,bufByteCnt);
      _cmBa.zeroBufByteCnt = bufByteCnt;
    }
  }
}

// Called from the audio driver within incoming samples to store (inPktArray)
// and empty buffers (outPktArray) to fill with outgoin samples.
cmBaRC_t cmAudioBufUpdate(
  cmApAudioPacket_t* inPktArray,  ///< full audio packets from incoming audio (from ADC)
  unsigned           inPktCnt,    ///< count of incoming audio packets
  cmApAudioPacket_t* outPktArray, ///< empty audio packet for outgoing audio (to DAC)  
  unsigned           outPktCnt    ///< count of outgoing audio packets
                          )
{
  cmBaRC_t rc = kOkBaRC;

  ++_cmBa.updateCnt;

  unsigned i;
  for(i=0; i<inPktCnt && inPktArray!=NULL; ++i)
  {
    cmApAudioPacket_t* ipp = inPktArray + i;

    // get a pointer to the device/direction recd
    cmAudioBufIO_t*    iop = _cmBa.devArray[ ipp->devIdx ].ioArray + kInIdx;

    // check for overruns
    if( iop->fullCnt == iop->subBufCnt )
    {
      // input overrun
      ++ip->faultCnt;
      rc = cmErrMsg(&_cmBa.err,kBufOverrunBaRC,"Input buffer overrun.");
    }
    else
    {
      // get the next available sub-buf
      cmAudioBufSubBuf_t* sbp = iop->subBufArray + iop->iSubBufIdx;

      // store the packet header
      sbp->pkt = *ipp;
      sbp->audioBytesPtr = sbp->buf;

      // calc the count of bytes of incoming packet audio
      unsigned pktBufByteCnt = ipp->chCnt * ipp->audioFramesCnt * (bitsPerSample/8);
      assert( pktBufByteCnt <= sbp->bufByteCnt );

      // copy the samples into the buffer
      memcpy(sbp->buf,ipp->audioBytesPtr,pktBufByteCnt);
    
      // advance the input sub-buffer
      iop->iSubBufIdx = (iop->iSubBufIdx + 1) % iop->subBufCnt;

      iop->fullCnt+=1;
    }
  }

  for(i=0; i<outPktCnt && outPktArray!=NULL; ++i)
  {
    unsigned j;
    cmApAudioPacket_t* opp = outPktArray + i;

    // get a pointer to the device/direction recd
    cmAudioBufIO_t*    iop = _cmBa.devArray[ opp->devIdx ].ioArray + kOutIdx;
    
    // calc the number of requested bytes
    unsigned pktBufByteCnt = opp->chCnt * opp->audioFramesCnt * (bitsPerSample/8);

    // locate the oldest sub-buf which matches the pkt begin channel index
    cmAudioBufSubBuf_t* sbp  = NULL;
    cmAudioBufSubBuf_t* tsbp = iop->subBufArray;
    for(j=0; j<iop->subBufCnt; ++j,++tsbp)
      if( tsbp->fullFl && (tsbp->pkt.begChIdx == opp->pkt.begChIdx) )
        if( sbp==NULL || tsbp->seqId < sbp->seqId )
          sbp = tsbp;
    
    if( sbp == NULL )
    {
      ++opp->faultCnt;
      rc = cmErrMsg(&_cmBa.err,kBufOverrunBaRC,"Output buffer underrun.");

      // zero the pkt buffer
      memset(opp->audioBytePtr,0,pktBufByteCnt);
    }
    else
    {
      // the channel count assoc'd with a given begin channel index should always match
      assert( sbp->pkt.chCnt == opp->chCnt );

      // we guarantee that the sample word width will always match
      assert( sbp->pkt.bitsPerSample == opp->bitsPerSample);
      
      // we don't want to deal with the case where the requested number of samples
      // is less than the number available from a single stored buffer - this would
      // require sending out a partial buffer 
      assert( opp->audioFrameCnt >= sbp->pkt.audioFrameCnt );

      // calc the number of bytes to copy out
      unsigned bufByteCnt = sbp->pkt.chCnt * sbp->pkt.audioFramesCnt * (sbp->pkt.bitsPerSample/8);
      
      assert(bufByteCnt <= pktBufByteCnt );

      // copy out the requested samples
      memcpy(opp->audioBytesPtr,sbp->buf,cmMin(bufByteCnt,pktBufByteCnt));

      opp->audioFramesCnt = sbp->pkt.audioFramesCnt;

      // mark the sub-buffer as available
      sbp->fullFl = false;
      iop->fullCnt -= 1;;
    }
  }


  returnr c;
}

bool cmAudioBufIsDeviceReady( unsigned devIdx, unsigned flags )
{
  unsigned i;
  assert( devIdx < _cmBa.devCnt );
  
  // 
  if( cmIsFlag(flags,kInBaFl) )
    if( _cmBa.devArray[devIdx].ioArray[kInIdx].fullCnt==0)
      return false;

  if( cmIsFlag(flags,kOutBaFl) )
    if( _cmBa.devArray[devIdx].ioArray[kOutIdx].fullCnt == _cmBa.devArray[devIdx].ioArray[kOutIdx].subBufCnt )
      return false;
  
  return true;

}
  
cmBaRC_t cmAudioBufGet(     
  unsigned           devIdx, 
  unsigned           flags, 
  cmApAudioPacket_t* pktArray[], 
  unsigned           pktCnt )
{
}

cmBaRC_t cmAudioBufAdvance( unsigned devIdx, unsigned flags )
{
}
