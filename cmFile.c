#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmFile.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include <sys/stat.h>
cmFileH_t cmFileNullHandle = { NULL };

typedef struct 
{
  FILE*     fp;
  cmErr_t   err;
  cmChar_t* fnStr;
} cmFile_t;

cmFile_t* _cmFileHandleToPtr( cmFileH_t h )
{
  cmFile_t* p = (cmFile_t*)h.h;
  assert(p != NULL);
  return p;
}

cmFileRC_t _cmFileError( cmFile_t* p, cmFileRC_t rc, int errNumb, const cmChar_t* msg  )
{
  if(errNumb == 0)
    rc = cmErrMsg(&p->err,rc,"%s on file '%s'",msg,p->fnStr);
  else
    rc = cmErrMsg(&p->err,rc,"%s on file '%s'\nSystem Msg:%s",msg,p->fnStr,strerror(errNumb));

  return rc;
}

cmFileRC_t cmFileOpen(    cmFileH_t* hp, const cmChar_t* fn, enum cmFileOpenFlags_t flags, cmRpt_t* rpt )
{
  char mode[] = "/0/0/0";
  cmFile_t* p = NULL;
  cmErr_t err;
  cmFileRC_t rc;

  if((rc = cmFileClose(hp)) != kOkFileRC )
    return rc;

  cmErrSetup(&err,rpt,"File");

  hp->h = NULL;

  if( cmIsFlag(flags,kReadFileFl) )
    mode[0]='r';
  else
    if( cmIsFlag(flags,kWriteFileFl) )
      mode[0]='w';
    else
      if( cmIsFlag(flags,kAppendFileFl) )
        mode[0]='a';
      else
        cmErrMsg(&err,kInvalidFlagFileRC,"File open flags must contain 'kReadFileFl','kWriteFileFl', or 'kAppendFileFl'.");
  
  if( cmIsFlag(flags,kUpdateFileFl) )
    mode[1]='+';

  if( fn == NULL )
    return cmErrMsg(&err,kObjAllocFailFileRC,"File object allocation failed due to empty file name.");

  unsigned byteCnt = sizeof(cmFile_t) + strlen(fn) + 1;

  if((p = (cmFile_t*)cmMemMallocZ(byteCnt)) == NULL )
    return cmErrMsg(&err,kObjAllocFailFileRC,"File object allocation failed for file '%s'.",cmStringNullGuard(fn));

  cmErrClone(&p->err,&err);

  p->fnStr = (cmChar_t*)(p+1);
  strcpy(p->fnStr,fn);
  

  errno = 0;
  if((p->fp = fopen(fn,mode)) == NULL )
  {
    cmFileRC_t rc = _cmFileError(p,kOpenFailFileRC,errno,"File open failed");
    cmMemFree(p);
    return rc;
  }
 
  hp->h    = p;

  return kOkFileRC;
}

cmFileRC_t cmFileClose(   cmFileH_t* hp )
{
  if( cmFileIsValid(*hp) == false )
    return kOkFileRC;

  cmFile_t* p = _cmFileHandleToPtr(*hp);
  
  errno = 0;
  if( p->fp != NULL )
    if( fclose(p->fp) != 0 )
      return _cmFileError(p,kCloseFailFileRC,errno,"File close failed");
  
  cmMemFree(p);
  hp->h = NULL;

  return kOkFileRC;
}

bool       cmFileIsValid( cmFileH_t h )
{ return h.h != NULL; }

cmFileRC_t cmFileRead(    cmFileH_t h, void* buf, unsigned bufByteCnt )
{
  cmFile_t* p = _cmFileHandleToPtr(h);
  
  errno = 0;
  if( fread(buf,bufByteCnt,1,p->fp) != 1 )
    return _cmFileError(p,kReadFailFileRC,errno,"File read failed");

  return kOkFileRC;
}

cmFileRC_t cmFileWrite(   cmFileH_t h, const void* buf, unsigned bufByteCnt )
{
  cmFile_t* p = _cmFileHandleToPtr(h);
  
  errno = 0;
  if( fwrite(buf,bufByteCnt,1,p->fp) != 1 )
    return _cmFileError(p,kWriteFailFileRC,errno,"File write failed");

  return kOkFileRC;
}

