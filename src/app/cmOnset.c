//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
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
#include "cmAudioFile.h"
#include "cmTime.h"
#include "cmMidi.h"
#include "cmFile.h"
#include "cmMath.h"

#include "cmProcObj.h"
#include "cmProcTemplateMain.h"
#include "cmProc.h"
#include "cmProc2.h"
#include "cmVectOps.h"

#include "cmOnset.h"

typedef struct
{
  cmErr_t        err;
  cmOnsetCfg_t   cfg;

  cmCtx*         ctxPtr;
  cmAudioFileRd* afRdPtr;
  cmPvAnl*       pvocPtr;
  
  cmAudioFileH_t afH; // output audio file
  cmFileH_t      txH; // output text file

  unsigned       frmCnt;   // spectral frame count
  cmReal_t*      sfV;      // sfV[frmCnt] spectral flux vector
  cmReal_t*      dfV;      // dfV[frmCnt] onset function vector
  cmReal_t       maxSf;

  cmAudioFileInfo_t afInfo;
  unsigned          fftSmpCnt;
  unsigned          hopSmpCnt;
  unsigned          binCnt;
  unsigned          medFiltFrmCnt;
  unsigned          preDelaySmpCnt;
} _cmOn_t;

cmOnH_t cmOnsetNullHandle = cmSTATIC_NULL_HANDLE;

_cmOn_t* _cmOnsetHandleToPtr( cmOnH_t h )
{
  _cmOn_t* p = (_cmOn_t*)h.h;
  assert(p!=NULL);
  return p;
}

cmOnRC_t _cmOnsetFinalize( _cmOn_t* p )
{
  cmOnRC_t rc = kOkOnRC;

  if( cmPvAnlFree(&p->pvocPtr) != cmOkRC )
  {
    rc = cmErrMsg(&p->err,kDspProcFailOnRC,"Phase voocoder free failed.");
    goto errLabel;
  }

  if( cmAudioFileRdFree(&p->afRdPtr) != cmOkRC )
  {
    rc = cmErrMsg(&p->err,kDspProcFailOnRC,"Audio file reader failed.");
    goto errLabel;
  }

  if( cmCtxFree(&p->ctxPtr) != cmOkRC )
  {
    rc = cmErrMsg(&p->err,kDspProcFailOnRC,"Context proc failed.");
    goto errLabel;
  }

  cmMemPtrFree(&p->sfV);
  cmMemPtrFree(&p->dfV);
  cmMemPtrFree(&p);

 errLabel:
  return rc;
}

cmOnRC_t cmOnsetInitialize( cmCtx_t* c, cmOnH_t* hp )
{
  cmOnRC_t rc;
  if((rc = cmOnsetFinalize(hp)) != kOkOnRC )
    return rc;

  _cmOn_t* p = cmMemAllocZ(_cmOn_t,1);
  cmErrSetup(&p->err,&c->rpt,"Onset");

  // create the proc context object
  if((p->ctxPtr  = cmCtxAlloc(NULL,&c->rpt,cmLHeapNullHandle,cmSymTblNullHandle)) == NULL )
  {
    rc = cmErrMsg(&p->err,kDspProcFailOnRC, "The ctx compoenent allocation failed.");
    goto errLabel;
  }

  // create the audio file reader
  if((p->afRdPtr = cmAudioFileRdAlloc( p->ctxPtr, NULL, 0, NULL, cmInvalidIdx, 0, cmInvalidIdx )) == NULL )
  {
    rc =  cmErrMsg(&p->err,kDspProcFailOnRC, "The audio file reader allocation failed.");
    goto errLabel;
  }

  // create the phase vocoder 
  if((p->pvocPtr = cmPvAnlAlloc( p->ctxPtr, NULL, 0, 0, 0, 0, 0 )) == NULL )
  {
    rc = cmErrMsg(&p->err,kDspProcFailOnRC,"The phase vocoder allocation failed.");
    goto errLabel;
  }

  hp->h = p;

 errLabel:
  if( rc != kOkOnRC )
    _cmOnsetFinalize(p);

  return rc;
}

cmOnRC_t cmOnsetFinalize( cmOnH_t* hp )
{
  cmOnRC_t rc = kOkOnRC;

  if( hp==NULL || cmOnsetIsValid(*hp)==false )
    return kOkOnRC;

  _cmOn_t* p = _cmOnsetHandleToPtr(*hp);

  rc = _cmOnsetFinalize(p);

  return rc;
}

bool     cmOnsetIsValid( cmOnH_t h )
{ return h.h!=NULL; }

