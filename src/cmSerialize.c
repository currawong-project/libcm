//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSerialize.h"


#define cmSrVersion (0)

#ifdef cmBIG_ENDIAN
#define _cmSrSwap16(v)  (v)
#define _cmSrSwap32(v)  (v)
#define _cmSrSwapFl (0)
#else
#define _cmSrSwap16(v)  (v)
#define _cmSrSwap32(v)  (v)
#define _cmSrSwapFl (1)
#endif


typedef struct
{
  const char* label;
  unsigned    typeId;
  unsigned    byteCnt;  
  bool        swapFl;
} _cmSrPrim_t;


struct _cmSrStruct_str;

// structure field desc record
typedef struct _cmSrField_str
{
  const _cmSrPrim_t*      primPtr;
  struct _cmSrStruct_str* structPtr;
  bool                    arrayFl;
  struct _cmSrField_str*  linkPtr;
} _cmSrField_t;

// structure description record
typedef struct _cmSrStruct_str
{
  unsigned                typeId;
  _cmSrField_t*           fieldList;
  struct _cmSrStruct_str* linkPtr;
} _cmSrStruct_t;

// state records track the format for read/write operations
typedef struct _cmSrState_str
{
  _cmSrStruct_t*           structPtr; // this states current structure
  _cmSrField_t*            fieldPtr;  // this states current field
  struct _cmSrState_str*   linkPtr;   // prev state on stack
  unsigned                 arrayCnt;  // array count for this struct array
  unsigned                 arrayIdx;  // current array index for this struct array
} _cmSrState_t;

// data records are formed by each write operation
typedef struct _cmSrData_str
{
  unsigned              typeId;
  unsigned              eleCnt;
  unsigned              byteCnt;
  void*                 dataPtr; 
  bool                  arrayFl;
  bool                  swapFl;
  struct _cmSrData_str* linkPtr;
} _cmSrData_t;


typedef struct
{
  cmErr_t            err;

  cmLHeapH_t         lhH;

  _cmSrStruct_t*     structList;

  _cmSrState_t*      wrStackPtr;
  _cmSrData_t*       wrDataList;
  char*              wrDataBufPtr;
  unsigned           wrDataBufByteCnt;

  _cmSrState_t*      rdStackPtr;
  const void*        rdDataBufPtr;
  unsigned           rdDataBufByteCnt;
  const char*        rdCursorPtr;
  unsigned           rdVersion;
  unsigned           rdFlags;

  cmSrRC_t           lastRC;

} _cmSr_t;

_cmSrPrim_t _cmSrPrimArray[] = 
{
  {"char",      kCharSrId,    sizeof(char),           false },
  {"uchar",     kUCharSrId,   sizeof(char),           false },
  {"short",     kShortSrId,   sizeof(short),          true  },
  {"ushort",    kUShortSrId,  sizeof(unsigned short), true  },
  {"int",       kIntSrId,     sizeof(int),            true  },
  {"uint",      kUIntSrId,    sizeof(unsigned),       true  },
  {"long",      kLongSrId,    sizeof(long),           true  },
  {"ulong",     kLongSrId,    sizeof(unsigned long),  true  },
  {"float",     kFloatSrId,   sizeof(float),          false },
  {"double",    kDoubleSrId,  sizeof(double),         false },
  {"bool",      kBoolSrId,    sizeof(bool),           sizeof(bool)>1 },
  {"<invalid>", kInvalidSrId, 0,                      false }
};

cmSrH_t cmSrNullHandle = { NULL };


void _cmSrPrint( _cmSr_t* p, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  //p->rptFuncPtr(p->rptUserPtr,fmt,vl);
  cmRptVPrintf( p->err.rpt, fmt, vl );
  va_end(vl);
}

/*
cmSrRC_t _cmSrErrorV( cmSrRC_t rc, _cmSr_t* p, const char* fmt, va_list vl )
{
  const int bufCharCnt = 511;
  char buf[bufCharCnt+1];
  snprintf(buf,bufCharCnt,"Serializer Error code:%i ",rc);
  unsigned n = strlen(buf);
  vsnprintf(buf+n,bufCharCnt-n,fmt,vl);
  buf[bufCharCnt]=0;
  _cmSrPrint(p,"%s\n",buf);
  p->lastRC = rc;
  return rc;
}
*/

cmSrRC_t _cmSrError( cmSrRC_t rc, _cmSr_t* p, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmErrVMsg(&p->err,rc,fmt,vl);
  va_end(vl);
  return rc;
}

_cmSr_t* _cmSrHandleToPtr( cmSrH_t h )
{
  assert( h.h != NULL );
  return (_cmSr_t*)h.h;
}

const _cmSrPrim_t* _cmSrIdToPrimPtr( unsigned primId )
{
  unsigned i;
  for(i=0; _cmSrPrimArray[i].typeId != kInvalidSrId; ++i)
    if( _cmSrPrimArray[i].typeId == primId )
      return _cmSrPrimArray + i;

  return NULL;
}

_cmSrStruct_t* _cmSrIdToStructPtr( _cmSr_t* p, unsigned typeId )
{
  _cmSrStruct_t* sp = p->structList;
  for(; sp != NULL; sp = sp->linkPtr )
    if( sp->typeId == typeId )
      return sp;
  return NULL;
}

void _cmSrClearStructList( _cmSr_t* p )
{
  _cmSrStruct_t* csp = p->structList;
  while( csp != NULL )
  {
    _cmSrStruct_t* nsp = csp->linkPtr;

    _cmSrField_t* cfp = csp->fieldList;
    
    while( cfp != NULL )
    {
      _cmSrField_t* nfp = cfp->linkPtr;

      cmLHeapFree(p->lhH,cfp);

      cfp = nfp;
    }

    
    cmLHeapFree(p->lhH,csp);

    csp = nsp;
  }

  p->structList = NULL;
}

void _cmSrClearDataList( _cmSr_t* p )
{
  
  _cmSrData_t* cdp = p->wrDataList;

  while( cdp != NULL )
  {
    _cmSrData_t* ndp = cdp->linkPtr;

    cmLHeapFree( p->lhH, cdp );

    cdp = ndp;
  }

  p->wrDataList = NULL;
}

cmSrRC_t _cmSrPopStateStack( _cmSr_t* p, _cmSrState_t** stackPtrPtr )
{
  cmSrRC_t      rc       = kOkSrRC;
  _cmSrState_t* stackPtr = *stackPtrPtr;
  _cmSrState_t* sp       = stackPtr;

  assert( sp != NULL );

  stackPtr = sp->linkPtr;

  if( sp->arrayCnt != cmInvalidCnt && sp->arrayIdx != sp->arrayCnt )
    rc = _cmSrError( kFormatViolationSrRC, p, "A type %i structure array field promised %i elements but only %i written.",sp->structPtr->typeId,sp->arrayCnt,sp->arrayIdx);

  cmLHeapFree(p->lhH,sp);
  *stackPtrPtr = stackPtr;

  return rc;
}

void _cmSrClearStateStack( _cmSr_t* p, _cmSrState_t** stackPtrPtr )
{
  while( *stackPtrPtr != NULL )
    if( _cmSrPopStateStack( p, stackPtrPtr ) != kOkSrRC )
      break;
}


cmSrRC_t _cmSrFree( _cmSr_t* p )
{
  cmSrRC_t rc = kOkSrRC;

  _cmSrClearDataList(p);
  _cmSrClearStateStack(p,&p->wrStackPtr);
  _cmSrClearStateStack(p,&p->rdStackPtr);
  cmMemPtrFree(&p->wrDataBufPtr);
  cmLHeapDestroy( &p->lhH );

  cmMemPtrFree(&p);
  return rc;
}


