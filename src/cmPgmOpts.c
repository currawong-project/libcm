#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmPgmOpts.h"
#include "cmText.h"
#include "config.h"

enum { 
  kFlagPoFl  = 0x001,
  kBoolPoFl  = 0x002,  // this parameter does not take a value
  kCharPoFl  = 0x004,  // parm. value is a character
  kIntPoFl   = 0x008,  // parm. value is a decimal int
  kUIntPoFl  = 0x010,  // parm. value is a decimal unsigned int
  kDblPoFl   = 0x040,  // parm. value is a decimal double
  kStrPoFl   = 0x080,  // parm. value is a string (default)
  kEnumPoFl  = 0x100,  // parm. valus is a enum type (automatically set by a non-zero enumId)

  kTypeMaskPoFl = 0x1ff
};

cmPgmOptH_t cmPgmOptNullHandle = cmSTATIC_NULL_HANDLE;

struct _cmPoArg_str;

typedef  union
{
  bool            b;
  char            c;
  int             i;
  unsigned        u;
  double          d;
  const cmChar_t* s;
} _cmPoValue_t;

typedef  union
{
  bool*            b;
  char*            c;
  int*             i;
  unsigned*        u;
  double*          d;
  const cmChar_t** s;
} _cmPoValPtr_t;


// opt records describe the a given parameters configuration
typedef struct _cmPoOpt_str
{
  unsigned              numId;      // 
  cmChar_t              charId;     // 
  cmChar_t*             wordId;     //
  unsigned              sflags;     // state flags (type flags)
  unsigned              cflags;     // cfg flags (user settable cfg flag)
  unsigned              enumId;     // enumerated set element id (numId is the group id)
  struct _cmPoOpt_str*  enumPtr;    // pointer to mast enum recd (the master is the first param. rcvd for a given enum set - so this field is NULL for the master record)
  unsigned              maxInstCnt; //
  _cmPoValue_t          dfltVal;    // default value for this parm 
  _cmPoValPtr_t         retVal;     // client supplied variable which recieves the value of the last arg. parsed for this parm.
  cmChar_t*             helpStr;    //
  cmChar_t*             mstrStr;
  struct _cmPoOpt_str*  link;       // link used by the _cmPo_t.list linked list
  struct _cmPoArg_str*  inst;       // arg's belonging to this opt record formed by _cmPoArg_t.inst links
} _cmPoOpt_t;


// arg records describe an instance of a given parameter.
// (there may be multiple instances of a given parameter)
typedef struct _cmPoArg_str
{
  const  _cmPoOpt_t*   opt;     // the opt record associated with this arg. instance
  struct _cmPoArg_str* link;    // link used by the _cmPo_t.args list
  struct _cmPoArg_str* inst;    // link used by the _cmPoOpt_t* inst list
  const  cmChar_t*     valStr;  // string value for this arg.
  _cmPoValue_t         u;       // parsed value for this arg.
} _cmPoArg_t;

typedef struct
{
  cmErr_t     err;
  cmLHeapH_t  lH;
  cmChar_t*   helpBegStr;
  cmChar_t*   helpEndStr;
  _cmPoOpt_t* list;         // list of control records formed by _cmPoOpt_t.link links
  _cmPoArg_t* args;         // list of arg. records formed by _cmPoArg_t.link links
  bool        execFl;       // set to false in cmPgmOptParse() if only built-in options were selected
} _cmPo_t;

typedef struct
{
  unsigned        id;
  const cmChar_t* label;
} _cmPoMap_t;

_cmPoMap_t _cmPoTypeMap[] = 
{
  { kFlagPoFl, "" },
  { kBoolPoFl, "" },
  { kCharPoFl, "<char>" },
  { kIntPoFl,  "<int>" },
  { kUIntPoFl, "<uint>" },
  { kDblPoFl,  "<real>" },
  { kStrPoFl,  "<text>" },
  { kEnumPoFl, "<enum>" },
  { 0,         "<unknown>"}
};

const cmChar_t* _cmPoTypeFlagToLabel( unsigned flag, unsigned cflags )
{
  if( cmIsFlag(cflags,kHexPoFl) )
    return "<hex>";

  const _cmPoMap_t* m = _cmPoTypeMap;
  for(; m->id != 0; ++m)
    if( m->id == flag )
      break;
  return m->label;
}

bool _cmPoOptUsesValue( const _cmPoOpt_t* r )
{ 
  return 
    cmIsNotFlag(r->sflags,kEnumPoFl) && 
    cmIsNotFlag(r->sflags,kBoolPoFl) && 
    cmIsNotFlag(r->sflags,kFlagPoFl);
}

_cmPo_t* _cmPoHandleToPtr( cmPgmOptH_t h )
{
  _cmPo_t* p = (_cmPo_t*)h.h;
  assert(p != NULL );
  return p;
}

cmPoRC_t _cmPgmOptFinalize( _cmPo_t* p )
{
  cmPoRC_t rc = kOkPoRC;
  cmLHeapDestroy(&p->lH);
  cmMemPtrFree(&p);
  return rc;
}

bool     cmPgmOptIsValid( cmPgmOptH_t h )
{ return h.h != NULL; }

cmPoRC_t cmPgmOptInitialize(cmCtx_t* c, cmPgmOptH_t* hp, const cmChar_t* helpBegStr, const cmChar_t* helpEndStr )
{
  cmPoRC_t rc;
  if((rc = cmPgmOptFinalize(hp)) != kOkPoRC )
    return rc;
  
  _cmPo_t* p = cmMemAllocZ(_cmPo_t,1);

  cmErrSetup(&p->err,&c->rpt,"Program Options");

  p->lH         = cmLHeapCreate(2048,c);
  p->helpBegStr = helpBegStr==NULL ? NULL : cmLhAllocStr( p->lH, helpBegStr );
  p->helpEndStr = helpEndStr==NULL ? NULL : cmLhAllocStr( p->lH, helpEndStr );

  hp->h = p;

  return cmErrLastRC(&p->err);
}

