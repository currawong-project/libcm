#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFile.h"
#include "cmTagFile.h"

cmTfH_t cmTfNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct
{
  cmErr_t    err;
  cmFileH_t  fH;
  cmLHeapH_t lH;
  cmTfTag_t* tagArray;
  unsigned   tagCnt;
  unsigned   tagAllocCnt;
} cmTf_t;


cmTf_t* _cmTfHandleToPtr( cmTfH_t h )
{
  cmTf_t* p = (cmTf_t*)h.h;
  assert( p != NULL );
  return p;
}

const cmChar_t* _cmTfFlagsToLabel( unsigned flags )
{
  switch(flags)
  {
    case kFuncProtoTfFl: return "p";
    case kFuncDefnTfFl: return "d";
    case kEnumTfFl:     return "e";
    case kMacroTfFl:    return "m";
    case kTypedefTfFl:  return "t";
    case kFieldTfFl:    return "f";
    case kExternTfFl:   return "x";
    case kStructTagTfFl:return "s";
    case kUnionTagTfFl: return "u";
    default:
      { assert(0); }
  }
  return "<unknown>";
}

void _cmTfPrintTag(cmRpt_t* rpt, const cmTfTag_t* r )
{
  cmRptPrintf(rpt,"%s %5i %s\n",_cmTfFlagsToLabel(r->flags),r->line,r->label);
}

cmTfRC_t _cmTfCloseFile( cmTf_t* p )
{
  cmTfRC_t rc = kOkTfRC;
  if( cmFileIsValid(p->fH) )
    cmFileClose(&p->fH);

  if( cmLHeapIsValid(p->lH) )
    cmLHeapDestroy(&p->lH);

  cmMemFree(p);
  return rc;
}

cmTfRC_t _cmTfSyntaxErr( cmTf_t* p, const cmChar_t* fn, unsigned line, const cmChar_t* msg )
{
  return cmErrMsg(&p->err,kSyntaxErrTfRC,"Syntax error: %s line:%i in file:%s\n",msg,line,cmStringNullGuard(fn));
}

cmTfRC_t _cmTfParseLine( cmTf_t* p, const cmChar_t* fn, unsigned line, cmChar_t* buf, unsigned bufCharCnt )
{  
  cmTfRC_t  rc               = kOkTfRC;
  const cmChar_t lineLabel[] = "line:";
  unsigned  lineLabelCharCnt = strlen(lineLabel);
  char*     s;
  cmTfTag_t r;
  memset(&r,0,sizeof(r));

  // if the line is empty
  if( buf==NULL || bufCharCnt == 0 )
    return rc;

  // eat leading white space
  while( *buf && isspace(*buf) )
    ++buf;

  // if the line is now empty
  if( *buf == 0 )
    return rc;

  // skip the file header lines - which begin with a '!' character
  if( *buf == '!' )
    return rc;

  // locate the end of the first (tag) field
  if( (s= strchr(buf,'\t')) == NULL )
    return _cmTfSyntaxErr(p,fn,line,"No tag label was found.");

  // zero terminate and copy construct the tag field  into r.label
  *s = 0;
  r.label = cmLhAllocStr( p->lH, buf );

  buf = s + 1; // buf now points to the file name
  if( (s = strchr(buf,'\t')) == NULL )
    return _cmTfSyntaxErr(p,fn,line,"No file name field found.");

  buf = s + 1; // buf now points to the 'EX" field
  if( (s = strchr(buf,'\t')) == NULL )
    return _cmTfSyntaxErr(p,fn,line,"No 'EX' field found.");

  buf = s + 1; // buf now points to the 'kind' field

  //
  // Use: 'ctags --list-kinds=c' to list all of the 'kind' character flags.
  //
  switch( *buf )
  {
    case 'd': // macro
      r.flags |= kMacroTfFl;
      break;

    case 'e': // enum value
      r.flags |= kEnumTfFl;
      break;

    case 'p': // function prototype
      r.flags |= kFuncProtoTfFl;
      break;

    case 'f': // function defn
      r.flags |= kFuncDefnTfFl;
      break;

    case 't': // typedef
      r.flags |= kTypedefTfFl;
      break;     

    case 'm': // member
      r.flags |= kFieldTfFl;
      break;

    case 'x': // externs and forward decl's
      r.flags |= kExternTfFl;
      break;

    case 's': // struct tag
      r.flags |= kStructTagTfFl;
      break;

    case 'u': // union tag
      r.flags |= kUnionTagTfFl;
      break;

    default: // unrecognized type
      return rc;
  }

  if( (s = strchr(buf,'\t')) == NULL )
    return _cmTfSyntaxErr(p,fn,line,"No 'kind' field found.");

  buf = s + 1; // buf now points to the 'line' field

  // 
  if( strncmp(buf,lineLabel,lineLabelCharCnt) != 0 )
    return _cmTfSyntaxErr(p,fn,line,"No 'line' field found.");

  buf += lineLabelCharCnt; // buf now points to the number part of the line field

  // parse the line number
  if( sscanf(buf,"%i",&r.line) != 1 )
    return _cmTfSyntaxErr(p,fn,line,"Line number parse failed.");


  // store the tag record
  p->tagArray[ p->tagCnt ] = r;
  ++p->tagCnt;

  return rc;
}

