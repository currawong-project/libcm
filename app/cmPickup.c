#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmMidi.h"
#include "cmAudioFile.h"
#include "cmFile.h"
#include "cmFileSys.h"
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmVectOpsTemplateMain.h"
#include "cmProc.h"
#include "cmProc2.h"
#include "cmProc3.h"
#include "cmPickup.h"
#include "cmAudLabelFile.h"

enum
{
  kRmsPuId,
  kMedPuId,
  kDifPuId,
  kAvgPuId,
  kOnsPuId,
  kFltPuId,
  kSupPuId,
  kAtkPuId,
  kRlsPuId,
  kSegPuId,
  kPuCnt
};

typedef struct
{
  cmErr_t   err;
  unsigned  chCnt;
  cmPuCh_t* chArray;
  cmCtx_t   ctx;    // stored ctx used by cmAudLabelFileAllocOpen()
  
  // cmProc objects
  cmCtx*             ctxp;
  cmAudioFileRd*     afrp;
  cmShiftBuf*        sbp;
  cmGateDetect2*     gdp;
  cmBinMtxFile_t*    mfp;

  const cmChar_t*    inAudFn;
  const cmChar_t*    inLabelFn;
  const cmChar_t*    outMtx0Fn;
  const cmChar_t*    outMtx1Fn;
  const cmChar_t*    outAudFn;
  cmReal_t           hopMs;

  cmGateDetectParams gd0Args;
  cmGateDetectParams gd1Args;
} cmPu_t;

cmPuH_t cmPuNullHandle = cmSTATIC_NULL_HANDLE;

cmPu_t* _cmPuHandleToPtr( cmPuH_t h )
{
  cmPu_t* p = (cmPu_t*)h.h;
  assert(p!=NULL);
  return p;
}

cmPuRC_t cmPuAlloc( cmCtx_t* ctx, cmPuH_t* hp )
{
  cmPuRC_t rc;
  if((rc = cmPuFree(hp)) != kOkPuRC )
    return rc;

  cmPu_t* p = cmMemAllocZ(cmPu_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"Pickup");
 
  p->ctx = *ctx;
  hp->h = p;

  return rc;
}


cmPuRC_t cmPuFree( cmPuH_t* hp )
{
  if( hp == NULL || cmPuIsValid(*hp) == false )
    return kOkPuRC;

  cmPu_t* p = _cmPuHandleToPtr(*hp);

  cmMemPtrFree(&p->chArray);
  cmMemPtrFree(&p);
  hp->h = NULL;
  return kOkPuRC;
}

bool     cmPuIsValid( cmPuH_t h )
{ return h.h != NULL; }
  

cmPuRC_t _cmPuReadLabelsAndCreateArray( cmPu_t* p, const cmChar_t* labelFn, cmReal_t srate )
{
  cmPuRC_t rc = kOkPuRC;
  cmAlfH_t h  = cmAlfNullHandle;
  unsigned i;

  if( cmAudLabelFileAllocOpen(&p->ctx, &h, labelFn) != kOkAlfRC )
    return cmErrMsg(&p->err,kAlfFileFailPuRC,"The auto-tune audio label file open failed on '%s'",cmStringNullGuard(labelFn));
  
  if((p->chCnt   = cmAudLabelFileCount(h)) == 0 )
  {
    rc = cmErrMsg(&p->err,kAlfFileFailPuRC,"The auto-tune audio label file '%s' does not contain any segment labels.",cmStringNullGuard(labelFn));
    goto errLabel;
  }

  p->chArray = cmMemResizeZ(cmPuCh_t,p->chArray,p->chCnt);

  for(i=0; i<p->chCnt; ++i)
  {
    const cmAlfLabel_t* lp;
    if(( lp = cmAudLabelFileLabel(h,i)) == NULL )
    {
      rc = cmErrMsg(&p->err,kAlfFileFailPuRC,"The auto-tune label in '%s' at row %i could not be read.",cmStringNullGuard(labelFn),i+1);
      goto errLabel;
    }

    p->chArray[i].begSmpIdx = floor(srate * lp->begSecs);
    p->chArray[i].endSmpIdx = p->chArray[i].begSmpIdx; // default the segment to have 0 length.
    
  }

 errLabel:
  if( cmAudLabelFileFree(&h) != kOkAlfRC )
    rc = cmErrMsg(&p->err,kAlfFileFailPuRC,"The auto-tune label file close failed.");

  return rc;
}

