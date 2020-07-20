#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmJson.h"
#include "cmFrameFile.h"
#include "cmLinkedHeap.h"
#include "cmMath.h"
#include "cmVectOps.h"

/*

File  Type:  4   0   
Chunk Bytes: 4   4
Frame Count: 4   8
Version:     4   16
EOF Frm Offs:8   20
Sample Rate: 8   28 32

Frame Type:  4   36      // Note: Update cmFrameFileFrameSkip()
Chunk Bytes: 4   40      // if the size of this header changes.
Mtx Count:   4   44
Stream Id:   4   48
Flags:       4   52
Sample Idx   4   56
Seconds:     8   60 32

Mtx Type:    4   68
Data Bytes:  4   72
Format Id:   4   76
Units Id:    4   80
Row Cnt:     4   86
Col Cnt:     4   90 24




 */

#define _cmFfSwap16(fl,v)           ((fl) ? cmSwap16(v) : (v))
#define _cmFfSwap32(fl,v)           ((fl) ? cmSwap32(v) : (v))
#define _cmFfSwap64(fl,v)           ((fl) ? cmSwap64(v) : (v)) 
#define _cmFfWrSwapF(fl,v)          ((fl) ? cmFfSwapFloatToUInt(v) : (*((unsigned*)&(v))))
#define _cmFfRdSwapF(fl,v)          ((fl) ? cmFfSwapUIntToFloat(v) : (*((float*)&(v))))
#define _cmFfWrSwapD(fl,v)          ((fl) ? cmFfSwapDoubleToULLong(v) : (*((unsigned long long*)&(v))))
#define _cmFfRdSwapD(fl,v)          ((fl) ? cmFfSwapULLongToDouble(v) : (*((double*)&(v))))


enum
{
  kSampleIdxTimeFl = 0x01,
  kSecondsTimeFl   = 0x02
}; 


typedef struct _cmFfOffs_str
{
  unsigned              frmIdx; // absolute frame index for this mtx
  off_t                 offs;   // file offset for mtx header
  struct _cmFfOffs_str* linkPtr;
} _cmFfOffs_t;

typedef struct 
{
  _cmFfOffs_t* beg;
  _cmFfOffs_t* end;
  unsigned     cnt;
} _cmFfOffsList_t;

typedef struct _cmFfToC_str
{
  unsigned             streamId;     //  
  unsigned             mtxType;      //  kInvalidMId when used with frmToC
  unsigned             mtxUnitsId;   //  kInvalidUId when used with frmToC
  unsigned             mtxFmtId;     //  kInvalidFmtId when used with frmToC
  _cmFfOffsList_t      offsList;     //
  unsigned             lastFrmIdx;   //  used to prevent duplicate records during ToC creation
  struct _cmFfToC_str* linkPtr;
} _cmFfToC_t;

// private matrix desc record
typedef struct
{
  cmFfMtx_t m;       // public mtx description record
  unsigned  byteCnt; // bytes in this->dataPtr block
  void*     dataPtr; // pointer to data for this mtx
} _cmFfMtx_t;

// private frame desc record
typedef struct
{
  cmFfFrame_t f;        // public frame description record
  unsigned    byteCnt;  // byte count of frame file chunk 
  _cmFfMtx_t* mtxArray; // mtx ctl record array
  char*       dataPtr;  // all memory used by all mtx's in this frame
} _cmFfFrame_t;

typedef struct
{    
  cmErr_t            err;
  cmCtx_t            ctx;
  FILE*              fp;             //   
  bool               writeFl;        // file is open for writing
  unsigned           fileChkByteCnt; // 
  unsigned           nxtFrmIdx;      // index of the next frame after the current frame
  unsigned           curFrmIdx;      // write: not used read:index of currently loaded frame
  off_t              frameOffset;    // read: offset to first mtx hdr in cur frame 
                                     // write:offset to cur frame hdr
  off_t              rewOffset;      // rewind offset (first frame)
  off_t              eofOffset;      // last frame (offset data frame)
  cmFfFile_t         f;              // file header
  _cmFfFrame_t       frame;          // cur frame
  cmLHeapH_t         lhH;            // linked heap handle 
  _cmFfToC_t*        mtxToC;         // one ToC recd for each existing matrix stream/type/units/fmt combination
  _cmFfToC_t*        frmToC;         // one ToC recd for each stream
  void*              writeMtxMem;
  bool               swapFl; 
} cmFf_t;

typedef struct 
{
  unsigned    fmtId;
  unsigned    wordByteCnt;
  const char* label;
} _cmFfFmt_t;

_cmFfFmt_t _cmFfFmtArray[] = 
{
  { kUCharFmtId,   1, "char" },   
  { kCharFmtId,    1, "uchar" },    
  { kUShortFmtId,  2, "ushort" },  
  { kShortFmtId,   2, "short" },   
  { kULongFmtId,   4, "ulong" },   
  { kLongFmtId,    4, "long" },    
  { kUIntFmtId,    4, "uint" },    
  { kIntFmtId,     4, "int" },     
  { kLLongFmtId,   8, "llong" },   
  { kULLongFmtId,  8, "ullong" },  
  { kOff_tFmtId,   sizeof(off_t), "off_t"},
  { kFloatFmtId,   4, "float" },   
  { kDoubleFmtId,  8, "double" },  
  { kStringZFmtId, 1, "string" }, 
  { kBlobFmtId,    1, "blob" },    
  { kJsonFmtId,    1, "json" },     
  { kInvalidFmtId, 0, "<invalid>" }
};


cmFrameFileH_t cmFrameFileNullHandle = { NULL };
/*
void _cmFfPrint( cmFf_t* p, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  
  if( p == NULL || p->vPrintFunc == NULL )
    vfprintf(stderr,fmt,vl);
  else
    p->vPrintFunc(p->rptDataPtr,fmt,vl);

  va_end(vl);
  
}

cmFfRC_t _cmFfVError( cmFf_t* p, cmFfRC_t rc, int sysErrCode, const char* fmt, va_list vl )
{
  int bufCharCnt = 256;
  char buf0[bufCharCnt+1];
  char buf1[bufCharCnt+1];
  char buf2[bufCharCnt+1];

  snprintf(buf0,bufCharCnt,"cmFrameFile Error: (%i): ",rc );
  vsnprintf(buf1,bufCharCnt,fmt,vl);
  snprintf(buf2,bufCharCnt,"System Error: ");
  

  unsigned sn = strlen(buf0) + strlen(buf1);

  sn += sysErrCode == 0 ? 0 : strlen(buf2) + strlen(strerror(sysErrCode));

  char buf3[sn+1];
  buf3[sn] = 0;
  buf3[0]  = 0;

  strncpy(buf3, buf0, sn-strlen(buf3) );
  strncat(buf3, buf1, sn-strlen(buf3) );
  if( sysErrCode )
  {
    strncat(buf3,buf2, sn - strlen(buf3) );
    strncat(buf3,strerror(sysErrCode), sn - strlen(buf3) );
  }
  
  assert(strlen(buf3)==sn);

  _cmFfPrint(p,"%s\n",buf3);

  return rc;

}
*/

cmFfRC_t _cmFfVError( cmFf_t* p, cmFfRC_t rc, int sysErrCode, const char* fmt, va_list vl )
{
  if( p != NULL )
    return cmErrVSysMsg(&p->err,rc,sysErrCode,fmt,vl);

  printf("cmFrameFile Error: rc=%i ",rc);
  vprintf(fmt,vl);
  printf("\n");

  if( sysErrCode )
    printf("cmFrameFile System Error code=%i %s\n\n",sysErrCode,strerror(sysErrCode)); 

  return rc;
}

cmFfRC_t _cmFfError( cmFf_t* p, cmFfRC_t rc, int sysErrCode, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  _cmFfVError( p, rc, sysErrCode, fmt, vl );
  va_end(vl);
  return rc;
}

cmFf_t* _cmFfHandleToPtr( cmFrameFileH_t h )
{
  cmFf_t* p = (cmFf_t*)h.h;
  if( p == NULL )
    _cmFfError(NULL,kInvalidHandleFfRC,0,"Null handle.");
  assert( p != NULL);
  return p;
}

_cmFfFmt_t* _cmFfIdToFmtPtr( unsigned fmtId )
{
  unsigned i;
  for(i=0; _cmFfFmtArray[i].fmtId != kInvalidFmtId; ++i)
    if( _cmFfFmtArray[i].fmtId == fmtId )
      break;

  return _cmFfFmtArray + i;
}


const void* _cmFfSwapVector( void* dV, const void* sV, unsigned n, unsigned bn )
{
  unsigned i;
  switch( bn )
  {
    case 1:
      return sV;

    case 2:
      {
        const unsigned short* x = (const unsigned short*)sV;
        unsigned short*       y = (unsigned short*)dV;
        for(i=0; i<n; ++i)
          y[i] = cmSwap16(x[i]);
      }
      break;

    case 4:
      {
        const unsigned long* x = (const unsigned long*)sV;
        unsigned long* y = (unsigned long*)dV;
        for(i=0; i<n; ++i)
          y[i] = cmSwap32(x[i]);
      }
      break;

    case 8:
      {
        // on 32 bit linux this is very slow
        const unsigned long long* x = (const unsigned long long*)sV;
        unsigned long long* y = (unsigned long long*)dV;
        for(i=0; i<n; ++i)
          y[i] = cmSwap64(x[i]);

      }
      break;
      
    default:
      { assert(0); }
  }

  return dV;
}


cmFfRC_t _cmFfWrite( cmFf_t* p, const void* dataPtr, unsigned byteCnt )
{
  if(fwrite(dataPtr,byteCnt,1,p->fp) != 1 )
    return _cmFfError( p, kFileWriteFailFfRC, errno, "File write failed." );

  p->fileChkByteCnt += byteCnt;
  p->frame.byteCnt  += byteCnt;
  return kOkFfRC;
}

cmFfRC_t _cmFfWriteOff_t( cmFf_t* p, off_t v )
{
  cmFfRC_t rc;

  assert(sizeof(off_t)==8);
  
  v = _cmFfSwap64(p->swapFl,v);

  if((rc = _cmFfWrite(p,&v,sizeof(v))) != kOkFfRC )
    return rc;

  return kOkFfRC;    
}

