#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFile.h"
#include "cmAudLabelFile.h"

cmAlfH_t cmAlfNullHandle = cmSTATIC_NULL_HANDLE;

typedef struct cmAlfRecd_str
{
  cmAlfLabel_t r;
  struct cmAlfRecd_str* link;
} cmAlfRecd_t; 

typedef struct
{
  cmErr_t      err;
  cmLHeapH_t   lH;
  cmFileH_t    fH;
  cmAlfRecd_t* list;
  int          cnt;
} cmAlf_t;

cmAlf_t* _cmAlfHandleToPtr( cmAlfH_t h )
{
  cmAlf_t* p = (cmAlf_t*)h.h;
  assert( p != NULL );
  return p;
}

cmAlfRC_t _cmAlfFree( cmAlf_t* p )
{
  cmAlfRC_t rc = kOkAlfRC;

  cmLHeapDestroy(&p->lH);

  cmMemPtrFree(&p);

  return rc;
}


cmAlfRC_t cmAudLabelFileAlloc(       cmCtx_t* ctx, cmAlfH_t* hp )
{
  cmAlfRC_t rc;
  if((rc = cmAudLabelFileFree(hp)) != kOkAlfRC )
    return rc;

  cmAlf_t* p = cmMemAllocZ(cmAlf_t,1);
  cmErrSetup(&p->err,&ctx->rpt,"Audio Label File");

  if(!cmLHeapIsValid( p->lH = cmLHeapCreate(1024,ctx)))
  {
    cmErrMsg(&p->err,kLHeapFailAlfRC,"Linked heap create failed.");
    goto errLabel;
  }
    
  hp->h = p;

 errLabel:
  return rc;
}

cmAlfRC_t cmAudLabelFileAllocOpen(   cmCtx_t* ctx, cmAlfH_t* hp, const cmChar_t* fn )
{
  cmAlfRC_t rc;
  if((rc = cmAudLabelFileAlloc(ctx,hp)) != kOkAlfRC)
    return rc;

  return cmAudLabelFileOpen(*hp,fn);
}