cmPuRC_t      _cmPuWriteMtxFile(cmPu_t* p, bool segFl )
{
  cmPuRC_t rc = kOkPuRC;

  cmReal_t outV[ kPuCnt ];
  outV[ kRmsPuId ] = p->gdp->rms;
  outV[ kMedPuId ] = p->gdp->med;
  outV[ kDifPuId ] = p->gdp->dif;
  outV[ kAvgPuId ] = p->gdp->avg;
  outV[ kOnsPuId ] = p->gdp->ons;
  outV[ kFltPuId ] = p->gdp->flt;
  outV[ kSupPuId ] = p->gdp->sup;
  outV[ kAtkPuId ] = p->gdp->onFl;
  outV[ kRlsPuId ] = p->gdp->offFl;
  outV[ kSegPuId ] = segFl;
      
  // write the output file - plot with cmGateDetectPlot.m
  if( cmBinMtxFileExecR(p->mfp,outV,kPuCnt) != cmOkRC )
    rc = cmErrMsg(&p->err,kProcFailPuRC,"Matrix file write failed.");
   
  return rc;
}

void _cmPuCalcGains( cmPu_t* p )
{
  unsigned i;
  cmReal_t avg = 0;

  if( p->chCnt == 0 )
    return;

  for(i=0; i<p->chCnt; ++i)
    avg += p->chArray[i].gateMaxAvg;

  avg /= p->chCnt;

  for(i=0; i<p->chCnt; ++i)
  {
    cmReal_t d = p->chArray[i].gateMaxAvg==0 ? 1.0 : p->chArray[i].gateMaxAvg;
    p->chArray[i].gain = avg / d;
  }
}

cmPuCh_t* _cmPuIncrCh( cmPu_t* p, cmPuCh_t* chp, unsigned* segSmpIdxPtr )
{
  if( *segSmpIdxPtr != p->chArray[0].begSmpIdx )
    ++chp;           

  if( chp >= p->chArray + p->chCnt )
    return NULL;

  if( chp+1 == p->chArray + p->chCnt )
    *segSmpIdxPtr = p->afrp->info.frameCnt;
  else
    *segSmpIdxPtr = (chp+1)->begSmpIdx;

  return chp;
}