cmFfRC_t _cmFfWriteUInt( cmFf_t* p, unsigned v )
{
  cmFfRC_t rc;
  
  v = _cmFfSwap32(p->swapFl,v);

  if((rc = _cmFfWrite(p,&v,sizeof(v))) != kOkFfRC )
    return rc;

  return kOkFfRC;    
}

cmFfRC_t _cmFfWriteUIntV( cmFf_t* p, const unsigned* vp, unsigned n )
{
  unsigned i;
  cmFfRC_t rc;

  for(i=0; i<n; ++i)
    if((rc = _cmFfWriteUInt( p, vp[i] )) !=  kOkFfRC )
      return rc;

  return kOkFfRC;
}

cmFfRC_t _cmFfWriteDouble( cmFf_t* p, double v )
{
  cmFfRC_t rc;

  unsigned long long vv = _cmFfWrSwapD(p->swapFl,v);

  if((rc = _cmFfWrite(p, &vv, sizeof(vv))) != kOkFfRC )
    return rc;

  return kOkFfRC;    
}

cmFfRC_t _cmFfRead( cmFf_t* p, void* vp, unsigned bn )
{
  if(fread(vp,bn,1,p->fp) != 1 )
  {
    if( feof(p->fp) )
      return kEofFfRC;

    return _cmFfError( p, kFileReadFailFfRC, errno, "File read failed.");
  }  

  return kOkFfRC;

}

cmFfRC_t _cmFfReadOff_t( cmFf_t* p, off_t* vp )
{
  cmFfRC_t rc;

  assert( sizeof(off_t)==8);

  if((rc = _cmFfRead(p,vp,sizeof(*vp))) != kOkFfRC )
    return rc;

  *vp = _cmFfSwap64(p->swapFl,*vp);
  return kOkFfRC;
}

cmFfRC_t _cmFfReadUInt( cmFf_t* p, unsigned* vp )
{
  cmFfRC_t rc;
  if((rc = _cmFfRead(p,vp,sizeof(*vp))) != kOkFfRC )
    return rc;

  *vp = _cmFfSwap32(p->swapFl,*vp);
  return kOkFfRC;
}

cmFfRC_t _cmFfReadDouble( cmFf_t* p, double* vp )
{
  cmFfRC_t rc;
  unsigned long long v;
  if((rc = _cmFfRead(p,&v,sizeof(v))) != kOkFfRC )
    return rc;

  *vp = _cmFfRdSwapD(p->swapFl,v);

  return rc;
}


cmFfRC_t _cmFfTell( cmFf_t* p, off_t* offsPtr )
{
  if((*offsPtr = ftello( p->fp )) == -1 )
    return _cmFfError( p, kFileTellFailFfRC, errno, "File tell failed.");

  return kOkFfRC;
}

cmFfRC_t _cmFfSeek( cmFf_t* p, int whence, off_t offset )
{
  //if( p->writeFl )
  //  return _cmFfError( p, kInvalidFileModeFfRC, 0, "Cannot seek on file opened for writing.");

  if(fseeko(p->fp, offset, whence) != 0 )
    return _cmFfError( p, kFileSeekFailFfRC, errno, "File seek failed.");

  return kOkFfRC;
}


//-------------------------------------------------------------------------------------------

// append a _cmFfOffs_t record to a _cmFfOffsList
void _cmFfAppendOffsList( cmFf_t* p, _cmFfOffsList_t* lp, unsigned frmIdx, unsigned offs )
{
  _cmFfOffs_t*  op = (_cmFfOffs_t*)cmLHeapAllocZ( p->lhH, sizeof(_cmFfOffs_t) );

  op->frmIdx     = frmIdx;
  op->offs       = offs;

  if( lp->end != NULL )
    lp->end->linkPtr = op;
  else
  {
    assert( lp->beg == NULL );
  }

  lp->end = op;

  if( lp->beg == NULL )
  {
    assert( lp->end == op );
    lp->beg = op;
  }

  ++lp->cnt;
}

// locate a ToC record in a ToC list
_cmFfToC_t* _cmFfFindToCPtr( cmFf_t* p, _cmFfToC_t* cp, unsigned streamId, unsigned mtxType, unsigned mtxUnitsId, unsigned mtxFmtId )
{

  while( cp != NULL )
  {
    if( cp->streamId==streamId && cp->mtxType==mtxType && cp->mtxUnitsId==mtxUnitsId && cp->mtxFmtId==mtxFmtId )
      break;

    cp = cp->linkPtr;
  }

  return cp;
}


cmFfRC_t _cmFfAppendToC( cmFf_t* p, _cmFfToC_t** tocPtrPtr, unsigned streamId, unsigned mtxType, unsigned mtxUnitsId, unsigned mtxFmtId, unsigned absFrameIdx, off_t fileOffset )
{
  cmFfRC_t    rc     = kOkFfRC;
  _cmFfToC_t* tocPtr = *tocPtrPtr;
  _cmFfToC_t* cp;

  // use p->eofOffset as a flags to prevent appending the TOC matrices themselves to the TOC
  if( p->writeFl && p->eofOffset != cmInvalidIdx )
    return rc;
  
  // find the contents record associated with this matrix stream,type,fmt,units
  if(( cp = _cmFfFindToCPtr(p,tocPtr,streamId,mtxType,mtxUnitsId,mtxFmtId)) == NULL )
  {
    // no existing contents recd was found so create a new one
    cp = (_cmFfToC_t*)cmLHeapAllocZ( p->lhH, sizeof(_cmFfToC_t));
    cp->streamId   = streamId;
    cp->mtxType    = mtxType;
    cp->mtxUnitsId = mtxUnitsId;
    cp->mtxFmtId   = mtxFmtId;
    cp->linkPtr    = tocPtr;
    cp->lastFrmIdx = cmInvalidIdx;

    //printf("create : stream:%i type:0x%x units:%i fmt:%i\n",streamId,mtxType,mtxUnitsId,mtxFmtId);

    *tocPtrPtr = cp;
  }

  assert( p->nxtFrmIdx > 0 );

  // verify that this frame does not have multiple matrixes of the same type
  // (this would result in multiple identical _cmFfOffs_t records being written for the same _cmFfToC_t record)
  if( absFrameIdx == cp->lastFrmIdx )
    rc = _cmFfError( p, kDuplicateMtxIdFfRC, 0, "Duplicate matrix types were found in the same frame: stream:%i type:%i units:%i fmt:%i.",streamId, mtxType, mtxUnitsId, mtxFmtId );
      
  cp->lastFrmIdx = absFrameIdx;

  _cmFfAppendOffsList(p, &cp->offsList, absFrameIdx, fileOffset );

  return rc;  
}

cmFfRC_t _cmFfAppendMtxToC( cmFf_t* p, unsigned streamId, unsigned mtxType, unsigned mtxUnitsId, unsigned mtxFmtId, unsigned absFrameIdx, off_t mtxFileOff )
{ return _cmFfAppendToC(p, &p->mtxToC, streamId, mtxType, mtxUnitsId, mtxFmtId, absFrameIdx, mtxFileOff );  }


cmFfRC_t _cmFfAppendFrameToC( cmFf_t* p, unsigned streamId, unsigned absFrameIdx, off_t frmFileOff )
{ return _cmFfAppendToC(p, &p->frmToC, streamId, kInvalidMId, kInvalidUId, kInvalidFmtId, absFrameIdx, frmFileOff ); }



//-------------------------------------------------------------------


cmFfRC_t   _cmFfWriteOffsList( cmFrameFileH_t h, _cmFfOffsList_t* lp, unsigned mtxId, void** arrayPtrPtr, unsigned* extraV, unsigned extraN )
{
  cmFfRC_t  rc    = kOkFfRC;
  unsigned  i     = 0;
  unsigned  j     = 0;
  unsigned  n     = (extraN + lp->cnt * sizeof(unsigned)) + (lp->cnt * sizeof(unsigned long long));

  // allocate memory
  *arrayPtrPtr = cmMemResizeZ( unsigned, *arrayPtrPtr, n );

  unsigned *idxV = (unsigned*)(*arrayPtrPtr);
  off_t*    offV = (off_t*)(idxV + extraN + lp->cnt);

  // store the extra values
  for(i=0; i<extraN; ++i)
    idxV[i] = extraV[i];

  _cmFfOffs_t* op = lp->beg;

  while( op != NULL )
  {
    idxV[i] = op->frmIdx;
    ++i;

    offV[j] = op->offs;
    ++j;

    op = op->linkPtr;
  }

  assert( i == extraN + lp->cnt );
  assert( j == lp->cnt );


  // write the frame index vector
  if((rc = cmFrameFileWriteMtxUInt(h, mtxId, kInvalidUId, idxV, extraN + lp->cnt, 1 )) != kOkFfRC )
    goto errLabel;

  // write the frame offset vector
  if((rc = cmFrameFileWriteMtxOff_t(h, mtxId, kInvalidUId, offV, lp->cnt, 1 )) != kOkFfRC )
    goto errLabel;

 errLabel:
  return rc;
  
}

cmFfRC_t _cmFfWriteToC( cmFrameFileH_t h, _cmFfToC_t* tocPtr, unsigned* mtxIdPtr, void** memPtr )
{
  cmFfRC_t rc = kOkFfRC;

  // write the mtx offset matrix
  _cmFfToC_t* cp = tocPtr;

  while( cp != NULL )
  {
    enum { hdrN = 4 };

    unsigned hdrV[hdrN];

    // add 4 elements to the frame index vector containing header information 
    hdrV[0] = cp->streamId;
    hdrV[1] = cp->mtxType;
    hdrV[2] = cp->mtxUnitsId;
    hdrV[3] = cp->mtxFmtId;

    //printf("write  : stream:%i type:0x%x units:%i fmt:%i\n",cp->streamId,cp->mtxType,cp->mtxUnitsId,cp->mtxFmtId);

    if((rc = _cmFfWriteOffsList(h,&cp->offsList,*mtxIdPtr,memPtr,hdrV,hdrN)) != kOkFfRC )
      goto errLabel;

    --(*mtxIdPtr);

    cp = cp->linkPtr;
  }

 errLabel:
  return rc;
}

