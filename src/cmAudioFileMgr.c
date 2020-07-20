#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmAudioFile.h"
#include "cmVectOpsTemplateMain.h"

#include "cmAudioFileMgr.h"

struct cmAfm_str;

typedef struct
{
  cmSample_t* minV;  // minV[summN]
  cmSample_t* maxV;  // maxV[summN]
  unsigned   summN;  // lenght of minV[] and maxV[]
} cmAfmSummary_t;

typedef struct cmAfmFile_str
{
  unsigned              id;
  cmAudioFileH_t        afH;
  cmAudioFileInfo_t     afInfo;
  unsigned              smpPerSummPt;

  cmAfmSummary_t* summArray;   // summArray[ afInfo.chCnt ]
  cmSample_t*     summMem;     // memory used by summArray[] vectors

  struct cmAfm_str*     p;
  struct cmAfmFile_str* next;
  struct cmAfmFile_str* prev;
} cmAfmFile_t;

typedef struct cmAfm_str
{
  cmErr_t      err;
  cmAfmFile_t* list;
} cmAfm_t;


cmAfmH_t     cmAfmNullHandle       = cmSTATIC_NULL_HANDLE;
cmAfmFileH_t cmAfmFileNullHandle   = cmSTATIC_NULL_HANDLE;

cmAfm_t* _cmAfmHandleToPtr( cmAfmH_t h )
{
  cmAfm_t* p = (cmAfm_t*)h.h;
  assert(p!=NULL);
  return p;
}

cmAfmFile_t* _cmAfmFileHandleToPtr( cmAfmFileH_t fh )
{
  cmAfmFile_t* fp = (cmAfmFile_t*)fh.h;
  assert(fp!=NULL);
  return fp;
}


cmAfmRC_t _cmAfmFileClose( cmAfmFile_t* fp )
{
  cmAfmRC_t rc = kOkAfmRC;

  if( cmAudioFileIsValid( fp->afH ) )
    if( cmAudioFileDelete( &fp->afH) != kOkAfRC )
      return cmErrMsg(&fp->p->err,kAudioFileFailAfmRC,"Audio file close failed on '%s'.",cmStringNullGuard(cmAudioFileName(fp->afH)));
  
  if( fp->next != NULL )
    fp->next->prev = fp->prev;

  if( fp->prev != NULL )
    fp->prev->next = fp->next;

  if( fp->p->list == fp )
    fp->p->list = fp->next;


  cmMemFree(fp->summArray);
  cmMemFree(fp->summMem);
  cmMemFree(fp);

  return rc;
}

cmAfmRC_t cmAfmFileOpen( cmAfmH_t h, cmAfmFileH_t* fhp, const cmChar_t* audioFn, unsigned id, cmAudioFileInfo_t* afInfo )
{
  cmAfmRC_t rc;
  cmRC_t afRC;

  if((rc = cmAfmFileClose(fhp)) != kOkAfmRC )
    return rc;

  cmAfmFile_t* fp = cmMemAllocZ(cmAfmFile_t,1);
  fp->p = _cmAfmHandleToPtr(h);

  // open the audio file
  if( cmAudioFileIsValid(fp->afH  = cmAudioFileNewOpen(audioFn, &fp->afInfo, &afRC, fp->p->err.rpt  )) == false )
  {
    rc = cmErrMsg(&fp->p->err,kAudioFileFailAfmRC,"The audio file '%s' could not be opened.",cmStringNullGuard(audioFn));
    goto errLabel;
  }

  // prepend the new file to the mgr's file list
  if( fp->p->list != NULL )
    fp->p->list->prev = fp;  
         
  fp->next          = fp->p->list;
  fp->p->list       = fp;

  fp->id            = id;

  fhp->h = fp;

  if( afInfo != NULL )
    *afInfo = fp->afInfo;
  

 errLabel:
  if( rc != kOkAfmRC )
    _cmAfmFileClose(fp);

  return rc;
}