cmPuRC_t _cmPuCalcRerunGateDetectors( 
  cmPu_t*                   p,
  const cmChar_t*           outMtxFn,
  const cmChar_t*           outAudFn,
  const cmGateDetectParams* gdArgs, 
  unsigned                  procSmpCnt, 
  unsigned                  wndSmpCnt, 
  unsigned                  hopSmpCnt )
{
  cmPuRC_t       rc         = kOkPuRC;
  cmAudioFileWr* afwp       = NULL;
  unsigned       outChCnt   = 1;
  unsigned       outChIdx   = 0;
  unsigned       bitsPerSmp = 16;
  unsigned       smpIdx     = 0;
  cmSample_t*    smpV       = NULL;
  

  // rewind the audio file reader
  if( cmAudioFileRdSeek(p->afrp,0) != cmOkRC )
  {
    cmErrMsg(&p->err,kProcFailPuRC,"Audio file seek failed.");
    goto errLabel;
  }

  // reset the shift buffer
  if( cmShiftBufInit( p->sbp, procSmpCnt, wndSmpCnt, hopSmpCnt ) != cmOkRC )
  {
    cmErrMsg(&p->err,kProcFailPuRC,"Shift buffer reset failed.");
    goto errLabel;
  }

  // reset the gate detector
  if( cmGateDetectInit2( p->gdp, procSmpCnt, gdArgs ) != cmOkRC )
  {
    cmErrMsg(&p->err,kProcFailPuRC,"Gate detector reset failed.");
    goto errLabel;
  }

  // create an new matrix output file
  if( cmBinMtxFileInit( p->mfp, outMtxFn ) != cmOkRC )
  {
    rc = cmErrMsg(&p->err,kProcFailPuRC,"Output matrix file '%s' initialization failed.",cmStringNullGuard(outMtxFn));
    goto errLabel;
  }

  // create an audio output file
  if( (afwp = cmAudioFileWrAlloc(p->ctxp, NULL, procSmpCnt, outAudFn, p->afrp->info.srate, outChCnt,  bitsPerSmp )) == NULL )
  {
    rc = cmErrMsg(&p->err,kProcFailPuRC,"Output audio file '%s' initialization failed.",cmStringNullGuard(outAudFn));
    goto errLabel;
  }
  
  smpV = cmMemAllocZ(cmSample_t,procSmpCnt);

  cmPuCh_t* chp      = p->chArray;
  unsigned segSmpIdx = chp->begSmpIdx; 
  bool      segFl    = false;

  // for each procSmpCnt samples
  for(; cmAudioFileRdRead(p->afrp) != cmEofRC; smpIdx += procSmpCnt )
  {
    // apply auto-gain to the audio vector
    cmVOS_MultVVS(smpV,p->afrp->outN,p->afrp->outV,chp->gain);

    // is this a segment boundary
    if( smpIdx+procSmpCnt >= p->afrp->info.frameCnt || (smpIdx <= segSmpIdx && segSmpIdx < smpIdx + procSmpCnt) )
    {
      segFl = true;

      if((chp = _cmPuIncrCh(p,chp, &segSmpIdx )) == NULL )
        break;
    }

    // shift the new samples into the shift buffer
    while(cmShiftBufExec(p->sbp,smpV,p->afrp->outN))
    {
      
      // update the gate detector
      cmGateDetectExec2(p->gdp,p->sbp->outV,p->sbp->outN);

      if( _cmPuWriteMtxFile(p,segFl) != kOkPuRC )
        goto errLabel;

      segFl =false;
    }

    // write the audio output file
    if( cmAudioFileWrExec(afwp, outChIdx,smpV,p->afrp->outN ) != cmOkRC )
    {
      cmErrMsg(&p->err,kProcFailPuRC,"A write failed to the audio output file '%s'.",outAudFn);
      goto errLabel;
    }

  }

 errLabel:  

  cmMemPtrFree(&smpV);
  
  if( cmAudioFileWrFree(&afwp) != cmOkRC )
  {
    rc = cmErrMsg(&p->err,kProcFailPuRC,"Output audio file '%s' close failed.",cmStringNullGuard(outAudFn));
    goto errLabel;
  }

  return rc;
}