cmPoRC_t cmPgmOptFinalize( cmPgmOptH_t* hp )
{
  cmPoRC_t rc = kOkPoRC;

  if( hp == NULL || cmPgmOptIsValid(*hp) == false )
    return kOkPoRC;

  _cmPo_t* p = _cmPoHandleToPtr(*hp);

  if((rc = _cmPgmOptFinalize(p)) != kOkPoRC )
    return rc;

  hp->h = NULL;
  return rc;
}

_cmPoOpt_t* _cmPgmOptNumIdToOptRecd( _cmPo_t* p, unsigned numId )
{
  _cmPoOpt_t* r = p->list;
  while( r != NULL )
  {
    if( r->numId == numId )
    {

      if( cmIsFlag(r->sflags,kEnumPoFl) && r->enumPtr!=NULL )
        r = r->enumPtr;

      return r;
    }

    r = r->link;
  }
  return NULL;
}

_cmPoOpt_t* _cmPgmOptEnumIdToOptRecd( _cmPo_t* p, unsigned numId, unsigned enumId )
{
  _cmPoOpt_t* r = p->list;
  while( r != NULL )
  {
    if( r->numId == numId && r->enumId == enumId )
      return r;

    r = r->link;
  }
  return NULL;
}


_cmPoOpt_t* _cmPgmOptCharIdToOptRecd( _cmPo_t* p, cmChar_t charId )
{
  _cmPoOpt_t* r = p->list;
  while( r != NULL )
  {
    if( r->charId == charId )
      return r;

    r = r->link;
  }
  return NULL;
}

_cmPoOpt_t* _cmPgmOptWordIdToOptRecd( _cmPo_t* p, const cmChar_t* wordId )
{
  _cmPoOpt_t* r = p->list;
  while( r != NULL )
  {
    if( strcmp(wordId,r->wordId) == 0 )
      return r;

    r = r->link;
  }
  return NULL;
}


cmPoRC_t _cmPgmOptInstall( _cmPo_t* p, unsigned numId, const cmChar_t charId, const cmChar_t* wordId, unsigned cflags, unsigned sflags, unsigned enumId, unsigned cnt, const cmChar_t* helpStr, _cmPoOpt_t** rpp )
{  
  // validate the num. id
  if( cmIsNotFlag(sflags,kEnumPoFl) &&  _cmPgmOptNumIdToOptRecd(p,numId) != NULL )
    return cmErrMsg(&p->err,kDuplicateIdPoRC,"The numeric id '%i' was already used by another parameter.",numId);

  
  // validate the char. id
  if(_cmPgmOptCharIdToOptRecd(p,charId) != NULL )
    return cmErrMsg(&p->err,kDuplicateIdPoRC,"The character id -'%c' was already used by another parameter.",charId);

  // validate the word. id
  if( _cmPgmOptWordIdToOptRecd(p,wordId) != NULL )
    return cmErrMsg(&p->err,kDuplicateIdPoRC,"The word id --'%s' was already used by another parameter.",wordId);

  // clear the kHexPoFl if this is not an int or uint.
  if( cmIsNotFlag(sflags,kFlagPoFl) && cmIsNotFlag(sflags,kIntPoFl) && cmIsNotFlag(sflags,kUIntPoFl) )
    cflags = cmClrFlag(cflags,kHexPoFl);

  // allocate the new parm recd
  _cmPoOpt_t* r = cmLhAllocZ( p->lH, _cmPoOpt_t, 1 );


  // if this is an enum type.
  if( cmIsFlag(sflags,kEnumPoFl) )
  {
    sflags = cmClrFlag(sflags,kTypeMaskPoFl) | kEnumPoFl;

    // set the master recd for this enum ptr
    _cmPoOpt_t* erp;
    if((erp = _cmPgmOptNumIdToOptRecd(p,numId)) != NULL )
    {
      r->enumPtr = erp->enumPtr==NULL ? erp : erp->enumPtr;  

      // if this child enum has it's required flags set 
      if( cmIsFlag(cflags,kReqPoFl) )
      {
        // then set the required flag in the parent and clear it in the child
        // (this way both the parent and child will not be required (which would be impossible for an enum))
        r->enumPtr->cflags = cmSetFlag(r->enumPtr->cflags,kReqPoFl);
        cflags = cmClrFlag(cflags,kReqPoFl);
      }
    }
  }

  r->cflags     = cflags; 
  r->sflags     = sflags;
  r->numId      = numId;
  r->charId     = charId;
  r->wordId     = cmLhAllocStr( p->lH, wordId );
  r->enumId     = enumId;
  r->maxInstCnt = cnt;
  r->helpStr    = helpStr==NULL ? NULL : cmLhAllocStr( p->lH, helpStr );


  _cmPoOpt_t* rp = p->list;
  while( rp != NULL && rp->link != NULL )
    rp = rp->link;

  if( rp == NULL )
    p->list = r;
  else
    rp->link = r;

  //r->link       = p->list;
  //p->list       = r;

  *rpp = r;

  return kOkPoRC;  
}

cmPoRC_t cmPgmOptInstallChar(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* wordId, unsigned flags, cmChar_t dfltVal, cmChar_t* retValPtr, unsigned cnt, const cmChar_t* helpStr )
{
  cmPoRC_t    rc;
  _cmPoOpt_t* r     = NULL;
  _cmPo_t*    p     = _cmPoHandleToPtr(h);

  if((rc= _cmPgmOptInstall(p, numId, charId, wordId, flags, kCharPoFl, 0, cnt, helpStr, &r )) != kOkPoRC )
    return rc;

  r->dfltVal.c = dfltVal;
  if( retValPtr != NULL )
  {
    r->retVal.c  = retValPtr;
    *r->retVal.c = dfltVal;
  }
  return rc;
}