cmAfmRC_t cmAfmFileClose( cmAfmFileH_t* fhp )
{
  cmAfmRC_t rc = kOkAfmRC;
  if( fhp==NULL || cmAfmFileIsValid(*fhp)==false)
    return rc;

  cmAfmFile_t* fp = _cmAfmFileHandleToPtr( *fhp );
  if((rc = _cmAfmFileClose(fp)) != kOkAfmRC )
    return rc;

  fhp->h = NULL;

  return rc;
}

bool      cmAfmFileIsValid( cmAfmFileH_t fh )
{ return fh.h != NULL; }


unsigned cmAfmFileId( cmAfmFileH_t fh )
{
  cmAfmFile_t* fp = _cmAfmFileHandleToPtr( fh );
  return fp->id;
}

cmAudioFileH_t cmAfmFileHandle( cmAfmFileH_t fh )
{
  cmAfmFile_t* fp = _cmAfmFileHandleToPtr( fh );
  return fp->afH;
}
  
const cmAudioFileInfo_t* cmAfmFileInfo( cmAfmFileH_t fh )
{
  cmAfmFile_t* fp = _cmAfmFileHandleToPtr( fh );
  return &fp->afInfo;
}

cmAfmRC_t cmAfmFileSummarize( cmAfmFileH_t fh, unsigned smpPerSummPt )
{
  cmAfmFile_t* fp    = _cmAfmFileHandleToPtr(fh);
  cmAfmRC_t    rc    = kOkAfmRC;
  unsigned     chCnt = fp->afInfo.chCnt;
  
  // summary points per channel per vector
  unsigned summN = (unsigned)ceil((double)fp->afInfo.frameCnt / smpPerSummPt );

  // total summary points in all channels and vectors
  unsigned n  = chCnt*2*summN;

  // Calc the number of summary points per audio file read
  unsigned ptsPerRd = cmMax(1,cmMax(smpPerSummPt,8192) / smpPerSummPt);  

  // Calc the number samples per audio file read as an integer multiple of ptsPerRd.
  unsigned frmCnt   = ptsPerRd * smpPerSummPt;                       

  unsigned    actualFrmCnt = 0;
  cmSample_t* chBuf[ chCnt ];
  cmSample_t  buf[ frmCnt * chCnt ];
  unsigned    i;

  // allocate the summary record array
  if( fp->summArray == NULL )
    fp->summArray = cmMemAllocZ( cmAfmSummary_t, chCnt );

  // allocate the summary vector memory for all channels
  fp->summMem      = cmMemResizeZ( cmSample_t, fp->summMem, n);  
  fp->smpPerSummPt = smpPerSummPt;

  // setup the summary record array and audio file read buffer
  for(i=0; i<chCnt; ++i)
  {
    // assign memory to the summary vectors
    fp->summArray[i].minV = fp->summMem + i * summN * 2;
    fp->summArray[i].maxV = fp->summArray[i].minV + summN;
    fp->summArray[i].summN   = summN;
    
    // setup the audio file reading channel buffer
    chBuf[i] = buf + (i*frmCnt);
  }

  // read the entire file and calculate the summary vectors
  i = 0;
  do
  {
    unsigned chIdx = 0;
    unsigned j,k;
    
    // read the next frmCnt samples from the 
    if( cmAudioFileReadSample(fp->afH, frmCnt, chIdx, chCnt, chBuf, &actualFrmCnt ) != kOkAfRC )
    {
      rc = cmErrMsg(&fp->p->err,kAudioFileFailAfmRC,"Audio file read failed on '%s'.",cmStringNullGuard(cmAudioFileName(fp->afH)));
      goto errLabel;
    }

    // for each summary point 
    for(k=0; k<actualFrmCnt && i<summN; k+=smpPerSummPt,++i)
    {
      // cnt of samples in this summary report
      unsigned m = cmMin(smpPerSummPt,actualFrmCnt-k);

      // for each channel
      for(j=0; j<chCnt; ++j)
      {
        fp->summArray[j].minV[i] = cmVOS_Min(chBuf[j]+k,m,1);
        fp->summArray[j].maxV[i] = cmVOS_Max(chBuf[j]+k,m,1);
      }
    }
  }while( i<summN && actualFrmCnt==frmCnt );
  
 errLabel:
  return rc;
}