cmPuRC_t cmPuAutoGainCfg( 
  cmPuH_t                   h,
  const cmChar_t*           audioFn,
  const cmChar_t*           labelFn,
  const cmChar_t*           outMtx0Fn,
  const cmChar_t*           outMtx1Fn,
  const cmChar_t*           outAudFn,
  unsigned                  procSmpCnt,
  cmReal_t                  hopMs,
  const cmGateDetectParams* gd0Args,
  const cmGateDetectParams* gd1Args )
{
  cmPuRC_t        rc;
  cmPu_t*         p       = _cmPuHandleToPtr(h);
  int             smpIdx  = 0;
  int             chIdx   = 0;
  const cmReal_t  rmsMax  = 1.0;
  cmReal_t        minRms  = rmsMax;
  cmReal_t        gateMax = 0;
  cmReal_t        gateSum = 0;
  unsigned        gateCnt = 0;
  cmPuCh_t*       chp     = NULL;
  bool            segFl   = false;
  unsigned        segSmpIdx = cmInvalidIdx;

  // create a cmProc context
  if((p->ctxp = cmCtxAlloc(NULL, p->err.rpt, cmLHeapNullHandle, cmSymTblNullHandle )) == NULL )
  {
    rc = cmErrMsg(&p->err,kProcFailPuRC,"Proc context create failed.");
    goto errLabel;
  }

  // create a cmProc audio file reader
  if((p->afrp =  cmAudioFileRdAlloc(p->ctxp, NULL, procSmpCnt, audioFn, chIdx, 0, cmInvalidIdx)) == NULL )
  {
    rc = cmErrMsg(&p->err,kProcFailPuRC,"Audio file reader creation failed on '%s'.",cmStringNullGuard(audioFn));
    goto errLabel;
  }

  // given the sample rate calculate the hop and window size in samples
  unsigned       hopSmpCnt = floor(p->afrp->info.srate * hopMs / 1000);
  unsigned       wndSmpCnt = hopSmpCnt * gd0Args->medCnt;

  // create a shift buffer to maintain the RMS window for the gate detector
  if((p->sbp = cmShiftBufAlloc(p->ctxp,NULL,procSmpCnt,wndSmpCnt,hopSmpCnt )) == NULL )
  {
    rc = cmErrMsg(&p->err,kProcFailPuRC,"Shift buffer create failed.");
    goto errLabel;
  }

  // create a gate detector
  if((p->gdp = cmGateDetectAlloc2(p->ctxp,NULL,procSmpCnt,gd0Args)) == NULL )
  {
    rc = cmErrMsg(&p->err,kProcFailPuRC,"Gate detect create failed.");
    goto errLabel;
  }

  // create an output file to hold the results of the gate detector
  if( (p->mfp = cmBinMtxFileAlloc(p->ctxp,NULL,outMtx0Fn)) == NULL )
  {
    rc = cmErrMsg(&p->err,kProcFailPuRC,"Binary matrix file create failed.");
    goto errLabel;
  }

  // read the label file and create p->chArray
  if((rc = _cmPuReadLabelsAndCreateArray(p, labelFn, p->afrp->info.srate)) != kOkPuRC )
    goto errLabel;

  chp       = p->chArray;
  segSmpIdx = chp->begSmpIdx;

  // for each procSmpCnt samples
  for(; cmAudioFileRdRead(p->afrp) != cmEofRC; smpIdx += procSmpCnt )
  {
    // if this audio frame marks a segment beginning or 
    // the end-of-audio-file will occur on the next frame
    if( smpIdx+procSmpCnt >= p->afrp->info.frameCnt || (smpIdx <= segSmpIdx && segSmpIdx < smpIdx + procSmpCnt) )
    {
      segFl = true;

      // if no ending offset was located then update the gate sum
      if( gateMax != 0 )
      {
        gateSum += gateMax;
        gateCnt += 1;
        gateMax  = 0;
      } 
      
      // calc the avg max RMS value for this segment
      chp->gateMaxAvg = gateCnt == 0 ? 0.0 : gateSum / gateCnt;
      gateCnt = 0;
      gateSum = 0;
      gateMax = 0;

      minRms = rmsMax; // force the segment end to be after the segment beginning

      if((chp = _cmPuIncrCh(p,chp, &segSmpIdx )) == NULL )
        break;
    }

    // shift the new samples into the shift buffer
    while(cmShiftBufExec(p->sbp,p->afrp->outV,p->afrp->outN))
    {
      // update the gate detector
      cmGateDetectExec2(p->gdp,p->sbp->outV,p->sbp->outN);

      // write the output matrix file
      if( _cmPuWriteMtxFile(p,segFl) != kOkPuRC )
        goto errLabel;

      segFl = false;

      // if this frame is an RMS minimum or onset or offset
      // then select it as a possible segment end.
      // Note that for onsets this will effectively force the end to
      // come after the onset because the onset will not be an energy minimum
      // relative to subsequent frames.
      if( p->gdp->rms < minRms || p->gdp->onFl || p->gdp->offFl )
      {
        minRms         = p->gdp->rms;
        chp->endSmpIdx = smpIdx;

        // count onsets
        if( p->gdp->onFl )
          ++chp->onCnt;

        // count offsets
        if( p->gdp->offFl )
        {
          ++chp->offCnt;
          
          // update the gate sum and count
          gateSum += gateMax;
          gateCnt += 1;
          gateMax  = 0;
        }
      }

      // track the max RMS value during this gate
      if( p->gdp->gateFl && p->gdp->rms > gateMax )
        gateMax = p->gdp->rms;
      
    }
  }

  // calculate the channel gains
  if( rc == kOkPuRC )
    _cmPuCalcGains(p);

  rc = _cmPuCalcRerunGateDetectors(p,outMtx1Fn,outAudFn,gd1Args,procSmpCnt,wndSmpCnt,hopSmpCnt);

  p->gd0Args = *gd0Args;
  p->gd1Args = *gd1Args;

 errLabel:
  if( p->mfp != NULL )
    cmBinMtxFileFree(&p->mfp);

  if( p->gdp != NULL )
    cmGateDetectFree2(&p->gdp);

  if( p->sbp != NULL )
    cmShiftBufFree(&p->sbp);
  
  if( p->afrp != NULL )
    cmAudioFileRdFree(&p->afrp);

  if( p->ctxp != NULL )
    cmCtxFree(&p->ctxp);

  return rc;
}