cmSrRC_t cmSrAlloc( cmSrH_t* hp, cmCtx_t* ctx )
{
  cmSrRC_t rc   = kOkSrRC;
  _cmSr_t* p    = cmMemAllocZ( _cmSr_t, 1 );

  cmErrSetup(&p->err,&ctx->rpt,"Serializer");

  p->structList = NULL;
  p->lastRC     = kOkSrRC;

  if( cmLHeapIsValid( p->lhH = cmLHeapCreate(1024,ctx)) == false )
  {
    rc = _cmSrError( kLHeapFailSrRC, p, "Linked heap initialization failed.");
    goto errLabel;
  }

  hp->h = p;
 errLabel:
  return rc;
}

cmSrRC_t cmSrFree(  cmSrH_t* hp )
{
  cmSrRC_t rc = kOkSrRC;
  _cmSr_t* p  = NULL;

  if( hp == NULL  )
    return kOkSrRC;

  if( cmSrIsValid(*hp) == false )
    return kOkSrRC;

  p = _cmSrHandleToPtr(*hp);

  if(( rc = _cmSrFree(p)) != kOkSrRC )
    goto errLabel;

  hp->h = NULL;

 errLabel:
  return rc;
}

bool     cmSrIsValid( cmSrH_t h )
{ return h.h != NULL; }

cmSrRC_t cmSrLastErrorCode( cmSrH_t h )
{
  _cmSr_t* p = _cmSrHandleToPtr(h);
  return p->lastRC;
}

cmSrRC_t   cmSrGetAndClearLastErrorCode( cmSrH_t h )
{
  _cmSr_t* p  = _cmSrHandleToPtr(h);
  cmSrRC_t rc = p->lastRC;
  p->lastRC = kOkSrRC;
  return rc;
}

cmSrRC_t cmSrFmtReset(    cmSrH_t h )
{
  _cmSr_t* p = _cmSrHandleToPtr(h);

  _cmSrClearStructList(p);

  p->lastRC = kOkSrRC;

  return cmSrWrReset(h);
}

cmSrRC_t cmSrFmtDefineStruct( cmSrH_t h, unsigned structTypeId )
{
  cmSrRC_t       rc        = kOkSrRC;
  _cmSr_t*       p         = _cmSrHandleToPtr(h);
  _cmSrStruct_t* structPtr = NULL;

  assert( structTypeId >= kStructSrId );

  if( p->lastRC != kOkSrRC )
    return p->lastRC;

  if( structTypeId < kStructSrId )
  {
    rc = _cmSrError( kParamErrSrRC, p, "The structure type id %i is no greater than or equal to %i. (kStructSrId)",structTypeId,kStructSrId );
    goto errLabel;
  }

  if( _cmSrIdToStructPtr( p, structTypeId ) != NULL)
  {
    rc = _cmSrError( kParamErrSrRC, p, "The structure type id %i is already in use.",structTypeId );
    goto errLabel;
  }

  if(( structPtr = (_cmSrStruct_t*)cmLHeapAllocZ( p->lhH, sizeof(_cmSrStruct_t))) == NULL )
  {
    rc = _cmSrError( kLHeapFailSrRC, p, "New structure allocate failed.");
    goto errLabel;
  }

  structPtr->typeId  = structTypeId;
  structPtr->linkPtr = p->structList;
  p->structList      = structPtr;
  
 errLabel:
  return rc;

}


cmSrRC_t _cmSrFmtField(     cmSrH_t h,  unsigned typeId, bool arrayFl )
{
  cmSrRC_t           rc        = kOkSrRC;
  _cmSrStruct_t*     structPtr = NULL;
  const _cmSrPrim_t* primPtr   = NULL;
  _cmSrField_t*      fieldPtr  = NULL;
  _cmSrField_t*      cfp       = NULL;
  _cmSrField_t*      pfp       = NULL;
  _cmSr_t*           p         = _cmSrHandleToPtr(h);

  if( p->lastRC != kOkSrRC )
    return p->lastRC;

  if( typeId >= kStructSrId )
  {
    if(( structPtr = _cmSrIdToStructPtr( p, typeId )) == NULL )
    {
      rc = _cmSrError( kParamErrSrRC, p, "The structure associated with the type id %i could not be found.",typeId);
      goto errLabel;
    }
  }
  else
  {
    if( (typeId==kInvalidSrId) || ((primPtr = _cmSrIdToPrimPtr( typeId )) == NULL) )
    {
      rc = _cmSrError( kParamErrSrRC, p, "Type primitive type id %i is not valid.",typeId);
      goto errLabel;
    }
  }

  if(( fieldPtr = (_cmSrField_t*)cmLHeapAllocZ( p->lhH, sizeof(_cmSrField_t))) == NULL )
  {
    rc = _cmSrError( kLHeapFailSrRC, p, "Field allocation failed for type %i.",typeId );
    goto errLabel;
  }

  fieldPtr->primPtr   = primPtr;
  fieldPtr->structPtr = structPtr;
  fieldPtr->arrayFl   = arrayFl;

  pfp = NULL;
  cfp = p->structList->fieldList;

  for(; cfp != NULL; cfp = cfp->linkPtr )
    pfp = cfp;

  if( pfp == NULL )
    p->structList->fieldList = fieldPtr;
  else
    pfp->linkPtr = fieldPtr;
  
 errLabel:
  return rc;
  
}

cmSrRC_t cmSrFmtStruct(  cmSrH_t h, unsigned id ) { return _cmSrFmtField(h,id,          false); }
cmSrRC_t cmSrFmtChar(    cmSrH_t h )              { return _cmSrFmtField(h,kCharSrId,   false); }
cmSrRC_t cmSrFmtUChar(   cmSrH_t h )              { return _cmSrFmtField(h,kUCharSrId,  false); }
cmSrRC_t cmSrFmtShort(   cmSrH_t h )              { return _cmSrFmtField(h,kShortSrId,  false); }
cmSrRC_t cmSrFmtUShort(  cmSrH_t h )              { return _cmSrFmtField(h,kUShortSrId, false); }
cmSrRC_t cmSrFmtLong(    cmSrH_t h )              { return _cmSrFmtField(h,kLongSrId,   false); }
cmSrRC_t cmSrFmtULong(   cmSrH_t h )              { return _cmSrFmtField(h,kULongSrId,  false); }
cmSrRC_t cmSrFmtInt(     cmSrH_t h )              { return _cmSrFmtField(h,kIntSrId,    false); }
cmSrRC_t cmSrFmtUInt(    cmSrH_t h )              { return _cmSrFmtField(h,kUIntSrId,   false); }
cmSrRC_t cmSrFmtFloat(   cmSrH_t h )              { return _cmSrFmtField(h,kFloatSrId,  false); }
cmSrRC_t cmSrFmtDouble(  cmSrH_t h )              { return _cmSrFmtField(h,kDoubleSrId, false); }
cmSrRC_t cmSrFmtBool(    cmSrH_t h )              { return _cmSrFmtField(h,kBoolSrId,   false); }