cmFileRC_t cmFileSeek(    cmFileH_t h, enum cmFileSeekFlags_t flags, int offsByteCnt )
{
  cmFile_t* p = _cmFileHandleToPtr(h);
  unsigned fileflags = 0;

  if( cmIsFlag(flags,kBeginFileFl) )
    fileflags = SEEK_SET;
  else
    if( cmIsFlag(flags,kCurFileFl) )
      fileflags = SEEK_CUR;
    else
      if( cmIsFlag(flags,kEndFileFl) )
        fileflags = SEEK_END;
      else
        return cmErrMsg(&p->err,kInvalidFlagFileRC,"Invalid file seek flag on '%s'.",p->fnStr);
  
  errno = 0;
  if( fseek(p->fp,offsByteCnt,fileflags) != 0 )
    return _cmFileError(p,kSeekFailFileRC,errno,"File seek failed");

  return kOkFileRC;
}

cmFileRC_t cmFileTell( cmFileH_t h, long* offsPtr )
{
  assert( offsPtr != NULL );
  *offsPtr     = -1;
  cmFile_t* p  = _cmFileHandleToPtr(h);
  errno        = 0;
  if((*offsPtr = ftell(p->fp)) == -1)
    return _cmFileError(p,kTellFailFileRC,errno,"File tell failed");
  return kOkFileRC;
}


bool       cmFileEof(     cmFileH_t h )
{ return feof( _cmFileHandleToPtr(h)->fp ) != 0; }


unsigned   cmFileByteCount(  cmFileH_t h )
{
  struct stat sr;
  int         f;
  cmFile_t*   p = _cmFileHandleToPtr(h);
  const cmChar_t errMsg[] = "File byte count request failed.";

  errno = 0;

  if((f = fileno(p->fp)) == -1)
  {
    _cmFileError(p,kHandleInvalidFileRC,errno,errMsg);
    return 0;
  }
  
  if(fstat(f,&sr) == -1)
  {
    _cmFileError(p,kStatFailFileRC,errno,errMsg);
    return 0;
  }

  return sr.st_size;
}

cmFileRC_t   cmFileByteCountFn( const cmChar_t* fn, cmRpt_t* rpt, unsigned* fileByteCntPtr )
{
  assert( fileByteCntPtr != NULL );
  cmFileRC_t rc;
  cmFileH_t h = cmFileNullHandle;
  if((rc = cmFileOpen(&h,fn,kReadFileFl,rpt)) != kOkFileRC )
    return rc;

  if( fileByteCntPtr != NULL)
    *fileByteCntPtr = cmFileByteCount(h);

  cmFileClose(&h);

  return rc;    
}

cmFileRC_t cmFileCompare( const cmChar_t* fn0, const cmChar_t* fn1, cmRpt_t* rpt, bool* isEqualPtr )
{
  cmFileRC_t rc         = kOkFileRC;
  unsigned   bufByteCnt = 2048;
  cmFileH_t  h0         = cmFileNullHandle;
  cmFileH_t  h1         = cmFileNullHandle;

  char       b0[ bufByteCnt ];
  char       b1[ bufByteCnt ];

  assert(isEqualPtr != NULL );
  *isEqualPtr = true;

  if((rc = cmFileOpen(&h0,fn0,kReadFileFl,rpt)) != kOkFileRC )
    goto errLabel;

  if((rc = cmFileOpen(&h1,fn1,kReadFileFl,rpt)) != kOkFileRC )
    goto errLabel;

  cmFile_t*   p0 = _cmFileHandleToPtr(h0);
  cmFile_t*   p1 = _cmFileHandleToPtr(h1);

  while(1)
  {
    size_t n0 = fread(b0,1,bufByteCnt,p0->fp);
    size_t n1 = fread(b1,1,bufByteCnt,p1->fp);
    if( n0 != n1 || memcmp(b0,b1,n0)!=0 )
    {
      *isEqualPtr = false;
      break;
    }

    if( n0 != bufByteCnt || n1 != bufByteCnt )
      break;
  }

 errLabel:
  cmFileClose(&h0);
  cmFileClose(&h1);
  return rc;
}


const cmChar_t* cmFileName( cmFileH_t h )
{
  cmFile_t* p = _cmFileHandleToPtr(h);
  return p->fnStr;
}

cmFileRC_t cmFileFnWrite( const cmChar_t* fn, cmRpt_t* rpt, const void* buf, unsigned bufByteCnt )
{
  cmFileH_t h = cmFileNullHandle;
  cmFileRC_t rc;

  if((rc = cmFileOpen(&h,fn,kWriteFileFl,rpt)) != kOkFileRC )
    goto errLabel;

  rc = cmFileWrite(h,buf,bufByteCnt);

 errLabel:
  cmFileClose(&h);
  
  return rc;
}

