#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmAudioFile.h"
#include "cmMath.h"
#include "cmFileSys.h"


// #define _24to32_aif( p ) ((int)( ((p[0]>127?255:0) << 24) + (((int)p[0]) << 16) +  (((int)p[1]) <<8) + p[2]))  // no-swap equivalent
// See note in:_cmAudioFileReadFileHdr()
// Note that this code byte swaps as it converts - this is to counter the byte swap that occurs in cmAudioFileReadInt().
#define _24to32_aif( p ) ((int)( ((p[0]>127?255:0) <<  0) + (((int)p[2]) << 24) +  (((int)p[1]) <<16) + (((int)p[0]) << 8)))

#define _24to32_wav( p ) ((int)( ((p[2]>127?255:0) << 24) + (((int)p[2]) << 16) +  (((int)p[1]) <<8) + p[0]))

#define _cmAfSwap16(v)  cmSwap16(v)
#define _cmAfSwap32(v)  cmSwap32(v)

#ifdef cmBIG_ENDIAN
#define _cmAifSwapFl    (0)
#define _cmWavSwapFl    (1)
#else
#define _cmAifSwapFl    (1)
#define _cmWavSwapFl    (0)
#endif

enum
{
  kAiffFileId   = 'FORM',
  kAiffChkId    = 'AIFF',
  kAifcChkId    = 'AIFC',
  kSowtCompId   = 'sowt',
  kNoneCompId   = 'NONE',

  kWavFileId    = 'FFIR',
  kWavChkId     = 'EVAW',
};

enum { kWriteAudioGutsFl=0x01 };

typedef struct 
{
  unsigned    rc;
  const char* msg;
} cmAudioErrRecd;

typedef struct
{
  cmErr_t              err;
  FILE*                fp;          // file handle
  cmAudioFileInfo_t    info;        // audio file details 
  unsigned             curFrmIdx;   // current frame offset 
  unsigned             fileByteCnt; // file byte cnt 
  unsigned             smpByteOffs; // byte offset of the first sample
  cmAudioFileMarker_t* markArray;
  unsigned             flags;
  cmChar_t*            fn;
} cmAf_t;

cmAudioErrRecd _cmAudioFileErrArray[] = 
{
  { kOkAfRC,              "No Error." },
  { kOpenFailAfRC,        "Audio file open failed."},
  { kReadFailAfRC,        "Audio file read failed."},
  { kWriteFailAfRC,       "Audio file write failed."},
  { kSeekFailAfRC,        "Audio file seek failed."},
  { kCloseFailAfRC,       "Audio file close failed."},
  { kNotAiffAfRC,         "Not an audio file."},
  { kInvalidBitWidthAfRC, "Invalid audio file bit width."},
  { kInvalidFileModeAfRC, "Invalid audio file mode."},
  { kInvalidHandleAfRC,   "Invalid audio file handle."},
  { kInvalidChCountAfRC,  "Invalid channel index or count."},
  { kUnknownErrAfRC,      "Uknown audio file error."}
};

cmAudioFileH_t cmNullAudioFileH = { NULL };

cmAf_t* _cmAudioFileHandleToPtr( cmAudioFileH_t h )
{
  cmAf_t* p = (cmAf_t*)h.h;
  
  if( p == NULL )
    assert(p!=NULL);

  return p;
}

cmRC_t _cmAudioFileError( cmAf_t* p, cmRC_t rc )
{
  if( rc > kUnknownErrAfRC )
    rc = kUnknownErrAfRC;

  cmErrMsg(&p->err,rc,"%s Error:%s",cmStringNullGuard(p->fn),_cmAudioFileErrArray[rc].msg);
  return rc;
}

cmAf_t* _cmAudioFileValidate( cmAudioFileH_t h, cmRC_t* rcPtr, bool writeFl )
{
  *rcPtr = kOkAfRC;

  cmAf_t* p = _cmAudioFileHandleToPtr(h);

  if( p == NULL )
    *rcPtr =  kInvalidHandleAfRC;
  else
    if( cmIsFlag(p->flags,kWriteAudioGutsFl) != writeFl )
      *rcPtr = _cmAudioFileError( p, kInvalidFileModeAfRC );

  
  return *rcPtr == kOkAfRC ? p : NULL;
}

cmAf_t* _cmAudioFileReadGutsPtr( cmAudioFileH_t h, cmRC_t* rcPtr )
{ return _cmAudioFileValidate( h, rcPtr, false ); }

cmAf_t* _cmAudioFileWriteGutsPtr( cmAudioFileH_t h, cmRC_t* rcPtr )
{ return _cmAudioFileValidate( h, rcPtr, true ); }



cmRC_t _cmAudioFileSeek( cmAf_t* p, long byteOffset, int origin )
{
  if( fseek(p->fp,byteOffset,origin) != 0 )
    return _cmAudioFileError(p,kSeekFailAfRC);
  return kOkAfRC;
}

cmRC_t _cmAudioFileRead( cmAf_t* p, void* eleBuf, unsigned bytesPerEle, unsigned eleCnt )
{
  if( fread(eleBuf,bytesPerEle,eleCnt,p->fp) != eleCnt )
    return _cmAudioFileError(p,kReadFailAfRC);
    
  return kOkAfRC;
}

cmRC_t _cmAudioFileReadUInt32( cmAf_t* p, cmUInt32_t* valuePtr )
{
  cmRC_t rc;

  if(( rc = _cmAudioFileRead(p, valuePtr, sizeof(*valuePtr), 1 )) != kOkAfRC )
    return rc;
    
  if( cmIsFlag(p->info.flags,kSwapAfFl) )
      *valuePtr = _cmAfSwap32(*valuePtr);

  return rc;
} 


cmRC_t _cmAudioFileReadUInt16( cmAf_t* p, cmUInt16_t* valuePtr )
{
  cmRC_t rc;

  if(( rc = _cmAudioFileRead(p, valuePtr, sizeof(*valuePtr), 1 )) != kOkAfRC )
    return rc;
    
  if( cmIsFlag(p->info.flags,kSwapAfFl) )
      *valuePtr = _cmAfSwap16(*valuePtr);

  return rc;
} 

cmRC_t _cmAudioFileReadPascalString( cmAf_t* p, char s[kAudioFileLabelCharCnt] )
{
  cmRC_t rc;
  unsigned char n;

  if((rc = _cmAudioFileRead(p,&n,sizeof(n),1)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileRead(p,s,n,1)) != kOkAfRC )
    return rc;

  s[n] = '\0';

  if( n % 2 == 0 )
    rc = _cmAudioFileSeek(p,1,SEEK_CUR);

  return rc;
}

cmRC_t _cmAudioFileReadString( cmAf_t* p, char* s, unsigned sn )
{
  cmRC_t rc;
  if((rc = _cmAudioFileRead(p,s,sn,1)) != kOkAfRC )
    return rc;

  return kOkAfRC;
}

cmRC_t _cmAudioFileReadX80( cmAf_t* p, double* x80Ptr )
{
  unsigned char s[10];
  cmRC_t rc = kOkAfRC;

  if((rc = _cmAudioFileRead(p,s,10,1)) != kOkAfRC )
    return rc;

  *x80Ptr = cmX80ToDouble(s);
  return kOkAfRC;
}

cmRC_t _cmAudioFileReadChunkHdr( cmAf_t* p, cmUInt32_t* chkIdPtr, unsigned* chkByteCntPtr )
{
  cmRC_t rc      = kOkAfRC;

  *chkIdPtr      = 0;
  *chkByteCntPtr = 0;

  if((rc = _cmAudioFileReadUInt32(p,chkIdPtr)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileReadUInt32(p,chkByteCntPtr)) != kOkAfRC )
    return rc;

  // the actual on disk chunk size is always incrmented up to the next even integer
  *chkByteCntPtr += (*chkByteCntPtr) % 2;

  return rc;
}