cmPoRC_t cmPgmOptInstallBool(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* wordId, unsigned flags, bool dfltVal, bool* retValPtr, unsigned cnt, const cmChar_t* helpStr )
{
  cmPoRC_t    rc;
  _cmPoOpt_t* r     = NULL;
  _cmPo_t*    p     = _cmPoHandleToPtr(h);

  if((rc= _cmPgmOptInstall(p, numId, charId, wordId, flags, kBoolPoFl, 0, cnt, helpStr, &r )) != kOkPoRC )
    return rc;

  r->dfltVal.b = dfltVal;
  if( retValPtr != NULL )
  {
    r->retVal.b  = retValPtr;
    *r->retVal.b = dfltVal;
  }
  return rc;
}

cmPoRC_t cmPgmOptInstallInt( cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* wordId, unsigned flags, int  dfltVal, int* retValPtr, unsigned cnt, const cmChar_t* helpStr )
{
  cmPoRC_t    rc;
  _cmPoOpt_t* r     = NULL;
  _cmPo_t*    p     = _cmPoHandleToPtr(h);

  if((rc= _cmPgmOptInstall(p, numId, charId, wordId, flags, kIntPoFl, 0, cnt, helpStr, &r )) != kOkPoRC )
    return rc;

  r->dfltVal.i = dfltVal;
  if( retValPtr != NULL )
  {
    r->retVal.i  = retValPtr;
    *r->retVal.i = dfltVal;
  }
  return rc;
}

cmPoRC_t cmPgmOptInstallUInt(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* wordId, unsigned flags, unsigned dfltVal, unsigned* retValPtr, unsigned cnt, const cmChar_t* helpStr )
{
  cmPoRC_t    rc;
  _cmPoOpt_t* r     = NULL;
  _cmPo_t*    p     = _cmPoHandleToPtr(h);

  if((rc= _cmPgmOptInstall(p, numId, charId, wordId, flags, kUIntPoFl, 0, cnt, helpStr, &r )) != kOkPoRC )
    return rc;

  r->dfltVal.u = dfltVal;
  if( retValPtr != NULL )
  {
    r->retVal.u = retValPtr;
    *r->retVal.u = dfltVal;
  }
  return rc;
}

cmPoRC_t cmPgmOptInstallDbl( cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* wordId, unsigned flags, double dfltVal, double* retValPtr, unsigned cnt, const cmChar_t* helpStr )
{
  cmPoRC_t    rc;
  _cmPoOpt_t* r     = NULL;
  _cmPo_t*    p     = _cmPoHandleToPtr(h);

  if((rc= _cmPgmOptInstall(p, numId, charId, wordId, flags, kDblPoFl, 0, cnt, helpStr, &r )) != kOkPoRC )
    return rc;

  r->dfltVal.d = dfltVal;
  if( retValPtr != NULL )
  {
    r->retVal.d  = retValPtr;
    *r->retVal.d = dfltVal;
  }
  return rc;
}

cmPoRC_t cmPgmOptInstallStr( cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* wordId, unsigned flags, const cmChar_t* dfltVal, const cmChar_t** retValPtr, unsigned cnt, const cmChar_t* helpStr )
{
  cmPoRC_t    rc;
  _cmPoOpt_t* r     = NULL;
  _cmPo_t*    p     = _cmPoHandleToPtr(h);


  if((rc= _cmPgmOptInstall(p, numId, charId, wordId, flags, kStrPoFl, 0, cnt, helpStr, &r )) != kOkPoRC )
    return rc;

  r->dfltVal.s = dfltVal;
  if( retValPtr != NULL )
  {
    r->retVal.s  = retValPtr;
    *r->retVal.s = dfltVal;
  }
  return rc;
}

cmPoRC_t cmPgmOptInstallEnum(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* wordId, unsigned flags, unsigned enumId, unsigned dfltVal, unsigned* retValPtr, unsigned cnt, const cmChar_t* helpStr, const cmChar_t* mstrHelpStr  )
{
  cmPoRC_t    rc;
  _cmPoOpt_t* r     = NULL;
  _cmPo_t*    p     = _cmPoHandleToPtr(h);

  if((rc= _cmPgmOptInstall(p, numId, charId, wordId, flags, kEnumPoFl, enumId, cnt, helpStr, &r )) != kOkPoRC )
    return rc;

  r->dfltVal.u = dfltVal;
  if( retValPtr != NULL )
  {
    r->retVal.u  = retValPtr;
    *r->retVal.u = dfltVal;
  }

  if( mstrHelpStr != NULL )
  {
    _cmPoOpt_t* rp = r->enumPtr == NULL ? r : r->enumPtr;
    rp->mstrStr = cmLhAllocStr(p->lH,mstrHelpStr);
  }
  return rc;
}

cmPoRC_t cmPgmOptInstallFlag(cmPgmOptH_t h, unsigned numId, cmChar_t charId, const cmChar_t* wordId, unsigned flags, unsigned         dfltVal, unsigned*       retValPtr, unsigned cnt, const cmChar_t* helpStr )
{
  cmPoRC_t    rc;
  _cmPoOpt_t* r     = NULL;
  _cmPo_t*    p     = _cmPoHandleToPtr(h);

  if((rc= _cmPgmOptInstall(p, numId, charId, wordId, flags, kFlagPoFl, 0, cnt, helpStr, &r )) != kOkPoRC )
    return rc;

  r->dfltVal.u = dfltVal;
  if( retValPtr != NULL )
  {
    r->retVal.u  = retValPtr;
    // Unlike other parameters we don't set the initial value of flag parameter 
    // because we don't want to destroy any default flag that may be set there
  }

  return rc;
}