cmChar_t*  _cmFileToBuf( cmFileH_t h, unsigned nn, unsigned* bufByteCntPtr )
{
  errno = 0;

  unsigned  n   = cmFileByteCount(h);
  cmChar_t* buf = NULL;
  cmFile_t*   p = _cmFileHandleToPtr(h);
 

  // if the file size calculation is ok
  if( errno != 0 )
  {
    _cmFileError(p,kBufAllocFailFileRC,errno,"Invalid file buffer length.");
    goto errLabel;
  }
  
  // allocate the read target buffer
  if((buf = cmMemAlloc(cmChar_t,n+nn)) == NULL)
  {
    _cmFileError(p,kBufAllocFailFileRC,0,"Read buffer allocation failed.");
    goto errLabel;
  }

  // read the file
  if( cmFileRead(h,buf,n) != kOkFileRC )
    goto errLabel;

  // zero memory after the file data
  memset(buf+n,0,nn);

  if( bufByteCntPtr != NULL )
    *bufByteCntPtr = n;

  return buf;

 errLabel:
  if( bufByteCntPtr != NULL )
    *bufByteCntPtr = 0;

  cmMemFree(buf);

  return NULL;
    
}

cmChar_t* _cmFileFnToBuf( const cmChar_t* fn, cmRpt_t* rpt, unsigned nn, unsigned* bufByteCntPtr  )
{
  cmFileH_t h = cmFileNullHandle;
  cmChar_t* buf = NULL;

  if( cmFileOpen(&h,fn,kReadFileFl | kBinaryFileFl,rpt) != kOkFileRC )
    goto errLabel;

  buf = _cmFileToBuf(h,nn,bufByteCntPtr);

 errLabel:
  cmFileClose(&h);
  
  return buf;
}

cmChar_t*  cmFileToBuf( cmFileH_t h, unsigned* bufByteCntPtr )
{ return _cmFileToBuf(h,0,bufByteCntPtr); }

cmChar_t*  cmFileFnToBuf( const cmChar_t* fn, cmRpt_t* rpt, unsigned* bufByteCntPtr )
{ return _cmFileFnToBuf(fn,rpt,0,bufByteCntPtr); }

cmChar_t*  cmFileToStr( cmFileH_t h, unsigned* bufByteCntPtr )
{ return _cmFileToBuf(h,1,bufByteCntPtr); }

cmChar_t*  cmFileFnToStr( const cmChar_t* fn, cmRpt_t* rpt, unsigned* bufByteCntPtr )
{ return _cmFileFnToBuf(fn,rpt,1,bufByteCntPtr); }

cmFileRC_t cmFileLineCount( cmFileH_t h, unsigned* lineCntPtr )
{
  cmFileRC_t rc      = kOkFileRC;
  cmFile_t*  p       = _cmFileHandleToPtr(h);
  unsigned   lineCnt = 0;
  long       offs;
  int        c;


  assert( lineCntPtr != NULL );
  *lineCntPtr = 0;

  if((rc = cmFileTell(h,&offs)) != kOkFileRC )
    return rc;

  errno = 0;

  while(1)
  {
    c = fgetc(p->fp);

    if( c == EOF ) 
    {
      if( errno )
        rc =_cmFileError(p,kReadFailFileRC,errno,"File read char failed");
      else
        ++lineCnt; // add one in case the last line isn't terminated with a '\n'. 

      break;
    }

    // if an end-of-line was encountered
    if( c == '\n' )
      ++lineCnt;

  }

  if((rc = cmFileSeek(h,kBeginFileFl,offs)) != kOkFileRC )
    return rc;

  *lineCntPtr = lineCnt;

  return rc;
}

cmFileRC_t _cmFileGetLine( cmFile_t* p, cmChar_t* buf, unsigned* bufByteCntPtr )
{
  // fgets() reads up to n-1 bytes into buf[]
  if( fgets(buf,*bufByteCntPtr,p->fp) == NULL )
  {
    // an read error or EOF condition occurred
    *bufByteCntPtr = 0;

    if( !feof(p->fp ) )
      return _cmFileError(p,kReadFailFileRC,errno,"File read line failed");
    
    return kReadFailFileRC;
  }

  return kOkFileRC;
}