cmPuRC_t cmPuAutoGainExec( cmPuH_t h, const cmChar_t* fileDir, unsigned procSmpCnt )
{
  cmPu_t*         p         = _cmPuHandleToPtr(h);
  const cmChar_t* inAudFn   = cmFsMakeFn(fileDir, p->inAudFn,   NULL, NULL );
  const cmChar_t* inLabelFn = cmFsMakeFn(fileDir, p->inLabelFn, NULL, NULL );
  const cmChar_t* outMtx0Fn = cmFsMakeFn(fileDir, p->outMtx0Fn, NULL, NULL );
  const cmChar_t* outMtx1Fn = cmFsMakeFn(fileDir, p->outMtx1Fn, NULL, NULL );
  const cmChar_t* outAudFn  = cmFsMakeFn(fileDir, p->outAudFn,  NULL, NULL );

  cmPuRC_t rc = cmPuAutoGainCfg(h,inAudFn,inLabelFn,outMtx0Fn,outMtx1Fn,outAudFn,procSmpCnt,p->hopMs,&p->gd0Args,&p->gd1Args);

  cmFsFreeFn(outAudFn);
  cmFsFreeFn(outMtx1Fn);
  cmFsFreeFn(outMtx0Fn);
  cmFsFreeFn(inLabelFn);
  cmFsFreeFn(inAudFn);

  return rc;
}

cmPuRC_t cmPuAutoGainCfgFromJson( 
  cmPuH_t             h,
  const cmChar_t*     cfgDir,
  cmJsonH_t           jsH,
  cmJsonNode_t*       onp,
  unsigned            procSmpCnt )
{
  cmPuRC_t rc;
  if((rc = cmPuReadJson(h,jsH,onp)) != kOkPuRC )
    return rc;
  

  return cmPuAutoGainExec(h,cfgDir,procSmpCnt);
}


void cmPuReport( cmPuH_t h, cmRpt_t* rpt )
{
  cmPu_t*         p       = _cmPuHandleToPtr(h);
  
  unsigned i;
  for(i=0; i<p->chCnt; ++i)
  {
    const cmPuCh_t* chp = p->chArray + i;
    cmRptPrintf(rpt,"beg:%i end:%i on:%i off:%i max:%f gain:%f\n",
      chp->begSmpIdx, chp->endSmpIdx, chp->onCnt, chp->offCnt, chp->gateMaxAvg, chp->gain );
  }
}

cmPuRC_t _cmPuJsonGainRead( cmPu_t* p, cmJsonH_t jsH, cmJsonNode_t* onp, const cmChar_t* label )
{
  cmPuRC_t rc = kOkPuRC;
  cmJsonNode_t* arp;
  unsigned arrCnt = 0;
  cmPuCh_t* arr = NULL;

  // locate the JSON 'gain' array
  if(( arp = cmJsonFindValue(jsH,label,onp,kArrayTId)) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"Unable to locate the JSON array node %s.",cmStringNullGuard(label));
    goto errLabel;
  }

  // get the count of elements in the 'gain' array
  arrCnt = cmJsonChildCount(arp);

  if( arrCnt > 0 )
  {
    arr = cmMemAllocZ(cmPuCh_t,arrCnt);

    unsigned i;
    for(i=0; i<arrCnt; ++i)
    {
      if( i<p->chCnt )
        arr[i] = p->chArray[i];

      if( cmJsonRealValue( cmJsonArrayElement(arp,i), &arr[i].gain ) != kOkJsRC )
      {
        rc = cmErrMsg(&p->err,kJsonFailPuRC,"An error occurred while accessing a JSON 'gain' element.");
        goto errLabel;
      }

    }
  }


 errLabel:

  if( rc != kOkPuRC )
  {
    cmMemPtrFree(&arr);
    arrCnt = 0;
  }

  cmMemPtrFree(&p->chArray);
  p->chArray = arr;
  p->chCnt   = arrCnt;

  return rc;
}