cmRC_t _cmAudioFileReadFileHdr( cmAf_t* p, unsigned constFormId, unsigned constAifId, bool swapFl )
{
  cmRC_t     rc         = kOkAfRC;
  cmUInt32_t formId     = 0;
  cmUInt32_t aifId      = 0;
  unsigned   chkByteCnt = 0;

  p->info.flags     = 0;
  p->curFrmIdx      = 0;
  p->fileByteCnt    = 0;

  if((rc = _cmAudioFileSeek(p,0,SEEK_SET)) != kOkAfRC ) 
    return rc;

  // set the swap flags
  p->info.flags = cmEnaFlag(p->info.flags,kSwapAfFl,       swapFl);
  p->info.flags = cmEnaFlag(p->info.flags,kSwapSamplesAfFl,swapFl);

  if((rc = _cmAudioFileReadChunkHdr(p,&formId,&p->fileByteCnt)) != kOkAfRC )
    return rc;

  //
  // use -Wno-multichar on GCC cmd line to  disable the multi-char warning 
  //


  // check the FORM/RIFF id
  if( formId != constFormId )
      return kNotAiffAfRC;

  // read the AIFF/WAVE id
  if((rc = _cmAudioFileReadChunkHdr(p,&aifId,&chkByteCnt)) != kOkAfRC )
    return rc;

  // check for the AIFC 
  if( formId == kAiffFileId && aifId != constAifId )
  {
    if( aifId == kAifcChkId )
      p->info.flags = cmSetFlag(p->info.flags,kAifcAfFl);
    else
      return kNotAiffAfRC;
  }

  // set the audio file type flag 
  if( aifId==kAiffChkId || aifId==kAifcChkId )
    p->info.flags = cmSetFlag(p->info.flags,kAiffAfFl);
  
  if( aifId==kWavChkId )
    p->info.flags = cmSetFlag(p->info.flags,kWavAfFl);
    

  return rc;
}

cmRC_t _cmAudioFileReadCommChunk( cmAf_t* p )
{
  cmRC_t rc = kOkAfRC;
  cmUInt16_t ui16;
  cmUInt32_t ui32;

  if((rc = _cmAudioFileReadUInt16(p,&ui16)) != kOkAfRC )
    return rc;
  p->info.chCnt = ui16;

  if((rc = _cmAudioFileReadUInt32(p,&ui32)) != kOkAfRC )
    return rc;
  p->info.frameCnt = ui32;

  if((rc = _cmAudioFileReadUInt16(p,&ui16)) != kOkAfRC )
    return rc;
  p->info.bits = ui16;

  if((rc = _cmAudioFileReadX80(p,&p->info.srate)) != kOkAfRC )
    return rc;

  // if this is an AIFC format file  ...
  if( cmIsFlag(p->info.flags,kAifcAfFl) )
  {
    if((rc = _cmAudioFileReadUInt32(p,&ui32))  != kOkAfRC )
      return rc;

    switch( ui32 )
    {
      case kNoneCompId:
        break;

      case kSowtCompId:
        // If the compression type is set to 'swot' 
        // then the samples are written in little-endian (Intel) format
        // rather than the default big-endian format. 
        p->info.flags = cmTogFlag(p->info.flags,kSwapSamplesAfFl);
        break;

      default:
        rc = _cmAudioFileError(p,kNotAiffAfRC );
    }



  }

  return rc;
}

cmRC_t _cmAudioFileReadSsndChunk( cmAf_t* p )
{
  cmRC_t rc = kOkAfRC;

  cmUInt32_t smpOffs=0, smpBlkSize=0;
  
  if((rc = _cmAudioFileReadUInt32(p,&smpOffs)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileReadUInt32(p,&smpBlkSize)) != kOkAfRC )
    return rc;
  
  if((rc = _cmAudioFileSeek(p,smpOffs, SEEK_CUR)) != kOkAfRC )
    return rc;

  p->smpByteOffs = ftell(p->fp);

  return rc;
}

cmRC_t _cmAudioFileReadMarkerChunk( cmAf_t* p )
{
  cmRC_t rc = kOkAfRC;

  cmUInt16_t ui16;
  cmUInt32_t ui32;
  unsigned   i;

  if((rc = _cmAudioFileReadUInt16(p,&ui16)) != kOkAfRC )
    return rc;

  p->info.markerCnt = ui16;

  assert(p->markArray == NULL);

  cmAudioFileMarker_t* m = cmMemAllocZ(cmAudioFileMarker_t,p->info.markerCnt);

  p->info.markerArray = m; 

  for(i=0; i<p->info.markerCnt; ++i)
  {
    if((rc = _cmAudioFileReadUInt16(p,&ui16)) != kOkAfRC )
      return rc;

    m[i].id = ui16;

    if((rc = _cmAudioFileReadUInt32(p,&ui32)) != kOkAfRC )
      return rc;

    m[i].frameIdx = ui32;

    if((rc = _cmAudioFileReadPascalString(p,m[i].label)) != kOkAfRC )
      return rc;
    
  }
  return rc;
}

cmRC_t _cmAudioFileReadFmtChunk( cmAf_t* p )
{
  cmRC_t rc = kOkAfRC;
  unsigned short fmtId, chCnt, blockAlign, bits;
  unsigned srate, bytesPerSec;

  if((rc = _cmAudioFileReadUInt16(p,&fmtId)) != kOkAfRC )
    return rc;
  
  if((rc = _cmAudioFileReadUInt16(p,&chCnt)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileReadUInt32(p,&srate)) != kOkAfRC )
    return rc;
  
  if((rc = _cmAudioFileReadUInt32(p,&bytesPerSec)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileReadUInt16(p,&blockAlign)) != kOkAfRC )
    return rc;
  
  if((rc = _cmAudioFileReadUInt16(p,&bits)) != kOkAfRC )
    return rc;

  p->info.chCnt = chCnt;
  p->info.bits  = bits;
  p->info.srate = srate;

  // if the 'data' chunk was read before the 'fmt' chunk then info.frameCnt 
  // holds the number of bytes in the data chunk
  if( p->info.frameCnt != 0 )
      p->info.frameCnt = p->info.frameCnt / (p->info.chCnt * p->info.bits/8);

  return rc;
}

cmRC_t _cmAudioFileReadDatcmhunk( cmAf_t* p, unsigned chkByteCnt )
{
  // if the 'fmt' chunk was read before the 'data' chunk then info.chCnt is non-zero
  if( p->info.chCnt != 0 )
    p->info.frameCnt = chkByteCnt / (p->info.chCnt * p->info.bits/8);
  else
    p->info.frameCnt = chkByteCnt;

  p->smpByteOffs = ftell(p->fp);

  return kOkAfRC;
}

cmRC_t _cmAudioFileReadBextChunk( cmAf_t* p)
{
  cmRC_t rc = kOkAfRC;

  if((rc = _cmAudioFileReadString(p,p->info.bextRecd.desc,kAfBextDescN)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileReadString(p,p->info.bextRecd.origin,kAfBextOriginN)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileReadString(p,p->info.bextRecd.originRef,kAfBextOriginRefN)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileReadString(p,p->info.bextRecd.originDate,kAfBextOriginDateN)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileReadString(p,p->info.bextRecd.originTime,kAfBextOriginTimeN)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileReadUInt32(p,&p->info.bextRecd.timeRefLow)) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileReadUInt32(p,&p->info.bextRecd.timeRefHigh)) != kOkAfRC )
    return rc;

  return rc;
}



cmAudioFileH_t cmAudioFileNewOpen( const cmChar_t* fn, cmAudioFileInfo_t* afInfoPtr, cmRC_t* rcPtr, cmRpt_t* rpt  ) 
{
  cmAudioFileH_t h;
  cmRC_t       rc        = kOkAfRC;

  h.h = cmMemAllocZ( cmAf_t, 1 );  
  cmErrSetup(&((cmAf_t*)h.h)->err,rpt,"Audio File");

  if( fn != NULL )
    if((rc = cmAudioFileOpen(h,fn,afInfoPtr)) != kOkAfRC )
    {

      if( rcPtr != NULL )
        *rcPtr = rc;

      cmAudioFileDelete(&h);
    }

  if( rcPtr != NULL )
    *rcPtr = rc;

  return h;    
}

cmAudioFileH_t cmAudioFileNewCreate( const cmChar_t* fn, double srate, unsigned bits, unsigned chCnt, cmRC_t* rcPtr, cmRpt_t* rpt )
{
  cmAudioFileH_t h;
  cmRC_t rc = kOkAfRC;

  h.h = cmMemAllocZ(cmAf_t,1);
  cmErrSetup(&((cmAf_t*)h.h)->err,rpt,"Audio File");
  
  if( fn != NULL )
    if((rc = cmAudioFileCreate(h,fn,srate,bits,chCnt)) != kOkAfRC )
    {

      if( rcPtr != NULL )
        *rcPtr = rc;

      cmAudioFileDelete(&h);
    }

  if( rcPtr != NULL )
    *rcPtr = rc;

  return h;    
}