cmFileRC_t cmFileGetLine( cmFileH_t h, cmChar_t* buf, unsigned* bufByteCntPtr )
{
  assert( bufByteCntPtr != NULL );
  cmFile_t* p  = _cmFileHandleToPtr(h);
  unsigned  tn = 128;
  cmChar_t  t[ tn ];
  unsigned  on = *bufByteCntPtr;
  long      offs;
  cmFileRC_t rc;

  // store the current file offset
  if((rc = cmFileTell(h,&offs)) != kOkFileRC )
    return rc;
  
  // if no buffer was given then use t[]
  if( buf == NULL || *bufByteCntPtr == 0 )
  {
    *bufByteCntPtr = tn;
    buf            = t;
  }

  // fill the buffer from the current line 
  if((rc = _cmFileGetLine(p,buf,bufByteCntPtr)) != kOkFileRC )
    return rc;

  // get length of the string  in the buffer
  // (this is one less than the count of bytes written to the buffer)
  unsigned n = strlen(buf);

  // if the provided buffer was large enough to read the entire string 
  if( on > n+1 )
  {
    //*bufByteCntPtr = n+1;
    return kOkFileRC;
  }

  //
  // the provided buffer was not large enough 
  //

  // m tracks the length of the string
  unsigned m = n;

  while( n+1 == *bufByteCntPtr )
  {
    // fill the buffer from the current line
    if((rc = _cmFileGetLine(p,buf,bufByteCntPtr)) != kOkFileRC )
      return rc;

    n = strlen(buf);
    m += n;
  }

  // restore the original file offset
  if((rc = cmFileSeek(h,kBeginFileFl,offs)) != kOkFileRC )
    return rc;

  // add 1 for /0, 1 for /n and 1 to detect buf-too-short
  *bufByteCntPtr = m+3;
  
  return kBufTooSmallFileRC;
  
}

cmFileRC_t cmFileGetLineAuto( cmFileH_t h, cmChar_t** bufPtrPtr, unsigned* bufByteCntPtr )
{
  cmFileRC_t rc         = kOkFileRC;
  bool       fl         = true;
  cmChar_t*  buf        = *bufPtrPtr;

  *bufPtrPtr = NULL;

  while(fl)
  {
    fl         = false;

    switch( rc = cmFileGetLine(h,buf,bufByteCntPtr) )
    {
      case kOkFileRC:
        {
          *bufPtrPtr = buf;
        }
        break;
        
      case kBufTooSmallFileRC:
        buf = cmMemResizeZ(cmChar_t,buf,*bufByteCntPtr);
        fl  = true;
        break;

      default:
        cmMemFree(buf);
        break;
    }
  }

  

  return rc;
}

cmFileRC_t cmFileReadChar(   cmFileH_t h, char*           buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileReadUChar(  cmFileH_t h, unsigned char*  buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileReadShort(  cmFileH_t h, short*          buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileReadUShort( cmFileH_t h, unsigned short* buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileReadLong(   cmFileH_t h, long*           buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileReadULong(  cmFileH_t h, unsigned long*  buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileReadInt(    cmFileH_t h, int*            buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileReadUInt(   cmFileH_t h, unsigned int*   buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileReadFloat(  cmFileH_t h, float*          buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileReadDouble( cmFileH_t h, double*         buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileReadBool(   cmFileH_t h, bool*           buf, unsigned cnt ) 
{ return cmFileRead(h,buf,sizeof(buf[0])*cnt); }



cmFileRC_t cmFileWriteChar(   cmFileH_t h, const char*           buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileWriteUChar(  cmFileH_t h, const unsigned char*  buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileWriteShort(  cmFileH_t h, const short*          buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileWriteUShort( cmFileH_t h, const unsigned short* buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileWriteLong(   cmFileH_t h, const long*           buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileWriteULong(  cmFileH_t h, const unsigned long*  buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileWriteInt(    cmFileH_t h, const int*            buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileWriteUInt(   cmFileH_t h, const unsigned int*   buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileWriteFloat(  cmFileH_t h, const float*          buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileWriteDouble( cmFileH_t h, const double*         buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }

cmFileRC_t cmFileWriteBool(   cmFileH_t h, const bool*           buf, unsigned cnt )
{ return cmFileWrite(h,buf,sizeof(buf[0])*cnt); }




cmFileRC_t cmFilePrint(   cmFileH_t h, const cmChar_t* text )
{
  cmFile_t* p = _cmFileHandleToPtr(h);

  errno = 0;
  if( fputs(text,p->fp) < 0 )
    return _cmFileError(p,kPrintFailFileRC,errno,"File print failed");

  return kOkFileRC;
}


cmFileRC_t cmFileVPrintf( cmFileH_t h, const cmChar_t* fmt, va_list vl )
{
  cmFile_t* p = _cmFileHandleToPtr(h);
  
  if( vfprintf(p->fp,fmt,vl) < 0 )
    return _cmFileError(p,kPrintFailFileRC,errno,"File print failed");
  
  return kOkFileRC;
}

cmFileRC_t cmFilePrintf(  cmFileH_t h, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmFileRC_t rc = cmFileVPrintf(h,fmt,vl);
  va_end(vl);
  return rc;
}