cmPuRC_t _cmPuJsonGdParmsRead( cmPu_t* p, cmJsonH_t jsH, cmJsonNode_t* onp, const cmChar_t* label, cmGateDetectParams* gdParms )
{
  cmPuRC_t rc = kOkPuRC;
  cmJsonNode_t* gdp;
  const char* errLabelPtr;
  if(( gdp = cmJsonFindValue(jsH,label,onp,kObjectTId)) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"Unable to locate the JSON object node %s.",cmStringNullGuard(label));
    goto errLabel;
  }

  if( cmJsonMemberValues(gdp, &errLabelPtr, 
      "medCnt",      kIntTId,  &gdParms->medCnt,
      "avgCnt",      kIntTId,  &gdParms->avgCnt,
      "suprCnt",     kIntTId,  &gdParms->suprCnt,
      "offCnt",      kIntTId,  &gdParms->offCnt,
      "suprCoeff",   kRealTId, &gdParms->suprCoeff,
      "onThreshDb",  kRealTId, &gdParms->onThreshDb,
      "offThreshDb", kRealTId, &gdParms->offThreshDb,
      NULL ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"Gate detect parameter restore failed for '%s'.",cmStringNullGuard(label));
    goto errLabel;
  }

 errLabel:
  return rc;
}

cmPuRC_t _cmPuJsonCfgParmsRead(cmPu_t* p, cmJsonH_t jsH, cmJsonNode_t* onp)
{
  cmPuRC_t      rc = kOkPuRC;
  cmJsonNode_t* gdp = onp;
  const char*   errLabelPtr;

 
  //if(( gdp = cmJsonFindValue(jsH,label,onp,kObjectTId)) == NULL )
  //{
  //  rc = cmErrMsg(&p->err,kJsonFailPuRC,"Unable to locate the JSON object node %s.",cmStringNullGuard(label));
  //  goto errLabel;
  // }

  if( cmJsonMemberValues(gdp, &errLabelPtr, 
      "audioFn",   kStringTId, &p->inAudFn,
      "labelFn", kStringTId, &p->inLabelFn,
      "outMtx0Fn", kStringTId, &p->outMtx0Fn,
      "outMtx1Fn", kStringTId, &p->outMtx1Fn,
      "outAudFn",  kStringTId, &p->outAudFn,
      "hopMs",     kRealTId,   &p->hopMs,
      NULL ) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"Autotune cfg parameter read failed.");
    goto errLabel;
  }

 errLabel:
  return rc;

}

cmPuRC_t cmPuReadJson( cmPuH_t h, cmJsonH_t jsH, cmJsonNode_t* onp )
{
  cmPuRC_t      rc = kOkPuRC;
  cmPu_t*       p  = _cmPuHandleToPtr(h);
  cmJsonNode_t* atp;

  if(( atp = cmJsonFindValue(jsH,"cfg",onp,kObjectTId)) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"The JSON 'autotune' object was not found.");
    goto errLabel;
  }

  if((rc = _cmPuJsonCfgParmsRead(p,jsH,atp)) != kOkPuRC )
    goto errLabel;

  if((rc = _cmPuJsonGdParmsRead(p,jsH,atp,"gdParms0",&p->gd0Args)) != kOkPuRC )
    goto errLabel;

  if((rc = _cmPuJsonGdParmsRead(p,jsH,atp,"gdParms1",&p->gd1Args)) != kOkPuRC )
    goto errLabel;

  if((rc = _cmPuJsonGainRead(p,jsH,atp,"gain")) != kOkPuRC )
    goto errLabel;

 errLabel:
  return rc;

}

unsigned        cmPuChannelCount(   cmPuH_t h )
{ 
  cmPu_t* p = _cmPuHandleToPtr(h);
  return p->chCnt;
}

const cmPuCh_t* cmPuChannel( cmPuH_t h, unsigned chIdx )
{
  cmPu_t* p = _cmPuHandleToPtr(h);
  assert( chIdx < p->chCnt );
  return p->chArray + chIdx;
}
    