cmRC_t _cmAudioFileOpen( cmAf_t* p, const cmChar_t* fn, const char* fileModeStr )
{
  cmRC_t rc = kOkAfRC;
  
  // zero the info record
  memset(&p->info,0,sizeof(p->info));

  // open the file
  if((p->fp = fopen(fn,fileModeStr)) == NULL )
  {
    p->fn = (cmChar_t*)fn; // set the file name so that the error msg can use it
    rc    = _cmAudioFileError(p,kOpenFailAfRC);
    p->fn = NULL;
    goto errLabel;
  }

  // read the file header
  if((rc = _cmAudioFileReadFileHdr(p,kAiffFileId,kAiffChkId,_cmAifSwapFl)) != kOkAfRC )
    if((rc = _cmAudioFileReadFileHdr(p,kWavFileId,kWavChkId,_cmWavSwapFl)) != kOkAfRC )
      goto errLabel;
  
  // seek past the file header
  if((rc = _cmAudioFileSeek(p,12,SEEK_SET)) != kOkAfRC )
    goto errLabel;

  // zero chCnt and frameCnt to allow the order of the 'data' and 'fmt' chunks to be noticed
  p->info.chCnt    = 0;
  p->info.frameCnt = 0;

  while( ftell(p->fp ) < p->fileByteCnt )
  {
    unsigned chkId, chkByteCnt;
    if((rc = _cmAudioFileReadChunkHdr(p,&chkId,&chkByteCnt)) != kOkAfRC )
      goto errLabel;

    unsigned offs = ftell(p->fp);

    if( cmIsFlag(p->info.flags,kAiffAfFl) )
      switch(chkId)
      {
        case 'COMM':
          if((rc = _cmAudioFileReadCommChunk(p)) != kOkAfRC )
            goto errLabel;
          break;

        case 'SSND':
          if((rc = _cmAudioFileReadSsndChunk(p)) != kOkAfRC )
            goto errLabel;
          break;

        case 'MARK':
          if((rc = _cmAudioFileReadMarkerChunk(p)) != kOkAfRC )
            goto errLabel;
          break;
      }
    else
      switch(chkId)
      {
        case ' tmf':
          if((rc = _cmAudioFileReadFmtChunk(p)) != kOkAfRC )
            goto errLabel;
          break;

        case 'atad':
          if((rc = _cmAudioFileReadDatcmhunk(p,chkByteCnt)) != kOkAfRC )
            goto errLabel;
          break;

        case 'txeb':
          if((rc = _cmAudioFileReadBextChunk(p)) != kOkAfRC )
            goto errLabel;
          break;
      }


    // seek to the end of this chunk
    if((rc = _cmAudioFileSeek(p,offs+chkByteCnt,SEEK_SET)) != kOkAfRC )
      goto errLabel;
  }

 errLabel:
  if( rc!=kOkAfRC && p->fp != NULL )
  {
    fclose(p->fp);
    p->fp = NULL;
  }

  return rc;
}


cmRC_t     cmAudioFileOpen(  cmAudioFileH_t h, const cmChar_t* fn, cmAudioFileInfo_t* infoPtr )
{
  cmRC_t rc = kOkAfRC;

  cmAf_t* p = _cmAudioFileHandleToPtr(h);

  // verify the file is closed before opening
  if( cmAudioFileIsOpen(h) )
    if((rc = cmAudioFileClose(&h)) != kOkAfRC )
      return rc;

  // read the file header
  if((rc = _cmAudioFileOpen(p, fn, "rb" )) != kOkAfRC )
    goto errLabel;

  // seek to the first sample offset
  if((rc = _cmAudioFileSeek(p,p->smpByteOffs,SEEK_SET)) != kOkAfRC )
    goto errLabel;

  p->fn = cmMemResize( char, p->fn, strlen(fn)+1 );
  strcpy(p->fn,fn);

  if( infoPtr != NULL)
    memcpy(infoPtr,&p->info,sizeof(*infoPtr));

  return rc;

 errLabel:
  cmAudioFileClose(&h);
  return rc;  
}

cmRC_t   _cmAudioFileWriteBytes( cmAf_t* p, const void* b, unsigned bn )
{
  cmRC_t rc = kOkAfRC;
  if( fwrite( b, bn, 1, p->fp ) != 1 )
    return _cmAudioFileError(p,kWriteFailAfRC);

  return rc;
}

cmRC_t _cmAudioFileWriteId( cmAf_t* p, const char* s )
{  return _cmAudioFileWriteBytes( p,  s, strlen(s)) ; }

cmRC_t _cmAudioFileWriteUInt32( cmAf_t* p, unsigned v )
{
  if( cmIsFlag(p->info.flags,kSwapAfFl) )
    v = _cmAfSwap32(v);
  
  return _cmAudioFileWriteBytes( p, &v, sizeof(v)) ; 
}

cmRC_t _cmAudioFileWriteUInt16( cmAf_t* p, unsigned short v )
{
  if( cmIsFlag(p->info.flags,kSwapAfFl) )
    v = _cmAfSwap16(v);
  
  return _cmAudioFileWriteBytes( p, &v, sizeof(v)) ; 

}

cmRC_t _cmAudioFileWriteAiffHdr( cmAf_t* p )
{
  cmRC_t        rc = kOkAfRC;
  unsigned char srateX80[10];
 
  cmDoubleToX80( p->info.srate, srateX80 );  

  unsigned hdrByteCnt  = 54;
  unsigned ssndByteCnt = 8 + (p->info.chCnt * p->info.frameCnt * (p->info.bits/8));
  unsigned formByteCnt = hdrByteCnt + ssndByteCnt - 8;
  unsigned commByteCnt = 18;
  unsigned ssndSmpOffs = 0;
  unsigned ssndBlkSize = 0;

  if( cmIsOddU( ssndByteCnt ) )
  {
    formByteCnt++;
  }

  if(( rc = _cmAudioFileSeek( p, 0, SEEK_SET )) != kOkAfRC )
    return rc;

  if(( rc = _cmAudioFileWriteId(     p, "FORM"))           != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteUInt32( p, formByteCnt))      != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteId(     p, "AIFF"))           != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteId(     p, "COMM"))           != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteUInt32( p, commByteCnt))      != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteUInt16( p, p->info.chCnt))    != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteUInt32( p, p->info.frameCnt)) != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteUInt16( p, p->info.bits))     != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteBytes(  p, &srateX80,10))     != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteId(     p, "SSND"))           != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteUInt32( p, ssndByteCnt))      != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteUInt32( p, ssndSmpOffs))      != kOkAfRC ) return rc;
  if(( rc = _cmAudioFileWriteUInt32( p, ssndBlkSize))      != kOkAfRC ) return rc;

  return rc;
}

cmRC_t _cmAudioFileWriteWavHdr( cmAf_t* p )
{
  cmRC_t   rc            = kOkAfRC;
  short    chCnt         = p->info.chCnt;
  unsigned frmCnt        = p->info.frameCnt;
  short    bits          = p->info.bits;
  unsigned srate         = p->info.srate;
  short    fmtId         = 1;
  unsigned bytesPerSmp   = bits/8;
  unsigned hdrByteCnt    = 36;
  unsigned fmtByteCnt    = 16;
  unsigned blockAlignCnt = chCnt * bytesPerSmp;
  unsigned sampleCnt     = chCnt * frmCnt;
  unsigned dataByteCnt   = sampleCnt * bytesPerSmp;

  if(( rc = _cmAudioFileSeek( p, 0, SEEK_SET )) != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileWriteId(     p, "RIFF"))                   != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteUInt32( p, hdrByteCnt + dataByteCnt)) != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteId(     p, "WAVE"))                   != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteId(     p, "fmt "))                   != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteUInt32( p, fmtByteCnt))               != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteUInt16( p, fmtId))                    != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteUInt16( p, chCnt))                    != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteUInt32( p, srate))                    != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteUInt32( p, srate * blockAlignCnt))    != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteUInt16( p, blockAlignCnt))            != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteUInt16( p, bits))                     != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteId(     p, "data"))                   != kOkAfRC ) goto errLabel;
  if((rc = _cmAudioFileWriteUInt32( p, dataByteCnt))              != kOkAfRC ) goto errLabel;

 errLabel:
  return rc;
}

cmRC_t _cmAudioFileWriteHdr( cmAf_t* p )
{
  if( cmIsFlag(p->info.flags,kWavAfFl) )
    return _cmAudioFileWriteWavHdr(p);

  return _cmAudioFileWriteAiffHdr(p);
}

