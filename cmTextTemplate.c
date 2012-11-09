#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmText.h"
#include "cmFile.h"
#include "cmJson.h"
#include "cmTextTemplate.h"

/*
  $var$      // global var

  ${name$ $var0$ $var1$ $}$   // name.var0 name.var1 

  ${name$ $var0$ $var1$ $}$   // name.var0 name.var1 


  replace(tpl,"val","var0")               // var0 = val
  replace(tpl,"val","name","var0",NULL)   // name.var0 = val          - named assignment
  replace(tpl,"val0","val0","name")       // name.0=val0; name.1=val1 - place assignment
  repeat(  tpl, "name" )                  // create a copy of "name" just after name
  clean(   tpl, "name" )                  // remove unused variables from "name"
*/


#define kVarBegChar '$'
#define kVarEndChar '$'
#define kSetBegChar '{'
#define kSetEndChar '}'

typedef enum
{
  kTextTtId,
  kVarTtId,
  kSetTtId
} cmTtId_t;

typedef struct cmTtNode_str
{
  cmTtId_t             typeId;
  cmChar_t*            label;
  cmChar_t*            text;
  struct cmTtNode_str* parent;
  struct cmTtNode_str* children;
  struct cmTtNode_str* rsib;
  struct cmTtNode_str* lsib;
} cmTtNode_t;

typedef struct
{
  cmCtx_t*     ctx;
  cmErr_t      err;
  cmLHeapH_t   lhH;
  cmChar_t*    buf;
  cmChar_t*    fn;
  cmTtNode_t*  tree;
} cmTt_t;

cmTtH_t cmTtNullHandle = cmSTATIC_NULL_HANDLE;

cmTt_t* _cmTtHandleToPtr( cmTtH_t h )
{
  cmTt_t* p = (cmTt_t*)h.h;
  assert( p != NULL );
  return p;
}

cmTtRC_t _cmTtFinalize( cmTt_t* p )
{
  cmTtRC_t rc = kOkTtRC;
  if( p == NULL )
    return rc;

  cmLHeapDestroy(&p->lhH);
  cmMemPtrFree(&p->buf);
  cmMemFree(p);
  return rc;
}

void _cmTtAppendChild( cmTtNode_t* parent, cmTtNode_t* np )
{
  np->parent = parent;
  np->rsib   = NULL;

  if( parent->children == NULL )
  {
    parent->children = np;
    np->lsib         = NULL;
  }
  else
  {
    cmTtNode_t* cnp = parent->children;
    while( cnp->rsib != NULL )
      cnp=cnp->rsib;

    cnp->rsib = np;
    np->lsib  = cnp;
  }
}

void  _cmTtCreateTokenNode( cmTt_t* p, cmTtId_t typeId, cmChar_t* s0, cmChar_t* s1 )
{
  if( typeId == kVarTtId )
  {
    ++s0;
    --s1;    
  }

  cmChar_t* s = cmLhAllocStrN(p->lhH,s0,s1-s0+1);

  cmTtNode_t* t  = cmLhAllocZ(p->lhH,cmTtNode_t,1);
  t->typeId      = typeId;
  t->text        = typeId == kTextTtId ? s : NULL;
  t->label       = typeId == kVarTtId  ? s : NULL;

  _cmTtAppendChild(p->tree,t);  
}

cmTtNode_t* _cmTtCreateSetNode( cmTt_t* p, cmTtNode_t* parent, cmTtNode_t* child )
{
  cmTtNode_t* nnp = cmLhAllocZ(p->lhH,cmTtNode_t,1);
  nnp->typeId  = kSetTtId;
  nnp->parent  = parent;
  if( child != NULL )
  {
    nnp->children = child->rsib;
      
    // The set node's label is taken from the label of the first child.
    if( child->label != NULL && strlen(child->label)>0 )
      nnp->label = cmLhAllocStr(p->lhH,child->label+1);  // (strip '{' from label)

    child->label  = NULL;
  }

  return nnp;
}