cmFfRC_t _cmFfWriteTocFrame( cmFrameFileH_t h )
{
  cmFfRC_t rc    = kOkFfRC;
  cmFf_t*  p     = _cmFfHandleToPtr(h);
  void*    uV    = NULL;
  unsigned mtxId = kTocMId;

  // seek to the end of the file
  if((rc = _cmFfSeek(p,SEEK_END,0)) != kOkFfRC )
    goto errLabel;

  // store the offset to this frame
  if((rc = _cmFfTell(p,&p->eofOffset)) != kOkFfRC )
    goto errLabel;

  // create the offset data frame
  if((rc = cmFrameFileFrameCreate(h, kTocFrameTId, kTocStreamId, cmInvalidIdx, DBL_MAX)) != kOkFfRC )
    goto errLabel;

  // write the frame offset ToC
  if((rc = _cmFfWriteToC(h, p->frmToC, &mtxId, &uV )) != kOkFfRC )
    goto errLabel;

  // write the mtx offset ToC
  if((rc = _cmFfWriteToC(h, p->mtxToC, &mtxId, &uV )) != kOkFfRC )
    goto errLabel;

  // write the EOF frame
  if((rc = cmFrameFileFrameClose(h)) != kOkFfRC )
    goto errLabel;

  // decrease the frameCnt so that the eof frame is not included in the file header frame count
  //--p->f.frameCnt;
   
 errLabel:
  cmMemPtrFree(&uV);
  return rc;
}

cmFfRC_t _cmFfLoadTocFrame( cmFrameFileH_t h )
{
  cmFf_t*            p          = _cmFfHandleToPtr(h);
  cmFfRC_t           rc         = kOkFfRC;
  const cmFfFrame_t* frmDescPtr = NULL;
  off_t              orgOff;
  unsigned           i,j,k;

  if((rc = _cmFfTell(p,&orgOff)) != kOkFfRC )
    goto errLabel;

  if((rc = _cmFfSeek(p,SEEK_SET,p->eofOffset)) != kOkFfRC )
    goto errLabel;

  if((rc = cmFrameFileFrameNext(h, kTocFrameTId, kTocStreamId )) != kOkFfRC )
  {
    rc = _cmFfError( p, kTocFrameRdFailFfRC, 0, "Error reading EOF frame header.");
    goto errLabel;
  }

  if((rc = cmFrameFileFrameLoad(h, &frmDescPtr)) != kOkFfRC )
  {
    rc = _cmFfError( p, kTocFrameRdFailFfRC, 0, "Error loading EOF frame.");
    goto errLabel;
  }

  for(i=0,j=0; i<frmDescPtr->mtxCnt; i+=2,++j)
  {
    const cmFfMtx_t* frmIdxMtxDescPtr = NULL;
    const cmFfMtx_t* offsMtxDescPtr   = NULL;
    const unsigned*  frmIdxV;
    const off_t*     frmOffV;

    // read the frame index vector
    if((frmIdxV = cmFrameFileMtxUInt(  h, kTocMId-j, kInvalidUId, &frmIdxMtxDescPtr )) == NULL )
    {
      rc =  _cmFfError( p, kTocFrameRdFailFfRC, 0, "Matrix frame index read failed for matrix type id %i.",kTocMId-j);
      goto errLabel;
    }

    // read the offset vector
    if((frmOffV = cmFrameFileMtxOff_t( h, kTocMId-j, kInvalidUId, &offsMtxDescPtr )) == NULL )
    {
      rc =  _cmFfError( p, kTocFrameRdFailFfRC, 0, "Matrix frame offset read failed for matrix type id %i.",kTocMId-j);
      goto errLabel;
    }

    assert( frmIdxMtxDescPtr->rowCnt>=4 && frmIdxMtxDescPtr->rowCnt-4 == offsMtxDescPtr->rowCnt );

    // decode the frame index header
    unsigned streamId   = frmIdxV[0];
    unsigned mtxType    = frmIdxV[1];
    unsigned mtxUnitsId = frmIdxV[2];
    unsigned mtxFmtId   = frmIdxV[3];
    
    // increment the frame index vector passed the header
    frmIdxV += 4;


    bool frmTocFl = mtxType==kInvalidMId && mtxUnitsId==kInvalidUId && mtxFmtId==kInvalidUId;

    for(k=0; k<offsMtxDescPtr->rowCnt && rc==kOkFfRC; ++k)
    {
      if( frmTocFl )
        rc = _cmFfAppendFrameToC(p, streamId, frmIdxV[k], frmOffV[k] );
      else
        rc = _cmFfAppendMtxToC(p, streamId, mtxType, mtxUnitsId, mtxFmtId, frmIdxV[k], frmOffV[k] );
    }
  }

  
  if((rc = _cmFfSeek(p,SEEK_SET,orgOff)) != kOkFfRC )
    goto errLabel;

 errLabel:
  return rc;  
}

//--------------------------------------------------------------------

cmFfRC_t _cmFrameFileFree( cmFf_t* p )
{
  cmFfRC_t rc = kOkFfRC;

  if( p == NULL )
    return rc;

    // free the frame data ptr
  cmMemPtrFree(&p->frame.dataPtr);

  // free the mtx array ptr
  cmMemPtrFree(&p->frame.mtxArray);

  // close the file
  if( p->fp != NULL )
  {
    if( fclose(p->fp) == EOF )
       rc =  _cmFfError(p,kFileCloseFailFfRC,errno,"File close failed.");
    else
      p->fp = NULL;
  }

  cmMemPtrFree(&p->writeMtxMem);

  // release the filename string
  cmMemPtrFree(&p->f.filenameStr);

  cmLHeapDestroy(&p->lhH);
  
  cmMemPtrFree(&p);

  return rc;
}


cmFfRC_t cmFrameFileCreate( cmFrameFileH_t* hPtr, const char* fn, double srate, cmCtx_t* ctx )
{
  cmFfRC_t rc;

  // be sure the handle is not already in use
  if( (rc = cmFrameFileClose(hPtr)) != kOkFfRC )
    return rc;

  unsigned version = 0;
  cmFf_t*  p;

  // allocate the file object
  if((p = cmMemAllocZ( cmFf_t, 1 )) == NULL )
    return _cmFfError(NULL,kMemAllocErrFfRC,0,"Memory allocation failed.");

  cmErrSetup(&p->err,&ctx->rpt,"FrameFile");
  p->ctx = *ctx;

  // create the linked heap
  if( cmLHeapIsValid(p->lhH = cmLHeapCreate( 16384, ctx )) == false )
  {
    rc = _cmFfError( p, kLHeapFailFfRC,0,"Linked heap create failed.");
    goto errLabel;
  }

  // create the output file
  if((p->fp = fopen(fn,"w+b")) == NULL )
  {
    rc = _cmFfError( p,kFileOpenFailFfRC,errno,"Unable to create the file:'%s'.",fn);
    goto errLabel;
  }

  //                type,     byteCnt, frameCnt, , version 
  unsigned v[] = { kFileFfTId, 0,        0,        version };
  if((rc = _cmFfWriteUIntV( p, v, sizeof(v)/sizeof(unsigned) ) ) != kOkFfRC )
    goto errLabel;

  // eof frame offset
  if((rc = _cmFfWriteOff_t( p, p->eofOffset)) != kOkFfRC )
    goto errLabel;

  // file sample rate
  if((rc = _cmFfWriteDouble( p, srate ) ) != kOkFfRC )
    goto errLabel;


  p->writeFl        = true;
  p->fileChkByteCnt = 4 * sizeof(unsigned); // hdr bytes after byteCnt
  p->nxtFrmIdx      = 1;
  p->curFrmIdx      = cmInvalidIdx;
  p->frameOffset    = cmInvalidIdx;
  p->eofOffset      = cmInvalidIdx;
  p->f.frameCnt     = 0;
  p->f.srate        = srate;
  p->f.version      = version;
  p->f.filenameStr  = cmMemResizeStr(p->f.filenameStr,fn);
  p->swapFl         = false;
  hPtr->h           = p;
  
  if((rc = _cmFfTell( p, &p->rewOffset )) != kOkFfRC )
    goto errLabel;

  return rc;

 errLabel:

  _cmFrameFileFree(p);

  return rc;

}

cmFfRC_t cmFrameFileOpen(   cmFrameFileH_t* hPtr, const char* fn, cmCtx_t* ctx, const cmFfFile_t** fileDescPtrPtr )
{
  cmFfRC_t rc = kOkFfRC;
  cmFf_t*  p;
  unsigned fileId;

  if( fileDescPtrPtr != NULL )
    *fileDescPtrPtr = NULL;

  // be sure the handle is not already in use
  if((rc = cmFrameFileClose(hPtr)) != kOkFfRC )
    return rc;

  // allocate the file object
  if((p = cmMemAllocZ( cmFf_t, 1 )) == NULL )
    return _cmFfError(NULL,kMemAllocErrFfRC,0,"Memory allocation failed.");

  cmErrSetup(&p->err,&ctx->rpt,"Frame File");
  p->ctx = *ctx;


  // create the linked heap
  if( cmLHeapIsValid(p->lhH = cmLHeapCreate( 2048, ctx )) == false )
  {
    rc = _cmFfError( p, kLHeapFailFfRC,0,"Linked heap create failed.");
    goto errLabel;
  }

  // open the file for reading
  if((p->fp = fopen(fn,"r+b")) == NULL )
  {
    rc = _cmFfError( p,kFileOpenFailFfRC,errno,"Unable to open the file:'%s'.",fn);
    goto errLabel;
  }

  p->writeFl = false;

  // file type id
  if((rc = _cmFfReadUInt( p, &fileId ) ) != kOkFfRC )
    goto errLabel;

  // verify that this is a frame file
  if( fileId != kFileFfTId )
  {
    if( cmSwap32(fileId) == kFileFfTId )
      p->swapFl = true;
    else
    {
      rc = _cmFfError( p,kNotFrameFileFfRC,0,"'%s' is not a frame file.",fn);
      goto errLabel;
    }
  }

  // file chunk size
  if((rc = _cmFfReadUInt( p, &p->fileChkByteCnt ) ) != kOkFfRC )
    goto errLabel;

  // file frame count
  if((rc = _cmFfReadUInt( p, &p->f.frameCnt ) ) != kOkFfRC )
    goto errLabel;

  // file format version
  if((rc = _cmFfReadUInt( p, &p->f.version ) ) != kOkFfRC )
    goto errLabel;

  // eof offset
  if((rc = _cmFfReadOff_t( p, &p->eofOffset) ) != kOkFfRC )
    goto errLabel;

  // file sample rate
  if((rc = _cmFfReadDouble( p, &p->f.srate ) ) != kOkFfRC )
    goto errLabel;

  p->f.filenameStr = cmMemResizeStr(p->f.filenameStr,fn);
  p->nxtFrmIdx     = 0;
  p->curFrmIdx     = cmInvalidIdx;
  p->frameOffset   = cmInvalidIdx;

  hPtr->h = p;

  if((rc = _cmFfLoadTocFrame(*hPtr)) != kOkFfRC )
    goto errLabel;

  if((rc = _cmFfTell(p,&p->rewOffset)) != kOkFfRC )
    goto errLabel;
  
  if( fileDescPtrPtr != NULL )
    *fileDescPtrPtr = &p->f;

  return rc;

 errLabel:

  _cmFrameFileFree(p);

  hPtr->h = NULL;

  return rc;

}