_cmPoArg_t* _cmPgmOptInsertArg( _cmPo_t* p,  _cmPoOpt_t* r )
{
  _cmPoArg_t* a = cmLhAllocZ(p->lH, _cmPoArg_t, 1);

  // if this is an enumerated type then switch to the master enum recd
  unsigned enumId = r->enumId;
  _cmPoOpt_t* e   = r;
  if( r->enumPtr != NULL )
    r = r->enumPtr;

  a->opt        = r;
  a->valStr     = NULL;

  // link onto the end of the  master arg list
  _cmPoArg_t* ap = p->args;
  while( ap!=NULL && ap->link != NULL )
    ap=ap->link;
  if( ap == NULL )
    p->args = a;
  else
    ap->link = a;
  
   // link onto the end of the opt recd list
  ap = r->inst;
  while( ap!=NULL && ap->inst!=NULL)
    ap=ap->inst;
  if( ap==NULL)
    r->inst = a;
  else
    ap->inst = a;

  // if no parm. type flag was given then the arg is implicitely a bool and the value is true.
  //if( (r->flags & kTypeMaskPoFl) == 0 )
  //  a->u.b = true;

  // if this is an enumerated type
  if( cmIsFlag(r->sflags,kEnumPoFl) )
  {
    a->u.u = enumId;

    if( r->retVal.u != NULL )
      *r->retVal.u = enumId;

    if( e->retVal.u != NULL )
      *e->retVal.u = enumId;
  }

  // if this is a flag type
  if( cmIsFlag(r->sflags,kFlagPoFl) )
  {
    a->u.u = r->dfltVal.u;
    if( r->retVal.u != NULL )
      *r->retVal.u |= a->u.u;
  }
  return a;
}

cmPoRC_t _cmPgmOptParseBool(_cmPo_t* p, const cmChar_t* valStr, bool* retValPtr )
{
  cmPoRC_t rc = kOkPoRC;
  unsigned n = strlen("false");
  char val[n+1];
  int i;
  for(i=0; i<n && valStr[i]; ++i)
    val[i] = tolower(valStr[i]);
  val[i] = 0;

  if( !strcmp(val,"0") || !strcmp(val,"false") || !strcmp(val,"f") )
    *retValPtr = false;
  else
    if( !strcmp(val,"1") || !strcmp(val,"true") || !strcmp(val,"t") )
      *retValPtr = true;
    else
      rc = cmErrMsg(&p->err,kParseFailPoRC,"The argument '%s' could not be parsed into a bool. Legal bool values are 0,F,FALSE,1,T,TRUE (case insensitive).",valStr); 

  return rc;
}

cmPoRC_t _cmPgmOptParseValue( _cmPo_t* p, _cmPoOpt_t* r, _cmPoArg_t* a, const cmChar_t* valStr )
{
  cmPoRC_t rc = kOkPoRC;

  assert(a != NULL && valStr != NULL);
  a->valStr = valStr;

  errno = 0;
  switch( r->sflags & kTypeMaskPoFl )
  {
    // case kFlagPoFl: 
    // enum values get set in _cmPgmOptInsertArg()


    case kBoolPoFl: 
      //rc = _cmPgmOptParseBool(p,valStr,&a->u.b); 
      a->u.b = true;
      if( r->retVal.b != NULL )
        *r->retVal.b = a->u.b;
      break;

    case kCharPoFl: 
      a->u.c = valStr[0];              
      if( r->retVal.c != NULL )
        *r->retVal.c = a->u.c;
      break;

    case kIntPoFl:  
      a->u.i = strtol(valStr,NULL, cmIsFlag(r->cflags,kHexPoFl) ? 16 : 10); 
      if( r->retVal.i != NULL )
        *r->retVal.i = a->u.i;
      break;

    case kUIntPoFl: 
      a->u.u = strtol(valStr,NULL, cmIsFlag(r->cflags,kHexPoFl) ? 16 : 10); 
      if( r->retVal.u != NULL )
        *r->retVal.u = a->u.u;
      break;
      
      // case kEnumPoFl: 
      // enum values get set in _cmPgmOptInsertArg()

    case kDblPoFl:  
      a->u.d = strtod(valStr,NULL);    
      if( r->retVal.d != NULL )
        *r->retVal.d = a->u.d;
      break;

    case kStrPoFl:  
      a->u.s = valStr;                 
      if( r->retVal.s != NULL )
        *r->retVal.s = a->u.s;
      break;

    default:
      { assert(0); }
  }


  if( errno != 0 )
    rc =  cmErrMsg(&p->err,kParseFailPoRC,"The value '%s' for option '%s' could not be parsed.",valStr,r->wordId);

  return rc;
}

unsigned _cmPgmOptInstCount( const _cmPoOpt_t* r )
{
  const _cmPoArg_t* a = r->inst;
  unsigned          n = 0;

  while( a != NULL )
  {
    ++n;
    a = a->inst;
  }

  return n;
}


// check if any non-built-in options were provided 
// If only built in options were selected then no execution is assumed
// and required arg's are not checked.
bool _cmPgmOptCheckNoExec( _cmPo_t* p )
{
  _cmPoArg_t* a = p->args;
  while( a != NULL )
  {
    if( a->opt->numId >= kBasePoId )
      return true;

    a = a->link;
  }
  return false;
}

cmPoRC_t  _cmPgmOptMissingEnumError( _cmPo_t* p, _cmPoOpt_t* r )
{
  _cmPoOpt_t* mstr = r->enumPtr==NULL ? r : r->enumPtr;
  _cmPoOpt_t* rp  = p->list;
  cmChar_t*   msg = cmTsPrintfP(NULL,"No argument was supplied for the required enumerated parameter containing the following set of possible flags: { ");
  
  for(; rp!=NULL; rp=rp->link)
    if( cmIsFlag(rp->sflags,kEnumPoFl) && (rp==mstr || rp->enumPtr==mstr) )
    {
      msg = cmTextAppendSS(msg,rp->wordId);
      msg = cmTextAppendSS(msg," ");
    }

  msg = cmTextAppendSS(msg,"}");
  
  cmPoRC_t rc = cmErrMsg(&p->err,kNoReqArgPoRC,msg);

  cmMemFree(msg);
  
  return rc;
}