cmSrRC_t cmSrFmtStructV(  cmSrH_t h, unsigned id ){ return _cmSrFmtField(h,id,         true); }
cmSrRC_t cmSrFmtCharV(    cmSrH_t h )             { return _cmSrFmtField(h,kCharSrId,   true); }
cmSrRC_t cmSrFmtUCharV(   cmSrH_t h )             { return _cmSrFmtField(h,kUCharSrId,  true); }
cmSrRC_t cmSrFmtShortV(   cmSrH_t h )             { return _cmSrFmtField(h,kShortSrId,  true); }
cmSrRC_t cmSrFmtUShortV(  cmSrH_t h )             { return _cmSrFmtField(h,kUShortSrId, true); }
cmSrRC_t cmSrFmtLongV(    cmSrH_t h )             { return _cmSrFmtField(h,kLongSrId,   true); }
cmSrRC_t cmSrFmtULongV(   cmSrH_t h )             { return _cmSrFmtField(h,kULongSrId,  true); }
cmSrRC_t cmSrFmtIntV(     cmSrH_t h )             { return _cmSrFmtField(h,kIntSrId,    true); }
cmSrRC_t cmSrFmtUIntV(    cmSrH_t h )             { return _cmSrFmtField(h,kUIntSrId,   true); }
cmSrRC_t cmSrFmtFloatV(   cmSrH_t h )             { return _cmSrFmtField(h,kFloatSrId,  true); }
cmSrRC_t cmSrFmtDoubleV(  cmSrH_t h )             { return _cmSrFmtField(h,kDoubleSrId, true); }
cmSrRC_t cmSrFmtBoolV(    cmSrH_t h )             { return _cmSrFmtField(h,kBoolSrId,   true); }


cmSrRC_t cmSrDefFmt( cmSrH_t h,  unsigned structTypeId, ... )
{
  cmSrRC_t rc = kOkSrRC;
  va_list vl;
  va_start(vl,structTypeId);
  
  if((rc = cmSrFmtDefineStruct( h, structTypeId )) != kOkSrRC )
    goto errLabel;

  unsigned typeId = kCharSrId;

  while( typeId != kInvalidSrId )
  {
    typeId = va_arg(vl,unsigned);
    
    if( typeId == kInvalidSrId )
      break;

    bool arrayFl = cmIsFlag(typeId,kArraySrFl);

    typeId = cmClrFlag(typeId,kArraySrFl);

    if(( rc = _cmSrFmtField(h,typeId,arrayFl)) != kOkSrRC )
      goto errLabel;
      
  }

 errLabel:
  va_end(vl);
  return rc;
}

cmSrRC_t cmSrFmtPrint( cmSrH_t h )
{
  _cmSr_t* p  = _cmSrHandleToPtr(h);
  cmSrRC_t rc = kOkSrRC;


  const _cmSrStruct_t* sp = p->structList;

  for(; sp != NULL; sp = sp->linkPtr )
  {
    _cmSrPrint( p, "struct: %i", sp->typeId );
    unsigned indent = 2;

    const _cmSrField_t* fp = sp->fieldList;

    for(; fp != NULL; fp = fp->linkPtr )
    {
      unsigned i;
      char sp[ indent+1 ];
      sp[indent]=0;
      for(i=0; i<indent; ++i)
        sp[i] = ' ';

      
      if( fp->primPtr != NULL )
        _cmSrPrint( p, "%s prim:  %i %s%s", sp, fp->primPtr->byteCnt, fp->primPtr->label, fp->arrayFl?"[]":"" );
      else
      {
        _cmSrPrint( p, "%s struct: %i %s", sp, fp->structPtr->typeId, fp->arrayFl?"[]":"" );
        indent += 2;
      }
    }
  }

  _cmSrPrint(p,"\n");

  return rc;  
}



cmSrRC_t _cmSrPushStateStack( _cmSr_t* p, unsigned structTypeId, _cmSrState_t** stackPtrPtr )
{
  cmSrRC_t rc = kOkSrRC;

  _cmSrStruct_t* refStructPtr;

  _cmSrState_t* stackPtr = *stackPtrPtr;

  // if this is the base structure then the wr stack will be empty
  if( stackPtr == NULL )
    refStructPtr = p->structList;
  else
  {
    // the wr stack contains members so this structure type should be the same as the current field type
    assert( stackPtr->fieldPtr != NULL );
    refStructPtr = stackPtr->fieldPtr->structPtr;
  }

  // no reference structure exists
  if( refStructPtr == NULL )
  {
    // if the write state stack is empty then the structList must also be empty
    if( stackPtr == NULL )
    {
      rc = _cmSrError( kFormatViolationSrRC, p, "Cannot write data without first providing the data format.");
      goto errLabel;
    }
    else
    {
      // the data format has been described but the current format field is not a structure
      assert( stackPtr->fieldPtr->primPtr != NULL );

      rc = _cmSrError( kFormatViolationSrRC, p, "Format violation. Expected primitive type:'%s'.", cmStringNullGuard(stackPtr->fieldPtr->primPtr->label ));
      goto errLabel;
    }
  }

  // if wrong type of structure
  if( refStructPtr->typeId != structTypeId )
  {
    rc = _cmSrError( kFormatViolationSrRC, p, "Format violation. Expected structure type:%i instead of structure type:%i.",refStructPtr->typeId,structTypeId );
      goto errLabel;

  }

  // allocate the new structure state
  _cmSrState_t* wsp = (_cmSrState_t*)cmLHeapAllocZ( p->lhH, sizeof(_cmSrState_t));
  assert( wsp != NULL );

  // push the new structure state on the stack
  wsp->structPtr = refStructPtr;
  wsp->fieldPtr  = NULL; 
  wsp->linkPtr   = stackPtr;
  wsp->arrayCnt  = cmInvalidCnt;
  wsp->arrayIdx  = cmInvalidIdx;
  stackPtr  = wsp;

  *stackPtrPtr = stackPtr;

 errLabel:
  return rc;
}

  
cmSrRC_t cmSrWrReset(     cmSrH_t h )
{
  cmSrRC_t rc = kOkSrRC;
  _cmSr_t* p = _cmSrHandleToPtr(h);
  
  _cmSrClearStateStack(p,&p->wrStackPtr);
  p->wrStackPtr = NULL;

  _cmSrClearDataList(p);

  cmMemPtrFree(&p->wrDataBufPtr);
  p->wrDataBufByteCnt = 0;

  

  return rc;
}

cmSrRC_t _cmSrOnStruct( _cmSr_t* p, unsigned structTypeId, _cmSrState_t** stackPtrPtr )
{
  cmSrRC_t rc = kOkSrRC;

  if( p->lastRC != kOkSrRC )
    return p->lastRC;

  if((rc = _cmSrPushStateStack(p,structTypeId,stackPtrPtr)) != kOkSrRC )
    goto errLabel;

  _cmSrState_t* stackPtr = *stackPtrPtr;

  // the base structure is never an array so it's linkPtr is NULL
  if( stackPtr->linkPtr != NULL )
  {
    stackPtr->linkPtr->arrayIdx += 1;

    if( stackPtr->linkPtr->arrayIdx > stackPtr->linkPtr->arrayCnt )
    {
      rc = _cmSrError( kFormatViolationSrRC, p, "The structure array is out of bounds (array count:%i).",stackPtr->linkPtr->arrayIdx );
      goto errLabel;
    }
  }

  *stackPtrPtr = stackPtr;

 errLabel:
  return rc;

}

cmSrRC_t cmSrWrStructBegin(    cmSrH_t h, unsigned structTypeId )
{
  _cmSr_t* p = _cmSrHandleToPtr(h);
  return _cmSrOnStruct( p, structTypeId, &p->wrStackPtr );
}

cmSrRC_t cmSrWrStructEnd( cmSrH_t h )
{
  _cmSr_t* p = _cmSrHandleToPtr(h);
  assert( p->wrStackPtr != NULL );

  if( p->lastRC != kOkSrRC )
    return p->lastRC;

  return  _cmSrPopStateStack(p, &p->wrStackPtr);
}