cmFfRC_t cmFrameFileClose( cmFrameFileH_t* hp )
{
  cmFfRC_t rc;

  if( hp== NULL || cmFrameFileIsValid(*hp)==false)
    return kOkFfRC;

  cmFf_t* p = _cmFfHandleToPtr(*hp);

  if( p->fp != NULL )
  {

    // update the file header
    if( p->writeFl )
    {
      if((rc = _cmFfWriteTocFrame(*hp)) != kOkFfRC )
        return rc;

      // rewind into the file header
      if((rc = _cmFfSeek( p, SEEK_SET, sizeof(unsigned) )) != kOkFfRC )
        return rc;

      // update the file chunk size
      if((rc = _cmFfWriteUInt( p, p->fileChkByteCnt ) ) != kOkFfRC )
        return rc;

      // update the file frame count
      if((rc = _cmFfWriteUInt( p, p->f.frameCnt ) ) != kOkFfRC )
        return rc;      

      // rewrite the version
      if((rc = _cmFfWriteUInt( p, p->f.version ) ) != kOkFfRC )
        return rc;      

      // update the eof frame offset
      if((rc = _cmFfWriteOff_t(p, p->eofOffset ) ) != kOkFfRC )
        return rc;
    }
  }   

  _cmFrameFileFree(p);

  hp->h = NULL;

  return kOkFfRC;
}

bool     cmFrameFileIsValid( cmFrameFileH_t h )
{  return h.h != NULL; }

const cmFfFile_t* cmFrameFileDesc( cmFrameFileH_t h )
{
  cmFf_t* p = _cmFfHandleToPtr(h);
  return &p->f;
}

unsigned cmFrameFileFrameCount( cmFrameFileH_t h, unsigned streamId )
{
  cmFf_t*    p  = _cmFfHandleToPtr(h);
  _cmFfToC_t* cp = p->frmToC;

  while(cp != NULL )
  {
    if( cp->streamId == streamId )
      return cp->offsList.cnt;

    cp = cp->linkPtr;
  }

  return 0;
}

cmFfRC_t cmFrameFileFrameCreate( cmFrameFileH_t h, unsigned frameType, unsigned streamId, unsigned sampleIdx, double secs  )
{
  cmFfRC_t rc;
  cmFf_t*  p     = _cmFfHandleToPtr(h);
  unsigned flags = sampleIdx == -1 ? kSecondsTimeFl : kSampleIdxTimeFl;

  if( p->writeFl == false )
    return _cmFfError( p, kInvalidFileModeFfRC, 0, "Cannot create new frames on frame files opened in read mode.");


  // save the frame offset for later use in cmFrameFileCloseFrame()
  if((rc = _cmFfTell(p,&p->frameOffset)) != kOkFfRC )
    return rc;

  // update the frame offset list
  assert( p->nxtFrmIdx > 0 );
  rc = _cmFfAppendFrameToC(p, streamId, p->nxtFrmIdx-1, p->frameOffset );


  // frame:         type,     byteCnt, mtxCnt, streamId, flags, sampleIdx
  unsigned v[] = { frameType, 0,       0,      streamId, flags, sampleIdx };
  if((rc = _cmFfWriteUIntV( p, v, sizeof(v)/sizeof(unsigned) ) ) != kOkFfRC )
    return rc;

  if((rc = _cmFfWriteDouble( p, secs)) != kOkFfRC )
    return rc;
  
  p->frame.f.type         = frameType;
  p->frame.byteCnt        = 6 * sizeof(unsigned);
  p->frame.f.mtxCnt       = 0;
  p->frame.f.streamId     = streamId;
  p->frame.f.flags        = flags;
  p->frame.f.tm.seconds = 0;

  return rc;
}

cmFfRC_t cmFrameFileFrameClose(  cmFrameFileH_t h )
{
  cmFfRC_t rc = kOkFfRC;

  if( h.h == NULL )
    return kOkFfRC;

  cmFf_t*  p     = _cmFfHandleToPtr(h);

  // frames open in read-mode do not need to be closed
  if( p->writeFl == false )
    return kOkFfRC;

  assert( p->frameOffset != 0 );

  // store the current file position
  off_t offs; // = ftello(p->fp);
  if((rc = _cmFfTell(p,&offs)) != kOkFfRC )
    return rc;

  // seek to the frame byte count
  if((rc = _cmFfSeek( p, SEEK_SET, p->frameOffset+sizeof(unsigned) )) != kOkFfRC )
    return rc;

  // write frame byteCnt
  if((rc = _cmFfWriteUInt( p, p->frame.byteCnt ) ) != kOkFfRC )
    return rc;

  // write frame mtxCnt
  if((rc = _cmFfWriteUInt( p, p->frame.f.mtxCnt ) ) != kOkFfRC )
    return rc;

  p->f.frameCnt++;
  p->nxtFrmIdx++;
  p->frameOffset = 0;

  p->frame.byteCnt = 0;
  memset( &p->frame.f, 0, sizeof(p->frame.f));

  

  // jump back to the end of the file
  return _cmFfSeek(p, SEEK_SET, offs );

}


cmFfRC_t _cmFrameFileWriteMtx(    cmFf_t* p, unsigned type, unsigned unitsId, unsigned fmtId, const void* dataPtr, unsigned rn, unsigned cn, bool writeTocFl )
{
  cmFfRC_t rc;

  // track the file offset to this matrix
  if( p->writeFl && writeTocFl )
  {
    off_t fileOff;

    // get file offs to this mtx
    if((rc = _cmFfTell(p,&fileOff)) != kOkFfRC )
      return rc;

    assert( p->nxtFrmIdx >= 1 );

    // append a recd representing this matrix to the mtx TOC
    rc = _cmFfAppendMtxToC(p, p->frame.f.streamId, type, unitsId, fmtId, p->nxtFrmIdx-1, fileOff );
  }

  unsigned wordByteCnt = _cmFfIdToFmtPtr(fmtId)->wordByteCnt; 
  unsigned byteCnt     = rn*cn*wordByteCnt;


  // write the mtx header
  // mtx:          type, byteCnt, fmtId, unitsId, rowCnt, colCnt
  unsigned v[] = { type, byteCnt, fmtId, unitsId, rn,     cn };
  if((rc = _cmFfWriteUIntV( p, v, sizeof(v)/sizeof(unsigned))) != kOkFfRC )
    return rc;

  const void*   src_buf = dataPtr;

  if( p->swapFl )
  {
    p->writeMtxMem = cmMemResize( char, p->writeMtxMem, byteCnt );
    src_buf = _cmFfSwapVector(p->writeMtxMem,src_buf,rn*cn,wordByteCnt);
  }

  // write the mtx data
  if(( rc = _cmFfWrite(p,src_buf,byteCnt)) != kOkFfRC )
    return rc;


  // write pad - all matrices must end on 64 bit boundaries
  unsigned n = byteCnt % 8;

  if( n )
  {
    assert( n < 8 );

    char v[8];
    memset(v,0,8);
    if(( rc = _cmFfWrite(p,v,n)) != kOkFfRC )
      return rc;

  }

  ++p->frame.f.mtxCnt;

  return kOkFfRC;
}

cmFfRC_t cmFrameFileWriteMtx(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, unsigned dataFmtId, const void* dataPtr, unsigned rn, unsigned cn )
{
  cmFf_t* p = _cmFfHandleToPtr(h);

  return _cmFrameFileWriteMtx(p, mtxType, unitsId, dataFmtId, dataPtr, rn, cn, true );
}

cmFfRC_t cmFrameFileWriteMtxUChar(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const unsigned char* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kUCharFmtId, dataPtr, rn, cn ); }

cmFfRC_t cmFrameFileWriteMtxChar(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const char* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kCharFmtId, dataPtr, rn, cn ); }

cmFfRC_t cmFrameFileWriteMtxUShort(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const unsigned short* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kUShortFmtId, dataPtr,rn,cn ); }

cmFfRC_t cmFrameFileWriteMtxShort(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const short* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kShortFmtId, dataPtr, rn, cn ); }

cmFfRC_t cmFrameFileWriteMtxULong(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const unsigned long* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kULongFmtId, dataPtr, rn, cn );}

cmFfRC_t cmFrameFileWriteMtxLong(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const long* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kLongFmtId, dataPtr, rn, cn );}

cmFfRC_t cmFrameFileWriteMtxUInt(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const unsigned* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kUIntFmtId, dataPtr, rn, cn );}

cmFfRC_t cmFrameFileWriteMtxInt(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const int* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kIntFmtId, dataPtr, rn, cn );}