// Downsample the summary data to produce the output.
// There must be  1 or more summary points per output point.
cmAfmRC_t  _cmAfmFileGetDownSummary( 
  cmAfmFile_t* fp, 
  unsigned     chIdx, 
  unsigned     begSmpIdx, 
  unsigned     smpCnt, 
  cmSample_t*  minV, 
  cmSample_t*  maxV, 
  unsigned     outCnt )
{
  assert( smpCnt >= outCnt );
  
  double smpPerOut  = (double)smpCnt/outCnt;
  double summPerOut = smpPerOut/fp->smpPerSummPt;

  unsigned i;

  for(i=0; i<outCnt; ++i)
  {
    double   fsbi = (begSmpIdx + (i*smpPerOut)) / fp->smpPerSummPt; // starting summary pt index
    double   fsei = fsbi + summPerOut;                              // endiing summary pt index
    unsigned si   = (unsigned)floor(fsbi);                          
    unsigned sn   = (unsigned)floor(fsei - fsbi + 1);

    if( si > fp->summArray[chIdx].summN )
    {
      minV[i] = 0;
      maxV[i] = 0;
    } 
    else
    {
      if( si + sn > fp->summArray[chIdx].summN )
        sn = fp->summArray[chIdx].summN - si;

      if( sn == 0 )
      {
        minV[i] = 0;
        maxV[i] = 0;
      }
      else
      {
        minV[i] = cmVOS_Min(fp->summArray[chIdx].minV+si,sn,1);
        maxV[i] = cmVOS_Max(fp->summArray[chIdx].maxV+si,sn,1);
      }
    }
  }
  
  return kOkAfmRC;
}

// Downsample the audio data to produce the output.
cmAfmRC_t  _cmAfmFileGetDownAudio( 
  cmAfmFile_t* fp, 
  unsigned     chIdx, 
  unsigned     begSmpIdx, 
  unsigned     smpCnt, 
  cmSample_t*  minV, 
  cmSample_t*  maxV, 
  unsigned     outCnt )
{
  assert( smpCnt >= outCnt );

  cmAfmRC_t  rc           = kOkAfmRC;
  unsigned   actualFrmCnt = 0;
  unsigned   chCnt        = 1;
  unsigned   i;
  cmSample_t buf[ smpCnt ];
  cmSample_t* chBuf[] = { buf };

  // seek to the read location
  if( cmAudioFileSeek( fp->afH, begSmpIdx ) != kOkAfRC )
  {
    rc = cmErrMsg(&fp->p->err,kAudioFileFailAfmRC,"Audio file seek failed on '%s'.",cmStringNullGuard(cmAudioFileName(fp->afH)));
    goto errLabel;
  }

  // read 'smpCnt' samples into chBuf[][]
  if( cmAudioFileReadSample(fp->afH, smpCnt, chIdx, chCnt, chBuf, &actualFrmCnt ) != kOkAfRC )
  {
    rc = cmErrMsg(&fp->p->err,kAudioFileFailAfmRC,"Audio file read failed on '%s' durnig upsample.",cmStringNullGuard(cmAudioFileName(fp->afH)));
    goto errLabel;
  }

  
  double smpPerOut  = (double)smpCnt/outCnt;

  for(i=0; i<outCnt; ++i)
  {
    double   fsbi = i*smpPerOut;
    double   fsei = fsbi + smpPerOut;
    unsigned si   = (unsigned)floor(fsbi);
    unsigned sn   = (unsigned)floor(fsei - fsbi + 1);

    if( si > smpCnt )
    {
      minV[i] = 0;
      maxV[i] = 0;
    } 

    if( si + sn > smpCnt )
      sn = smpCnt - si;

    minV[i] = cmVOS_Min(chBuf[chIdx]+si,sn,1);
    maxV[i] = cmVOS_Max(chBuf[chIdx]+si,sn,1);
  }
  
 errLabel:
  return rc;
}