// check that all required arg.s were actually given and that the actual
// number of instances does not exceed the defined limit
cmPoRC_t _cmPgmOptCheckReqArgs( _cmPo_t* p )
{
  cmPoRC_t rc = kOkPoRC;

  _cmPoOpt_t* r = p->list;
  while( r != NULL )
  {
    if( cmIsFlag(r->cflags, kReqPoFl ) )
    {
      _cmPoArg_t* a = p->args;
      while( a != NULL )
      {
        if( a->opt == r )
          break;
        a = a->link;
      }
    
      if( a == NULL )
      {
        if( cmIsFlag(r->sflags, kEnumPoFl ) )
          rc =  _cmPgmOptMissingEnumError(p,r);
        else
          rc = cmErrMsg(&p->err,kNoReqArgPoRC,"No argument was supplied for the required parameter '%s'.",r->wordId);
      }

    }

    unsigned cnt;
    if( r->maxInstCnt > 0 && r->maxInstCnt < (cnt=_cmPgmOptInstCount(r)) )
      rc = cmErrMsg(&p->err,kArgCntErrPoRC,"The parameter '%s' has %i instances which exceeds the defined limit of %i instances.",r->wordId,cnt,r->maxInstCnt);

    r = r->link;
  }
  return rc;
}



cmPoRC_t cmPgmOptParse( cmPgmOptH_t h, unsigned argCnt,  char* argArray[] )
{
  enum { kDash, kCharId, kWordId, kArgVal };
  cmPoRC_t    rc    = kOkPoRC;
  _cmPo_t*    p     = _cmPoHandleToPtr(h);
  unsigned    state = kDash;
  _cmPoOpt_t* r     = NULL;
  _cmPoArg_t* a     = NULL;
  int         i     = 1; // arg index
  int         j     = 0; // arg label character index


  if( _cmPgmOptNumIdToOptRecd(p,kPrintHelpPoId) == NULL )
    cmPgmOptInstallBool(h, kPrintHelpPoId, 'h', "help",    0, false, NULL,0,"Print this usage information." );

  if( _cmPgmOptNumIdToOptRecd(p,kVersionPoId) == NULL )
    cmPgmOptInstallBool(h, kVersionPoId,   'v', "version", 0, false, NULL,0,"Print version information." );

  if( _cmPgmOptNumIdToOptRecd(p,kPrintParmsPoId) == NULL )
    cmPgmOptInstallBool(h, kPrintParmsPoId,'p', "parms",   0, false, NULL,0,"Print the arguments."); 

  
  while(i<argCnt)
  {
    const cmChar_t* valStr = NULL;

    switch(state)
    {

      case kDash:
        // this token must begin with a '-' character
        if( argArray[i][0] != '-')
          return cmErrMsg(&p->err,kSyntaxErrPoRC,"Syntax error. Expecting a '-'.");

        // if the second char. is also a '-' then this is a wordId
        if( argArray[i][1] == '-' )
        {
          state = kWordId;
          j     = 2;      // word id's always begin on the third token char
        }
        else // otherwise it is a charId
        {
          state = kCharId;
          j     = 1;     // char id's always begin on the second token char
        }

        r = NULL;
        break;

      case kCharId:
        // we complete our parsing of charId's only when we encounter a '\0'
        if( argArray[i][j] == '\0' )
        {
          ++i;
          state = kDash;
          r     = NULL;
          a     = NULL;
        }
        else
        {
          // get the opt recd assoc'd with the jth character in this charId
          if((r = _cmPgmOptCharIdToOptRecd(p,argArray[i][j])) == NULL )
            return cmErrMsg(&p->err,kSyntaxErrPoRC,"The program option selector char '%c' is not valid.",argArray[i][j]);

          // if this charId is not a flag,bool or enum then it must be followed by a value.
          if( _cmPoOptUsesValue(r) )
            ++i;
          else // otherwise process the next char id in this charId token            
            ++j;
        }
        break;

      case kWordId:
        // get the wordId 
        if((r = _cmPgmOptWordIdToOptRecd(p,argArray[i]+j)) == NULL )
          return cmErrMsg(&p->err,kSyntaxErrPoRC,"The program option selector word '%s' is not valid.",cmStringNullGuard(argArray[i]+j));        
        ++i; // advance to the next token
        break;

      case kArgVal:
        assert(r != NULL );
        valStr = argArray[i];        
        ++i;
        break;
    }

    // if a valid opt recd has been selected
    if( r != NULL )
    {
      // if the cur token reprsents a value ...
      if( state == kArgVal )
      {
        // ... then parse the value string
        if((rc = _cmPgmOptParseValue(p,r,a,valStr)) != kOkPoRC )
          goto errLabel;

        r      = NULL;
        a      = NULL;
        state  = kDash; // value strings are always followed by char or word id's
      }
      else
      {
        // create an arg record for the cur char or word id.
        a = _cmPgmOptInsertArg(p, r );

        // and the value type is not 'flag','bool' or 'enum'
        if( _cmPoOptUsesValue(r) )
          state = kArgVal;

        switch(state)
        {
          case kDash:   assert(i==argCnt);  break;
          case kCharId:                 break;
          case kWordId: state = kDash;  break;
          case kArgVal:                 break;
          default:      assert(0);      break;
        }

        
      }
    }
  }

 errLabel:

  // if no errors have occurred then check for missing parameters.
  if( rc == kOkPoRC )
  {
    if((p->execFl =  _cmPgmOptCheckNoExec(p)) == true )
      rc = _cmPgmOptCheckReqArgs(p);
  }
  return rc;
}
  
unsigned cmPgmOptArgCount( cmPgmOptH_t h)
{
  _cmPo_t*          p = _cmPoHandleToPtr(h);
  const _cmPoArg_t* a = p->args;
  unsigned          n = 0;

  while( a != NULL )
  {
    ++n;
    a = a->link;
  }

  return n;
}