cmFfRC_t cmFrameFileWriteMtxULLong(  cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const unsigned long long* dataPtr, unsigned rn, unsigned cn )
{  return cmFrameFileWriteMtx( h, mtxType, unitsId, kULLongFmtId, dataPtr, rn, cn );}

cmFfRC_t cmFrameFileWriteMtxLLong(   cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const long long*  dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kLLongFmtId, dataPtr, rn, cn );}

cmFfRC_t cmFrameFileWriteMtxOff_t(   cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const off_t*  dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kOff_tFmtId, dataPtr, rn, cn );}

cmFfRC_t cmFrameFileWriteMtxFloat(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const float* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kFloatFmtId, dataPtr, rn, cn ); }

cmFfRC_t cmFrameFileWriteMtxDouble(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const double* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kDoubleFmtId, dataPtr, rn, cn ); }

cmFfRC_t cmFrameFileWriteMtxBlob(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const void* dataPtr, unsigned rn, unsigned cn )
{ return cmFrameFileWriteMtx( h, mtxType, unitsId, kBlobFmtId, dataPtr, rn, cn ); }


cmFfRC_t cmFrameFileWriteMtxStringZ(    cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, const char* stringPtr )
{ 
  unsigned n = strlen(stringPtr);
  return cmFrameFileWriteMtx( h, mtxType, kInvalidUId, kStringZFmtId, stringPtr, n+1, 1 ); 
}

cmFfRC_t cmFrameFileWriteMtxJson(    cmFrameFileH_t h, unsigned mtxType, cmJsonH_t jsH, const cmJsonNode_t* nodePtr )
{
  cmFf_t*  p          = _cmFfHandleToPtr(h);
  void*    buf        = NULL;
  unsigned bufByteCnt = 0;

  if( cmJsonSerializeTree( jsH, nodePtr, &buf, &bufByteCnt ) != kOkJsRC )
    return _cmFfError(p,kJsonFailFfRC,0,"JSON serialuze failed.");

  return _cmFrameFileWriteMtx(p, mtxType, kNoUnitsUId, kJsonFmtId, buf, 1, bufByteCnt, true );
}

// Can only be called when p->fp is pointed to the beginning of a frame.
// Leaves file pointing to frame header 'flags' field.
cmFfRC_t _cmFrameFileFrameSeek( cmFf_t* p, unsigned keyFrameTypeId, unsigned keyFrameStreamId )
{
  cmFfRC_t rc = kOkFfRC;

  while( rc == kOkFfRC )
  {
    // frame type
    if((rc = _cmFfReadUInt(p,&p->frame.f.type)) != kOkFfRC )
      break;

    // frame byte count
    if((rc = _cmFfReadUInt(p,&p->frame.byteCnt)) != kOkFfRC )
      break;

    // frame mtx count
    if((rc = _cmFfReadUInt(p,&p->frame.f.mtxCnt)) != kOkFfRC )
      return rc;

    // frame stream id
    if((rc = _cmFfReadUInt(p,&p->frame.f.streamId)) != kOkFfRC )
      break;

    // condition: no match on type
    if( (keyFrameTypeId == kInvalidFrameTId) && (keyFrameStreamId == kInvalidFrameTId || keyFrameStreamId == p->frame.f.streamId) )
      break;

    // condition: match on type
    if( (keyFrameTypeId == p->frame.f.type)  && (keyFrameStreamId == kInvalidFrameTId || keyFrameStreamId == p->frame.f.streamId) )
      break;    

    // goto the next frame
    if((rc = _cmFfSeek(p,SEEK_CUR,p->frame.byteCnt - (2*sizeof(unsigned)))) != kOkFfRC )
      break;

    ++p->nxtFrmIdx;
  }

  return rc;

}

cmFfRC_t cmFrameFileRewind( cmFrameFileH_t h )
{
  cmFf_t* p = _cmFfHandleToPtr(h);

  p->nxtFrmIdx = 0;

  return _cmFfSeek(p,SEEK_SET,p->rewOffset);
}

cmFfRC_t cmFrameFileSeek( cmFrameFileH_t h, unsigned streamId, unsigned frameIdx )
{
  cmFfRC_t    rc = kOkFfRC;
  cmFf_t*     p  = _cmFfHandleToPtr(h);
  unsigned    i  = 0;
  _cmFfToC_t* tocPtr;



  // locate the frame TOC recd assoc'd with stream id
  if((tocPtr =  _cmFfFindToCPtr(p, p->frmToC, streamId, kInvalidMId, kInvalidUId, kInvalidFmtId )) == NULL )
  {
    rc = _cmFfError(p,kTocRecdNotFoundFfRC,0,"Unable to locate the TOC record for stream id %i.",streamId);
    goto errLabel;
  }

  // locate the TOC offset recd assoc'd with frameIdx
  _cmFfOffs_t* cp = tocPtr->offsList.beg;
  for(; cp != NULL && i!=frameIdx; ++i )
    cp = cp->linkPtr;


  // if the frame index was not valid
  if( cp == NULL )
  {
    rc = _cmFfError(p,kInvalidFrameIdxFfRC,0,"%i is an invalid frame index for stream id %i.",frameIdx,streamId);
    goto errLabel;
  }

  // seek to the beginning of the frame
  if((rc = _cmFfSeek(p,SEEK_SET,cp->offs)) != kOkFfRC )
    goto errLabel;

 errLabel:
  return rc;
}


// Can only be called when p->fp is pointed to the beginning of a frame.
cmFfRC_t cmFrameFileFrameNext( cmFrameFileH_t h, unsigned keyFrameTypeId, unsigned keyFrameStreamId  )
{
  cmFfRC_t rc;
  cmFf_t*  p  = _cmFfHandleToPtr(h);
  unsigned sampleIdx;
  double   seconds;

  // go to the requested frame
  if((rc = _cmFrameFileFrameSeek(p, keyFrameTypeId, keyFrameStreamId)) != kOkFfRC )
    return rc;

  // frame flags
  if((rc = _cmFfReadUInt(p,&p->frame.f.flags)) != kOkFfRC )
    return rc;

  // frame sample idx
  if((rc = _cmFfReadUInt(p,&sampleIdx)) != kOkFfRC )
    return rc;

  // frame seconds
  if((rc = _cmFfReadDouble(p,&seconds)) != kOkFfRC )
    return rc;

  if( cmIsFlag(p->frame.f.flags,kSampleIdxTimeFl) )
    p->frame.f.tm.sampleIdx = sampleIdx;

  if( cmIsFlag(p->frame.f.flags,kSecondsTimeFl) )
    p->frame.f.tm.seconds = seconds;

  
  return rc;
}

cmFfRC_t  _cmFrameFileCheckForDuplicateMtxId( cmFrameFileH_t h )
{
  cmFfRC_t rc = kOkFfRC;
  cmFf_t*  p  = _cmFfHandleToPtr(h);
  unsigned i;

  for(i=0; i<p->frame.f.mtxCnt; ++i)
  {
    unsigned mtxIdx = cmFrameFileMtxIndex(h,  p->frame.mtxArray[i].m.type, p->frame.mtxArray[i].m.unitsId, p->frame.mtxArray[i].m.fmtId );

    assert( mtxIdx != cmInvalidIdx );

    if( mtxIdx != i )
    {
      rc = _cmFfError( p, kDuplicateMtxIdFfRC,  0, "Duplicate matrix signatures exist form type:%i units:%i fmt:%i at frame index %i.", p->frame.mtxArray[i].m.type, p->frame.mtxArray[i].m.unitsId, p->frame.mtxArray[i].m.fmtId,p->nxtFrmIdx );
      goto errLabel;
    }
    
  }

 errLabel:
  return rc;
}

// read a matrix header and data
cmFfRC_t _cmFfReadMtx( cmFf_t* p, _cmFfMtx_t* mp, void* buf,  unsigned bufByteCnt )
{
  cmFfRC_t rc;

  if((rc = _cmFfReadUInt(p,&mp->m.type)) != kOkFfRC )
    goto errLabel;

  if((rc = _cmFfReadUInt(p,&mp->byteCnt)) != kOkFfRC )
    goto errLabel;

  if((rc = _cmFfReadUInt(p,&mp->m.fmtId)) != kOkFfRC )
    goto errLabel;

  if((rc = _cmFfReadUInt(p,&mp->m.unitsId)) != kOkFfRC )
    goto errLabel;

  if((rc = _cmFfReadUInt(p,&mp->m.rowCnt)) != kOkFfRC )
    goto errLabel;

  if((rc = _cmFfReadUInt(p,&mp->m.colCnt)) != kOkFfRC )
    goto errLabel;

  if( buf != NULL )
  {
    if( mp->byteCnt > bufByteCnt )
    {
      rc = _cmFfError(p,kBufTooSmallFfRC,0, "Matrix buffer too small to complete the read.");
      goto errLabel;
    }

    // read in the mtx data
    if((rc = _cmFfRead(p,buf,mp->byteCnt)) != kOkFfRC )
      goto errLabel;


    if( p->swapFl )
    {
      // swap on read
      _cmFfSwapVector(buf,buf,mp->m.rowCnt*mp->m.colCnt, _cmFfIdToFmtPtr(mp->m.fmtId)->wordByteCnt );
    }

  }


 errLabel:
  return rc;
}