cmPuRC_t _cmPuJsonSetInt( cmPu_t* p, cmJsonH_t jsH, cmJsonNode_t* gdp, const cmChar_t* label, int val )
{
  cmPuRC_t rc = kOkPuRC;

  if( cmJsonReplacePairInt(jsH, gdp, label, kIntTId | kRealTId, val ) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"Error setting integer JSON field: %s.",cmStringNullGuard(label));
  return rc;
}

cmPuRC_t _cmPuJsonSetReal( cmPu_t* p, cmJsonH_t jsH, cmJsonNode_t* gdp, const cmChar_t* label, cmReal_t val )
{
  cmPuRC_t rc = kOkPuRC;

  if( cmJsonReplacePairReal(jsH, gdp, label, kIntTId | kRealTId, val ) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"Error setting real JSON field: %s.",cmStringNullGuard(label));
  return rc;
}

cmPuRC_t _cmPuJsonGdParmsUpdate( cmPu_t* p, cmJsonH_t jsH, cmJsonNode_t* onp, const cmChar_t* label, const cmGateDetectParams* gdParms )
{
  cmPuRC_t rc = kOkPuRC;
  cmJsonNode_t* gdp;

  if(( gdp = cmJsonFindValue(jsH,label,onp,kObjectTId)) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"Unable to locate the JSON object node %s.",cmStringNullGuard(label));
    goto errLabel;
  }

  _cmPuJsonSetInt(  p, jsH, gdp, "medCnt",     gdParms->medCnt);
  _cmPuJsonSetInt(  p, jsH, gdp, "avgCnt",     gdParms->avgCnt);
  _cmPuJsonSetInt(  p, jsH, gdp, "suprCnt",    gdParms->suprCnt);
  _cmPuJsonSetInt(  p, jsH, gdp, "offCnt",     gdParms->offCnt);
  _cmPuJsonSetReal( p, jsH, gdp, "suprCoeff",  gdParms->suprCoeff);
  _cmPuJsonSetReal( p, jsH, gdp, "onThreshDb", gdParms->onThreshDb);
  _cmPuJsonSetReal( p, jsH, gdp, "offThresDb", gdParms->offThreshDb);

  rc = cmErrLastRC(&p->err);
 errLabel:
  return rc;

}

cmPuRC_t _cmPuJsonGainUpdate( cmPu_t* p, cmJsonH_t jsH, cmJsonNode_t* onp, const cmChar_t* label )
{
  cmPuRC_t rc = kOkPuRC;
  cmJsonNode_t* arp;
  unsigned i;

  // locate the JSON 'gain' array
  if(( arp = cmJsonFindValue(jsH,label,onp,kArrayTId)) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"Unable to locate the JSON array node %s.",cmStringNullGuard(label));
    goto errLabel;
  }

  // get the count of elements in the 'gain' array
  unsigned arrCnt = cmJsonChildCount(arp);

  // update the existing 'gain' array elmements from p->chArray[]
  for(i=0; i<arrCnt && i<p->chCnt; ++i)
  {
    if(cmJsonSetReal( jsH, cmJsonArrayElement(arp,i), p->chArray[i].gain ) != kOkPuRC )
    {
      rc = cmErrMsg(&p->err,kJsonFailPuRC,"Set JSON 'gain' array elment failed.");
      goto errLabel;
    }
  }    

  // create new elements if the array was not long enough
  for(; i<p->chCnt; ++i)
    if( cmJsonCreateReal(jsH,arp,p->chArray[i].gain) != kOkJsRC )
    {
      rc = cmErrMsg(&p->err,kJsonFailPuRC,"JSON 'gain' element create failed.");
      goto errLabel;
    }

  // remove elements if the array begain with extra elements.
  if( arrCnt > p->chCnt )
  {
    if( cmJsonRemoveNode( jsH, cmJsonArrayElement(arp,i), true ) != kOkJsRC )
    {
      rc = cmErrMsg(&p->err,kJsonFailPuRC,"JSON 'gain' element removal failed.");
      goto errLabel;
    }
  }

 errLabel:
  return rc;
  
}