cmTtRC_t  _cmTtScan( cmTt_t* p, cmChar_t* s)
{
  enum { kBeg, kEnd, kQuote };

  cmTtRC_t  rc    = kOkTtRC;
  unsigned  i     = 0;
  unsigned  line  = 1;
  unsigned  state = kBeg;
  cmChar_t* s0    = s;

  for(; rc==kOkTtRC && s[i]; ++i)
  {
    cmChar_t c = s[i];

    if( c == '\n')
      ++line;

    switch(state)
    {
      case kBeg:  // searching for begin '$'
        switch( c )
        {
          case kVarBegChar:
            {
              _cmTtCreateTokenNode(p,kTextTtId,s0,s+i-1);          
              state = kEnd;
              s0    = s + i;
            }
            break;

          case '"':
            state = kQuote;
            break;
        }
        break;

      case kEnd:  // searching for end '$'
        switch(c)
        {
          case kVarEndChar:
            {
              _cmTtCreateTokenNode(p,kVarTtId,s0,s+i);
              state = kBeg;
              s0    = s + i + 1;
            }
            break;

          case '\n':
            rc = cmErrMsg(&p->err,kSyntaxErrTtRC,"A end-of-line was encountered inside a template variable on line %i in '%s'.",line,p->fn);
            break;

          case '"':
            rc = cmErrMsg(&p->err,kSyntaxErrTtRC,"A double-quote character was found inside a template variable on line %i in '%s'.",line,p->fn);
            break;
        }
        break;

      case kQuote: // searching for '"'
        switch(c)
        {
          case '"':   
            state = kBeg; 
            break;

          case '\n':
            rc = cmErrMsg(&p->err,kSyntaxErrTtRC,"A double-quote character was found inside a quoted string on line %i in '%s'.",line,p->fn);
            break;
        }
        break;

      default:
        { assert(0); }
    }
  }

  switch(state)
  {
    case kBeg: _cmTtCreateTokenNode(p,kTextTtId,s0,s0+strlen(s0)-1); break;        
    case kEnd:   rc = cmErrMsg(&p->err,kSyntaxErrTtRC,"Missing template variable ending '%c'.",kVarEndChar); break;
    case kQuote: rc = cmErrMsg(&p->err,kSyntaxErrTtRC,"Missing ending double-quote in quoated string."); break;
    default:
      { assert(0); }
  }
  

  return rc;
}

bool _cmTtTokenIsBegin( cmTtNode_t* tp ) 
{ 
  return tp->typeId==kVarTtId && tp->label[0]==kSetBegChar; 
}

bool _cmTtTokenIsEnd( cmTtNode_t* tp ) 
{ 
  return tp->typeId==kVarTtId && tp->label[0]==kSetEndChar; 
}


cmTtNode_t*  _cmTtBuildTree( cmTt_t* p, cmTtNode_t* np, cmTtNode_t* tp )
{

  while( tp != NULL )
  {
    tp->parent = np;

    if( _cmTtTokenIsBegin(tp) )
    {      
      cmTtNode_t* nnp = _cmTtCreateSetNode(p,np,tp);
      tp->parent = nnp;
      nnp->lsib  = tp->lsib;

      // break the token chain before the 'begin' token
      if( tp->lsib != NULL )
        tp->lsib->rsib = nnp;
      

      // create a new child variable node and advance to token string
      if((tp  = _cmTtBuildTree(p, nnp, tp->rsib)) == NULL )
        break;      

      nnp->rsib = tp;
    }

    
    if( _cmTtTokenIsEnd(tp) )
    {
      // break the token chain before the 'end' token
      if( tp->lsib != NULL )
        tp->lsib->rsib = NULL;
      
      
      // the token after 'end' become the current token
      tp = tp->rsib;
      if( tp != NULL )
      {
        if( tp->lsib != NULL )
          tp->lsib->rsib = NULL;
        tp->lsib = NULL;
      }

      break;
    }

    tp = tp->rsib;
  }

  
  return tp;
}