cmRC_t     cmAudioFileCreate(  cmAudioFileH_t h, const cmChar_t* fn, double srate, unsigned bits, unsigned chCnt )
{
  cmRC_t               rc     = kOkAfRC;
  cmAf_t*              p      = _cmAudioFileHandleToPtr(h);
  cmFileSysPathPart_t* pp     = NULL;

  // verify the file is closed before opening
  if( cmAudioFileIsOpen(h) )
    if((rc = cmAudioFileClose(&h)) != kOkAfRC )
      return rc;

  // all audio files are written as AIF's or WAV's -
  // if the file name is given some other extension then issue a warning and write an AIF.
  if( fn!=NULL && strlen(fn) && ((pp = cmFsPathParts(fn)) != NULL) )
  {
    unsigned i;
    unsigned n = pp->extStr==NULL ? 0 : strlen(pp->extStr);
    cmChar_t ext[n+1];

    if( pp->extStr != NULL )
    {
      strcpy(ext,pp->extStr);

      // convert the extension to upper case
      for(i=0; i<n; ++i)
        ext[i] = toupper(ext[i]);
    }

    // determine the file type to write
    if( cmIsFlag(p->info.flags,kWavAfFl) || strcmp(ext,"WAV")==0 )
    {
      p->info.flags = cmSetFlag(p->info.flags,kWavAfFl);
      p->info.flags = cmClrFlag(p->info.flags,kAiffAfFl);      
    }
    else
    {
      if( pp->extStr==NULL || (strcmp(ext,"AIF") && strcmp(ext,"AIFF")) )
        cmRptPrintf(p->err.rpt,"The AIF audio file '%s' is being written with a file extension other than 'AIF' or 'AIFF'.",cmStringNullGuard(fn));
      
      p->info.flags = cmClrFlag(p->info.flags,kWavAfFl);
      p->info.flags = cmSetFlag(p->info.flags,kAiffAfFl);
    }
    
    cmFsFreePathParts(pp);
  }

  // open the file for writing
  if((p->fp = fopen(fn,"wb")) == NULL )
  {
    p->fn = (cmChar_t*)fn; // set the file name so that the error msg can use it
    rc = _cmAudioFileError(p,kOpenFailAfRC);
    p->fn = NULL;
    goto errLabel;
  }

  p->fn            = cmMemResize( char, p->fn, strlen(fn)+1 );
  p->info.srate    = srate;
  p->info.bits     = bits;
  p->info.chCnt    = chCnt;
  p->info.frameCnt = 0;
  p->flags         = kWriteAudioGutsFl;

    // set the swap flags
  bool swapFl = cmIsFlag(p->info.flags,kWavAfFl) ?  _cmWavSwapFl :  _cmAifSwapFl;

  p->info.flags = cmEnaFlag(p->info.flags,kSwapAfFl,       swapFl);
  p->info.flags = cmEnaFlag(p->info.flags,kSwapSamplesAfFl,swapFl);

  
  strcpy(p->fn,fn);

  if((rc = _cmAudioFileWriteHdr(p)) != kOkAfRC )
    goto errLabel;
    
  return rc;

 errLabel:
  cmAudioFileClose(&h);
  return rc;
  
}

cmRC_t     cmAudioFileClose( cmAudioFileH_t* h )
{
  assert( h != NULL);

  cmAf_t* p  = _cmAudioFileHandleToPtr(*h);
  cmRC_t         rc = kOkAfRC;

  if( p->fp == NULL )
    return kOkAfRC;

  if( cmIsFlag( p->flags, kWriteAudioGutsFl ) )
    if((rc = _cmAudioFileWriteHdr(p)) != kOkAfRC )
      return rc;

  if( fclose(p->fp) != 0 )
    rc = _cmAudioFileError(p,kCloseFailAfRC);
  else
  {
    p->fp = NULL;

    cmMemPtrFree( &(p->info.markerArray));

    memset(&p->info,0,sizeof(p->info));
  }
  return rc;
  
}

cmRC_t     cmAudioFileDelete( cmAudioFileH_t* h)
{ 
  assert(h!=NULL);

  cmRC_t rc = kOkAfRC;

  // prevent double deletes
  if( h->h == NULL )
    return kOkAfRC;

  cmAf_t* p = _cmAudioFileHandleToPtr(*h);

  if( p->fp != NULL )
    rc = cmAudioFileClose(h);

  cmMemPtrFree(&p->fn);

  cmMemPtrFree(&(h->h));

  return rc; 
}

bool     cmAudioFileIsValid( cmAudioFileH_t h )
{ return h.h != NULL; }

bool     cmAudioFileIsOpen( cmAudioFileH_t h )
{
  if( !cmAudioFileIsValid(h) )
    return false;
 
  return _cmAudioFileHandleToPtr(h)->fp != NULL;
}


bool   cmAudioFileIsEOF(      cmAudioFileH_t h )
{
  cmRC_t           rc = kOkAfRC;
  cmAf_t* p  = _cmAudioFileReadGutsPtr(h,&rc);
  return  (rc != kOkAfRC) || (p==NULL) || (p->curFrmIdx >= p->info.frameCnt) || (p->fp==NULL) ||  feof(p->fp) ? true : false;
}

unsigned   cmAudioFileTell(       cmAudioFileH_t h )
{
  cmRC_t           rc = kOkAfRC;
  cmAf_t* p  = _cmAudioFileReadGutsPtr(h,&rc);
  return (rc==kOkAfRC && p!=NULL) ? p->curFrmIdx : cmInvalidIdx;
}

cmRC_t     cmAudioFileSeek(       cmAudioFileH_t h, unsigned frmIdx )
{
  cmRC_t           rc = kOkAfRC;
  cmAf_t* p  = _cmAudioFileReadGutsPtr(h,&rc);
  
  if( rc != kOkAfRC )
    return rc;

  if((rc = _cmAudioFileSeek(p,p->smpByteOffs + (frmIdx * p->info.chCnt * (p->info.bits/8)), SEEK_SET)) != kOkAfRC )
    return rc;

  p->curFrmIdx = frmIdx;

  return rc;

}