cmPoRC_t  _cmPgmOptIndexToPtr( _cmPo_t* p, unsigned idx, const _cmPoArg_t** app )
{
  unsigned          n = 0;
  const _cmPoArg_t* a = p->args;

  assert(app != NULL );
  *app = NULL;

  while( a != NULL && n < idx )
  {
    ++n;
    a = a->link;
  }

  if( n!=idx || a==NULL)
    return cmErrMsg(&p->err,kInvalidIdxPoRC,"The option index '%i' is not valid.",idx);

  *app = a;

  return kOkPoRC;
}
  
unsigned cmPgmOptNumId( cmPgmOptH_t h, unsigned argIdx )
{
  const _cmPoArg_t* a = NULL;
  cmPoRC_t          rc;
  if((rc = _cmPgmOptIndexToPtr(_cmPoHandleToPtr(h),argIdx,&a)) != kOkPoRC )
    return cmInvalidId;
  assert( a != NULL );
  return a->opt->numId;
}

unsigned cmPgmOptCharId( cmPgmOptH_t h, unsigned argIdx )
{
  const _cmPoArg_t* a = NULL;
  cmPoRC_t          rc;
  if((rc = _cmPgmOptIndexToPtr(_cmPoHandleToPtr(h),argIdx,&a)) != kOkPoRC )
    return cmInvalidId;
  assert(a != NULL );
  return a->opt->charId;
}

const cmChar_t* cmPgmOptWordId( cmPgmOptH_t h, unsigned argIdx )
{
  const _cmPoArg_t* a = NULL;
  cmPoRC_t          rc;
  if((rc = _cmPgmOptIndexToPtr(_cmPoHandleToPtr(h),argIdx,&a)) != kOkPoRC )
    return NULL;
  assert(a!=NULL);
  return a->opt->wordId;
}

cmPoRC_t  _cmPgmOptArgPtr( _cmPo_t* p, unsigned argIdx, const _cmPoArg_t** app )
{
  cmPoRC_t rc;

  if((rc = _cmPgmOptIndexToPtr(p,argIdx,app)) != kOkPoRC )
  {
    if( (*app)->valStr == NULL )
      return cmErrMsg(&p->err,kParseFailPoRC,"The parameter at index '%i' is not a value.",argIdx);
  }

  return rc;
}

char cmPgmOptParseArgChar( cmPgmOptH_t h, unsigned argIdx )
{
  cmPoRC_t          rc;
  const _cmPoArg_t* a;
  _cmPo_t*          p = _cmPoHandleToPtr(h);

  if((rc = _cmPgmOptArgPtr(p,argIdx,&a)) != kOkPoRC )
    return 0;

  return a->valStr==NULL ? 0 : a->valStr[0];
}

bool cmPgmOptParseArgBool( cmPgmOptH_t h, unsigned argIdx )
{
  cmPoRC_t          rc;
  const _cmPoArg_t* a;
  _cmPo_t*          p = _cmPoHandleToPtr(h);
  bool              val = false;

  if((rc = _cmPgmOptArgPtr(p,argIdx,&a)) != kOkPoRC )
    return false;

  if( a->valStr != NULL )
    _cmPgmOptParseBool(p,a->valStr,&val);

  return val;
}

int         cmPgmOptParseArgInt(    cmPgmOptH_t h, unsigned argIdx )
{
  cmPoRC_t          rc;
  const _cmPoArg_t* a;
  _cmPo_t*          p = _cmPoHandleToPtr(h);

  if((rc = _cmPgmOptArgPtr(p,argIdx,&a)) != kOkPoRC )
    return rc;

  errno = 0;
  int v = strtol(a->valStr,NULL,10);
  if( errno != 0 )
     cmErrMsg(&p->err,kParseFailPoRC,"The parameter '%s' could not be converted to an 'int'.",a->valStr);

  return v;
}

unsigned    cmPgmOptParseArgUInt(   cmPgmOptH_t h, unsigned argIdx )
{
  cmPoRC_t          rc;
  const _cmPoArg_t* a;
  _cmPo_t*          p = _cmPoHandleToPtr(h);

  if((rc = _cmPgmOptArgPtr(p,argIdx,&a)) != kOkPoRC )
    return rc;

  errno = 0;
  unsigned v = strtol(a->valStr,NULL,10);
  if( errno != 0 )
     cmErrMsg(&p->err,kParseFailPoRC,"The parameter '%s' could not be converted to an 'unsigned int'.",a->valStr);

  return v;
}

double      cmPgmOptParseArgDbl( cmPgmOptH_t h, unsigned argIdx )
{
  cmPoRC_t          rc;
  const _cmPoArg_t* a;
  _cmPo_t*          p = _cmPoHandleToPtr(h);

  if((rc = _cmPgmOptArgPtr(p,argIdx,&a)) != kOkPoRC )
    return rc;

  errno = 0;
  double v = strtod(a->valStr,NULL);
  if( errno != 0 )
     cmErrMsg(&p->err,kParseFailPoRC,"The parameter '%s' could not be converted to a 'double'.",a->valStr);

  return v;
}

const char* cmPgmOptParseArgStr( cmPgmOptH_t h, unsigned argIdx )
{
  cmPoRC_t          rc;
  const _cmPoArg_t* a;
  _cmPo_t*          p = _cmPoHandleToPtr(h);

  if((rc = _cmPgmOptArgPtr(p,argIdx,&a)) != kOkPoRC )
    return NULL;
  return a->valStr;
}

unsigned    cmPgmOptParmArgCount( cmPgmOptH_t h, unsigned numId )
{
  const _cmPoOpt_t* r;
  _cmPo_t*          p = _cmPoHandleToPtr(h);
  unsigned          n = 0;

  if((r = _cmPgmOptNumIdToOptRecd(p,numId)) == NULL )
    cmErrMsg(&p->err,kParmNotFoundPoRC,"No parameter definition was found for numeric id %i.",numId);
  else
    n = _cmPgmOptInstCount(r);

  return n;      
}