cmPuRC_t cmPuWriteJson( cmPuH_t h, cmJsonH_t jsH, cmJsonNode_t* onp )
{
  cmPuRC_t      rc = kOkPuRC;
  cmPu_t*       p  = _cmPuHandleToPtr(h);
  cmJsonNode_t* atp;

  if(( atp = cmJsonFindValue(jsH,"autoTune",onp,kObjectTId)) == NULL )
  {
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"The JSON 'autotune' object was not found.");
    goto errLabel;
  }

  if((rc = _cmPuJsonGdParmsUpdate(p,jsH,atp,"gdParms0",&p->gd0Args)) != kOkPuRC )
    goto errLabel;

  if((rc = _cmPuJsonGdParmsUpdate(p,jsH,atp,"gdParms1",&p->gd1Args)) != kOkPuRC )
    goto errLabel;

  if((rc = _cmPuJsonGainUpdate(p,jsH,atp,"gain")) != kOkPuRC )
    goto errLabel;
    
 errLabel:

  return rc;
}

cmPuRC_t cmPuWriteJsonFile( cmPuH_t h, const cmChar_t* jsonFn  )
{
  cmPuRC_t  rc  = kOkPuRC;
  cmJsonH_t jsH = cmJsonNullHandle;
  cmPu_t*   p   = _cmPuHandleToPtr(h);

  // initialize a JSON tree from 'jsonFn'.
  if( cmJsonInitializeFromFile(&jsH,jsonFn,&p->ctx) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"JSON file initialization failed on '%s'.",cmStringNullGuard(jsonFn));
    goto errLabel;
  }

  // update the 'autoTune' object in the JSON tree
  if((rc = cmPuWriteJson(h,jsH,cmJsonRoot(jsH))) != kOkPuRC )
    goto errLabel;

  // write the JSON tree back to 'jsonFn'.
  if( cmJsonWrite(jsH,cmJsonRoot(jsH),jsonFn) != kOkJsRC )
  {
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"JSON file save failed on '%s'.",cmStringNullGuard(jsonFn));
    goto errLabel;
  }

 errLabel:
  // release the JSON tree
  if( cmJsonFinalize(&jsH) != kOkJsRC )
    rc = cmErrMsg(&p->err,kJsonFailPuRC,"JSON file finalization failed on '%s'.",cmStringNullGuard(jsonFn));

  return rc;
}

void cmPuTest(cmCtx_t* ctx)
{
  cmPuH_t            h          = cmPuNullHandle;
  cmGateDetectParams gd0Args;
  cmGateDetectParams gd1Args;
  const cmChar_t*    audioFn    = "/home/kevin/media/audio/gate_detect/gate_detect0.aif";
  const cmChar_t*    labelFn    = "/home/kevin/media/audio/gate_detect/gate_detect0_labels.txt";
  const cmChar_t*    outMtx0Fn  = "/home/kevin/media/audio/gate_detect/gd0.mtx";
  const cmChar_t*    outMtx1Fn  = "/home/kevin/media/audio/gate_detect/gd1.mtx";
  const cmChar_t*    outAudFn   = "/home/kevin/media/audio/gate_detect/gd_gain0.aif";
  const cmChar_t*    jsonFn     = "/home/kevin/src/kc/src/data/rsrc1.txt";
  unsigned           procSmpCnt = 64;
  cmReal_t           hopMs      = 10;

  gd0Args.medCnt      = 5;
  gd0Args.avgCnt      = 9;
  gd0Args.suprCnt     = 6;
  gd0Args.offCnt      = 3;
  gd0Args.suprCoeff   = 1.4;
  gd0Args.onThreshDb  = -53.0;
  gd0Args.offThreshDb = -80.0;

  gd1Args = gd0Args;
  gd1Args.onThreshDb = -45;

  if( cmPuAlloc(ctx, &h ) != kOkPuRC )
    goto errLabel;

  if( cmPuAutoGainCfg(h,audioFn,labelFn,outMtx0Fn,outMtx1Fn,outAudFn,procSmpCnt,hopMs,&gd0Args,&gd1Args) == kOkPuRC )
  {
    cmPuReport(h,&ctx->rpt);

    cmPuWriteJsonFile( h, jsonFn  );
  }
 errLabel:
   cmPuFree(&h);
 
   
}