cmOnRC_t _cmOnsetExec( _cmOn_t* p )
{
  cmOnRC_t    rc     = kOkOnRC;
  int         fi     = 0;
  unsigned    binCnt = p->binCnt; //p->pvocPtr->binCnt;
  cmReal_t    mag0V[ binCnt ];
  double      prog   = 0.1;
  cmReal_t    b0     = 1;
  cmReal_t    b[]    = {1  };
  cmReal_t    a[]    = {p->cfg.filtCoeff};
  cmReal_t    d[]    = {0};
  cmReal_t    maxVal = 0;


  cmVOR_Zero(mag0V,binCnt);

  // for each frame - read the next block of audio
  for(; fi<p->frmCnt && cmAudioFileRdRead(p->afRdPtr) != cmEofRC; ++fi )
  {
    // calc the spectrum 
    if( cmPvAnlExec(p->pvocPtr, p->afRdPtr->outV, p->afRdPtr->outN ))
    {
      unsigned i;

      // calc the spectral flux into sfV[fi].
      cmReal_t sf = 0;
      for(i=0; i<binCnt; ++i)
      {
        cmReal_t m1 = p->pvocPtr->magV[i] * 2.0;

        if( m1 > maxVal )
          maxVal = m1;

        cmReal_t dif  = m1 - mag0V[i];   // calc. spectral flux
        if( dif > 0 )                                            
          sf += dif;                     // accum. flux
        mag0V[i] = m1;                   // store magn. for next frame
      }

      p->sfV[fi] = sf;

      // filter the spectral flux 
      switch( p->cfg.filterId)
      {
        case kNoneFiltId:
          break;
        case kSmoothFiltId:
          cmVOR_Filter( p->sfV + fi, 1, &sf, 1, b0, b, a, d, 1 );
          break;
        case kMedianFiltId:
          {
            cmReal_t* mfb = p->sfV + cmMax(0,fi-17);
            if( mfb < p->sfV-3 )
              p->sfV[fi] = cmVOR_Median(mfb,p->sfV-mfb);
          }
          break;
        default:
          { assert(0); }
      }

      if( fi >= prog*p->frmCnt )
      {
        cmRptPrintf(p->err.rpt,"%i ",lround(prog*10));
        prog += 0.1;      
      }
    }
  }

  p->frmCnt = fi;

  // normalize the spectral flux vector
  cmReal_t mean   = cmVOR_Mean(p->sfV,p->frmCnt);
  cmReal_t stdDev = sqrt(cmVOR_Variance(p->sfV, p->frmCnt, &mean ));
  unsigned detectCnt = 0;
  cmVOR_SubVS(p->sfV,p->frmCnt,mean);
  cmVOR_DivVS(p->sfV,p->frmCnt,stdDev);
  p->maxSf = cmVOR_Max(p->sfV,p->frmCnt,1);
  prog = 0.1;

  cmRptPrintf(p->err.rpt,"magn. max:%f flux mean:%f max:%f sd:%f\n",maxVal,mean,p->maxSf,stdDev);

  // Pick peaks from the onset detection function using a subset
  // of the rules from Dixon, 2006, Onset Detection Revisited.
  // locate the onsets and store them in dfV[]
  for(fi=0; fi<p->frmCnt; ++fi)
  {
    int bi = cmMax(0,         fi - p->cfg.wndFrmCnt);       // begin wnd index
    int ei = cmMin(p->frmCnt, fi + p->cfg.wndFrmCnt);       // end wnd index
    int nn = ei - bi;                                       // wnd frm cnt 
    int wi = fi < p->cfg.wndFrmCnt ? fi : p->cfg.wndFrmCnt; // cur wnd index


    p->dfV[fi] = 0;

    // if cur index is a peak in the window
    if( cmVOR_MaxIndex(p->sfV + bi, nn, 1 ) == wi )
    {
      // calc an extended window going backwards in time
      bi = cmMax(0, fi - p->cfg.wndFrmCnt * p->cfg.preWndMult );
      nn = ei - bi;

      // if the cur value is greater than the mean of the extended window plus a threshold
      if( p->sfV[fi] > cmVOR_Mean(p->sfV + bi, nn ) + p->cfg.threshold )
      {
        p->dfV[fi]  = p->sfV[fi];
        ++detectCnt;
      }
    }

    
    if( fi >= prog*p->frmCnt )
    {
      cmRptPrintf(p->err.rpt,"%i ",lround(prog*10));
      prog += 0.1;      
    }

  }
    
  cmRptPrintf(p->err.rpt,"Detect Count:%i\n",detectCnt);
  return rc;
}

