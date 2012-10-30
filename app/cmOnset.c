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

  cmAudioFileInfo_t afInfo;
  unsigned          fftSmpCnt;
  unsigned          hopSmpCnt;
  unsigned          binCnt;

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

cmOnRC_t _cmOnsetExec( _cmOn_t* p, unsigned chCnt )
{
  cmOnRC_t    rc     = kOkOnRC;
  int         fi     = 0;
  unsigned    binCnt = p->binCnt; //p->pvocPtr->binCnt;
  cmReal_t    mag0V[ binCnt ];
  cmSample_t  out0V[ p->hopSmpCnt ];
  cmSample_t  out1V[ p->hopSmpCnt ];
  cmSample_t* aoutV[chCnt];
  double      prog   = 0.1;
  cmReal_t    b0     = 1;
  cmReal_t    b[]    = {1  };
  cmReal_t    a[]    = {p->cfg.filtCoeff};
  cmReal_t    d[]    = {0};
  cmReal_t    maxVal = 0;

  if( chCnt > 0 )
    aoutV[0] = out0V;

  if( chCnt > 1 )
    aoutV[1] = out1V;

  cmVOR_Zero(mag0V,binCnt);

  // for each frame - read the next block of audio
  for(; fi<p->frmCnt && cmAudioFileRdRead(p->afRdPtr) != cmEofRC; ++fi )
  {
    // calc the spectrum 
    while( cmPvAnlExec(p->pvocPtr, p->afRdPtr->outV, p->afRdPtr->outN ) )
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
      cmVOR_Filter( p->sfV + fi, 1, &sf, 1, b0, b, a, d, 1 );

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
  cmVOR_SubVS(p->sfV,p->frmCnt,mean);
  cmVOR_DivVS(p->sfV,p->frmCnt,stdDev);
  cmReal_t maxSf = cmVOR_Max(p->sfV,p->frmCnt,1);
  prog = 0.1;

  printf("max:%f ",maxVal);
  printf("mean:%f max:%f sd:%f\n",mean,maxSf,stdDev);

  // Pick peaks from the onset detection function using a subset
  // of the rules from Dixon, 2006, Onset Detection Revisited.
  // locate the onsets and store them in dfV[]
  for(fi=0; fi<p->frmCnt; ++fi)
  {
    int bi = cmMax(0,         fi - p->cfg.wndFrmCnt);       // begin wnd index
    int ei = cmMin(p->frmCnt, fi + p->cfg.wndFrmCnt);       // end wnd index
    int nn = ei - bi;                                       // wnd frm cnt 
    int wi = fi < p->cfg.wndFrmCnt ? fi : p->cfg.wndFrmCnt; // cur wnd index

    // initialize the out
    cmVOS_Fill(out1V,p->hopSmpCnt,p->sfV[fi]/maxSf);
    cmVOS_Zero(out0V,p->hopSmpCnt);

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
        p->dfV[fi]              = p->sfV[fi];
        out0V[ p->hopSmpCnt/2 ] = p->sfV[fi]/maxSf;

        unsigned smpIdx = fi * p->hopSmpCnt + p->hopSmpCnt/2;

        // write the output text file
        if( cmFilePrintf(p->txH, "[ %i, %f ]\n", smpIdx, p->sfV[fi] ) != kOkFileRC )
        {
          rc = cmErrMsg(&p->err,kDspTextFileFailOnRC,"Text output write to '%s' failed.", cmFileName(p->txH));
          goto errLabel;
        }
      }
    }

    // write the output audio file
    if( cmAudioFileWriteFloat(p->afH, p->hopSmpCnt, chCnt, aoutV ) != kOkAfRC )
    {
      rc = cmErrMsg(&p->err,kDspAudioFileFailOnRC,"Audio file write to '%s' failed.",cmAudioFileName(p->afH));
      goto errLabel;
    }
    
    if( fi >= prog*p->frmCnt )
    {
      cmRptPrintf(p->err.rpt,"%i ",lround(prog*10));
      prog += 0.1;      
    }

  }
    
 errLabel:
  
  return rc;
}

cmOnRC_t cmOnsetExec( cmOnH_t h, const cmOnsetCfg_t* cfg, const cmChar_t* inAudioFn, const cmChar_t* outAudioFn, const cmChar_t* outTextFn )
{
  cmOnRC_t rc            = kOkOnRC;
  _cmOn_t* p             = _cmOnsetHandleToPtr(h);
  unsigned audioOutChCnt = 2;
  p->cfg = *cfg;

  // get the audio file header information
  if( cmAudioFileGetInfo(inAudioFn, &p->afInfo, p->err.rpt ) != kOkAfRC )
  {
    rc =  cmErrMsg(&p->err,kDspProcFailOnRC,"The audio file open failed on '%s'.",cmStringNullGuard(inAudioFn));
    goto errLabel;
  }

  p->fftSmpCnt = cmNearPowerOfTwo( (unsigned)floor( p->cfg.wndMs * p->afInfo.srate / 1000.0 ) );
  p->hopSmpCnt = p->fftSmpCnt / p->cfg.hopFact;
  p->binCnt    = cmMin(p->fftSmpCnt/2 + 1, floor(p->cfg.maxFrqHz / (p->afInfo.srate / p->fftSmpCnt)));
  p->frmCnt    = (p->afInfo.frameCnt - p->fftSmpCnt) / p->hopSmpCnt;
  p->sfV       = cmMemResizeZ(cmReal_t,p->sfV,p->frmCnt);
  p->dfV       = cmMemResizeZ(cmReal_t,p->dfV,p->frmCnt);

  // initialize the audio file reader
  if( cmAudioFileRdOpen( p->afRdPtr, p->hopSmpCnt, inAudioFn, p->cfg.audioChIdx, 0, cmInvalidIdx ) != cmOkRC )
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

  // initalize the audio output file
  if( outAudioFn != NULL )
    if( cmAudioFileIsValid( p->afH = cmAudioFileNewCreate( outAudioFn, p->afInfo.srate, p->afInfo.bits, audioOutChCnt, NULL, p->err.rpt)) == false )
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

  rc = _cmOnsetExec(p,audioOutChCnt);

 errLabel:
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
  return rc;  
}


cmOnRC_t cmOnsetTest( cmCtx_t* c )
{
  cmOnsetCfg_t    cfg;
  cmOnH_t         h          = cmOnsetNullHandle;
  cmOnRC_t        rc         = kOkOnRC;
  const cmChar_t* inAudioFn  = "/home/kevin/temp/onset0.wav";
  const cmChar_t* outAudioFn = "/home/kevin/temp/mas/mas0.aif";
  const cmChar_t* outTextFn  = "/home/kevin/temp/mas/mas0.txt";

  cfg.wndMs      = 42;
  cfg.hopFact    = 4;
  cfg.audioChIdx = 0;
  cfg.wndFrmCnt  = 3;
  cfg.preWndMult = 3;
  cfg.threshold  = 0.6;
  cfg.maxFrqHz   = 24000;
  cfg.filtCoeff  = -0.7;

  if((rc = cmOnsetInitialize(c,&h)) != kOkOnRC )
    goto errLabel;
  
  rc = cmOnsetExec(h,&cfg,inAudioFn,outAudioFn,outTextFn);
  
 errLabel:
  cmOnsetFinalize(&h);

  return rc;
  
}