cmSrRC_t _cmSrWrField(     cmSrH_t h, unsigned typeId, const void* dataPtr, unsigned eleCnt )
{
  cmSrRC_t       rc           = kOkSrRC;
  _cmSr_t* p                  = _cmSrHandleToPtr(h);
  _cmSrField_t*  refFieldPtr  = NULL;
  unsigned       refTypeId    = cmInvalidId;
  unsigned       dataByteCnt  = 0;
  _cmSrData_t*   dataRecdPtr  = NULL;
  bool           swapFl       = false;

  if( p->lastRC != kOkSrRC )
    return p->lastRC;

  // verify the write stack exists
  if( p->wrStackPtr == NULL )
  {
    rc = _cmSrError( kFormatViolationSrRC, p, "The reference format is unexpectedly empty.");
    goto errLabel;
  }

  //
  // advance the current field
  //
  
  // if cur state fieldPtr is NULL then this is the first field in a structure
  if( p->wrStackPtr->fieldPtr  == NULL )
    p->wrStackPtr->fieldPtr = p->wrStackPtr->structPtr->fieldList;
  else
    p->wrStackPtr->fieldPtr = p->wrStackPtr->fieldPtr->linkPtr;

  // verify that a reference field exists
  refFieldPtr = p->wrStackPtr->fieldPtr;

  if( refFieldPtr == NULL )
  {
      rc = _cmSrError( kFormatViolationSrRC, p, "The format reference structure has run out of fields.");
      goto errLabel;
  }

  // validate the array status of this field
  if( refFieldPtr->arrayFl==false && eleCnt>1 )
  {
    rc = _cmSrError( kFormatViolationSrRC, p, "The format reference indicates that this field is not an array however an array element count (%i) was given.",eleCnt);
    goto errLabel;
  }

  // get the reference type id
  refTypeId =  refFieldPtr->primPtr == NULL ? refFieldPtr->structPtr->typeId : refFieldPtr->primPtr->typeId;


  // verify that the type being written matches the reference type
  if( refTypeId != typeId )
  {
    const char* refLbl = "struct";
    const char* cmtLbl = refLbl;

    if( refFieldPtr->primPtr != NULL )
      refLbl = refFieldPtr->primPtr->label;

    if( typeId < kStructSrId )
      cmtLbl = _cmSrIdToPrimPtr( typeId )->label;

    rc = _cmSrError( kFormatViolationSrRC, p, "Format violation: Exepected type %i (%s) but received %i (%s).", refTypeId,refLbl,typeId,cmtLbl);
    goto errLabel;
  }

  if( typeId < kStructSrId )
  {
    dataByteCnt += refFieldPtr->primPtr->byteCnt * eleCnt;
    swapFl       = refFieldPtr->primPtr->swapFl && _cmSrSwapFl;
  }
  else
  {
    p->wrStackPtr->arrayCnt = eleCnt;
    p->wrStackPtr->arrayIdx = 0;    
  }  

  dataRecdPtr = (_cmSrData_t*)cmLHeapAllocZ( p->lhH, sizeof(_cmSrData_t) +  dataByteCnt );

  // iniit the new data recd
  dataRecdPtr->typeId  = typeId;
  dataRecdPtr->eleCnt  = eleCnt;
  dataRecdPtr->byteCnt = dataByteCnt;
  dataRecdPtr->arrayFl = refFieldPtr->arrayFl;
  dataRecdPtr->swapFl  = swapFl;
  dataRecdPtr->linkPtr = NULL;
  dataRecdPtr->dataPtr = dataRecdPtr + 1;

  if( dataByteCnt > 0 )
    memcpy( dataRecdPtr->dataPtr, dataPtr, dataByteCnt );

  // link the new data recd to the end of the data chain
  _cmSrData_t* dp = p->wrDataList;
  for(; dp != NULL; dp = dp->linkPtr )
    if( dp->linkPtr == NULL )
      break;

  if( p->wrDataList == NULL )
    p->wrDataList = dataRecdPtr;
  else
    dp->linkPtr = dataRecdPtr;

 errLabel:
  return rc;
}

cmSrRC_t cmSrWrStruct( cmSrH_t h, unsigned structTypeId, unsigned eleCnt )
{ return _cmSrWrField(h,structTypeId,NULL,eleCnt); }

cmSrRC_t cmSrWrChar(   cmSrH_t h, char val )
{ return _cmSrWrField(h,kCharSrId,&val,1); }

cmSrRC_t cmSrWrUChar(  cmSrH_t h, unsigned char val )
{ return _cmSrWrField(h,kUCharSrId,&val,1); }

cmSrRC_t cmSrWrShort(  cmSrH_t h, short val )
{ return _cmSrWrField(h,kShortSrId,&val,1); }

cmSrRC_t cmSrWrUShort( cmSrH_t h, unsigned short val )
{ return _cmSrWrField(h,kUShortSrId,&val,1); }

cmSrRC_t cmSrWrLong(   cmSrH_t h, long val )
{ return _cmSrWrField(h,kLongSrId,&val,1); }

cmSrRC_t cmSrWrULong(  cmSrH_t h, unsigned long val )
{ return _cmSrWrField(h,kULongSrId,&val,1); }

cmSrRC_t cmSrWrInt(    cmSrH_t h, int val )
{ return _cmSrWrField(h,kIntSrId,&val,1); }

cmSrRC_t cmSrWrUInt(   cmSrH_t h, unsigned val )
{ return _cmSrWrField(h,kUIntSrId,&val,1); }

cmSrRC_t cmSrWrFloat(  cmSrH_t h, float val )
{ return _cmSrWrField(h,kFloatSrId,&val,1); }

cmSrRC_t cmSrWrDouble( cmSrH_t h, double val )
{ return _cmSrWrField(h,kDoubleSrId,&val,1); }

cmSrRC_t cmSrWrBool( cmSrH_t h, bool val )
{ return _cmSrWrField(h,kBoolSrId,&val,1); }


cmSrRC_t cmSrWrStr(   cmSrH_t h, const char* val )
{ 
  const char* ns = ""; 
  if( val == NULL )
    val = ns;
  return _cmSrWrField(h,kCharSrId,val,strlen(val)+1); 
}

cmSrRC_t _cmSrWrArrayField( cmSrH_t h, unsigned typeId, const void* val, unsigned eleCnt )
{
  if( val == NULL || eleCnt == 0 )
  {
    val = NULL;
    eleCnt = 0;
  }

  return _cmSrWrField(h,typeId,val,eleCnt);
}

cmSrRC_t cmSrWrCharV(   cmSrH_t h, const char*           val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kCharSrId,val,eleCnt); }

cmSrRC_t cmSrWrUCharV(  cmSrH_t h, const unsigned char*  val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kUCharSrId,val,eleCnt); }

cmSrRC_t cmSrWrShortV(  cmSrH_t h, const short*          val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kShortSrId,val,eleCnt); }

cmSrRC_t cmSrWrUShortV( cmSrH_t h, const unsigned short* val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kUShortSrId,val,eleCnt); }

cmSrRC_t cmSrWrLongV(   cmSrH_t h, const long*           val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kLongSrId,val,eleCnt); }

cmSrRC_t cmSrWrULongV(  cmSrH_t h, const unsigned long*  val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kULongSrId,val,eleCnt); }

cmSrRC_t cmSrWrIntV(    cmSrH_t h, const int*            val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kIntSrId,val,eleCnt); }

cmSrRC_t cmSrWrUIntV(   cmSrH_t h, const unsigned*       val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kUIntSrId,val,eleCnt); }

cmSrRC_t cmSrWrFloatV(  cmSrH_t h, const float*          val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kFloatSrId,val,eleCnt); }