cmFfRC_t           cmFrameFileFrameLoad(     cmFrameFileH_t h, const cmFfFrame_t** frameDescPtrPtr )
{
  cmFfRC_t rc;
  cmFf_t*  p = _cmFfHandleToPtr(h);
  unsigned i;

  if(frameDescPtrPtr != NULL)
    *frameDescPtrPtr = NULL;

  // store pointer to matrix data offset -  for use in cmFrameFileFrameUpdate()
  if((rc = _cmFfTell(p,&p->frameOffset)) != kOkFfRC )
    goto errLabel;

  // create a block of memory large enough to hold the entire frame
  // (this is more than is actually needed because it includes the mtx header records)
  p->frame.dataPtr  = cmMemResizeZ( char, p->frame.dataPtr, p->frame.byteCnt );

  // create a mtx array to hold each mtx record
  p->frame.mtxArray = cmMemResizeZ( _cmFfMtx_t, p->frame.mtxArray, p->frame.f.mtxCnt );

  char*     dp           = p->frame.dataPtr;
  unsigned  emptyByteCnt = p->frame.byteCnt;

  // for each matrix in this frame
  for(i=0; i<p->frame.f.mtxCnt; ++i)
  {
    _cmFfMtx_t* mp = p->frame.mtxArray + i;

    mp->dataPtr = dp;

    // read the matrix header and data
    if((rc = _cmFfReadMtx(p, mp, dp, emptyByteCnt )) != kOkFfRC )
      goto errLabel;

    // read any pad bytes
    unsigned n = mp->byteCnt % 8;

    if( n )
    {
      char v[8];
      if((rc = _cmFfRead(p,v,n)) != kOkFfRC )
        goto errLabel;
    }

    // verify the buffer size
    if(mp->byteCnt > emptyByteCnt )
    {
      rc = _cmFfError(p,kBufTooSmallFfRC,0, "Matrix buffer too small to complete the read.");
      goto errLabel;
    }
    
    emptyByteCnt -= mp->byteCnt;  // decrement the available buffer space
    dp += mp->byteCnt;            // advance the matrix data buffer pointer

  }

  if(rc==kOkFfRC && frameDescPtrPtr != NULL)
    *frameDescPtrPtr = &p->frame.f;

  // verify that duplicate matrx signatures do not exist. 
  // (only the first of the duplicate  will be accessable)
  assert( _cmFrameFileCheckForDuplicateMtxId(h) == kOkFfRC );
  
  p->curFrmIdx = p->nxtFrmIdx;
  ++p->nxtFrmIdx;


 errLabel:
  return rc;
}

cmFfRC_t   cmFrameFileFrameSkip( cmFrameFileH_t h )
{
  cmFfRC_t rc       = kOkFfRC;
  cmFf_t*  p        = _cmFfHandleToPtr(h);
  unsigned hdrBytes = 32 - 8; // sizeof(frame hdr) - (sizeof(hdr.type) + sizeof(hdr.chkbyteCnt))

  assert(hdrBytes<=p->frame.byteCnt);

  if((rc = _cmFfSeek( p, SEEK_CUR, p->frame.byteCnt  - hdrBytes)) == kOkFfRC )
  {
    ++p->nxtFrmIdx;
  }

  return rc;
}

cmFfRC_t           cmFrameFileFrameLoadNext( cmFrameFileH_t h, unsigned frameTypeId, unsigned streamId, const cmFfFrame_t** frameDescPtrPtr )
{
  cmFfRC_t rc;
  if((rc = cmFrameFileFrameNext(h,frameTypeId,streamId)) != kOkFfRC )
    return rc;

  return cmFrameFileFrameLoad(h,frameDescPtrPtr);
}

cmFfRC_t           cmFrameFileFrameUpdate( cmFrameFileH_t h )
{
  cmFfRC_t rc = kOkFfRC;
  cmFf_t*  p  = _cmFfHandleToPtr(h);
  unsigned i    = 0;
  off_t    offs; 

  if((rc = _cmFfTell(p,&offs)) != kOkFfRC )
    goto errLabel;

  // seek to the matrix data
  if((rc = _cmFfSeek(p, SEEK_SET, p->frameOffset )) != kOkFfRC )
    goto errLabel;

  // for each matrix
  for(i=0; i<p->frame.f.mtxCnt; ++i)
  {
    const _cmFfMtx_t* m = p->frame.mtxArray + i;

    // rewrite each matrix
    if((rc = _cmFrameFileWriteMtx(p, m->m.type, m->m.unitsId, m->m.fmtId, m->dataPtr, m->m.rowCnt, m->m.colCnt, false )) != kOkFfRC )
      goto errLabel;

    // cmFrameFileWriteMtx increments the matrix count - so we decrement it here
    --p->frame.f.mtxCnt;
  }

  // restore the file position
  if((rc = _cmFfSeek(p, SEEK_SET, offs )) != kOkFfRC )
    goto errLabel;

 errLabel:
  return rc;
  
}

const cmFfFrame_t* cmFrameFileFrameDesc( cmFrameFileH_t h )
{
  cmFf_t* p = _cmFfHandleToPtr(h);
  return &p->frame.f;
}

unsigned           cmFrameFileFrameLoadedIndex( cmFrameFileH_t h )
{
  cmFf_t* p = _cmFfHandleToPtr(h);
  return p->curFrmIdx;
  
}

unsigned  cmFrameFileMtxIndex(      cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId, unsigned fmtId )
{
  cmFf_t*  p = _cmFfHandleToPtr(h); 
  unsigned i;

  for(i=0; i<p->frame.f.mtxCnt; ++i)
  {
    if( mtxTypeId==kInvalidMId || mtxTypeId == p->frame.mtxArray[i].m.type )
      if( unitsId==kInvalidUId || unitsId == p->frame.mtxArray[i].m.unitsId )
        if( fmtId==kInvalidFmtId || fmtId == p->frame.mtxArray[i].m.fmtId )
          return i;
    
  }

  return cmInvalidIdx;
}

const cmFfMtx_t* cmFrameFileMtxDesc(   cmFrameFileH_t h, unsigned mtxIdx )
{ cmFf_t* p = _cmFfHandleToPtr(h);
  assert( mtxIdx < p->frame.f.mtxCnt );
  return &p->frame.mtxArray[ mtxIdx ].m;
}

void* _cmFrameFileMtxIndexDataPtr( cmFrameFileH_t h, unsigned dataFmtId, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{
  if( mtxIdx == cmInvalidIdx )
    return NULL;

  cmFf_t* p =  _cmFfHandleToPtr(h);

  assert( mtxIdx < p->frame.f.mtxCnt );
  assert( p->frame.mtxArray[mtxIdx].m.fmtId == dataFmtId );

  if( descPtrPtr != NULL )
    *descPtrPtr = &p->frame.mtxArray[ mtxIdx ].m;

  return p->frame.mtxArray[mtxIdx].dataPtr;
}


  
unsigned char*  cmFrameFileMtxIndexUChar( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (unsigned char*)_cmFrameFileMtxIndexDataPtr( h, kUCharFmtId, mtxIdx, descPtrPtr ); }

char*           cmFrameFileMtxIndexChar( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (char*)_cmFrameFileMtxIndexDataPtr( h, kCharFmtId, mtxIdx, descPtrPtr ); }

unsigned short* cmFrameFileMtxIndexUShort( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{  return (unsigned short*) _cmFrameFileMtxIndexDataPtr( h, kUShortFmtId, mtxIdx, descPtrPtr ); }

short*          cmFrameFileMtxIndexShort( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (short*)_cmFrameFileMtxIndexDataPtr( h, kShortFmtId, mtxIdx, descPtrPtr ); }

unsigned long*  cmFrameFileMtxIndexULong( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (unsigned long*)_cmFrameFileMtxIndexDataPtr( h, kULongFmtId, mtxIdx, descPtrPtr ); }

long*           cmFrameFileMtxIndexLong( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (long*) _cmFrameFileMtxIndexDataPtr( h, kLongFmtId, mtxIdx, descPtrPtr ); }

unsigned*  cmFrameFileMtxIndexUInt( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (unsigned*) _cmFrameFileMtxIndexDataPtr( h, kUIntFmtId, mtxIdx, descPtrPtr ); }

int*           cmFrameFileMtxIndexInt( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (int*) _cmFrameFileMtxIndexDataPtr( h, kIntFmtId, mtxIdx, descPtrPtr ); }

unsigned long long*  cmFrameFileMtxIndexULLong( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (unsigned long long*) _cmFrameFileMtxIndexDataPtr( h, kULLongFmtId, mtxIdx, descPtrPtr ); }

long long*           cmFrameFileMtxIndexLLong( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (long long*) _cmFrameFileMtxIndexDataPtr( h, kLLongFmtId, mtxIdx, descPtrPtr ); }

off_t*           cmFrameFileMtxIndexOff_t( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (off_t*) _cmFrameFileMtxIndexDataPtr( h, kOff_tFmtId, mtxIdx, descPtrPtr ); }

float*          cmFrameFileMtxIndexFloat( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (float*)_cmFrameFileMtxIndexDataPtr( h, kFloatFmtId, mtxIdx, descPtrPtr ); }

double*         cmFrameFileMtxIndexDouble( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (double*)_cmFrameFileMtxIndexDataPtr( h, kDoubleFmtId, mtxIdx, descPtrPtr ); }

char*           cmFrameFileMtxIndexStringZ( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return (char*)_cmFrameFileMtxIndexDataPtr( h, kStringZFmtId, mtxIdx, descPtrPtr ); }

void*           cmFrameFileMtxIndexBlob( cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{ return _cmFrameFileMtxIndexDataPtr( h, kBlobFmtId, mtxIdx, descPtrPtr );} 

cmJsonH_t        cmFrameFileMtxIndexJson(    cmFrameFileH_t h, unsigned mtxIdx, const cmFfMtx_t** descPtrPtr )
{
  cmFfRC_t         rc  = kOkFfRC;
  const void*      buf;
  const cmFfMtx_t* dp  = NULL;
  cmJsRC_t         jsRC;
  cmJsonH_t        jsH = cmJsonNullHandle;
  cmFf_t*          p   =  _cmFfHandleToPtr(h);

  if( descPtrPtr != NULL )
    *descPtrPtr = NULL;

  if( (buf= _cmFrameFileMtxIndexDataPtr( h, kJsonFmtId, mtxIdx, &dp)) == NULL )
    goto errLabel;

  if((jsRC = cmJsonInitialize( &jsH, &p->ctx  )) != kOkJsRC )
  {
    rc = _cmFfError(p,kJsonFailFfRC,0,"JSON object allocation failed.");
    goto errLabel;
  }
    
  if((jsRC = cmJsonDeserialize( jsH, buf, NULL )) != kOkJsRC )
  {
    rc = _cmFfError(p, kJsonFailFfRC, 0, "JSON deserialization failed.");
    goto errLabel;
  }

 errLabel:

  if( rc != kOkFfRC )
    cmJsonFinalize(&jsH);
  else
    if( descPtrPtr != NULL )
      *descPtrPtr = dp;

  return jsH;
}



unsigned char*  cmFrameFileMtxUChar( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (unsigned char*)_cmFrameFileMtxIndexDataPtr( h, kUCharFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId, kUCharFmtId), descPtrPtr ); }

char*           cmFrameFileMtxChar( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (char*)_cmFrameFileMtxIndexDataPtr( h, kCharFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kCharFmtId), descPtrPtr ); }

unsigned short* cmFrameFileMtxUShort( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (unsigned short*)_cmFrameFileMtxIndexDataPtr( h, kUShortFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kUShortFmtId), descPtrPtr); }