cmTtNode_t* _cmTtCloneNode( cmTt_t* p, const cmTtNode_t* snp )
{
  cmTtNode_t* np = cmLhAllocZ(p->lhH,cmTtNode_t,1);
  np->typeId = snp->typeId;
  np->label  = snp->label == NULL ? NULL : cmLhAllocStr(p->lhH,snp->label);
  np->text   = snp->text  == NULL ? NULL : cmLhAllocStr(p->lhH,snp->text);
  
  
  cmTtNode_t* csnp = snp->children;
  for(; csnp!=NULL; csnp=csnp->rsib)
  {
    cmTtNode_t* cnp = _cmTtCloneNode(p,csnp);
    _cmTtAppendChild(np,cnp);
  }
  
  return np;
}

cmTtNode_t* _cmTtRepeatNode( cmTt_t* p, cmTtNode_t* snp )
{
  cmTtNode_t* stnp = _cmTtCloneNode(p,snp);
  stnp->parent = snp->parent;
  stnp->lsib   = snp;
  stnp->rsib   = snp->rsib;
  if( snp->rsib != NULL )
    snp->rsib->lsib = stnp;
  snp->rsib = stnp;
  return stnp;
}

cmTtNode_t* _cmTtFindNodeV( cmTt_t* p, const cmChar_t* label, unsigned index, va_list vl )
{
  cmTtNode_t* np = p->tree;

  if( label == NULL )
    return NULL;

  assert( np!=NULL); // the tree should never be empty.

  while(1)
  {
    cmTtNode_t* cnp = np->children;

    // locate the label for the current path level
    for(; cnp!=NULL; cnp=cnp->rsib)
      if( cnp->label != NULL && strcmp(cnp->label,label)==0 )
        break;

    // the label at the current path level was not found
    if( cnp==NULL )
      return NULL;

    unsigned i;
    // locate the index at the current level - all labels
    // must match the current label 
    for(i=0; cnp!=NULL && i<index; cnp=cnp->rsib,++i)
      if( cnp->label==NULL || strcmp(cnp->label,label) )
      {
        // a label mismatch occurred.
        return NULL;
      }

    // the index was not found
    if( cnp==NULL )
      return NULL;

    // cnp is the matched node at this level
    np = cnp;

    // the end of the path was located - success!
    if((label = va_arg(vl,const cmChar_t*)) == NULL )
      break;
      
    index = va_arg(vl,unsigned);

  }

  return np;
}