cmSrRC_t cmSrWrDoubleV( cmSrH_t h, const double*         val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kDoubleSrId,val,eleCnt); }

cmSrRC_t cmSrWrBoolV( cmSrH_t h,   const bool*           val, unsigned eleCnt )
{ return _cmSrWrArrayField(h,kBoolSrId,val,eleCnt); }


void _cmSrCopyDataToBuf( void* dstPtr, const void* srcPtr, unsigned byteCnt, unsigned typeId, bool swapFl, unsigned eleCnt )
{

  if( !swapFl )
  {
    memcpy(dstPtr,srcPtr,byteCnt);
    return;
  }

  switch( typeId )
  {
    case kShortSrId:
    case kUShortSrId:
      {
        unsigned     i;
        const short* sp = (const short*)srcPtr; 
        short*       dp = (short*)dstPtr;
        for(i=0; i<eleCnt; ++i)
          dp[i] = _cmSrSwap16(sp[i]);
      }
      break;

    case kLongSrId:
    case kULongSrId:
    case kIntSrId:
    case kUIntSrId:
      {
        unsigned    i;
        const long* sp = (const long*)srcPtr; 
        long*       dp = (long*)dstPtr;
        for(i=0; i<eleCnt; ++i)
          dp[i] = _cmSrSwap32(sp[i]);
      }
      break;

    default:
      assert(0);
  }
}

void* cmSrWrAllocBuf( cmSrH_t h, unsigned* bufByteCntPtr )
{
  _cmSr_t*     p   = _cmSrHandleToPtr(h);
  _cmSrData_t* cdp = p->wrDataList;;

  assert( bufByteCntPtr != NULL );

  *bufByteCntPtr      = 0;
  p->wrDataBufByteCnt = 2 * sizeof(unsigned); // header words

  if( p->lastRC != kOkSrRC )
    return NULL;
  

  // determine the data buf size
  for(; cdp!=NULL; cdp = cdp->linkPtr )
  {
    // include spcme for structure type id
    if( cdp->typeId >= kStructSrId )
      p->wrDataBufByteCnt += sizeof(unsigned);

    // include spcme for array element count
    if( cdp->arrayFl )
      p->wrDataBufByteCnt += sizeof(unsigned);

    // include spcme for cmtual data
    p->wrDataBufByteCnt += cdp->byteCnt;

  }
  
  // allocate the data buffer
  p->wrDataBufPtr = cmMemResizeZ( char, p->wrDataBufPtr, p->wrDataBufByteCnt );
  
  cdp = p->wrDataList;

  char* dp = p->wrDataBufPtr;
  char* ep = dp + p->wrDataBufByteCnt; 

  // header version
  *(unsigned*)dp = _cmSrSwap32(cmSrVersion);
  dp += sizeof(unsigned);

  // header flags
  *(unsigned*)dp = _cmSrSwap32(0);
  dp += sizeof(unsigned);

  // fill the data buffer
  for(; cdp!=NULL; cdp=cdp->linkPtr)
  {
    // structure data records contain only the typeId and optionally an array count
    if( cdp->typeId >= kStructSrId )
    {
      *((unsigned*)dp) = _cmSrSwap32(cdp->typeId);
      dp += sizeof(unsigned);
      assert(dp <= ep);
    }

    // array data elements begin with a element count
    if( cdp->arrayFl )
    {
      *((unsigned*)dp) = _cmSrSwap32(cdp->eleCnt);
      dp += sizeof(unsigned);
      assert(dp <= ep);
    }

    // copy data into buf
    if( cdp->byteCnt > 0 )
    {
      assert( cdp->typeId < kStructSrId );
      _cmSrCopyDataToBuf(dp, cdp->dataPtr, cdp->byteCnt, cdp->typeId, cdp->swapFl, cdp->eleCnt );
      //memcpy( dp, cdp->dataPtr, cdp->byteCnt );
      dp += cdp->byteCnt;
      assert(dp <= ep);
    }

  }

  *bufByteCntPtr = p->wrDataBufByteCnt;

  _cmSrClearDataList(p);
  
  return p->wrDataBufPtr;
}

void* cmSrWrGetBuf(   cmSrH_t h, unsigned* bufByteCntPtr )
{
  _cmSr_t* p   = _cmSrHandleToPtr(h);
  assert( bufByteCntPtr != NULL);
  *bufByteCntPtr = p->wrDataBufByteCnt;
  return p->wrDataBufPtr;
}

unsigned _cmSrProcUInt( _cmSr_t* p )
{
  unsigned val                = _cmSrSwap32(*(unsigned*)p->rdCursorPtr);
  *(unsigned*)p->rdCursorPtr  = val;
  p->rdCursorPtr             += sizeof(unsigned);
  return val;
}

cmSrRC_t  _cmSrProcBuf( _cmSr_t* p, _cmSrStruct_t* structPtr )
{
  cmSrRC_t rc = kOkSrRC;
  const _cmSrField_t* cfp = structPtr->fieldList;

  for(; cfp != NULL; cfp = cfp->linkPtr )
  {
    unsigned eleCnt = 1;

    // if this is a structure
    if( cfp->structPtr != NULL )
    {
      unsigned structTypeId = _cmSrProcUInt(p);

      if( structTypeId != cfp->structPtr->typeId )
      {
        rc = _cmSrError( kFormatViolationSrRC, p, "Expected type id:%i encountered type id:%i",cfp->structPtr->typeId,structTypeId);
        goto errLabel;
      }
    }

    // if this is an array      
    if( cfp->arrayFl )
      eleCnt = _cmSrProcUInt(p);


    // if this is a primitive type
    if( cfp->primPtr != NULL )
    {
      unsigned dataByteCnt = eleCnt * cfp->primPtr->byteCnt;

      _cmSrCopyDataToBuf( (void*)p->rdCursorPtr, p->rdCursorPtr, dataByteCnt, cfp->primPtr->typeId, cfp->primPtr->swapFl & _cmSrSwapFl,  eleCnt );
      p->rdCursorPtr += dataByteCnt;
    }
    else // this is a structure type
    {
      unsigned i;
      for(i=0; i<eleCnt; ++i)
        if((rc = _cmSrProcBuf(p,cfp->structPtr)) != kOkSrRC )
          goto errLabel;
    }
  }
 errLabel:
  return rc;
}

cmSrRC_t cmSrRdProcessBuffer( cmSrH_t h, void* buf, unsigned bufByteCnt )
{
  cmSrRC_t rc         = kOkSrRC;
  _cmSr_t* p          = _cmSrHandleToPtr(h);

  p->rdDataBufPtr     = buf;
  p->rdDataBufByteCnt = bufByteCnt;
  p->rdCursorPtr      = buf;

  // process the header
  _cmSrProcUInt(p);
  _cmSrProcUInt(p);
    
  if((rc = _cmSrProcBuf(p,p->structList)) != kOkSrRC )
    goto errLabel;

 errLabel:
  return rc;  
}

cmSrRC_t cmSrRdSetup( cmSrH_t h, const void* buf, unsigned bufByteCnt )
{
  cmSrRC_t rc         = kOkSrRC;
  _cmSr_t* p          = _cmSrHandleToPtr(h);
  
  p->rdDataBufPtr     = buf;
  p->rdDataBufByteCnt = bufByteCnt;
  p->rdCursorPtr      = buf;
  _cmSrClearStateStack(p,&p->rdStackPtr);
  p->rdStackPtr = NULL;

  // buffer must at least contain a header
  assert( bufByteCnt >= 2 * sizeof(unsigned) );

  p->rdVersion = (*(unsigned*)p->rdCursorPtr);
  p->rdCursorPtr += sizeof(unsigned);

  p->rdFlags = (*(unsigned*)p->rdCursorPtr);
  p->rdCursorPtr += sizeof(unsigned);

  return rc;
}
  