cmTfRC_t         cmTfOpenFile( cmCtx_t* ctx, cmTfH_t* hp, const cmChar_t* fn )
{
  cmTfRC_t  rc      = kOkTfRC;
  cmTf_t*   p       = cmMemAllocZ(cmTf_t,1);
  cmChar_t* bufPtr  = NULL;
  unsigned  lineCnt = 0;

  cmErrSetup(&p->err,&ctx->rpt,"Tag File");
  
  // create the internal linked heap
  if( cmLHeapIsValid(p->lH = cmLHeapCreate(8192,ctx)) == false )
  {
    rc = cmErrMsg(&p->err,kLHeapFailTfRC,"The internal link heap create failed.");
    goto errLabel;
  }

  // open the file
  if( cmFileOpen( &p->fH, fn, kReadFileFl, &ctx->rpt ) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailTfRC,"The tag file '%s' could not be opened.",cmStringNullGuard(fn));
    goto errLabel;
  }

  // get a count of the lines in the file
  if( cmFileLineCount( p->fH, &p->tagAllocCnt ) != kOkFileRC || p->tagAllocCnt == 0 )
  {
    rc = cmErrMsg(&p->err,kFileInvalidTfRC,"The tag file '%s' was invalid or empty.",cmStringNullGuard(fn));
    goto errLabel;
  }

  // allocate the tag array
  p->tagArray = cmLhAllocZ(p->lH,cmTfTag_t,p->tagAllocCnt);

  while( 1 )
  {
    unsigned   bufByteCnt = 0;
    cmFileRC_t frc;

    // read a line from the file
    if((frc = cmFileGetLineAuto(p->fH, &bufPtr, &bufByteCnt )) != kOkFileRC )
    {
      if( cmFileEof(p->fH)==false )
        rc = cmErrMsg(&p->err,kFileFailTfRC,"File read failed on tag line:%i in '%s'\n",lineCnt+1,cmStringNullGuard(fn));
      
      break;          
    }

    // parse a file line
    if((rc = _cmTfParseLine(p,fn,lineCnt+1,bufPtr,bufByteCnt)) != kOkTfRC )
      break;
    
    ++lineCnt;
  }


  cmMemFree(bufPtr);

  hp->h = p;

 errLabel:
  if( rc != kOkTfRC )
     _cmTfCloseFile(p);

  return rc;
}

cmTfRC_t         cmTfCloseFile( cmTfH_t* hp )
{
  cmTfRC_t rc = kOkTfRC;

  if( hp == NULL || cmTfIsValid(*hp) == false )
    return kOkTfRC;

  cmTf_t* p = _cmTfHandleToPtr(*hp);

  if((rc = _cmTfCloseFile(p)) != kOkTfRC )
    return rc;

  hp->h = NULL;
  return rc;
}

bool             cmTfIsValid( cmTfH_t h )
{ return h.h != NULL; }


unsigned         cmTfCount( cmTfH_t h )
{ 
  cmTf_t* p = _cmTfHandleToPtr(h);
  return p->tagCnt;
}

const cmTfTag_t* cmTfRecd( cmTfH_t h, unsigned index )
{
  cmTf_t* p = _cmTfHandleToPtr(h);
  assert( index < p->tagCnt );
  return p->tagArray + index;
}

cmTfRC_t cmTfReport( cmTfH_t h, cmRpt_t* rpt )
{
  unsigned i;
  cmTf_t* p = _cmTfHandleToPtr(h);  
  for(i=0; i<p->tagCnt; ++i)
  {
    cmRptPrintf(rpt,"%5i ",i);
    _cmTfPrintTag(rpt, p->tagArray + i );
  }
  return kOkTfRC;
}


cmTfRC_t cmTfTest( cmCtx_t* ctx, const cmChar_t* fn )
{
  cmTfRC_t rc = kOkTfRC;
  cmTfH_t h = cmTfNullHandle;

  if((rc = cmTfOpenFile(ctx,&h,fn)) == kOkTfRC )
  {
    cmTfReport(h,&ctx->rpt);
    rc = cmTfCloseFile(&h);
  }
  return rc;
}