cmRC_t _cmAudioFileReadInt( cmAudioFileH_t h, unsigned totalFrmCnt, unsigned chIdx, unsigned chCnt, int* buf[], unsigned* actualFrmCntPtr, bool sumFl )
{
  cmRC_t           rc  = kOkAfRC;
  cmAf_t* p   = _cmAudioFileReadGutsPtr(h,&rc);

  if( rc != kOkAfRC )
    return rc;  

  if( chIdx+chCnt > p->info.chCnt )
    return _cmAudioFileError(p,kInvalidChCountAfRC);

  if( actualFrmCntPtr != NULL )
    *actualFrmCntPtr = 0;

  unsigned       bps            = p->info.bits / 8;       // bytes per sample
  unsigned       bpf            = bps * p->info.chCnt;    // bytes per file frame
  unsigned       bufFrmCnt      = cmMin(totalFrmCnt,cmAudioFile_MAX_FRAME_READ_CNT);
  unsigned       bytesPerBuf    = bufFrmCnt * bpf;
  unsigned char  fbuf[ bytesPerBuf ];                     // raw bytes buffer 
  unsigned       ci;
  unsigned       frmCnt = 0;  
  unsigned       totalReadFrmCnt;
  int*           ptrBuf[ chCnt ];
  

  for(ci=0; ci<chCnt; ++ci)
    ptrBuf[ci] = buf[ci];

  for(totalReadFrmCnt=0; totalReadFrmCnt<totalFrmCnt; totalReadFrmCnt+=frmCnt )
  {
    
    // don't read past the end of the file or past the end of the buffer
    frmCnt = cmMin( p->info.frameCnt - p->curFrmIdx, cmMin( totalFrmCnt-totalReadFrmCnt, bufFrmCnt ));


    // read the file frmCnt sample 
    if((rc = _cmAudioFileRead(p,fbuf,frmCnt*bpf,1)) != kOkAfRC )
      return rc;

    if( actualFrmCntPtr != NULL )
      *actualFrmCntPtr += frmCnt;

    assert( chIdx+chCnt <= p->info.chCnt );


    for(ci=0; ci<chCnt; ++ci)
    {
      unsigned char* sp = fbuf + (ci+chIdx)*bps;
      int*           dp = ptrBuf[ci];
      int*           ep = dp + frmCnt;

      if( !sumFl )
        memset(dp,0,frmCnt*sizeof(int));

      // 8 bit AIF files use 'signed char' and WAV files use 'unsigned char' for the sample data type. 
      if( p->info.bits == 8 )
      {
        if( cmIsFlag(p->info.flags,kAiffAfFl) )
        {
          for(; dp<ep; sp+=bpf,++dp)
            *dp +=  *(char*)sp;
        }
        else
        {
          for(; dp<ep; sp+=bpf,++dp)
          {
            int v = *(unsigned char*)sp;
            *dp +=  v -= 128;
          }
        }

      }

      // handle non-8 bit files here
      if( cmIsFlag(p->info.flags,kSwapSamplesAfFl) )
      {
        switch( p->info.bits )
        {
          case 8:
            break;

          case 16:
            for(; dp<ep; sp+=bpf,++dp)
              *dp += (short)_cmAfSwap16(*(short*)sp);
            break;

          case 24:
            if( cmIsFlag(p->info.flags,kAiffAfFl) )
            {
              for(; dp<ep; sp+=bpf,++dp)
                *dp += (int)(_cmAfSwap32(_24to32_aif(sp)));
            }
            else
            {
              for(; dp<ep; sp+=bpf,++dp)
                *dp += (int)(_cmAfSwap32(_24to32_wav(sp)));
            }
            break;

          case 32:
            for(; dp<ep; sp+=bpf,++dp)
              *dp += (int)_cmAfSwap32(*(int*)sp  );
            break;
        }
      }
      else
      {
        switch(p->info.bits)
        {
          case 8:
            break;

          case 16:
            for(; dp<ep; sp+=bpf,++dp)
              *dp += *(short*)sp;
            break;

          case 24:
            if( cmIsFlag(p->info.flags,kAiffAfFl) )
            {
              for(; dp<ep; sp+=bpf,++dp)
                *dp +=  _24to32_aif(sp);
            }
            else
            {
              for(; dp<ep; sp+=bpf,++dp)
                *dp +=  _24to32_wav(sp);
            }
            break;

          case 32:
            for(; dp<ep; sp+=bpf,++dp)
              *dp += *(int*)sp;


            break;
        }

        ptrBuf[ci] = dp;
        assert( dp <= buf[ci] + totalFrmCnt );
      }
      /*
      dp = ptrBuf[ci];
      ep = dp + frmCnt;
      while(dp<ep)
        sum += (double)*dp++;
      */
    }

    p->curFrmIdx += frmCnt;
  }

  if( totalReadFrmCnt < totalFrmCnt  )
  {
    for(ci=0; ci<chCnt; ++ci)
      memset(buf[ci] + frmCnt,0,(totalFrmCnt-totalReadFrmCnt)*sizeof(int));
  }



  //if( actualFrmCntPtr != NULL )
  //  *actualFrmCntPtr = totalReadFrmCnt;

  //printf("SUM: %f %f swap:%i\n", sum, sum/(totalFrmCnt*chCnt), cmIsFlag(p->info.flags,kSwapAfFl));

  return rc;  
}

cmRC_t _cmAudioFileReadRealSamples(  cmAudioFileH_t h, unsigned totalFrmCnt, unsigned chIdx, unsigned chCnt, float**  fbuf, double** dbuf, unsigned* actualFrmCntPtr, bool sumFl )
{
  cmRC_t           rc = kOkAfRC;
  cmAf_t* p  = _cmAudioFileReadGutsPtr(h,&rc);

  if( rc != kOkAfRC )
    return rc;

  if( actualFrmCntPtr != NULL )
    *actualFrmCntPtr = 0;


  unsigned         totalReadCnt = 0;
  unsigned         bufFrmCnt    = cmMin( totalFrmCnt, cmAudioFile_MAX_FRAME_READ_CNT );
  unsigned         bufSmpCnt    = bufFrmCnt * chCnt;
  float            fltMaxSmpVal = 0;  

  int              buf[ bufSmpCnt ];
  int*             ptrBuf[ chCnt ];
  float*           fPtrBuf[ chCnt ];
  double*          dPtrBuf[ chCnt ];
  unsigned         i;
  unsigned         frmCnt = 0;

  switch( p->info.bits )
  {
    case 8:   fltMaxSmpVal = 0x80;       break;
    case 16:  fltMaxSmpVal = 0x8000;     break;
    case 24:  fltMaxSmpVal = 0x800000;   break;
    case 32:  fltMaxSmpVal = 0x80000000; break;
    default:
      return _cmAudioFileError(p,kInvalidBitWidthAfRC);
  }

  double         dblMaxSmpVal = fltMaxSmpVal;

  // initialize the audio ptr buffers
  for(i=0; i<chCnt; ++i)
  {
    ptrBuf[i] = buf + (i*bufFrmCnt);

    if( dbuf != NULL )
      dPtrBuf[i] = dbuf[i];

    if( fbuf != NULL )
      fPtrBuf[i] = fbuf[i];

  }

  // 
  for(totalReadCnt=0; totalReadCnt<totalFrmCnt && p->curFrmIdx < p->info.frameCnt; totalReadCnt+=frmCnt)
  {
    unsigned actualReadFrmCnt = 0;
    frmCnt = cmMin( p->info.frameCnt - p->curFrmIdx, cmMin( totalFrmCnt-totalReadCnt, bufFrmCnt ) );

    // fill the integer audio buffer from the file
    if((rc = _cmAudioFileReadInt( h, frmCnt, chIdx, chCnt, ptrBuf, &actualReadFrmCnt, false )) != kOkAfRC )
      return rc;

    if( actualFrmCntPtr != NULL )
      *actualFrmCntPtr += actualReadFrmCnt;

    // convert the integer buffer to floating point
    for(i=0; i<chCnt; ++i)
    {

      int* sp = ptrBuf[i];

      if( fbuf != NULL )
      {

        float* dp  = fPtrBuf[i];
        float* ep = dp + frmCnt;

        if( sumFl )
        {
          for(; dp<ep; ++dp,++sp)
            *dp += ((float)*sp) / fltMaxSmpVal;

        }
        else
        {
          for(; dp<ep; ++dp,++sp)
            *dp = ((float)*sp) / fltMaxSmpVal;
        }

        assert( dp <= fbuf[i] + totalFrmCnt );

        fPtrBuf[i] = dp;
      }
      else
      {
        double* dp = dPtrBuf[i];
        double* ep = dp + frmCnt;

        if( sumFl )
        {
          for(; dp<ep; ++dp,++sp)
            *dp += ((double)*sp) / dblMaxSmpVal;                
        }
        else
        {
          for(; dp<ep; ++dp,++sp)
            *dp = ((double)*sp) / dblMaxSmpVal;      
        }

        assert( dp <= dbuf[i] + totalFrmCnt );
        dPtrBuf[i] = dp;
      }
      
    }
  }


  return rc;
}

cmRC_t _cmAudioFileReadFloat( cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, float** buf, unsigned* actualFrmCntPtr, bool sumFl )
{
  return _cmAudioFileReadRealSamples(h,frmCnt,chIdx,chCnt,buf, NULL, actualFrmCntPtr, sumFl );
}

cmRC_t _cmAudioFileReadDouble( cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, double** buf, unsigned* actualFrmCntPtr, bool sumFl )
{
  return _cmAudioFileReadRealSamples(h,frmCnt,chIdx,chCnt,NULL, buf, actualFrmCntPtr, sumFl );
}

cmRC_t     cmAudioFileReadInt(    cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, int**    buf, unsigned* actualFrmCntPtr )
{ return _cmAudioFileReadInt( h, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, false ); }

cmRC_t     cmAudioFileReadFloat(  cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, float**  buf, unsigned* actualFrmCntPtr )
{ return _cmAudioFileReadFloat( h, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, false ); }

cmRC_t     cmAudioFileReadDouble( cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, double** buf, unsigned* actualFrmCntPtr )
{ return _cmAudioFileReadDouble( h, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, false ); }

cmRC_t     cmAudioFileReadSumInt(    cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, int**    buf, unsigned* actualFrmCntPtr )
{ return _cmAudioFileReadInt( h, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, true ); }

cmRC_t     cmAudioFileReadSumFloat(  cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, float**  buf, unsigned* actualFrmCntPtr )
{ return _cmAudioFileReadFloat( h, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, true ); }