cmPoRC_t  _cmPgmOptInstPtr( _cmPo_t* p, unsigned numId, unsigned instIdx, const _cmPoOpt_t** rpp, const _cmPoValue_t** vpp )
{
  *rpp = NULL;
  *vpp = NULL;

  // locate the opt recd
  if((*rpp = _cmPgmOptNumIdToOptRecd(p,numId)) == NULL )
    return cmErrMsg(&p->err,kParmNotFoundPoRC,"No parameter definition was found for the numeric id %i.",numId);
  
  const _cmPoArg_t* a = (*rpp)->inst;
  unsigned          i;

  // locate the instance recd
  for(i=0; i<instIdx && a != NULL; ++i)
    a = a->inst;

  // if the instance recd was not found
  if( i != instIdx || a == NULL )
  {
    // if the instance index is 0 then and no instance recd exists then
    // we will return the default value.
    if( instIdx == 0 )
    {
      *vpp = &((*rpp)->dfltVal);
      return kOkPoRC;
    }

    // otherwise signal an error
    return cmErrMsg(&p->err,kInstNotFoundPoRC,"The instance index %i is not valid for parameter %s instance count %i.",instIdx,(*rpp)->wordId,i);
  }

  *vpp = &a->u;
  
  return kOkPoRC;
}

char        cmPgmOptArgChar( cmPgmOptH_t h, unsigned numId, unsigned instIdx )
{
  const _cmPoOpt_t*   rp = NULL;
  const _cmPoValue_t* vp = NULL;
  _cmPo_t*            p  = _cmPoHandleToPtr(h);

  if(_cmPgmOptInstPtr(p,numId,instIdx,&rp,&vp) != kOkPoRC )
    return 0;

  if( cmIsFlag(rp->sflags,kCharPoFl) )
  {
    cmErrMsg(&p->err,kTypeErrPoRC,"The parameter '%s' is not a 'char'.",rp->wordId);
    return 0;
  }

  return vp->c;
}


bool        cmPgmOptArgBool(   cmPgmOptH_t h, unsigned numId, unsigned instIdx )
{
  const _cmPoOpt_t*   rp = NULL;
  const _cmPoValue_t* vp = NULL;
  _cmPo_t*            p  = _cmPoHandleToPtr(h);


  if(_cmPgmOptInstPtr(p,numId,instIdx,&rp,&vp) != kOkPoRC )
    return 0;
  
  // all types have a meaning in terms of bool
  return vp->b;
}

int         cmPgmOptArgInt(    cmPgmOptH_t h, unsigned numId, unsigned instIdx )
{
  cmPoRC_t            rc;
  const _cmPoOpt_t*   rp = NULL;
  const _cmPoValue_t* vp = NULL;
  _cmPo_t*            p  = _cmPoHandleToPtr(h);
  int                 v  = 0;

  if((rc = _cmPgmOptInstPtr(p,numId,instIdx,&rp,&vp)) != kOkPoRC )
    return rc;
 
  switch( rp->sflags & kTypeMaskPoFl )
  {
    case kFlagPoFl: v = vp->u; break;
    case kBoolPoFl: v = vp->b; break;
    case kCharPoFl: v = vp->c; break;
    case kIntPoFl:
    case kEnumPoFl: v = vp->i; break;
    case kUIntPoFl:
    case kDblPoFl:  v = roundl(vp->d); break;
    case kStrPoFl:  
      cmErrMsg(&p->err,kTypeErrPoRC,"The string parameter '%s' cannot be converted to an integer.",rp->wordId);
      break;
    default:
      assert(0); 
      break;
  }

  return v;
}

unsigned    cmPgmOptArgUInt(   cmPgmOptH_t h, unsigned numId, unsigned instIdx )
{ return cmPgmOptArgInt(h,numId,instIdx);   }

double      cmPgmOptArgDbl( cmPgmOptH_t h, unsigned numId, unsigned instIdx )
{
  cmPoRC_t            rc;
  const _cmPoOpt_t*   rp = NULL;
  const _cmPoValue_t* vp = NULL;
  _cmPo_t*            p  = _cmPoHandleToPtr(h);
  double              v  = 0;

  if((rc = _cmPgmOptInstPtr(p,numId,instIdx,&rp,&vp)) != kOkPoRC )
    return rc;
 
  switch( rp->sflags & kTypeMaskPoFl )
  {
    case kFlagPoFl: v = vp->u; break;
    case kBoolPoFl: v = vp->b; break;
    case kCharPoFl: v = vp->c; break;
    case kEnumPoFl:
    case kIntPoFl:  v = vp->i; break;
    case kUIntPoFl: v = vp->u; break;
    case kDblPoFl:  v = vp->d; break;
    case kStrPoFl:  
      cmErrMsg(&p->err,kTypeErrPoRC,"The string parameter '%s' cannot be converted to a double.",rp->wordId);
      break;
    default:
      assert(0); 
      break;
  }

  return v;
}

const char* cmPgmOptArgStr(    cmPgmOptH_t h, unsigned numId, unsigned instIdx )
{
  cmPoRC_t            rc;
  const _cmPoOpt_t*   rp = NULL;
  const _cmPoValue_t* vp = NULL;
  _cmPo_t*            p  = _cmPoHandleToPtr(h);

  if((rc = _cmPgmOptInstPtr(p,numId,instIdx,&rp,&vp)) != kOkPoRC )
    return NULL;

  // if the requested param is a defined as a string
  if( cmIsFlag(rp->sflags,kStrPoFl) )
    return vp->s;

  // otherwise the requested param is not defined as a string - so try to return the instance string value
  const _cmPoArg_t* a = rp->inst;
  unsigned          i;
  for(i=0; i<instIdx && a != NULL; ++i)
    a = a->inst;

  // if the index is not valid
  if( i != instIdx )
  {
    cmErrMsg(&p->err,kInstNotFoundPoRC,"The instance index %i is not valid for parameter %s instance count %i.",instIdx,rp->wordId,i);
    return NULL;
  }

  // otherwise return the instance string
  return a->valStr;
}