cmSrRC_t cmSrRdStructBegin( cmSrH_t h, unsigned structTypeId )
{
  _cmSr_t* p = _cmSrHandleToPtr(h);

  return _cmSrOnStruct( p, structTypeId, &p->rdStackPtr );
}

cmSrRC_t cmSrRdStructEnd( cmSrH_t h )
{
  _cmSr_t* p = _cmSrHandleToPtr(h);
  assert( p->rdStackPtr != NULL );

  if( p->lastRC != kOkSrRC )
    return p->lastRC;

  return _cmSrPopStateStack(p,&p->rdStackPtr);
}

cmSrRC_t _cmSrRead( _cmSr_t* p, unsigned typeId, const void** dataPtrPtr, unsigned* dataByteCntPtr, unsigned* eleCntPtr )
{
   cmSrRC_t     rc          = kOkSrRC;
  _cmSrField_t* refFieldPtr = NULL;
  unsigned      refTypeId   = cmInvalidId;
  unsigned      eleCnt      = 1;

  if( eleCntPtr != NULL )
    *eleCntPtr = 0;

  if( dataByteCntPtr != NULL )
    *dataByteCntPtr = 0;
  
  if( dataPtrPtr != NULL )
    *dataPtrPtr = NULL;

  if( p->lastRC != kOkSrRC )
    return p->lastRC;

  // verify the write stack exists - all fields exists inside structures so the stack must have at least one element
  if( p->rdStackPtr == NULL )
  {
    rc = _cmSrError( kFormatViolationSrRC, p, "The reference format is unexpectedly empty.");
    goto errLabel;
  }

  //
  // advance the current field
  //
  
  // if cur state fieldPtr is NULL then this is the first field in a structure
  if( p->rdStackPtr->fieldPtr  == NULL )
    p->rdStackPtr->fieldPtr = p->rdStackPtr->structPtr->fieldList;
  else
    p->rdStackPtr->fieldPtr = p->rdStackPtr->fieldPtr->linkPtr;

  // verify that a reference field exists
  refFieldPtr = p->rdStackPtr->fieldPtr;

  if( refFieldPtr == NULL )
  {
      rc = _cmSrError( kFormatViolationSrRC, p, "The format reference structure has run out of fields.");
      goto errLabel;
  }


  // get the reference type id
  refTypeId =  refFieldPtr->primPtr == NULL ? refFieldPtr->structPtr->typeId : refFieldPtr->primPtr->typeId;

  // verify that the type being written matches the reference type
  if( refTypeId != typeId )
  {
    const char* refLbl = "struct";
    const char* cmtLbl = refLbl;

    if( refFieldPtr->primPtr != NULL )
      refLbl = refFieldPtr->primPtr->label;

    if( typeId < kStructSrId )
      cmtLbl = _cmSrIdToPrimPtr( typeId )->label;

    rc = _cmSrError( kFormatViolationSrRC, p, "Format violation: Exepected type %i (%s) but received %i (%s).", refTypeId,refLbl,typeId,cmtLbl);
    goto errLabel;
  }

  // if this is a primitive type
  if( typeId < kStructSrId )
  {
    unsigned byteCnt = refFieldPtr->primPtr->byteCnt;

    if( refFieldPtr->arrayFl )
    {
      eleCnt         = *(unsigned*)p->rdCursorPtr;

      byteCnt        *= eleCnt;
      p->rdCursorPtr += sizeof(unsigned);

      if( eleCntPtr == NULL )
      {
        rc = _cmSrError( kFormatViolationSrRC, p, "A scalar read was performed where an array was expected. type:%s array count:%i.",refFieldPtr->primPtr->label,eleCnt);
        goto errLabel;
      }

      *eleCntPtr = eleCnt;
    }

    *dataPtrPtr     = p->rdCursorPtr;
    *dataByteCntPtr = byteCnt;
    p->rdCursorPtr += byteCnt;
    
  }
  else // this is a structure type
  {
    unsigned structTypeId = structTypeId = *(unsigned*)p->rdCursorPtr;

    p->rdCursorPtr += sizeof(unsigned);

    if( refFieldPtr->arrayFl )
    {
      eleCnt = *(unsigned*)p->rdCursorPtr;
      p->rdCursorPtr += sizeof(unsigned);
    }

    p->rdStackPtr->arrayCnt = eleCnt;
    p->rdStackPtr->arrayIdx = 0;    

    assert(eleCntPtr != NULL );
    *eleCntPtr = eleCnt;
  }  
  
 errLabel:
  return rc;
}


cmSrRC_t cmSrReadStruct(    cmSrH_t h, unsigned structTypeId, unsigned* arrayCnt )
{
  _cmSr_t* p = _cmSrHandleToPtr(h);
  return _cmSrRead(p, structTypeId, NULL, NULL, arrayCnt );
}



cmSrRC_t _cmSrReadScalar( cmSrH_t h, unsigned typeId, void* valPtr, unsigned scalarByteCnt )
{
  cmSrRC_t    rc          = kOkSrRC;
  _cmSr_t*    p           = _cmSrHandleToPtr(h);
  const void* dataPtr     = NULL;
  unsigned    dataByteCnt = 0;


  if((rc= _cmSrRead(p,typeId, &dataPtr, &dataByteCnt, NULL )) != kOkSrRC )
    return rc;

  memcpy(valPtr,dataPtr,dataByteCnt);

  return rc;
}

cmSrRC_t _cmSrReadV( cmSrH_t h, unsigned typeId, const void** valPtrPtr, unsigned scalarByteCnt, unsigned* eleCntPtr )
{
  cmSrRC_t    rc          = kOkSrRC;
  _cmSr_t*    p           = _cmSrHandleToPtr(h);
  unsigned    dataByteCnt = 0;

  if((rc= _cmSrRead(p,typeId, valPtrPtr, &dataByteCnt, eleCntPtr )) != kOkSrRC )
    return rc;

  assert( dataByteCnt == scalarByteCnt * (*eleCntPtr));

  return rc;
}



cmSrRC_t cmSrReadChar(   cmSrH_t h, char* valPtr )
{ return _cmSrReadScalar( h, kCharSrId, valPtr, sizeof(char)); }

cmSrRC_t cmSrReadUChar(  cmSrH_t h, unsigned char* valPtr )
{ return _cmSrReadScalar( h, kUCharSrId, valPtr, sizeof(unsigned char)); }

cmSrRC_t cmSrReadShort(  cmSrH_t h, short* valPtr )
{ return _cmSrReadScalar( h, kShortSrId, valPtr, sizeof(short)); }

cmSrRC_t cmSrReadUShort( cmSrH_t h, unsigned short* valPtr )
{ return _cmSrReadScalar( h, kUShortSrId, valPtr, sizeof(unsigned short)); }

cmSrRC_t cmSrReadLong(   cmSrH_t h, long* valPtr)
{ return _cmSrReadScalar( h, kLongSrId, valPtr, sizeof(long)); }

cmSrRC_t cmSrReadULong(  cmSrH_t h, unsigned long* valPtr )
{ return _cmSrReadScalar( h, kULongSrId, valPtr, sizeof(unsigned long)); }

cmSrRC_t cmSrReadInt(    cmSrH_t h, int* valPtr)
{ return _cmSrReadScalar( h, kIntSrId, valPtr, sizeof(int)); }