cmOnRC_t cmOnsetProc( cmOnH_t h, const cmOnsetCfg_t* cfg, const cmChar_t* inAudioFn )
{
  cmOnRC_t rc = kOkOnRC;
  _cmOn_t* p  = _cmOnsetHandleToPtr(h);
  p->cfg = *cfg;

  // get the audio file header information
  if( cmAudioFileGetInfo(inAudioFn, &p->afInfo, p->err.rpt ) != kOkAfRC )
  {
    rc =  cmErrMsg(&p->err,kDspProcFailOnRC,"The audio file open failed on '%s'.",cmStringNullGuard(inAudioFn));
    goto errLabel;
  }

  p->fftSmpCnt     = cmNearPowerOfTwo( (unsigned)floor( p->cfg.wndMs * p->afInfo.srate / 1000.0 ) );
  p->hopSmpCnt     = p->fftSmpCnt / p->cfg.hopFact;
  p->binCnt        = cmMin(p->fftSmpCnt/2 + 1, floor(p->cfg.maxFrqHz / (p->afInfo.srate / p->fftSmpCnt)));
  p->frmCnt        = (p->afInfo.frameCnt - p->fftSmpCnt) / p->hopSmpCnt;
  p->sfV           = cmMemResizeZ(cmReal_t,p->sfV,p->frmCnt);
  p->dfV           = cmMemResizeZ(cmReal_t,p->dfV,p->frmCnt);
  p->medFiltFrmCnt = cmMax(3,floor(cfg->medFiltWndMs * p->afInfo.srate / (1000.0 * p->hopSmpCnt)));
  p->preDelaySmpCnt= floor(cfg->preDelayMs * p->afInfo.srate / 1000.0);

  cmRptPrintf(p->err.rpt,"wndFrmCnt:%i preWndMult:%f thresh:%f maxHz:%f filtCoeff:%f filterId:%i preDelayMs:%f\n",cfg->wndFrmCnt,cfg->preWndMult,cfg->threshold,cfg->maxFrqHz,cfg->filtCoeff,cfg->medFiltWndMs,cfg->filterId,cfg->preDelayMs );
  cmRptPrintf(p->err.rpt,"Analysis Hop Duration: %8.2f ms %i smp\n",(double)p->hopSmpCnt*1000/p->afInfo.srate,p->hopSmpCnt);  
  cmRptPrintf(p->err.rpt,"Median Filter Window:  %8.2f ms %i frames\n",cfg->medFiltWndMs,p->medFiltFrmCnt);
  cmRptPrintf(p->err.rpt,"Detection Pre-delay:   %8.2f ms %i smp\n",cfg->preDelayMs, p->preDelaySmpCnt);

  // initialize the audio file reader
  if( cmAudioFileRdOpen( p->afRdPtr, p->hopSmpCnt, inAudioFn, p->cfg.audioChIdx, 0, 0 ) != cmOkRC )
  {
    rc =  cmErrMsg(&p->err,kDspProcFailOnRC, "The audio file reader open failed.");
    goto errLabel;
  }

  // initialize the phase vocoder 
  if( cmPvAnlInit( p->pvocPtr, p->hopSmpCnt, p->afInfo.srate, p->fftSmpCnt, p->hopSmpCnt, kNoCalcHzPvaFl ) != cmOkRC )
  {
    rc = cmErrMsg(&p->err,kDspProcFailOnRC," The phase vocoder initialization failed.");
    goto errLabel;
  }


  rc = _cmOnsetExec(p);

 errLabel:
  return rc;
}

unsigned cmOnsetCount( cmOnH_t h )
{
  _cmOn_t* p   = _cmOnsetHandleToPtr(h);
  unsigned i;
  unsigned n = 0;
  for(i=0; i<p->frmCnt; ++i)
    if( p->dfV[i] > 0 )
      ++n;

  return n;
}

unsigned cmOnsetSampleIndex( cmOnH_t h, unsigned idx )
{
  _cmOn_t* p   = _cmOnsetHandleToPtr(h);
  unsigned i;
  unsigned n = 0;
  for(i=0; i<p->frmCnt; ++i)
    if( p->dfV[i] > 0 )
    {
      if( n == idx )
      {
        unsigned r = i * p->hopSmpCnt;
        if( r > p->preDelaySmpCnt )
          return r-p->preDelaySmpCnt;
        return 0;
      }

      ++n;
    }

  return cmInvalidIdx;
}

unsigned cmOnsetHopSampleCount( cmOnH_t h )
{
  _cmOn_t* p   = _cmOnsetHandleToPtr(h);
  return p->hopSmpCnt;
}