cmPoRC_t    cmPgmOptRC( cmPgmOptH_t h, cmPoRC_t rc )
{ 
  _cmPo_t* p = _cmPoHandleToPtr(h);
  return cmErrSetRC(&p->err,rc);
}

bool cmPgmOptHandleBuiltInActions( cmPgmOptH_t h, cmRpt_t* rpt )
{
  _cmPo_t* p = _cmPoHandleToPtr(h);
  _cmPoArg_t* a = p->args;
  while( a != NULL )
  {

    switch( a->opt->numId )
    {
      case kPrintHelpPoId:
        cmPgmOptPrintHelp(h,rpt);
        break;

      case kVersionPoId:
        cmPgmOptPrintVersion(h,rpt);
        break;

      case kPrintParmsPoId:
        cmPgmOptPrintParms(h,rpt);
        break;
    }

    a = a->link;
  }

  return p->execFl;
}

void cmPgmOptPrintHelp( cmPgmOptH_t h, cmRpt_t* rpt )
{
  _cmPo_t*          p = _cmPoHandleToPtr(h);
  const _cmPoOpt_t* r = p->list;

  if( p->helpBegStr != NULL )
    cmRptPrintf(rpt,"%s\n",p->helpBegStr);

  for(; r != NULL; r=r->link )
  {
    const cmChar_t* reqStr = "(required)";
    const cmChar_t* valueTypeLabel = "";
    const cmChar_t* reqLabel = "";
    const cmChar_t* indentStr = "";

    const _cmPoOpt_t* mstr = NULL;

    if( cmIsFlag(r->sflags,kEnumPoFl) )
    {
      if( r->enumPtr == NULL )
        mstr     = r;
      indentStr = "    ";
    }

    if( _cmPoOptUsesValue(r) ) 
      valueTypeLabel = _cmPoTypeFlagToLabel(r->sflags & kTypeMaskPoFl,r->cflags);

    if( cmIsNotFlag(r->sflags,kEnumPoFl)  && cmIsFlag(r->sflags,kReqPoFl) )
      reqLabel = reqStr;

    if( mstr != NULL )
      cmRptPrintf(rpt,"Enumerated group: %s %s\n",mstr->mstrStr==NULL ? "" : mstr->mstrStr, cmIsFlag(mstr->cflags,kReqPoFl) ? reqStr : "" );
   
    cmRptPrintf(rpt,"%s-%c --%s %s %s",indentStr,r->charId,r->wordId,valueTypeLabel,reqLabel);

    if( r->helpStr != NULL )
      cmRptPrintf(rpt,"    %s\n",r->helpStr);

  }
  

  if( p->helpEndStr != NULL )
    cmRptPrintf(rpt,"\n%s\n",p->helpEndStr);
  
}

void cmPgmOptPrintVersion( cmPgmOptH_t h, cmRpt_t* rpt )
{
  cmRptPrintf(rpt,"%s\n",PACKAGE_STRING); // PACKAGE_STRING is defined in config.h
}


bool _cmPgmOptPrint( _cmPo_t* p, cmRpt_t* rpt, const _cmPoOpt_t* r, const _cmPoValue_t* v, const cmChar_t* valStr )
{
  const _cmPoOpt_t* e = r;
  if( cmIsFlag(r->sflags,kEnumPoFl) )
  {
    if( r->enumPtr != NULL )
      return false;

    if((e = _cmPgmOptEnumIdToOptRecd(p,r->numId,v->u)) == NULL )
    {
      cmErrMsg(&p->err,kParmNotFoundPoRC,"The parm. defn. could not be found for numId=%i enumId=%i.",r->numId,v->u);
      return false;
    }
  }

  cmRptPrintf(rpt,"-%c --%-20s %i ",e->charId,e->wordId, _cmPgmOptInstCount(r));
  switch(r->sflags & kTypeMaskPoFl)
  {
    case kFlagPoFl: cmRptPrintf(rpt,r->cflags & kHexPoFl ? "0x%x ": "%u ",v->u ); break;
    case kBoolPoFl: cmRptPrintf(rpt,"%c ",  v->b ? 'T' : 'F'); break;
    case kCharPoFl: cmRptPrintf(rpt,"%c ",  v->c); break;
    case kIntPoFl:  cmRptPrintf(rpt,r->cflags & kHexPoFl ? "0x%x ": "%i ",  v->i); break;
    case kUIntPoFl: cmRptPrintf(rpt,r->cflags & kHexPoFl ? "0x%x ": "%u ",  v->u); break;
    case kDblPoFl:  cmRptPrintf(rpt,"%f ",  v->d); break;
    case kStrPoFl:  cmRptPrintf(rpt,"%s ",  v->s); break;
    case kEnumPoFl: cmRptPrintf(rpt,"%i ",  v->u); break;
    default:
      cmRptPrintf(rpt,"%s ",cmStringNullGuard(valStr)); break;
  }
  
  return true;
}

void cmPgmOptPrintParms( cmPgmOptH_t h, cmRpt_t* rpt )
{
  _cmPo_t*    p = _cmPoHandleToPtr(h);
  _cmPoArg_t* a = p->args;

  // print the given arguments
  while( a != NULL )
  {
    if(_cmPgmOptPrint(p, rpt, a->opt,&a->u,a->valStr))
      cmRptPrintf(rpt,"\n");

    a = a->link;
  }

  // print the default values
  _cmPoOpt_t* r = p->list;
  while( r != NULL )
  {
    if( r->inst == NULL )
    {
      if(_cmPgmOptPrint(p,rpt,r,&r->dfltVal,NULL))
        cmRptPrintf(rpt,"\n");
    }  
    r = r->link;
  }

}