cmAlfRC_t cmAudLabelFileFree( cmAlfH_t* hp )
{
  cmAlfRC_t rc = kOkAlfRC;

  if( hp == NULL || cmAudLabelFileIsValid(*hp)==false )
    return kOkAlfRC;
    
  cmAlf_t* p = _cmAlfHandleToPtr(*hp);

  if((rc = _cmAlfFree(p)) != kOkAlfRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool cmAudLabelFileIsValid( cmAlfH_t h )
{ return h.h != NULL; }

void _cmAlfInsert( cmAlf_t* p, cmReal_t begSecs, cmReal_t endSecs, const cmChar_t* label )
{
  cmAlfRecd_t* np     = p->list;
  cmAlfRecd_t* pp     = NULL;
  cmAlfRecd_t* ip      = cmLhAllocZ(p->lH,cmAlfRecd_t,1);

  ip->r.begSecs = begSecs;
  ip->r.endSecs = endSecs;
  ip->r.label   = label==NULL || strlen(label)==0 ? NULL : cmLhAllocStr(p->lH,label);

  // set np to the next recd and
  // set pp to the prev recd
  while(np != NULL )
  {
    if( np->r.begSecs > begSecs )
      break;

    pp = np;
    np = np->link;
  }

  ip->link = np;

  // if the new recd is first on the list
  if( pp == NULL )
    p->list  = ip;
  else
    pp->link = ip;

  // incr the recd count
  ++p->cnt;
}

// remove the record just after pp
void _cmAlfRemove( cmAlf_t* p, cmAlfRecd_t* pp )
{
  // if the list is already empty
  if( p->list == NULL )
    return;

  // if the first recd should be removed
  if( pp == NULL )
  {
    p->list = p->list->link;
  }
  else
  {
    // if pp points to the last recd
    if( pp->link == NULL )
      return;

    // remove pp->link from the list
    pp->link = pp->link->link;
  }

  assert( p->cnt != 0 );
  --p->cnt;

}
  
cmAlfRC_t cmAudLabelFileOpen(   cmAlfH_t h, const cmChar_t* fn )
{
  cmAlfRC_t rc             = kOkAlfRC;
  cmAlf_t*  p              = _cmAlfHandleToPtr(h);
  cmChar_t* lineBuf        = NULL;
  unsigned  lineBufByteCnt = 0;
  unsigned  line           = 1;
  cmFileH_t fH             = cmFileNullHandle;

  // open the label file
  if( cmFileOpen(&fH,fn,kReadFileFl,p->err.rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailAlfRC,"The audio label file '%s' could not be openend.",cmStringNullGuard(fn));
    goto errLabel;
  }

  // read each line
  while( cmFileGetLineAuto(fH,&lineBuf,&lineBufByteCnt) == kOkFileRC )
  {
    cmReal_t  begSecs;
    cmReal_t  endSecs;
    cmChar_t* label  = NULL;
    cmChar_t* begPtr = lineBuf;
    cmChar_t* endPtr = NULL;

    // parse the start time in seconds
    errno = 0;
    begSecs = strtod(begPtr,&endPtr);
    if( errno != 0 )
      return cmErrMsg(&p->err,kSyntaxErrAlfRC,"Begin time conversion error on line %i in '%s'.",line,cmFileName(fH));
      
    // parse the end time in seconds
    begPtr = endPtr;
    endSecs = strtod(begPtr,&endPtr);
    if( errno != 0 )
      return cmErrMsg(&p->err,kSyntaxErrAlfRC,"End time conversion error on line %i in '%s'.",line,cmFileName(fH));

    label = endPtr;

    // eat any leading white space off the label
    while( *label )
    {
      if( isspace(*label) )
        ++label;
      else
        break;
    }

    // trim trailing space and '\n' from the label.
    int i = strlen(label)-1;
    for(; i>=0; --i)
    {
      if( isspace(label[i]) )
        label[i]=0;
      else
        break;
    }

    // if the label does not exist
    if( strlen(label)==0 )
      label = NULL;
    
    // insert a new recd
    _cmAlfInsert(p,begSecs,endSecs,label);

    ++line;
  }

  cmMemPtrFree(&lineBuf);

  if( cmFileClose(&fH) != kOkFileRC )
    rc = cmErrMsg(&p->err,kFileFailAlfRC,"The audio label file close failed.");

 errLabel:
  return rc;
}

cmAlfRC_t cmAudLabelFileInsert( cmAlfH_t h, cmReal_t begSecs, cmReal_t endSecs, const cmChar_t* label )
{
  cmAlfRC_t rc = kOkAlfRC;
  cmAlf_t*  p  = _cmAlfHandleToPtr(h);
  _cmAlfInsert(p,begSecs,endSecs,label);
  return rc;
}
  
unsigned cmAudLabelFileCount( cmAlfH_t h )
{
  cmAlf_t*  p  = _cmAlfHandleToPtr(h);
  return p->cnt;
}
const cmAlfLabel_t* cmAudLabelFileLabel( cmAlfH_t h, unsigned idx )
{
  cmAlf_t*     p  = _cmAlfHandleToPtr(h);
  cmAlfRecd_t* lp = p->list;
  unsigned     i;

  for(i=0; lp!=NULL && i<idx; ++i)
    lp            = lp->link;

  return &lp->r;
}

cmAlfRC_t cmAudLabelFileWrite( cmAlfH_t h, const cmChar_t* fn )
{
  cmAlfRC_t    rc = kOkAlfRC;
  cmAlf_t*     p  = _cmAlfHandleToPtr(h);
  cmAlfRecd_t* lp = p->list;
  cmFileH_t    fH = cmFileNullHandle;

  if( cmFileOpen(&fH,fn,kWriteFileFl,p->err.rpt) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailAlfRC,"The audio label output file '%s' could not be created.",cmStringNullGuard(fn));
    goto errLabel;
  }

  for(; lp!=NULL; lp=lp->link)
  {
    if( cmFilePrintf(fH,"%f %f %s",lp->r.begSecs,lp->r.endSecs,lp->r.label == NULL ? "" : lp->r.label) != kOkFileRC )
    {
      rc = cmErrMsg(&p->err,kFileFailAlfRC,"The audio label output file write failed.");
      goto errLabel;
    }
  }

 errLabel:
  if( cmFileClose(&fH) != kOkFileRC )
  {
    rc = cmErrMsg(&p->err,kFileFailAlfRC,"The audio label output file '%s' close failed.",cmStringNullGuard(fn));
    
  }

  return rc;
}


void cmAudLabelFileTest( cmCtx_t* ctx )
{
  const cmChar_t* fn = "/home/kevin/temp/labels.txt";
  const cmChar_t* ofn = "/home/kevin/temp/labels_out.txt"; 
  cmAlfH_t h = cmAlfNullHandle;

  if( cmAudLabelFileAllocOpen(ctx,&h,fn) == kOkAlfRC )
  {
    unsigned n = cmAudLabelFileCount(h);
    unsigned i;
    for(i=0; i<n; ++i)
    {
      const cmAlfLabel_t* lp;
      if((lp = cmAudLabelFileLabel(h,i)) != NULL )
        cmRptPrintf(&ctx->rpt,"%f %f %s\n",lp->begSecs,lp->endSecs,lp->label);
        
    }

    cmAudLabelFileWrite(h,ofn);

    cmAudLabelFileFree(&h);
  }
}