cmOnRC_t cmOnsetWrite( cmOnH_t h, const cmChar_t* outAudioFn, const cmChar_t* outTextFn)
{
  enum { kChCnt = 2 };
  cmOnRC_t rc  = kOkOnRC;
  _cmOn_t* p   = _cmOnsetHandleToPtr(h);

  cmSample_t  out0V[ p->hopSmpCnt ];  
  cmSample_t  out1V[ p->hopSmpCnt ];
  cmSample_t* aoutV[kChCnt];
  unsigned    pdn = 0;

  aoutV[0] = out0V;
  aoutV[1] = out1V;

  // initalize the audio output file
  if( outAudioFn != NULL )
    if( cmAudioFileIsValid( p->afH = cmAudioFileNewCreate( outAudioFn, p->afInfo.srate, p->afInfo.bits, kChCnt, NULL, p->err.rpt)) == false )
    {
      rc = cmErrMsg(&p->err,kDspAudioFileFailOnRC, "The audio output file '%s' could not be opened.", outAudioFn);
      goto errLabel;
    }

  // open the text output file
  if( outTextFn != NULL )
  {
    if( cmFileOpen( &p->txH, outTextFn, kWriteFileFl, p->err.rpt ) != kOkFileRC )
    {
      rc = cmErrMsg(&p->err,kDspTextFileFailOnRC, "The text output file '%s' could not be opened.",outTextFn);
      goto errLabel;
    }

    cmFilePrint(p->txH,"{\n onsetArray : \n[\n");
  }

  unsigned fi;
  for(fi=0; fi<p->frmCnt; ++fi)
  {
    // count of samples to write to the audio output file
    unsigned osn = p->hopSmpCnt;

    // audio channel 1 is filled with the spectral flux
    // initialize the out
    cmVOS_Fill(out1V,p->hopSmpCnt,p->sfV[fi]/p->maxSf);
    cmVOS_Zero(out0V,p->hopSmpCnt);

    if( p->dfV[fi] > 0 )
    {
      // audio channel 0 is set with the detection indicators
      unsigned smpIdx = fi * p->hopSmpCnt + p->hopSmpCnt/2;
      out0V[ p->hopSmpCnt/2 ] = p->sfV[fi]/p->maxSf;


      // if the pre-delay is still active
      if( pdn < p->preDelaySmpCnt )
      {
        osn = 0;

        pdn += p->hopSmpCnt;

        if( pdn > p->preDelaySmpCnt )
          osn = pdn - p->preDelaySmpCnt;
      }


      // write the output text file
      if( cmFileIsValid(p->txH) )
        if( cmFilePrintf(p->txH, "[ %i, %f ]\n", smpIdx, p->sfV[fi] ) != kOkFileRC )
        {
          rc = cmErrMsg(&p->err,kDspTextFileFailOnRC,"Text output write to '%s' failed.", cmFileName(p->txH));
          goto errLabel;
        }
    }

    // write the output audio file
    if( osn > 0 && cmAudioFileIsValid(p->afH) )
    {
      if( cmAudioFileWriteFloat(p->afH, osn, kChCnt, aoutV ) != kOkAfRC )
      {
        rc = cmErrMsg(&p->err,kDspAudioFileFailOnRC,"Audio file write to '%s' failed.",cmAudioFileName(p->afH));
        goto errLabel;
      }
    }

  }
  
  // close the output audio file
  if( cmAudioFileDelete(&p->afH) != kOkAfRC )
    rc = cmErrMsg(&p->err,kDspAudioFileFailOnRC,"The audio file close failed.");

  // close the text file
  if( cmFileIsValid(p->txH) )
  {
    cmFilePrint(p->txH,"]\n}\n");

    if( cmFileClose(&p->txH) != kOkFileRC )
      rc = cmErrMsg(&p->err,kDspTextFileFailOnRC,"The text file close failed.");
  }

 errLabel:
  return rc;  

}

cmOnRC_t cmOnsetTest( cmCtx_t* c )
{
  cmOnsetCfg_t    cfg;
  cmOnH_t         h          = cmOnsetNullHandle;
  cmOnRC_t        rc         = kOkOnRC;
  const cmChar_t* inAudioFn  = "/home/kevin/media/audio/20110723-Kriesberg/Audio Files/Piano 3_15.wav";
  const cmChar_t* outAudioFn = "/home/kevin/temp/ons/ons0.aif";
  const cmChar_t* outTextFn  = "/home/kevin/temp/ons/ons0.txt";

  cfg.wndMs      = 42;
  cfg.hopFact    = 4;
  cfg.audioChIdx = 0;
  cfg.wndFrmCnt  = 3;
  cfg.preWndMult = 3;
  cfg.threshold  = 0.6;
  cfg.maxFrqHz   = 24000;
  cfg.filtCoeff  = -0.7;
  cfg.medFiltWndMs = 50;
  cfg.filterId     = kMedianFiltId;
  cfg.preDelayMs   = 20;

  if((rc = cmOnsetInitialize(c,&h)) != kOkOnRC )
    goto errLabel;
  
  if((rc = cmOnsetProc(h,&cfg,inAudioFn)) == kOkOnRC )
    cmOnsetWrite(h,outAudioFn,outTextFn);
  
  
 errLabel:
  cmOnsetFinalize(&h);

  return rc;
  
}