// If there is one or less summary points per output
cmAfmRC_t  _cmAfmFileGetUpSummary( 
  cmAfmFile_t* fp, 
  unsigned     chIdx, 
  unsigned     begSmpIdx, 
  unsigned     smpCnt, 
  cmSample_t*  minV, 
  cmSample_t*  maxV, 
  unsigned     outCnt )
{
  assert( outCnt >= smpCnt );
  cmAfmRC_t  rc           = kOkAfmRC;
  unsigned   actualFrmCnt = 0;
  unsigned   chCnt        = 1;
  unsigned   i;
  cmSample_t buf[ smpCnt ];
  cmSample_t* chBuf[] = { buf };

  if( cmAudioFileSeek( fp->afH, begSmpIdx ) != kOkAfRC )
  {
    rc = cmErrMsg(&fp->p->err,kAudioFileFailAfmRC,"Audio file seek failed on '%s'.",cmStringNullGuard(cmAudioFileName(fp->afH)));
    goto errLabel;
  }

  if( cmAudioFileReadSample(fp->afH, smpCnt, chIdx, chCnt, chBuf, &actualFrmCnt ) != kOkAfRC )
  {
    rc = cmErrMsg(&fp->p->err,kAudioFileFailAfmRC,"Audio file read failed on '%s' durnig upsample.",cmStringNullGuard(cmAudioFileName(fp->afH)));
    goto errLabel;
  }

  
  for(i=0; i<outCnt; ++i)
  {
    unsigned   si = cmMin(smpCnt-1, (unsigned)floor(i * smpCnt / outCnt));
    cmSample_t v  = buf[si];
    minV[i]       = v;
    maxV[i]       = v;
  }

 errLabel:
  return rc;

}


cmAfmRC_t cmAfmFileGetSummary( cmAfmFileH_t fh, unsigned chIdx, unsigned begSmpIdx, unsigned smpCnt, cmSample_t* minV, cmSample_t* maxV, unsigned outCnt )
{
  cmAfmRC_t    rc = kOkAfmRC;
  cmAfmFile_t* fp = _cmAfmFileHandleToPtr(fh);
  double       maxHiResDurSecs = 20.0;

  if( smpCnt <= outCnt )
    rc = _cmAfmFileGetUpSummary( fp, chIdx, begSmpIdx, smpCnt, minV, maxV, outCnt );
  else
  {
    if( smpCnt/fp->afInfo.srate < maxHiResDurSecs )
      rc = _cmAfmFileGetDownAudio( fp, chIdx, begSmpIdx, smpCnt, minV, maxV, outCnt );
    else
      rc = _cmAfmFileGetDownSummary( fp, chIdx, begSmpIdx, smpCnt, minV, maxV, outCnt );
  }

  return rc;
}

//----------------------------------------------------------------------------
// Audio File Manager
//----------------------------------------------------------------------------  
cmAfmRC_t _cmAfmDestroy( cmAfm_t* p )
{
  cmAfmRC_t rc = kOkAfmRC;

  while( p->list != NULL )
  {
    if((rc = _cmAfmFileClose(p->list)) != kOkAfmRC )
      goto errLabel;
  }

  cmMemFree(p);

 errLabel:
  return rc;
}

cmAfmRC_t cmAfmCreate( cmCtx_t* ctx, cmAfmH_t* hp )
{
  cmAfmRC_t rc;
  if((rc = cmAfmDestroy(hp)) != kOkAfmRC )
    return rc;

  cmAfm_t* p = cmMemAllocZ(cmAfm_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"Audio File Mgr");

  hp->h = p;

  return rc;
}

cmAfmRC_t cmAfmDestroy( cmAfmH_t* hp )
{
  cmAfmRC_t rc = kOkAfmRC;

  if( hp==NULL || cmAfmIsValid(*hp)==false)
    return rc;

  cmAfm_t* p = _cmAfmHandleToPtr(*hp);

  if((rc = _cmAfmDestroy(p)) != kOkAfmRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool cmAfmIsValid( cmAfmH_t h )
{ return h.h != NULL; }

cmAfmFileH_t cmAfmIdToHandle( cmAfmH_t h, unsigned fileId )
{
  cmAfm_t*     p  = _cmAfmHandleToPtr(h);
  cmAfmFile_t* fp = p->list;
  cmAfmFileH_t fh = cmAfmFileNullHandle;

  for(; fp!=NULL; fp=fp->next)
    if( fp->id == fileId )
    {
      fh.h = fp;
      break;
    }

  return fh;
}