cmTtRC_t cmTextTemplateInitialize( cmCtx_t* ctx, cmTtH_t* hp, const cmChar_t* fn )
{
  cmTtRC_t rc;

  if((rc = cmTextTemplateFinalize(hp)) != kOkTtRC )
    return rc;

  cmTt_t* p = cmMemAllocZ(cmTt_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"TextTemplate");

  // create the local linked heap
  if( cmLHeapIsValid(p->lhH = cmLHeapCreate(1024, ctx )) == false )
  {
    rc = cmErrMsg(&p->err,kLHeapFailTtRC,"Lheap Mgr. allocation failed.");
    goto errLabel;
  }
  
  // read the template file 
  if((p->buf = cmFileFnToStr(fn,p->err.rpt,NULL)) == NULL )
  {
    rc = cmErrMsg(&p->err,kFileFailTtRC,"Unable to open the file '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }
  
  // store the template file name
  p->fn   = cmLhAllocStr(p->lhH,fn);

  // create the root node
  p->tree = _cmTtCreateSetNode(p,NULL,NULL);

  // break the template file into tokens
  if((rc = _cmTtScan(p,p->buf)) != kOkTtRC )
    goto errLabel;

  // The tree now has two levels. The root node
  // and a flat linked list of token nodes which are the children
  // of the root node.

  // build the node tree
  _cmTtBuildTree(p,p->tree,p->tree->children);

  // check for errors
  rc = cmErrLastRC(&p->err);

  p->ctx = ctx;
  hp->h = p;
  
 errLabel:

  if( rc != kOkTtRC )
    _cmTtFinalize(p);
  
  return rc;
}

cmTtRC_t cmTextTemplateFinalize( cmTtH_t* hp )
{
  cmTtRC_t rc = kOkTtRC;

  if( hp==NULL || cmTextTemplateIsValid(*hp)==false ) 
    return rc;

  cmTt_t* p = _cmTtHandleToPtr(*hp);

  if((rc = _cmTtFinalize(p)) != kOkTtRC )
    return rc;

  hp->h = NULL;

  return rc;
}

bool     cmTextTemplateIsValid( cmTtH_t h )
{ return h.h != NULL; }

cmTtRC_t _cmTtSetValue( cmTt_t* p, cmTtNode_t* np, const cmChar_t* label, unsigned index, const cmChar_t* value )
{
  // only the value of variable nodes may be set
  if( np->typeId != kVarTtId )
    return cmErrMsg(&p->err,kInvalidTypeTtRC,"The template variable beginning at the path '%s' index:%i could not be found.",cmStringNullGuard(label),index);

  // set the value 
  if( value != NULL )
    np->text = cmLhResizeStr(p->lhH,np->text,value);
  else
  {
    cmLhFree(p->lhH,np->text);
    np->text = NULL;
  }

  return kOkTtRC;
}

cmTtRC_t cmTextTemplateSetValueV( cmTtH_t h, const cmChar_t* value, const cmChar_t* label, unsigned index, va_list vl )
{
  cmTt_t*     p = _cmTtHandleToPtr(h);
  cmTtNode_t* np;

  // locate the requested node
  if((np = _cmTtFindNodeV(p,label,index,vl)) == NULL )
    return cmErrMsg(&p->err,kFindFailTtRC,"The template variable beginning at the path '%s' index:%i could not be found.",cmStringNullGuard(label),index);

  return _cmTtSetValue(p,np,label,index,value);
}

cmTtRC_t cmTextTemplateSetValue( cmTtH_t h, const cmChar_t* value, const cmChar_t* label, unsigned index, ... )
{
  cmTtRC_t rc;
  va_list vl;
  va_start(vl,index);
  rc = cmTextTemplateSetValueV(h,value,label,index,vl);
  va_end(vl);
  return rc;
}

cmTtRC_t cmTextTemplateRepeatV( cmTtH_t h, const cmChar_t* label, unsigned index, va_list vl )
{
  cmTt_t*     p = _cmTtHandleToPtr(h);
  cmTtNode_t* np;

  // locate the requested node
  if((np = _cmTtFindNodeV(p,label,index,vl)) == NULL )
    return cmErrMsg(&p->err,kFindFailTtRC,"The template variable beginning at the path '%s' index:%i could not be found.",cmStringNullGuard(label),index);

   _cmTtRepeatNode(p,np);

   return kOkTtRC;
}

cmTtRC_t cmTextTemplateRepeat( cmTtH_t h, const cmChar_t* label, unsigned index, ... )
{
  cmTtRC_t rc;
  va_list vl;
  va_start(vl,index);
  rc = cmTextTemplateRepeatV(h,label,index,vl);
  va_end(vl);
  return rc;
}

cmTtRC_t  _cmTtWriteNode( cmTt_t* p, cmTtNode_t* np, cmFileH_t fh )
{
  cmTtRC_t   rc  = kOkTtRC;
  cmFileRC_t frc = kOkFileRC;

  switch( np->typeId )
  {
    case kTextTtId:
    case kVarTtId:
      {
        if( np->text != NULL )
          if((frc = cmFilePrint(fh,np->text)) != kOkFileRC )
            rc = cmErrMsg(&p->err,kFileFailTtRC,"File write failed on '%s'.", cmFileName(fh));
      }
      break;

    case kSetTtId:
      {
        cmTtNode_t* cnp;
        for(cnp=np->children; cnp!=NULL && rc==kOkTtRC; cnp=cnp->rsib)
          rc = _cmTtWriteNode(p,cnp,fh);
      }
      break;

    default:
      { assert(0); }
  }

  return rc;
}

cmTtRC_t cmTextTemplateWrite( cmTtH_t h, const cmChar_t* fn )
{
  cmTtRC_t rc = kOkTtRC;
  cmTt_t* p = _cmTtHandleToPtr(h);
  cmFileH_t fh;

  if( cmFileOpen(&fh,fn,kReadFileFl,p->err.rpt) != kOkFileRC )
    return cmErrMsg(&p->err,kFileFailTtRC,"The file '%s' could not be opened.",cmStringNullGuard(fn));

  rc = _cmTtWriteNode(p,p->tree,fh);

  if( cmFileClose(&fh) != kOkFileRC )
    rc = cmErrMsg(&p->err,kFileFailTtRC,"The output file '%s' failed on close.",cmStringNullGuard(fn));

  return rc;      
}

typedef struct cmTtPath_str
{
  const cmChar_t*      label;
  unsigned             index;
  struct cmTtPath_str* next;
  struct cmTtPath_str* prev;
} cmTtPath_t;


cmTtNode_t* _cmTtFindNode( cmTt_t* p, const cmTtPath_t* pp  )
{
  cmTtNode_t* np = p->tree;
 
  if( pp == NULL )
    return NULL;

  assert( np!=NULL); // the tree should never be empty.

  for(; pp!=NULL; pp=pp->next)
  {
    cmTtNode_t* cnp = np->children;

    // locate the label for the current path level
    for(; cnp!=NULL; cnp=cnp->rsib)
      if( cnp->label != NULL && strcmp(cnp->label,pp->label)==0 )
        break;

    // the label at the current path level was not found
    if( cnp==NULL )
      return NULL;

    unsigned i;
    // locate the index at the current level - all labels
    // must match the current label 
    for(i=0; cnp!=NULL && i<pp->index; cnp=cnp->rsib,++i)
      if( cnp->label==NULL || strcmp(cnp->label,pp->label) )
      {
        // a label mismatch occurred.
        return NULL;
      }

    // the index was not found
    if( cnp==NULL )
      return NULL;

    // cnp is the matched node at this level
    np = cnp;

  }

  return np;
}

cmTtRC_t _cmTextTemplateSetValue(cmTt_t* p, const cmTtPath_t *pp, const cmChar_t* value)
{
  cmTtNode_t* np;

  assert( pp != NULL );

  if((np = _cmTtFindNode(p,pp)) == NULL )
    return cmErrMsg(&p->err,kFindFailTtRC,"The template variable beginning at the path '%s' index:%i could not be found.",cmStringNullGuard(pp->label),pp->index);
 
  return _cmTtSetValue(p,np,pp->label,pp->index,value);
}

cmTtRC_t _cmTextTemplateRepeatNodeN( cmTt_t* p, const cmTtPath_t* pp, unsigned n )
{
  cmTtNode_t* np;
  unsigned i;

  // locate the requested node
  if((np = _cmTtFindNode(p,pp)) == NULL )
    return cmErrMsg(&p->err,kFindFailTtRC,"The template variable beginning at the path '%s' index:%i could not be found.",cmStringNullGuard(pp->label),pp->index);

  for(i=0; i<n; ++i)
    _cmTtRepeatNode(p,np);

   return kOkTtRC;
}


cmTtPath_t*  _cmTtPathAppend( cmTtPath_t* list, cmTtPath_t* ele, const cmChar_t* label, unsigned index )
{
  cmTtPath_t* pp = list;

  if( pp == NULL )
    list = ele;
  else
  {
    while( pp->next!=NULL )
      pp=pp->next;
    
    pp->next   = ele;
  }

  ele->label = label;
  ele->index = index;
  ele->prev  = pp;
  ele->next  = NULL;

  return list;
}

cmTtRC_t  _cmTextTemplateApply( cmTt_t* p, cmJsonNode_t* jnp, cmTtPath_t* list, unsigned index )
{
  cmTtRC_t      rc   = kOkTtRC;
  switch( jnp->typeId & kMaskTId )
  {
    case kPairTId:     
      {
        const cmChar_t*  label = cmJsonPairLabel(jnp);
        cmJsonNode_t*    vjnp  = cmJsonPairValue(jnp);
        cmTtPath_t       ele0;

        // extend the path with the pair label
        list = _cmTtPathAppend(list,&ele0,label,index);

        switch( vjnp->typeId & kMaskTId )
        {
          case kStringTId: 
            _cmTextTemplateSetValue(p,list,vjnp->u.stringVal);
            break;

          case kObjectTId:
            {
              cmJsonNode_t* mjnp = vjnp->u.childPtr;
              for(; mjnp!=NULL; mjnp=mjnp->siblingPtr)
                rc = _cmTextTemplateApply(p,mjnp,list,0);
            }
            break;

          case kArrayTId:
            {
              unsigned n = cmJsonChildCount(vjnp);
              unsigned i;

              if( n > 1 )
                _cmTextTemplateRepeatNodeN(p,list,n-1);

              for(i=0; i<n && rc==kOkTtRC; ++i)
              {
                ele0.index = i;
                rc = _cmTextTemplateApply(p,cmJsonArrayElement(vjnp,i),list,i);
              }
            }
            break;

          default:
            { assert(0); }

        }

        if( ele0.prev != NULL )
          ele0.prev->next = NULL;
      }
      break;

    case kObjectTId:
      {
        cmJsonNode_t* mjnp = jnp->u.childPtr;
        for(; mjnp!=NULL; mjnp=mjnp->siblingPtr)
          rc = _cmTextTemplateApply(p,mjnp,list,0);
      }
      break;

    default:
      { assert(0); }
  }
  

  return rc;
}

cmTtRC_t cmTextTemplateApply( cmTtH_t h, const cmChar_t* fn )
{
  cmTtRC_t rc = kOkTtRC;
  cmTt_t* p = _cmTtHandleToPtr(h);
  cmJsonH_t jsH = cmJsonNullHandle;
  
  if( cmJsonInitializeFromFile(&jsH,fn,p->ctx) != kOkJsRC )
    return cmErrMsg(&p->err,kJsonFailTtRC,"A JSON tree could not be initialized from '%s'.",cmStringNullGuard(fn));

  cmJsonNode_t* jnp = cmJsonRoot(jsH);

  if( jnp!=NULL)
    for(jnp=jnp->u.childPtr; jnp!=NULL && rc==kOkTtRC; jnp=jnp->siblingPtr )
      rc =  _cmTextTemplateApply(p,jnp,NULL,0);

  cmJsonFinalize(&jsH);
  return rc;
}

void _cmTtPrintNode( cmRpt_t* rpt, cmTtNode_t* np )
{
  switch( np->typeId )
  {
    case kTextTtId:
      cmRptPrintf(rpt,"%s",np->text);
      break;

    case kVarTtId:
      if( np->text != NULL )
        cmRptPrintf(rpt,"|%s=%s|",np->label,np->text);
      else
        cmRptPrintf(rpt,"|%s|",np->label);
      break;

    case kSetTtId:
      {
        cmTtNode_t* cnp;
        cmRptPrintf(rpt,"{");

        if( np->label != NULL )
          cmRptPrintf(rpt,"%s:",np->label);

        for(cnp=np->children; cnp!=NULL; cnp=cnp->rsib)
          _cmTtPrintNode(rpt,cnp);

        cmRptPrintf(rpt,"}");
      }
      break;
  }
}

void cmTtPrintTree( cmTtH_t h, cmRpt_t* rpt )
{
  cmTt_t* p = _cmTtHandleToPtr(h);
  _cmTtPrintNode(rpt,p->tree);
}



cmTtRC_t cmTextTemplateTest( cmCtx_t* ctx, const cmChar_t* fn )
{
  cmTtRC_t rc;
  cmTtH_t h = cmTtNullHandle;

  if((rc = cmTextTemplateInitialize(ctx,&h,fn)) != kOkTtRC )
    return rc;

  if(0)
  {
    cmTextTemplateRepeat(h,"name",0,NULL);

    cmTextTemplateRepeat(h,"var2",0,NULL);

    cmTextTemplateSetValue(h, "val0", "var2", 0, NULL );
  }
  else
  {
    cmTextTemplateApply(h,"/home/kevin/src/cmtest/src/cmtest/data/tmpl_src.js");
  }

  cmTtPrintTree(h,&ctx->rpt);

  cmTextTemplateFinalize(&h);

  return rc;
}