cmSrRC_t cmSrReadUInt(   cmSrH_t h, unsigned* valPtr )
{ return _cmSrReadScalar( h, kUIntSrId, valPtr, sizeof(unsigned int)); }

cmSrRC_t cmSrReadFloat(  cmSrH_t h, float* valPtr )
{ return _cmSrReadScalar( h, kFloatSrId, valPtr, sizeof(float)); }

cmSrRC_t cmSrReadDouble( cmSrH_t h, double* valPtr )
{ return _cmSrReadScalar( h, kDoubleSrId, valPtr, sizeof(double)); }

cmSrRC_t cmSrReadBool( cmSrH_t h, bool* valPtr )
{ return _cmSrReadScalar( h, kBoolSrId, valPtr, sizeof(bool)); }



cmSrRC_t cmSrReadCharV(   cmSrH_t h, char** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kCharSrId, (const void**)valPtrPtr, sizeof(char), eleCntPtr); }

cmSrRC_t cmSrReadUCharV(  cmSrH_t h, unsigned char** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kUCharSrId, (const void**)valPtrPtr, sizeof(unsigned char), eleCntPtr); }

cmSrRC_t cmSrReadShortV(  cmSrH_t h, short** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kShortSrId, (const void**)valPtrPtr, sizeof(short), eleCntPtr); }

cmSrRC_t cmSrReadUShortV( cmSrH_t h, unsigned short** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kUShortSrId, (const void**)valPtrPtr, sizeof(unsigned short), eleCntPtr); }

cmSrRC_t cmSrReadLongV(   cmSrH_t h, long** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kLongSrId, (const void**)valPtrPtr, sizeof(long), eleCntPtr); }

cmSrRC_t cmSrReadULongV(  cmSrH_t h, unsigned long** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kULongSrId, (const void**)valPtrPtr, sizeof(unsigned long), eleCntPtr); }

cmSrRC_t cmSrReadIntV(    cmSrH_t h, int** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kIntSrId, (const void**)valPtrPtr, sizeof(int), eleCntPtr); }

cmSrRC_t cmSrReadUIntV(   cmSrH_t h, unsigned** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kUIntSrId, (const void**)valPtrPtr, sizeof(unsigned int), eleCntPtr); }

cmSrRC_t cmSrReadFloatV(  cmSrH_t h, float** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kFloatSrId, (const void**)valPtrPtr, sizeof(float), eleCntPtr); }

cmSrRC_t cmSrReadDoubleV( cmSrH_t h, double** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kDoubleSrId, (const void**)valPtrPtr, sizeof(double), eleCntPtr); }

cmSrRC_t cmSrReadBoolV( cmSrH_t h, bool** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kBoolSrId, (const void**)valPtrPtr, sizeof(bool), eleCntPtr); }



cmSrRC_t cmSrReadCharCV(   cmSrH_t h, const char** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kCharSrId, (const void**)valPtrPtr, sizeof(char), eleCntPtr); }

cmSrRC_t cmSrReadUCharCV(  cmSrH_t h, const unsigned char** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kUCharSrId, (const void**)valPtrPtr, sizeof(unsigned char), eleCntPtr); }

cmSrRC_t cmSrReadShortCV(  cmSrH_t h, const short** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kShortSrId, (const void**)valPtrPtr, sizeof(short), eleCntPtr); }

cmSrRC_t cmSrReadUShortCV( cmSrH_t h, const unsigned short** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kUShortSrId, (const void**)valPtrPtr, sizeof(unsigned short), eleCntPtr); }

cmSrRC_t cmSrReadLongCV(   cmSrH_t h, const long** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kLongSrId, (const void**)valPtrPtr, sizeof(long), eleCntPtr); }

cmSrRC_t cmSrReadULongCV(  cmSrH_t h, const unsigned long** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kULongSrId, (const void**)valPtrPtr, sizeof(unsigned long), eleCntPtr); }

cmSrRC_t cmSrReadIntCV(    cmSrH_t h, const int** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kIntSrId, (const void**)valPtrPtr, sizeof(int), eleCntPtr); }

cmSrRC_t cmSrReadUIntCV(   cmSrH_t h, const unsigned** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kUIntSrId, (const void**)valPtrPtr, sizeof(unsigned int), eleCntPtr); }

cmSrRC_t cmSrReadFloatCV(  cmSrH_t h, const float** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kFloatSrId, (const void**)valPtrPtr, sizeof(float), eleCntPtr); }

cmSrRC_t cmSrReadDoubleCV( cmSrH_t h, const double** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kDoubleSrId, (const void**)valPtrPtr, sizeof(double), eleCntPtr); }

cmSrRC_t cmSrReadBoolCV( cmSrH_t h, const bool** valPtrPtr, unsigned* eleCntPtr )
{ return _cmSrReadV( h, kBoolSrId, (const void**)valPtrPtr, sizeof(bool), eleCntPtr); }




unsigned  cmSrRdStruct( cmSrH_t h, unsigned structTypeId )
{
  unsigned eleCnt;
  return cmSrReadStruct(h,structTypeId,&eleCnt) == kOkSrRC ? eleCnt : cmInvalidCnt;
}

char           cmSrRdChar(   cmSrH_t h )
{
  char val;
  return cmSrReadChar(h,&val) == kOkSrRC ? val : 0;
}

unsigned char  cmSrRdUChar(  cmSrH_t h )
{
  unsigned char val;
  return cmSrReadUChar(h,&val) == kOkSrRC ? val : 0;
}

short          cmSrRdShort(  cmSrH_t h )
{
  short val;
  return cmSrReadShort(h,&val) == kOkSrRC ? val : 0;
}

unsigned short cmSrRdUShort( cmSrH_t h )
{
  unsigned short val;
  return cmSrReadUShort(h,&val) == kOkSrRC ? val : 0;
}

long           cmSrRdLong(   cmSrH_t h )
{
  long val;
  return cmSrReadLong(h,&val) == kOkSrRC ? val : 0;
}

unsigned long  cmSrRdULong(  cmSrH_t h )
{
  unsigned long val;
  return cmSrReadULong(h,&val) == kOkSrRC ? val : 0;
}

int            cmSrRdInt(    cmSrH_t h )
{
  int val;
  return cmSrReadInt(h,&val) == kOkSrRC ? val : 0;
}

unsigned int   cmSrRdUInt(   cmSrH_t h )
{
  unsigned val;
  return cmSrReadUInt(h,&val) == kOkSrRC ? val : 0;
}

float          cmSrRdFloat(  cmSrH_t h )
{
  float val;
  return cmSrReadFloat(h,&val) == kOkSrRC ? val : 0;
}

double         cmSrRdDouble( cmSrH_t h )
{
  double val;
  return cmSrReadDouble(h,&val) == kOkSrRC ? val : 0;
}

bool         cmSrRdBool( cmSrH_t h )
{
  bool val;
  return cmSrReadBool(h,&val) == kOkSrRC ? val : 0;
}