cmRC_t     cmAudioFileReadSumDouble( cmAudioFileH_t h, unsigned frmCnt, unsigned chIdx, unsigned chCnt, double** buf, unsigned* actualFrmCntPtr )
{ return _cmAudioFileReadDouble( h, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, true ); }


cmRC_t _cmAudioFileGet( const char* fn, unsigned begFrmIdx, cmAudioFileH_t* hp, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt )
{
  cmRC_t rc = kOkAfRC;

  *hp = cmAudioFileNewOpen( fn, afInfoPtr, &rc, rpt );

  if( (cmAudioFileIsValid(*hp)==false) || (rc!=kOkAfRC) )
    return rc;

  if( begFrmIdx > 0 )
    if((rc = cmAudioFileSeek( *hp, begFrmIdx )) != kOkAfRC )
      cmAudioFileDelete(hp);

  return rc;
}

cmRC_t     _cmAudioFileGetInt(    const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, int**    buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, bool sumFl, cmRpt_t* rpt )
{
  cmRC_t rc0,rc1;

  cmAudioFileH_t h;

  if((rc0 = _cmAudioFileGet(fn,begFrmIdx,&h,afInfoPtr,rpt)) != kOkAfRC )
    return rc0;
  
  rc0 = _cmAudioFileReadInt(h, frmCnt, chIdx,  chCnt, buf, actualFrmCntPtr, sumFl );

  if((rc1=cmAudioFileDelete(&h)) != kOkAfRC && rc0==kOkAfRC )
    rc0 = rc1;

  return rc0;
}

cmRC_t     _cmAudioFileGetFloat(  const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, float**  buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, bool sumFl, cmRpt_t* rpt )
{
  cmRC_t rc0,rc1;

  cmAudioFileH_t h;

  if((rc0 = _cmAudioFileGet(fn,begFrmIdx,&h,afInfoPtr,rpt)) != kOkAfRC )
    return rc0;
  
  rc0 = _cmAudioFileReadFloat(h, frmCnt, chIdx,  chCnt, buf, actualFrmCntPtr, sumFl );

  if((rc1=cmAudioFileDelete(&h)) != kOkAfRC && rc0==kOkAfRC )
    rc0 = rc1;

  return rc0;
}

cmRC_t     _cmAudioFileGetDouble( const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, double** buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, bool sumFl, cmRpt_t* rpt )
{
  cmRC_t rc0,rc1;

  cmAudioFileH_t h;

  if((rc0 = _cmAudioFileGet(fn,begFrmIdx,&h,afInfoPtr,rpt)) != kOkAfRC )
    return rc0;
  
  rc0 = _cmAudioFileReadDouble(h, frmCnt, chIdx,  chCnt, buf, actualFrmCntPtr, sumFl );

  if((rc1=cmAudioFileDelete(&h)) != kOkAfRC && rc0==kOkAfRC )
    rc0 = rc1;

  return rc0;
}

cmRC_t     cmAudioFileGetInt(    const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, int**    buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt )
{ return _cmAudioFileGetInt( fn, begFrmIdx, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, afInfoPtr, false, rpt ); }

cmRC_t     cmAudioFileGetFloat(  const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, float**  buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt )
{ return _cmAudioFileGetFloat( fn, begFrmIdx, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, afInfoPtr, false, rpt ); }

cmRC_t     cmAudioFileGetDouble( const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, double** buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt )
{ return _cmAudioFileGetDouble( fn, begFrmIdx, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, afInfoPtr, false, rpt); }

cmRC_t     cmAudioFileGetSumInt(    const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, int**    buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt )
{ return _cmAudioFileGetInt( fn, begFrmIdx, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, afInfoPtr, true, rpt ); }

cmRC_t     cmAudioFileGetSumFloat(  const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, float**  buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt )
{ return _cmAudioFileGetFloat( fn, begFrmIdx, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, afInfoPtr, true, rpt ); }

cmRC_t     cmAudioFileGetSumDouble( const char* fn, unsigned begFrmIdx, unsigned frmCnt, unsigned chIdx, unsigned chCnt, double** buf, unsigned* actualFrmCntPtr, cmAudioFileInfo_t* afInfoPtr, cmRpt_t* rpt )
{ return _cmAudioFileGetDouble( fn, begFrmIdx, frmCnt, chIdx, chCnt, buf, actualFrmCntPtr, afInfoPtr, true, rpt); }



cmRC_t    cmAudioFileWriteInt(    cmAudioFileH_t h, unsigned frmCnt, unsigned chCnt, int** srcPtrPtr )
{
  cmRC_t  rc = kOkAfRC;
  cmAf_t* p  = _cmAudioFileWriteGutsPtr(h,&rc );

  if( rc != kOkAfRC )
    return rc;

  unsigned bytesPerSmp = p->info.bits / 8;
  unsigned bufFrmCnt   = 1024;
  unsigned bufByteCnt  = bufFrmCnt * bytesPerSmp;
  unsigned ci;
  unsigned wrFrmCnt    = 0;
  char     buf[ bufByteCnt * chCnt ];
  
  while( wrFrmCnt < frmCnt )
  {
    unsigned n = cmMin( frmCnt-wrFrmCnt, bufFrmCnt );

    // interleave each channel into buf[]
    for(ci=0; ci<chCnt; ++ci)
    {
      // get the begin and end source pointers
      const int* sbp = srcPtrPtr[ci] + wrFrmCnt;
      const int* sep = sbp + n;

      // 8 bit samples can't be byte swapped
      if( p->info.bits == 8 )
      {
        char*  dbp = buf + ci;
        for(; sbp < sep; dbp+=chCnt )
          *dbp = (char)*sbp++;             
      }
      else
      {
        // if the samples do need to be byte swapped
        if( cmIsFlag(p->info.flags,kSwapSamplesAfFl) )
        {
          switch( p->info.bits )
          {
            case 16:
              {
                short*  dbp = (short*)buf;
                for(dbp+=ci; sbp < sep; dbp+=chCnt, ++sbp )
                  *dbp = _cmAfSwap16((short)*sbp);
              }
              break;

            case 24:
              {
                unsigned char* dbp = (unsigned char*)buf;
                for( dbp+=(ci*3); sbp < sep; dbp+=(3*chCnt), ++sbp)
                {
                  unsigned char* s = (unsigned char*)sbp;
                  dbp[0] = s[2];
                  dbp[1] = s[1];
                  dbp[2] = s[0];
                }
              }
              break;
          

            case 32:
              {
                int*  dbp = (int*)buf;
                for(dbp+=ci; sbp < sep; dbp+=chCnt, ++sbp )
                  *dbp = _cmAfSwap32(*sbp);
              }
              break;

            default:
              { assert(0);}
          } 

        }
        else // interleave without byte swapping
        {
          switch( p->info.bits )
          {
            case 16:
              {
                short*  dbp = (short*)buf;
                for(dbp+=ci; sbp < sep; dbp+=chCnt, ++sbp )
                  *dbp = (short)*sbp;
              }
              break;
              
            case 24:
              {
                unsigned char* dbp = (unsigned char*)buf;
                for( dbp+=(ci*3); sbp < sep; dbp+=(3*chCnt), ++sbp)
                {
                  unsigned char* s = (unsigned char*)sbp;
                  dbp[0] = s[0];
                  dbp[1] = s[1];
                  dbp[2] = s[2];
                }
              }
              break;
          

            case 32:
              {
                int*  dbp = (int*)buf;
                for(dbp+=ci; sbp < sep; dbp+=chCnt, ++sbp )
                  *dbp = *sbp;
              }
              break;

            default:
              { assert(0);}
          } // switch
        } // don't swap
      } // 8 bits
    } // ch

    // advance the source pointer index
    wrFrmCnt+=n;

    if( fwrite( buf, n*bytesPerSmp*chCnt, 1, p->fp ) != 1)
    {
        rc = _cmAudioFileError(p,kWriteFailAfRC);
        break;
    }
    
  } // while

  p->info.frameCnt += wrFrmCnt;

  return rc;
}