short*          cmFrameFileMtxShort( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (short*)_cmFrameFileMtxIndexDataPtr( h, kShortFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kShortFmtId), descPtrPtr); }

unsigned long*  cmFrameFileMtxULong( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (unsigned long*)_cmFrameFileMtxIndexDataPtr( h, kULongFmtId,  cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kULongFmtId), descPtrPtr ); }

long*           cmFrameFileMtxLong( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (long*)_cmFrameFileMtxIndexDataPtr( h, kLongFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kLongFmtId), descPtrPtr ); }

unsigned*       cmFrameFileMtxUInt( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (unsigned*)_cmFrameFileMtxIndexDataPtr( h, kUIntFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kUIntFmtId), descPtrPtr ); }

int*            cmFrameFileMtxInt( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (int*)_cmFrameFileMtxIndexDataPtr( h, kIntFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kIntFmtId), descPtrPtr ); }

unsigned long long*  cmFrameFileMtxULLong( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (unsigned long long*)_cmFrameFileMtxIndexDataPtr( h, kULLongFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kULLongFmtId), descPtrPtr ); }

long long*      cmFrameFileMtxLLong( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (long long*)_cmFrameFileMtxIndexDataPtr( h, kLLongFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kLLongFmtId), descPtrPtr ); }

off_t*      cmFrameFileMtxOff_t( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (off_t*)_cmFrameFileMtxIndexDataPtr( h, kOff_tFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kOff_tFmtId), descPtrPtr ); }

float*          cmFrameFileMtxFloat( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (float*)_cmFrameFileMtxIndexDataPtr( h, kFloatFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kFloatFmtId), descPtrPtr ); }

double*         cmFrameFileMtxDouble( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (double*)_cmFrameFileMtxIndexDataPtr( h, kDoubleFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kDoubleFmtId), descPtrPtr ); }

char*           cmFrameFileMtxStringZ( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return (char*)_cmFrameFileMtxIndexDataPtr( h, kStringZFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kStringZFmtId), descPtrPtr ); }

void*           cmFrameFileMtxBlob( cmFrameFileH_t h, unsigned mtxTypeId, unsigned unitsId,  const cmFfMtx_t** descPtrPtr )
{ return _cmFrameFileMtxIndexDataPtr( h, kBlobFmtId, cmFrameFileMtxIndex(h,mtxTypeId,unitsId,kBlobFmtId), descPtrPtr ); }

cmJsonH_t       cmFrameFileMtxJson(    cmFrameFileH_t h, unsigned mtxTypeId, const cmFfMtx_t** descPtrPtr )
{ return cmFrameFileMtxIndexJson(h, cmFrameFileMtxIndex(h,mtxTypeId,kNoUnitsUId,kJsonFmtId), descPtrPtr ); }


cmFfRC_t cmFrameFileMtxSize( cmFrameFileH_t h, unsigned streamId, unsigned mtxType, unsigned unitsId, unsigned fmtId, unsigned* frmCntPtr, unsigned* rowCntPtr, unsigned* colCntPtr, unsigned* eleCntPtr )
{
  cmFfRC_t     rc = kOkFfRC;
  cmFf_t*      p  = _cmFfHandleToPtr(h);
  _cmFfToC_t*  tocPtr;
  _cmFfOffs_t* op;
  _cmFfMtx_t   mtx;

  *frmCntPtr = 0;
  *eleCntPtr = 0;
  *rowCntPtr = 0;
  *colCntPtr = 0;

  if((tocPtr = _cmFfFindToCPtr(p, p->mtxToC, streamId, mtxType, unitsId, fmtId )) == NULL )
  {
    rc = _cmFfError( p, kTocRecdNotFoundFfRC, 0, "Unable to locate the requested matrix in stream:%i mtx:%i units:%i fmt:%i.",streamId, mtxType, unitsId, fmtId );
    goto errLabel;
  }

  op = tocPtr->offsList.beg;

  while(op != NULL )
  {
    if((rc = _cmFfSeek(p,SEEK_SET, op->offs )) != kOkFfRC )
      goto errLabel;

    if((rc = _cmFfReadMtx(p,&mtx,NULL,0)) != kOkFfRC )
      goto errLabel;

    *frmCntPtr += 1;

    *eleCntPtr += mtx.m.rowCnt * mtx.m.colCnt;

    if( mtx.m.rowCnt > *rowCntPtr )
      *rowCntPtr = mtx.m.rowCnt;

    if( mtx.m.colCnt > *colCntPtr )
      *colCntPtr = mtx.m.colCnt;

    op = op->linkPtr;
  }

 errLabel:
  return rc;
}

cmFfRC_t _cmFrameFileMtxLoad(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned fmtId, unsigned frmIdx, unsigned frmCnt, void* buf, unsigned bufEleCnt, unsigned* outCntPtr )
{
  cmFfRC_t    rc          = kOkFfRC;
  cmFf_t*     p           = _cmFfHandleToPtr(h);
  char*       dp          = buf;
  unsigned    wordByteCnt = _cmFfIdToFmtPtr(fmtId)->wordByteCnt;
  int         dpn         = bufEleCnt*wordByteCnt;
  _cmFfToC_t* tocPtr;
  _cmFfMtx_t  mtx;
  unsigned    fi;

  if( outCntPtr != NULL )
    *outCntPtr = 0;

  if((tocPtr = _cmFfFindToCPtr(p, p->mtxToC, streamId, mtxTypeId, unitsId, fmtId )) == NULL )
  {
    rc = _cmFfError( p, kTocRecdNotFoundFfRC, 0, "Unable to locate the requested matrix in stream:%i mtx:%i units:%i fmt:%i.",streamId, mtxTypeId, unitsId, fmtId );
    goto errLabel;
  }
  
  _cmFfOffs_t* op = tocPtr->offsList.beg;

  for(fi=0; op != NULL && (frmCnt==-1 || fi<(frmIdx+frmCnt)); ++fi )
  {
    if( frmIdx<=fi )
    {
      if((rc = _cmFfSeek(p,SEEK_SET, op->offs )) != kOkFfRC )
        goto errLabel;

      if((rc = _cmFfReadMtx(p,&mtx,dp,dpn)) != kOkFfRC )
        goto errLabel;

      int readByteCnt =  mtx.m.rowCnt * mtx.m.colCnt * wordByteCnt;

      if( readByteCnt > dpn )
      {
        rc = _cmFfError( p, kBufTooSmallFfRC, 0, "The matrix load buffer is too small.");
        goto errLabel;
      }

      dpn -= readByteCnt;
      dp  += readByteCnt;
    }
    
    op = op->linkPtr;
  }

  if( outCntPtr != NULL )
    *outCntPtr = bufEleCnt -  (dpn/wordByteCnt);

 errLabel:
  return rc;

}

