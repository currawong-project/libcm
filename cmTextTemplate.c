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


cmTtH_t cmTtNullHandle = cmSTATIC_NULL_HANDLE;

enum
{
  kVarTtId,   // variable node
  kValTtId,   // value (leaf) node
  kTextTtId   // text
};

typedef struct cmTtToken_str
{
  unsigned              typeId;
  cmChar_t*             text;
  cmChar_t*             end;
  struct cmTtToken_str* next;
  struct cmTtToken_str* prev;
} cmTtToken_t;

typedef struct cmTtNode_str
{
  unsigned             typeId;
  cmChar_t*            label;
  cmTtToken_t*         token;
  struct cmTtNode_str* parent;
  struct cmTtNode_str* children;
  struct cmTtNode_str* sibling;
} cmTtNode_t;

typedef struct
{
  cmErr_t      err;
  cmLHeapH_t   lhH;
  cmChar_t*    buf;
  cmChar_t*    fn;
  cmTtNode_t*  tree;
  cmTtToken_t* tokens;
} cmTt_t;

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

cmTtToken_t*  _cmTtCreateToken( cmTt_t* p, unsigned typeId, cmChar_t* s0, cmChar_t* s1 )
{
  cmTtToken_t* tp = p->tokens;
  cmTtToken_t* t = cmLhAllocZ(p->lhH,cmTtToken_t,1);
  t->typeId      = kVarTtId;
  t->text        = s0;
  t->end         = s1;
  
  if( tp == NULL )
    p->tokens = t;
  else
  {
    while( tp->next!=NULL )
      tp=tp->next;
      
    tp->next = t;
    t->prev  = tp;
  }

  return tp;
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
          case '$':
            {
              _cmTtCreateToken(p,kTextTtId,s0,s+i-1);          
              state = kEnd;
              s0    = s + i;
            }
            break;

          case '"':
            state = kQuote;
            break;
        }

      case kEnd:  // searching for end '$'
        switch(c)
        {
          case '$':
            {
              _cmTtCreateToken(p,kVarTtId,s0,s+i);
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

  return rc;
}

bool _cmTtTokenIsBegin( cmTtToken_t* tp ) 
{ 
  assert(tp->text!=NULL && tp->text[0]=='$'); 
  return tp->typeId==kVarTtId && tp->text[1]=='{'; 
}

bool _cmTtTokenIsEnd( cmTtToken_t* tp ) 
{ 
  assert(tp->text!=NULL && tp->text[0]=='$'); 
  return tp->typeId==kVarTtId && tp->text[1]=='}'; 
}

cmTtNode_t* _cmTtCreateNode( cmTt_t* p, cmTtNode_t* parent, unsigned typeId, cmTtToken_t* tp )
{
  cmTtNode_t* nnp = cmLhAllocZ(p->lhH,cmTtNode_t,1);
  nnp->typeId = typeId;
  nnp->token  = tp;
  nnp->parent = parent;
  
  if( parent != NULL )
  {
    if( parent->children == NULL )
      parent->children = nnp;
    else
    {
      cmTtNode_t* np = nnp->children;
      while( np->sibling != NULL )
        np=np->sibling;
      
      np->sibling = nnp;
    }
  }

  return nnp;
}

cmTtToken_t*  _cmTtBuildTree( cmTt_t* p, cmTtNode_t* np, cmTtToken_t* tp )
{
  cmTtToken_t* ftp = tp;
  int          cnt = 0;
  while( tp != NULL )
  {
    if( _cmTtTokenIsBegin(tp) )
    {      
      // attach preceding text to new right-most leaf-node on 'np'.
      _cmTtCreateNode(p,np,kTextTtId,ftp); 

      // break the token chain before the 'begin' token
      if( tp->prev != NULL )
        tp->prev->next = NULL;
      tp->prev = NULL;
      
      // create a new child variable node and advance to token string
      tp  = _cmTtBuildTree(p, _cmTtCreateNode(p,np,kVarTtId,NULL), tp->next );
      ftp = tp;
      ++cnt;
    }

    
    if( _cmTtTokenIsEnd(tp) )
    {
      --cnt;

      // break the token chain after the 'end' token
      if( tp->next != NULL )
        tp->next->prev = NULL;
      tp->next = NULL;

      // create a new right-most leaf-node 
      _cmTtCreateNode(p,np,kTextTtId,ftp);

      tp = tp->next;
      break;
    }

    tp = tp->next;
  }

  if( cnt != 0 )
    cmErrMsg(&p->err,kSyntaxErrTtRC,"The template file '%s' appears to have unbalanced begin/end markers.",cmStringNullGuard(p->fn));
  
  return tp;
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
  if((p->buf = cmFileFnToBuf(fn,p->err.rpt,NULL)) == NULL )
  {
    rc = cmErrMsg(&p->err,kFileFailTtRC,"Unable to open the file '%s'.",cmStringNullGuard(fn));
    goto errLabel;
  }
  
  // store the template file name
  p->fn   = cmLhAllocStr(p->lhH,fn);

  // break the template file into tokens
  if((rc = _cmTtScan(p,p->buf)) != kOkTtRC )
    goto errLabel;

  // create the root node
  p->tree = _cmTtCreateNode(p,NULL,kVarTtId,NULL);

  // build the node tree
  _cmTtBuildTree(p,p->tree,p->tokens);

  // check for errors
  rc = cmErrLastRC(&p->err);
  
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

void cmTextTemplatePrintTokens( cmTtH_t h, cmRpt_t* rpt )
{
  cmTt_t* p = _cmTtHandleToPtr(h);

  cmTtToken_t* tp = p->tokens;
  cmChar_t* ep = p->buf + strlen(p->buf);
  for(; tp!=NULL; tp=tp->next)
  {
    bool     fl = tp->end < ep;
    cmChar_t c  =  fl ? tp->end[1] : 0;

    cmRptPrintf(rpt,"%s",tp->text);

    if( fl )
      tp->end[1] = c;    
  }
}


cmTtRC_t cmTextTemplateTest( cmCtx_t* ctx, const cmChar_t* fn )
{
  cmTtRC_t rc;
  cmTtH_t h = cmTtNullHandle;

  if((rc = cmTextTemplateInitialize(ctx,&h,fn)) != kOkTtRC )
    return rc;

  cmTextTemplatePrintTokens(h,&ctx->rpt);

  cmTextTemplateFinalize(&h);

  return rc;
}