cmRC_t    _cmAudioFileWriteRealSamples( cmAudioFileH_t h, unsigned frmCnt, unsigned chCnt, const void*  srcPtrPtr, unsigned realSmpByteCnt )
{
  cmRC_t           rc        = kOkAfRC;
  cmAf_t* p         = _cmAudioFileWriteGutsPtr(h,&rc );

  if( rc != kOkAfRC )
    return rc;

  unsigned         bufFrmCnt = 1024;
  unsigned         wrFrmCnt  = 0;
  unsigned         i         = 0;
  int              maxSmpVal = 0;

  int              buf[   chCnt * bufFrmCnt ];
  int*             srcCh[ chCnt ];
  
  for(i=0; i<chCnt; ++i)
    srcCh[i] = buf + (i*bufFrmCnt);

  switch( p->info.bits )
  {
    case 8:   maxSmpVal = 0x7f;       break;
    case 16:  maxSmpVal = 0x7fff;     break;
    case 24:  maxSmpVal = 0x7fffff;   break;
    case 32:  maxSmpVal = 0x7fffffb0; break; // Note: the full range is not used for 32 bit numbers
    default:                                 // because it was found to cause difficult to detect overflows
      { assert(0); }                         // when the signal approached full scale. 
  }

  // duplicate the audio buffer ptr array - this will allow the buffer ptr's to be changed
  // during the float to int conversion without changing the ptrs passed in from the client
  const void* ptrArray[ chCnt ];
  memcpy(ptrArray,srcPtrPtr,sizeof(ptrArray));

  const float**  sfpp = (const float**)ptrArray;
  const double** sdpp = (const double**)ptrArray;

  while( wrFrmCnt < frmCnt )
  {
    unsigned n = cmMin( frmCnt - wrFrmCnt, bufFrmCnt );

    for(i=0; i<chCnt; ++i)
    {
      int*       obp = srcCh[i];

      switch( realSmpByteCnt )
      {
        case 4:
          {
            const float* sbp = sfpp[i];
            const float* sep = sbp + n;
            for(;sbp<sep; ++sbp)
            {
              *obp++ = (int)fmaxf(-maxSmpVal,fminf(maxSmpVal, *sbp * maxSmpVal));
            }

            sfpp[i] = sbp;
          }
          break;
 
        case 8:
          {
            const double* sbp = sdpp[i];
            const double* sep = sbp + n;
            for(; sbp<sep; ++sbp)
            {
              *obp++ = (int)fmax(-maxSmpVal,fmin(maxSmpVal,*sbp * maxSmpVal));
            }
            sdpp[i] = sbp;
          }
          break;

        default:
          { assert(0); }
      }  
    }

    if((rc = cmAudioFileWriteInt( h, n, chCnt, srcCh )) != kOkAfRC )
      break;

    wrFrmCnt += n;
  }
  
  return rc;
}

cmRC_t    cmAudioFileWriteFloat(  cmAudioFileH_t h, unsigned frmCnt, unsigned chCnt, float**  bufPtrPtr )
{ return _cmAudioFileWriteRealSamples(h,frmCnt,chCnt,bufPtrPtr,sizeof(float)); }

cmRC_t    cmAudioFileWriteDouble( cmAudioFileH_t h, unsigned frmCnt, unsigned chCnt, double** bufPtrPtr )
{ return _cmAudioFileWriteRealSamples(h,frmCnt,chCnt,bufPtrPtr,sizeof(double)); }


cmRC_t     cmAudioFileMinMaxMean( cmAudioFileH_t h, unsigned chIdx, cmSample_t* minPtr, cmSample_t* maxPtr, cmSample_t* meanPtr )
{
  assert( minPtr != NULL && maxPtr != NULL && meanPtr != NULL );

  *minPtr = -cmSample_MAX;
  *maxPtr = cmSample_MAX;
  *meanPtr = 0;

  cmRC_t           rc = kOkAfRC;
  cmAf_t* p  = _cmAudioFileReadGutsPtr(h,&rc );

  if( rc != kOkAfRC )
    return rc;

  unsigned orgFrmIdx   = p->curFrmIdx;

  if((rc = cmAudioFileSeek(h,0)) != kOkAfRC )
    return rc;

  *minPtr = cmSample_MAX;
  *maxPtr = -cmSample_MAX;

  unsigned    bufN = 1024;
  cmSample_t  buf[ bufN ];
  unsigned    frmCnt = 0;
  unsigned    actualFrmCnt;
  cmSample_t* bufPtr[1] = { &buf[0] };

  for(; frmCnt<p->info.frameCnt; frmCnt+=actualFrmCnt) 
  {
    actualFrmCnt = 0;
    unsigned n = cmMin( p->info.frameCnt-frmCnt, bufN );
 
    if((rc = cmAudioFileReadSample(h, n, chIdx, 1, bufPtr, &actualFrmCnt)) != kOkAfRC )
      return rc;

    const cmSample_t* sbp = buf;
    const cmSample_t* sep = buf + actualFrmCnt;

    for(; sbp < sep; ++sbp )
    {
      *meanPtr += *sbp;
      if( *minPtr > *sbp )
        *minPtr = *sbp;
      if( *maxPtr < *sbp )
        *maxPtr = *sbp;
    }
    
  }

  if( frmCnt > 0 )
    *meanPtr /= frmCnt;
  else
    *minPtr = *maxPtr = 0;
 
  return cmAudioFileSeek( h, orgFrmIdx );

}

cmRC_t    cmAudioFileWriteFileInt(    const char* fn, double srate, unsigned bits, unsigned frmCnt, unsigned chCnt, int**  bufPtrPtr, cmRpt_t* rpt )
{
  cmRC_t       rc0, rc1;
  cmAudioFileH_t h  = cmAudioFileNewCreate(fn, srate, bits, chCnt, &rc0, rpt );

  if( (cmAudioFileIsValid(h)==false) || (rc0!=kOkAfRC))
    return rc0;
  
  rc0 = cmAudioFileWriteInt( h, frmCnt, chCnt, bufPtrPtr );

  if(((rc1 = cmAudioFileDelete(&h))!=kOkAfRC) && (rc0!=kOkAfRC))
    rc0 = rc1;
  
  return rc0;  
}

cmRC_t    cmAudioFileWriteFileFloat(  const char* fn, double srate, unsigned bits, unsigned frmCnt, unsigned chCnt, float**  bufPtrPtr, cmRpt_t* rpt )
{
  cmRC_t       rc0, rc1;
  cmAudioFileH_t h  = cmAudioFileNewCreate(fn, srate, bits, chCnt, &rc0, rpt );

  if( (cmAudioFileIsValid(h)==false) || (rc0!=kOkAfRC))
    return rc0;
  
  rc0 = cmAudioFileWriteFloat( h, frmCnt, chCnt, bufPtrPtr );

  if(((rc1 = cmAudioFileDelete(&h))!=kOkAfRC) && (rc0!=kOkAfRC))
    rc0 = rc1;
  
  return rc0;  
}

cmRC_t    cmAudioFileWriteFileDouble( const char* fn, double srate, unsigned bits, unsigned frmCnt, unsigned chCnt, double** bufPtrPtr, cmRpt_t* rpt )
{
  cmRC_t       rc0, rc1;
  cmAudioFileH_t h  = cmAudioFileNewCreate(fn, srate, bits, chCnt, &rc0, rpt );

  if( (cmAudioFileIsValid(h)==false) || (rc0!=kOkAfRC))
    return rc0;
  
  rc0 = cmAudioFileWriteDouble( h, frmCnt, chCnt, bufPtrPtr );

  if(((rc1 = cmAudioFileDelete(&h))!=kOkAfRC) && (rc0!=kOkAfRC))
    rc0 = rc1;
  
  return rc0;  
}


cmRC_t cmAudioFileMinMaxMeanFn( const cmChar_t* fn, unsigned chIdx, cmSample_t* minPtr, cmSample_t* maxPtr, cmSample_t* meanPtr, cmRpt_t* rpt )
{
  cmRC_t rc0 = kOkAfRC;
  cmRC_t rc1;

  cmAudioFileH_t afH = cmAudioFileNewOpen( fn, NULL, &rc0, rpt ); 

  if( rc0 != kOkAfRC )
    return rc0;

  rc0 = cmAudioFileMinMaxMean( afH, chIdx, minPtr, maxPtr, meanPtr ); 
  rc1 =  cmAudioFileDelete(&afH);
  
  return rc0 != kOkAfRC ? rc0 : rc1;
}



const cmChar_t* cmAudioFileName( cmAudioFileH_t h )
{
  cmRC_t rc;
  cmAf_t* p  = _cmAudioFileReadGutsPtr(h,&rc );

  if( rc != kOkAfRC )
    return NULL;
 
  return p->fn;
}

const char* cmAudioFileErrorMsg( unsigned rc )
{
  unsigned i;

  for(i=0; _cmAudioFileErrArray[i].rc != kUnknownErrAfRC; ++i )
    if( _cmAudioFileErrArray[i].rc == rc )
      break;

  return _cmAudioFileErrArray[i].msg;

}