cmFfRC_t cmFrameFileMtxLoadUChar(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  unsigned char*      buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kUCharFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadChar(    cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  char*               buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kCharFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadUShort(  cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  unsigned short*     buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kUShortFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadShort(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  short*              buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kShortFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadULong(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  unsigned long*      buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kULongFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadLong(    cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  long*               buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kLongFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadUInt(    cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  unsigned int*       buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kUIntFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadInt(     cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  int*                buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kIntFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadULLong(  cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  unsigned long long* buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kULLongFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadLLong(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  long long*          buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kLLongFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadFloat(   cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  float*              buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kFloatFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadDouble(  cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  double*             buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kDoubleFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadStringZ( cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  char*               buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kStringZFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }

cmFfRC_t cmFrameFileMtxLoadBlob(    cmFrameFileH_t h, unsigned streamId, unsigned mtxTypeId, unsigned unitsId, unsigned frmIdx, unsigned frmCnt,  void*               buf, unsigned eleCnt, unsigned* outCntPtr )
{ return _cmFrameFileMtxLoad( h, streamId, mtxTypeId, unitsId, kUCharFmtId, frmIdx, frmCnt, buf, eleCnt, outCntPtr ); }


void _cmFrameFilePrint( cmRpt_t* rpt, const char* fmt, ... )
{
  assert(rpt != NULL);

  va_list vl;
  va_start(vl,fmt);
  cmRptVPrintf(rpt,fmt,vl);
  va_end(vl);
}


cmFfRC_t _cmFrameFileMtxReport( const cmFf_t* p, unsigned mtxIdx, cmRpt_t* rpt )
{
  assert( mtxIdx < p->frame.f.mtxCnt );

  const _cmFfMtx_t* mp = p->frame.mtxArray + mtxIdx;

  _cmFrameFilePrint(rpt,"    type:0x%x units:0x%x fmtId:0x%x rowCnt:%i colCnt:%i byteCnt:%i\n",mp->m.type,mp->m.unitsId,mp->m.fmtId,mp->m.rowCnt,mp->m.colCnt,mp->byteCnt);
 
  return kOkFfRC;
}

cmFfRC_t _cmFrameFileFrameReport( const cmFf_t* p, cmRpt_t* rpt )
{
  unsigned i;
  _cmFrameFilePrint(rpt,"type:0x%x mtxCnt:%i flags:0x%x streamId:%i byteCnt:%i\n", p->frame.f.type,p->frame.f.mtxCnt,p->frame.f.flags,p->frame.f.streamId,p->frame.byteCnt);

  for(i=0; i<p->frame.f.mtxCnt; ++i)
    _cmFrameFileMtxReport(p,i,rpt);

  return kOkFfRC;
}

void _cmFrameFileContentsReport(const cmFf_t* p, const _cmFfToC_t* tocPtr, cmRpt_t* rpt )
{
  const _cmFfToC_t* cp = tocPtr;
  unsigned i;

  for(i=0; cp != NULL; ++i )
  {
    bool frmFl = cp->mtxType==kInvalidMId && cp->mtxUnitsId==kInvalidUId && cp->mtxFmtId==kInvalidFmtId;

    _cmFrameFilePrint( rpt, "%i streamId:%i ",i, cp->streamId );

    if( !frmFl )
      _cmFrameFilePrint( rpt, "type:%i units:%i fmt:%i ",cp->mtxType, cp->mtxUnitsId, cp->mtxFmtId );

    _cmFrameFilePrint(  rpt, "cnt:%i\n", cp->offsList.cnt );

    cp = cp->linkPtr;
  }
}

cmFfRC_t cmFrameFileReport( cmFrameFileH_t h, bool summOnlyFl, cmRpt_t* rpt )
{
  cmFfRC_t rc = kOkFfRC;
  cmFf_t* p = _cmFfHandleToPtr(h);

  if(p->writeFl )
    return _cmFfError( p, kInvalidFileModeFfRC, 0, "Cannot report on files opened in write mode.");

  _cmFrameFilePrint(rpt,"frames:%i srate:%f\n",p->f.frameCnt,p->f.srate);

  _cmFrameFilePrint(rpt,"Frame Contents:\n");
  _cmFrameFileContentsReport(p, p->frmToC, rpt );

  _cmFrameFilePrint(rpt,"Matrix Contents:\n");
  _cmFrameFileContentsReport(p, p->mtxToC, rpt );

  if( summOnlyFl )
  {
    unsigned i;

    if((rc = cmFrameFileRewind(h)) != kOkFfRC )
      goto errLabel;

    for(i=0; cmFrameFileFrameLoadNext(h,kInvalidFrameTId,kInvalidStreamId,NULL) == kOkFfRC; ++i)
    {
      _cmFrameFilePrint(rpt,"  %i ",i);
      if((rc = _cmFrameFileFrameReport(p,rpt)) != kOkFfRC )
        break;
    }
  
    assert(i==p->f.frameCnt);
  }
 errLabel:

  return rc;
}

cmFfRC_t cmFrameFileNameReport( const char* fn, bool summOnlyFl, cmCtx_t* ctx )
{
  cmFrameFileH_t h;
  cmFfRC_t rc0,rc1;

  if((rc0 = cmFrameFileOpen( &h, fn, ctx, NULL)) != kOkFfRC )
    return rc0;

  rc0 = cmFrameFileReport(h,summOnlyFl,&ctx->rpt);


  rc1 = cmFrameFileClose(&h);
  return rc0 != kOkFfRC ? rc0 : rc1;
}

/*
void cmFrameFileVTestPrintFunc( void* userDataPtr, const char* fmt, va_list vl )
{
  vfprintf(stdout,fmt,vl);
}
*/


cmFfRC_t _cmFrameFileTestMtx( cmFrameFileH_t h, unsigned mtxType, unsigned unitsId, unsigned mtxCnt, unsigned i, bool modFl )
{
  cmFfRC_t         rc         = kOkFfRC;
  const cmFfMtx_t* mtxDescPtr = NULL;
  unsigned j,k;

  for(j=0; j<mtxCnt; ++j)
  {
    long*   dp  = NULL;
    double* ddp = NULL;

    if( j == 3 )
    {
      if((ddp = cmFrameFileMtxDouble(h, mtxType + j, unitsId, &mtxDescPtr )) == NULL )
      {
        printf("READ ERROR\n");
        goto errLabel;
      }
    } 
    else
    {
      if((dp = cmFrameFileMtxLong(h, mtxType + j, unitsId, &mtxDescPtr )) == NULL )
      {
        printf("READ ERROR\n");
        goto errLabel;
      }
    }

    printf("%2i %2i : ",i,j);

    // print the mtx data
    if( j == 3 )
    {
      for(k=0; k<mtxDescPtr->colCnt*mtxDescPtr->rowCnt; k++)
      {
        printf("%2.0f ",ddp[k]);

        // if pass 1 modify the data
        if( modFl )
          ++ddp[k];
      }

    }
    else
    {
      for(k=0; k<mtxDescPtr->colCnt*mtxDescPtr->rowCnt; k++)
      {
        printf("%2li ",dp[k]);

        // if pass 1 modify the data
        if( modFl )
          ++dp[k];
      }
    }

    printf("\n");
  }

 errLabel:
  return rc;
}

cmFfRC_t cmFrameFileTest2( const char* fn, cmCtx_t* ctx )
{
  cmFfRC_t rc;
  cmFrameFileH_t ffH;
  const cmFfFile_t* descPtr;

  if((rc = cmFrameFileOpen(&ffH, fn, ctx, &descPtr )) != kOkFfRC )
    goto errLabel;


  rc = cmFrameFileClose(&ffH);
 errLabel:
  return rc;
}

cmFfRC_t cmFrameFileTest( const char* fn, cmCtx_t* ctx )
{
  //return cmFrameFileTest2("/media/disk/home/kevin/temp/temp0.ft");

  cmFfRC_t           rc          = kOkFfRC;
  double             srate       = 44100;
  unsigned           frameType   = 0x32333435;
  unsigned           streamId    = 1;
  unsigned           sampleIdx   = 0;
  unsigned           unitsId     = kNoUnitsUId;
  cmFrameFileH_t     h;
  const cmFfFile_t*  fileDescPtr = NULL;
  const cmFfFrame_t* frmDescPtr  = NULL;
  unsigned           mtxType     = 0x40414243;
  unsigned           i,j,k,m;

  if((rc = cmFrameFileCreate( &h, fn, srate, ctx )) != kOkFfRC )
    return rc;

  // create 3 frames
  for(i=0; i<3; ++i,sampleIdx++)
  {
    if((rc = cmFrameFileFrameCreate(h, frameType, streamId, sampleIdx, 0 )) == kOkFfRC )
    {
      long     data[]  = { 0,1,2,3,4,5,6,7,8,9,10 };
      double   ddata[] = { 10,11,12,13,14,15,16,17,18,19,20 };
      unsigned n       = sizeof(data)/sizeof(data[0]);

      for(j=0; j<n; ++j)
        data[j] += i;
     
      // write 3 matrices
      for(k=0; k<3; ++k)
      {    
        if((rc = cmFrameFileWriteMtxLong( h, mtxType + k, unitsId, data, n, 1 )) != kOkFfRC )
          return rc;
      }

      if((rc = cmFrameFileWriteMtxDouble( h, mtxType + k, unitsId, ddata, n, 1 )) != kOkFfRC )
        return rc;

      if((rc = cmFrameFileFrameClose(h)) != kOkFfRC )
        return rc;
    }
  }

  if((rc = cmFrameFileClose(&h)) != kOkFfRC )
    return rc;


  if((rc = cmFrameFileOpen( &h, fn, ctx,&fileDescPtr )) != kOkFfRC )
    return rc;

  // make two passes:
  // pass 1: read/print/modify
  // pass 2: read/print
  for(m=0; m<2; ++m)
  {
    // report the overall file format and types
    if((rc = cmFrameFileReport( h, false, &ctx->rpt )) != kOkFfRC )
      goto errLabel;

    // rewind the file
    if((rc = cmFrameFileRewind(h)) != kOkFfRC )
      goto errLabel;

    // for each frame
    for(i=0; cmFrameFileFrameLoadNext(h,kInvalidFrameTId,kInvalidStreamId,&frmDescPtr)==kOkFfRC; ++i)
    {
      if( frmDescPtr->type == kTocFrameTId )
        break;

      // print each matrix in this frame
      if((rc = _cmFrameFileTestMtx( h, mtxType, unitsId, frmDescPtr->mtxCnt, i, m==0 )) != kOkFfRC )
          goto errLabel;

      // if pass 1 write the modified data back to disk
      if( m == 0 )
        if((rc = cmFrameFileFrameUpdate(h)) != kOkFfRC )
          goto errLabel;


    } // end frame loop

    
  } // end pass loop


  if((rc = cmFrameFileClose(&h)) != kOkFfRC )
    goto errLabel;


  //
  // test cmFrameFileSeek() by seeking to frame 'fi'
  //

  printf("seek test\n");
  unsigned fi = 2;

  if((rc = cmFrameFileOpen( &h, fn,ctx,&fileDescPtr )) != kOkFfRC )
    goto errLabel;

  if((rc = cmFrameFileSeek( h, streamId, fi )) != kOkFfRC )
    goto errLabel;

  if((rc = cmFrameFileFrameLoadNext(h,kInvalidFrameTId,kInvalidStreamId,&frmDescPtr)) != kOkFfRC )
    goto errLabel;

  if((rc = _cmFrameFileTestMtx( h, mtxType, unitsId, frmDescPtr->mtxCnt, fi, false )) != kOkFfRC )
    goto errLabel;

  //
  //  test cmFrameFileMtxSize
  //
  unsigned frmCnt = 0;
  unsigned rowCnt = 0;
  unsigned colCnt = 0;
  unsigned eleCnt = 0;
  if((rc = cmFrameFileMtxSize(h, streamId, mtxType, unitsId, kLongFmtId, &frmCnt, &rowCnt, &colCnt, &eleCnt )) != kOkFfRC )
    goto errLabel;

  printf("frames:%i rows:%i cols:%i eles:%i\n",frmCnt,rowCnt,colCnt,eleCnt);

  if(1)
  {
    unsigned actualEleCnt;
    unsigned eleCnt = frmCnt*rowCnt*colCnt;
    long buf[ eleCnt ];
    if((rc = cmFrameFileMtxLoadLong(h, streamId, mtxType, unitsId, 0, -1, buf, eleCnt, &actualEleCnt )) == kOkFfRC )
    {
      cmVOI_Print(&ctx->rpt,rowCnt,frmCnt,(int*)buf);
    }
  }


 errLabel:
  if( rc != kOkFfRC )
    printf("ERROR:%i\n",rc);

  if((rc = cmFrameFileClose(&h)) != kOkFfRC )
    return rc;
    
  return rc;
  
}