char*           cmSrRdCharV(   cmSrH_t h, unsigned* eleCntPtr)
{
  char* valPtr;
  return cmSrReadCharV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

unsigned char*  cmSrRdUCharV(  cmSrH_t h, unsigned* eleCntPtr)
{
  unsigned char* valPtr;
  return cmSrReadUCharV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

short*          cmSrRdShortV(  cmSrH_t h, unsigned* eleCntPtr)
{
  short* valPtr;
  return cmSrReadShortV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

unsigned short* cmSrRdUShortV( cmSrH_t h, unsigned* eleCntPtr)
{
  unsigned short* valPtr;
  return cmSrReadUShortV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

long*           cmSrRdLongV(   cmSrH_t h, unsigned* eleCntPtr)
{
  long* valPtr;
  return cmSrReadLongV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

unsigned long*  cmSrRdULongV(  cmSrH_t h, unsigned* eleCntPtr)
{
  unsigned long* valPtr;
  return cmSrReadULongV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

int*            cmSrRdIntV(    cmSrH_t h, unsigned* eleCntPtr)
{
  int* valPtr;
  return cmSrReadIntV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

unsigned int*   cmSrRdUIntV(   cmSrH_t h, unsigned* eleCntPtr)
{
  unsigned* valPtr;
  return cmSrReadUIntV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

float*          cmSrRdFloatV(  cmSrH_t h, unsigned* eleCntPtr)
{
  float* valPtr;
  return cmSrReadFloatV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

double*         cmSrRdDoubleV( cmSrH_t h, unsigned* eleCntPtr)
{
  double* valPtr;
  return cmSrReadDoubleV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

bool*         cmSrRdBoolV( cmSrH_t h, unsigned* eleCntPtr)
{
  bool* valPtr;
  return cmSrReadBoolV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}




const char*           cmSrRdCharCV(   cmSrH_t h, unsigned* eleCntPtr)
{
  const char* valPtr;
  return cmSrReadCharCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

const unsigned char*  cmSrRdUCharCV(  cmSrH_t h, unsigned* eleCntPtr)
{
  const unsigned char* valPtr;
  return cmSrReadUCharCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

const short*          cmSrRdShortCV(  cmSrH_t h, unsigned* eleCntPtr)
{
  const short* valPtr;
  return cmSrReadShortCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

const unsigned short* cmSrRdUShortCV( cmSrH_t h, unsigned* eleCntPtr)
{
  const unsigned short* valPtr;
  return cmSrReadUShortCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

const long*           cmSrRdLongCV(   cmSrH_t h, unsigned* eleCntPtr)
{
  const long* valPtr;
  return cmSrReadLongCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

const unsigned long*  cmSrRdULongCV(  cmSrH_t h, unsigned* eleCntPtr)
{
  const unsigned long* valPtr;
  return cmSrReadULongCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

const int*            cmSrRdIntCV(    cmSrH_t h, unsigned* eleCntPtr)
{
  const int* valPtr;
  return cmSrReadIntCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

const unsigned int*   cmSrRdUIntCV(   cmSrH_t h, unsigned* eleCntPtr)
{
  const unsigned* valPtr;
  return cmSrReadUIntCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

const float*          cmSrRdFloatCV(  cmSrH_t h, unsigned* eleCntPtr)
{
  const float* valPtr;
  return cmSrReadFloatCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

const double*         cmSrRdDoubleCV( cmSrH_t h, unsigned* eleCntPtr)
{
  const double* valPtr;
  return cmSrReadDoubleCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}

const bool*         cmSrRdBoolCV( cmSrH_t h, unsigned* eleCntPtr)
{
  const bool* valPtr;
  return cmSrReadBoolCV(h,&valPtr,eleCntPtr) == kOkSrRC ? valPtr : NULL;
}


//
// cmSrTest() is a serializer example function.
//
cmSrRC_t cmSrTest( cmCtx_t* ctx )
{
  unsigned i,j,k;
  cmSrRC_t rc = kOkSrRC;
  cmSrH_t h;
  unsigned    bufByteCnt;
  const void* bufPtr;


  enum
  {
    kVectSrId = kStructSrId,   // nested structure id
    kVectArrSrId               // outer structure id
  };

  // nested structure
  typedef struct
  {
    float*   data;
    unsigned cnt;
  } vect;

  // outer structure
  typedef struct
  {
    unsigned cnt;
    vect*    vectArray;
  } vectArray;

  float vd0[]  = { 0, 1, 2, 3, 4};
  float vd1[]  = { 10, 11, 12 };
  unsigned  vn = 2;
  vect v[]     = { {vd0,5}, {vd1,3} };
  vectArray va = { vn, v };

  if(( rc = cmSrAlloc( &h, ctx )) != kOkSrRC )
    goto errLabel;

  // repeat format processes to test cmSrFormatReset()
  for(k=0; k<2; ++k)
  {
    cmSrFmtReset(h);

    // Define the format of nested structures first
    //
    // Long Form:
    //   cmSrFmtDefineStruct(h,kVectSrId);
    //   cmSrFmtFloatV(h  );
    //   cmSrFmtUInt(h );
    //
    // Short Form:
    cmSrDefFmt(h, kVectSrId, kFloatVSrId, kUIntSrId, kInvalidSrId );

    // Define the format of the outer structure last
    //
    // Long Form:
    //   cmSrFmtDefineStruct(h,kVectArrSrId);
    //   cmSrFmtUInt(h );
    //   cmSrFmtStructV(h, kVectSrId );
    //
    // Short Form:
    cmSrDefFmt(h, kVectArrSrId, kUIntSrId, kVectSrId | kArraySrFl, kInvalidSrId );
 
    cmSrFmtPrint(h);

    // repeat write process to test cmSrWrReset()
    for(j=0; j<2; ++j)
    {
      cmSrWrReset(h);

      cmSrWrStructBegin( h, kVectArrSrId );
      cmSrWrUInt(    h, vn );
      cmSrWrStruct(  h, kVectSrId, vn );

      for(i=0; i<vn; ++i)
      {
        cmSrWrStructBegin( h, kVectSrId );
        cmSrWrFloatV(      h,  va.vectArray[i].data, va.vectArray[i].cnt );
        cmSrWrUInt(        h,  va.vectArray[i].cnt );
        cmSrWrStructEnd(h);
      }
  
      cmSrWrStructEnd(h);

      bufByteCnt = 0;
      bufPtr     = cmSrWrAllocBuf( h, &bufByteCnt );
    }

    // The serialized buffer has the following format:
    // Words Bytes   
    // ----- ----- -----------------------------------------
    //   1    4   [ uint (2) ]
    //   2    8   [ id  (11) ][ cnt (2) ]
    //   6   24   [ cnt  (5) ][0.0][1.0][2.0][3.0][4.0]
    //   1    4   [ uint (5) ]
    //   4   16   [ cnt  (3) ][10.][11.][12.]
    //   1    4   [ uint (3) ]
    // ----- ----
    //  14   60
    //

    unsigned    n0,n1,n2,n3;
    const float* fArr;

    cmSrRdProcessBuffer(h, (void*)bufPtr, bufByteCnt );
    cmSrRdSetup( h, bufPtr, bufByteCnt );

    cmSrRdStructBegin( h, kVectArrSrId );
    cmSrReadUInt( h, &n0 );
    cmSrReadStruct( h, kVectSrId, &n1 );
    for(i=0; i<n1; ++i)
    {
      cmSrRdStructBegin( h, kVectSrId );
      cmSrReadFloatCV( h, &fArr, &n2 ); 
      cmSrReadUInt( h,  &n3 );
      cmSrRdStructEnd( h );
    }


    cmSrRdSetup( h, bufPtr, bufByteCnt );

    cmSrRdStructBegin( h, kVectArrSrId );
    n0 = cmSrRdUInt( h );
    n1 = cmSrRdStruct( h, kVectSrId );
    for(i=0; i<n1; ++i)
    {
      cmSrRdStructBegin( h, kVectSrId );
      fArr = cmSrRdFloatV( h, &n2 ); 
      n3   = cmSrRdUInt( h );
      cmSrRdStructEnd( h );

      for(j=0; j<n2; ++j)
        printf("%f ",fArr[j]);
      printf("\n");
    }
  }

 errLabel:
  cmSrFree(&h);

  return rc;
}