cmRC_t cmAudioFileGetInfo(   const cmChar_t* fn, cmAudioFileInfo_t* infoPtr, cmRpt_t* rpt  )
{
  cmRC_t rc = kOkAfRC;

  cmAudioFileH_t afH = cmAudioFileNewOpen( fn, infoPtr, &rc, rpt ); 

  if( rc != kOkAfRC )
    return rc;

  return cmAudioFileDelete(&afH);
}


void   cmAudioFilePrintInfo( const cmAudioFileInfo_t* infoPtr, cmRpt_t* rpt )
{
  char*  typeStr = "AIFF";
  char*  swapStr = "";
  char*  aifcStr = "";
  unsigned i;
  
  if( cmIsFlag(infoPtr->flags,kWavAfFl) )
    typeStr = "WAV";

  if( cmIsFlag(infoPtr->flags,kSwapAfFl) )
    swapStr = "Swap:On";

  if( cmIsFlag(infoPtr->flags,kAifcAfFl))
    aifcStr = "AIFC";

  cmRptPrintf(rpt,"bits:%i chs:%i srate:%f frames:%i type:%s %s %s\n", infoPtr->bits, infoPtr->chCnt, infoPtr->srate, infoPtr->frameCnt, typeStr, swapStr, aifcStr );

  for(i=0; i<infoPtr->markerCnt; ++i)
    cmRptPrintf(rpt,"%i %i %s\n", infoPtr->markerArray[i].id, infoPtr->markerArray[i].frameIdx, infoPtr->markerArray[i].label);

  if( strlen(infoPtr->bextRecd.desc) )
    cmRptPrintf(rpt,"Bext Desc:%s\n",infoPtr->bextRecd.desc );

  if( strlen(infoPtr->bextRecd.origin) )
    cmRptPrintf(rpt,"Bext Origin:%s\n",infoPtr->bextRecd.origin );

  if( strlen(infoPtr->bextRecd.originRef) )
    cmRptPrintf(rpt,"Bext Origin Ref:%s\n",infoPtr->bextRecd.originRef );

  if( strlen(infoPtr->bextRecd.originDate) )
    cmRptPrintf(rpt,"Bext Origin Date:%s\n",infoPtr->bextRecd.originDate );

  if( strlen(infoPtr->bextRecd.originTime ) )
    cmRptPrintf(rpt,"Bext Origin Time:%s\n",infoPtr->bextRecd.originTime );

  cmRptPrintf(rpt,"Bext time high:%i low:%i  0x%x%x\n",infoPtr->bextRecd.timeRefHigh,infoPtr->bextRecd.timeRefLow, infoPtr->bextRecd.timeRefHigh,infoPtr->bextRecd.timeRefLow);

}

cmRC_t     cmAudioFileReport( cmAudioFileH_t h, cmRpt_t* rpt, unsigned frmIdx, unsigned frmCnt )
{
  cmRC_t  rc = kOkAfRC;
  cmAf_t* p  = _cmAudioFileReadGutsPtr(h,&rc);
  
  if( rc != kOkAfRC )
    return rc;
  
  cmRptPrintf(rpt,"function cm_audio_file_test()\n");
  cmRptPrintf(rpt,"#{\n");
  cmAudioFilePrintInfo(&p->info,rpt);
  cmRptPrintf(rpt,"#}\n");

  float           buf[ p->info.chCnt * frmCnt ];
  float*          bufPtr[p->info.chCnt];
  unsigned      i,j,cmtFrmCnt=0;

  for(i=0; i<p->info.chCnt; ++i)
    bufPtr[i] = buf + (i*frmCnt);

  if((rc = cmAudioFileSeek(h,frmIdx)) != kOkAfRC )
    return rc;
  
  if((rc= cmAudioFileReadFloat(h,frmCnt,0,p->info.chCnt,bufPtr,&cmtFrmCnt )) != kOkAfRC)
    return rc;

  cmRptPrintf(rpt,"m = [\n");
  for(i=0; i<frmCnt; i++)
  {
    for(j=0; j<p->info.chCnt; ++j)
      cmRptPrintf(rpt,"%f ", bufPtr[j][i] );
    cmRptPrintf(rpt,"\n");
  }
    cmRptPrintf(rpt,"];\nplot(m)\nendfunction\n");

  return rc;
  
}

cmRC_t     cmAudioFileReportFn( const cmChar_t* fn, unsigned frmIdx, unsigned frmCnt, cmRpt_t* rpt )
{
  cmAudioFileInfo_t info;
  cmRC_t        rc;
  cmAudioFileH_t    h = cmAudioFileNewOpen( fn, &info, &rc, rpt );

  if(rc == kOkAfRC )
  {  
    cmAudioFileReport(h,rpt,frmIdx,frmCnt);
  }

  return cmAudioFileDelete(&h);
}

cmRC_t     cmAudioFileSetSrate( const cmChar_t* fn, unsigned srate )
{
  cmRC_t  rc = kOkAfRC;
  cmAf_t  af;
  cmAf_t* p  = &af;

  memset(&af,0,sizeof(af));

  if((rc = _cmAudioFileOpen(p, fn, "r+b")) != kOkAfRC )
    goto errLabel;  

  if( p->info.srate != srate )
  {
    // change the sample rate
    p->info.srate = srate;

    // write the file header
    if((rc = _cmAudioFileWriteHdr(p)) != kOkAfRC )
      goto errLabel;
  }
  
 errLabel:
  if( p->fp != NULL )
    fclose(p->fp);
  
  return rc;
}

void _cmAudioFileTest( const cmChar_t* audioFn, cmRpt_t* rpt )
{
  cmAudioFileInfo_t afInfo;
  cmRC_t            cmRC;

  // open an audio file
  cmAudioFileH_t     afH   = cmAudioFileNewOpen( audioFn, &afInfo, &cmRC, rpt ); 

  if( cmRC != kOkAfRC || cmAudioFileIsValid(afH)==false )
  {
    cmRptPrintf(rpt,"Unable to open the audio file:%s\n",audioFn);
    return;
  }

  //   print the header information and one seconds worth of samples
  //cmAudioFileReport( afH, rpt, 66000, (unsigned)afInfo.srate);
  cmAudioFileReport( afH, rpt, 0, 0);

  // close and delete the audio file handle
  cmAudioFileDelete(&afH);
}

cmRC_t     cmAudioFileSine( cmCtx_t* ctx, const cmChar_t* fn, double srate, unsigned bits, double hz, double gain, double secs )
{
  cmRC_t      rc    = kOkAfRC;
  unsigned    bN    = srate * secs;
  cmSample_t* b     = cmMemAlloc(cmSample_t,bN);
  unsigned    chCnt = 1;

  unsigned    i;
  for(i=0; i<bN; ++i)
    b[i] = gain * sin(2.0*M_PI*hz*i/srate);

  if((rc = cmAudioFileWriteFileFloat(fn, srate, bits, bN, chCnt, &b, &ctx->rpt)) != kOkAfRC)
    return rc;

  return rc;
}

/// [cmAudioFileExample]

void cmAudioFileTest(cmCtx_t* ctx, int argc, const char* argv[])
{
  switch( argc )
  {
    case 3:
      //_cmAudioFileTest(argv[2],&ctx->rpt);
      cmAudioFileReportFn(argv[2],0,0,&ctx->rpt);
      break;
      
    case 4:
      {
        errno = 0;
        long srate =  strtol(argv[3], NULL, 10);
        if( srate == 0 && errno != 0 )
          cmRptPrintf(&ctx->rpt,"Invalid sample rate argument to cmAudioFileTest().");
        else
          cmAudioFileSetSrate(argv[2],srate);                
      }
      break;

    case 8:
      {
        errno = 0;
        double   srate = strtod(argv[3],NULL);
        unsigned bits  = strtol(argv[4],NULL,10);
        double   hz    = strtod(argv[5],NULL);
        double   gain  = strtod(argv[6],NULL);
        double   secs  = strtod(argv[7],NULL);
        
        if( errno != 0 )
          cmRptPrintf(&ctx->rpt,"Invalid arg. to cmAudioFileTest().");
        
        cmAudioFileSine( ctx, argv[2], srate, bits,  hz, gain,  secs );
      }
      break;

    default:
      cmRptPrintf(&ctx->rpt,"Invalid argument count to cmAudioFileTest().");
      break;
  }
}


/// [cmAudioFileExample]
