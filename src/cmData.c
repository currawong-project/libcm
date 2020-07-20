#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmData.h"
#include "cmLex.h"
#include "cmText.h"
#include "cmStack.h"

typedef struct
{
  cmDataTypeId_t  typeId; 
  unsigned        byteWidth;
  const cmChar_t* label;
} cmDtTypeInfo_t;

typedef struct
{
  cmDataContainerId_t id;
  const cmChar_t* label;
} cmDtCntInfo_t;

cmDtTypeInfo_t _cmDtTypeInfoArray[] =
{
  { kNullDtId,   0,                    "null"           },
  { kBoolDtId,   sizeof(bool),            "bool"         },
  { kUCharDtId,  sizeof(unsigned char),  "uchar"        },
  { kCharDtId,   sizeof(char),           "char"         },
  { kUShortDtId, sizeof(unsigned short), "ushort"       },
  { kShortDtId,  sizeof(short),          "short"        },
  { kUIntDtId,   sizeof(unsigned int),   "uint"         },
  { kIntDtId,    sizeof(int),            "int"          },
  { kULongDtId,  sizeof(unsigned long),  "ulong"        },
  { kLongDtId,   sizeof(long),           "long"         },
  { kFloatDtId,  sizeof(float),          "float"        },
  { kDoubleDtId, sizeof(double),         "double"       },
  { kStrDtId,    sizeof(char*),          "string"       },
  { kBlobDtId,   sizeof(void*),          "blob"         },
  { kInvalidTypeDtId, 0,               "<invalid-type>" },
};

cmDtCntInfo_t _cmDtCntInfoArray[] =
{
  { kScalarDtId, "scalar" },
  { kArrayDtId,  "array" },
  { kListDtId,   "list"  },
  { kPairDtId,   "pair"  },
  { kRecordDtId, "record"},
  { kInvalidCntDtId,"<invalid-container>"}
};

cmData_t cmDataNull = { kInvalidTypeDtId,kInvalidCntDtId,0,NULL,NULL,0 };

cmDtRC_t _cmDtErrMsgV( const cmData_t* d, cmDtRC_t rc, const cmChar_t* fmt, va_list vl )
{
  // REPLACE this with a global cmRpt call.
  vprintf(fmt,vl);
  return rc;
}

cmDtRC_t _cmDtErrMsg( const cmData_t* d, cmDtRC_t rc, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc = _cmDtErrMsgV(d,rc,fmt,vl);
  va_end(vl);
  return rc;
}

const cmChar_t* cmDataTypeToLabel( cmDataTypeId_t tid )
{
  unsigned i;
  for(i=0; _cmDtTypeInfoArray[i].typeId!=kInvalidTypeDtId; ++i)
    if( _cmDtTypeInfoArray[i].typeId == tid )
      return _cmDtTypeInfoArray[i].label;
  return NULL;
}

cmDataTypeId_t   cmDataLabelToType( const cmChar_t* typeLabelStr )
{
  unsigned i;
  for(i=0; _cmDtTypeInfoArray[i].typeId!=kInvalidTypeDtId; ++i)
    if( strcmp(_cmDtTypeInfoArray[i].label,typeLabelStr) == 0 )
      return _cmDtTypeInfoArray[i].typeId;
  return kInvalidTypeDtId;
}

unsigned cmDataByteWidth( cmDataTypeId_t tid )
{
  unsigned i;
  for(i=0; _cmDtTypeInfoArray[i].typeId!=kInvalidTypeDtId; ++i)
    if( _cmDtTypeInfoArray[i].typeId == tid )
      return _cmDtTypeInfoArray[i].byteWidth;
  return cmInvalidCnt;
}

const cmChar_t*  cmDataContainerIdToLabel( cmDataContainerId_t tid )
{
  unsigned i;
  for(i=0; _cmDtCntInfoArray[i].id!=kInvalidCntDtId; ++i)
    if( _cmDtCntInfoArray[i].id == tid )
      return _cmDtCntInfoArray[i].label;
  return NULL;  
}

cmDataContainerId_t  cmDataLabelToContainerId( const cmChar_t* contLabelStr )
{
  unsigned i;
  for(i=0; _cmDtCntInfoArray[i].id!=kInvalidCntDtId; ++i)
    if( strcmp(_cmDtCntInfoArray[i].label,contLabelStr) == 0 )
      return _cmDtCntInfoArray[i].id;
  return kInvalidCntDtId;
}

bool _cmDataIsDataOwner( const cmData_t* d )
{ return cmIsFlag(d->flags,kFreeValueDtFl) && (d->cid==kArrayDtId || d->tid==kStrDtId || d->tid==kBlobDtId); }


cmDtRC_t  _cmDataFreeData( cmData_t* d )
{
  if( _cmDataIsDataOwner(d) )
  {

    cmMemPtrFree(&d->u.vp);    
  }

  d->flags  = cmClrFlag(d->flags,kFreeValueDtFl | kConstValueDtFl );
  d->tid    = kNullDtId; // objects without data are always of type 'null'.
  d->cnt    = 0;
  memset(&d->u,0,sizeof(d->u));
  return kOkDtRC;
}

void _cmDataFree( cmData_t* p )
{
  if( p == NULL )
    return;

  if( cmDataIsStruct(p) )
  {
    cmData_t* cp = p->u.child;
    for(; cp!=NULL; cp=cp->sibling)
      _cmDataFree(cp);
  }
    
  _cmDataFreeData(p);

  if( cmIsFlag(p->flags,kFreeObjDtFl) )
  {
    // if the object body is going to be freed then be sure it is unlinked
    cmDataUnlink(p);
    cmMemFree(p);
  }
}


// Dynamically allocate a new data object.
cmData_t*  _cmDataNew(cmData_t* parent, cmDataContainerId_t cid, cmDataTypeId_t tid)
{
  cmData_t* d = cmMemAllocZ(cmData_t,1);
  d->tid    = tid;   // objects without data are of type 'null'.
  d->cid    = cid;
  d->flags  = kFreeObjDtFl;
  d->parent = parent;
  d->cnt = 0;

  if( parent != NULL )
    cmDataAppendChild(parent,d);  

  return d;
}


bool cmDataIsConstObj( const cmData_t* d )
{ return cmIsFlag(d->flags,kConstObjDtFl); }

void cmDataEnableConstObj( cmData_t* d, bool enaFl )
{ d->flags = cmEnaFlag(d->flags,kConstObjDtFl,enaFl); }

bool cmDataIsConstValue( const cmData_t* d )
{ return cmIsFlag(d->flags,kConstValueDtFl); }

void cmDataEnableConstValue( cmData_t* d, bool enaFl )
{ d->flags = cmEnaFlag(d->flags,kConstValueDtFl,enaFl); }

bool cmDataIsFreeValue( const cmData_t* d )
{ return cmIsFlag(d->flags,kFreeValueDtFl); }

void cmDataEnableFreeValue( cmData_t* d, bool enaFl )
{ d->flags = cmEnaFlag(d->flags,kFreeValueDtFl,enaFl); }

bool cmDataIsLeaf( const cmData_t* d)
{ return d->cid == kScalarDtId || d->cid == kArrayDtId; }

bool cmDataIsStruct( const cmData_t* d )
{ return !cmDataIsLeaf(d); }


cmDtRC_t cmDataNewScalar( cmData_t* parent, cmDataTypeId_t tid, unsigned flags, void* vp, unsigned byteCnt, cmData_t** ref )
{
  cmDtRC_t rc;

  if( ref != NULL )
    *ref = NULL;
  
  // create a scalar null object
  cmData_t* d = _cmDataNew(parent,kScalarDtId,kNullDtId);

  if( tid!=kStrDtId && tid!=kBlobDtId )
  {
    // When used with scalars kFreeValueDtFl and kNoCopyDtFl only 
    // has meaning for strings and blobs - so clear these flags for other types.
    flags = cmClrFlag(flags,kFreeValueDtFl | kNoCopyDtFl);    

    // if this is not a blob or string then the byteCnt is reset
    byteCnt = cmDataByteWidth(tid);
  }

  

  // assign the value
  if((rc = cmDataSetScalarValue(d,tid,vp,byteCnt,flags)) != kOkDtRC )
    return rc;

  // set the const flags for the new object
  d->flags = cmSetFlag(d->flags, flags & (kConstValueDtFl | kConstObjDtFl));

  if( ref != NULL )
    *ref = d;

  return rc;
}

cmDtRC_t cmDataNewNull(   cmData_t* parent, unsigned flags, cmData_t** ref )
{ *ref = _cmDataNew(parent, kScalarDtId, kNullDtId); return kOkDtRC; }
cmDtRC_t cmDataNewBool(   cmData_t* parent, unsigned flags, bool v, cmData_t** ref )
{ return cmDataNewScalar(parent,kBoolDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewChar(   cmData_t* parent, unsigned flags, char v, cmData_t** ref )
{ return cmDataNewScalar(parent,kCharDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewUChar(  cmData_t* parent, unsigned flags, unsigned char v,  cmData_t** ref )
{ return cmDataNewScalar(parent,kUCharDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewShort(  cmData_t* parent, unsigned flags, short v, cmData_t** ref )
{ return cmDataNewScalar(parent,kShortDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewUShort( cmData_t* parent, unsigned flags, unsigned short v, cmData_t** ref )
{ return cmDataNewScalar(parent,kUShortDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewInt(    cmData_t* parent, unsigned flags, int v,  cmData_t** ref )
{ return cmDataNewScalar(parent,kIntDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewUInt(   cmData_t* parent, unsigned flags, unsigned int v,  cmData_t** ref )
{ return cmDataNewScalar(parent,kUIntDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewLong(   cmData_t* parent, unsigned flags, long v,  cmData_t** ref )
{ return cmDataNewScalar(parent,kLongDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewULong(  cmData_t* parent, unsigned flags, unsigned long v, cmData_t** ref )
{ return cmDataNewScalar(parent,kULongDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewFloat(  cmData_t* parent, unsigned flags, float v, cmData_t** ref )
{ return cmDataNewScalar(parent,kFloatDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewDouble( cmData_t* parent, unsigned flags, double v,  cmData_t** ref )
{ return cmDataNewScalar(parent,kDoubleDtId,flags,&v,0,ref); }
cmDtRC_t cmDataNewStr( cmData_t* parent, unsigned flags, cmChar_t* v, cmData_t** ref )
{ return cmDataNewScalar(parent,kStrDtId,flags,v,strlen(v)+1,ref); }
cmDtRC_t cmDataNewConstStr( cmData_t* parent, unsigned flags, const cmChar_t* v, cmData_t** ref )
{ return cmDataNewScalar(parent,kStrDtId,flags | kConstValueDtFl, (void*)v,strlen(v)+1,ref); }
cmDtRC_t cmDataNewStrN( cmData_t* parent, unsigned flags, cmChar_t* v, unsigned charCnt, cmData_t** ref )
{ return cmDataNewScalar(parent,kStrDtId,flags,v,charCnt+1,ref); }
cmDtRC_t cmDataNewConstStrN( cmData_t* parent, unsigned flags, const cmChar_t* v, unsigned charCnt, cmData_t** ref )
{ return cmDataNewScalar(parent,kStrDtId,flags | kConstValueDtFl, (void*)v,charCnt+1,ref); }
cmDtRC_t cmDataNewBlob( cmData_t* parent, unsigned flags, void* v, unsigned byteCnt, cmData_t** ref )
{ return cmDataNewScalar(parent,kBlobDtId,flags,v,byteCnt,ref); }
cmDtRC_t cmDataNewConstBlob( cmData_t* parent, unsigned flags, const void* v, unsigned byteCnt, cmData_t** ref )
{ return cmDataNewScalar(parent,kBlobDtId,flags | kConstValueDtFl, (void*)v,byteCnt,ref); }


cmDtRC_t  cmDataSetScalarValue( cmData_t* d, cmDataTypeId_t tid, void* vp, unsigned byteCnt, unsigned flags )
{
  cmDtRC_t rc;

  // if the type of the object is changing
  if( d->tid != tid || d->cid != kScalarDtId )
  {
    // verify that it is legal to change the type of the object
    if( cmIsFlag(d->flags,kConstObjDtFl) )
      return _cmDtErrMsg(d,kConstErrDtRC,"Const object violation.");

    // convert this to a scalar null object.
    if((rc = _cmDataFreeData(d)) != kOkDtRC )
      return rc;
  }

  // verify that it is legal to change the value of this object
  if( cmIsFlag(d->flags,kConstValueDtFl) )
    return _cmDtErrMsg(d,kConstErrDtRC,"Const value violation.");

  switch( tid )
  {
    case kInvalidTypeDtId:    
      return  _cmDtErrMsg(d,kAssertErrDtRC,"Invalid data type.");

    case kNullDtId: // 'd' is already NULL.      
      break;
      
    case kBoolDtId:    d->u.b  = *(bool*)vp;           break;
    case kUCharDtId:   d->u.uc = *(unsigned char*)vp;  break;
    case kCharDtId:    d->u.c  = *(char*)vp;           break;  
    case kUShortDtId:  d->u.us = *(unsigned short*)vp; break;   
    case kShortDtId:   d->u.s  = *(short*)vp;          break;  
    case kUIntDtId:    d->u.ui = *(unsigned int*)vp;   break;  
    case kIntDtId:     d->u.i  = *(int*)vp;            break;  
    case kULongDtId:   d->u.ul = *(unsigned long*)vp;  break;  
    case kLongDtId:    d->u.l  = *(long*)vp;           break;  
    case kFloatDtId:   d->u.f  = *(float*)vp;          break;  
    case kDoubleDtId:  d->u.d  = *(double*)vp;         break;
    case kStrDtId:  
    case kBlobDtId:
      {
        cmChar_t* blankStr = "";

        // strings must have a byteCnt of at least one
        assert( tid==kBlobDtId || (tid==kStrDtId && byteCnt>0) );

        // if a NULL source string is encountered then make it a 0 length string
        if( d->tid==kStrDtId && vp==NULL )
          vp = blankStr;

        // if an empty blob was passed in then be sure it's src ptr is NULL and byteCnt==0
        if( d->tid==kBlobDtId && (vp==NULL || byteCnt==0) )
        {
          if((rc = _cmDataFreeData(d)) != kOkDtRC )
            return rc;

          byteCnt = 0;
          d->u.z  = NULL;
          break;
        }

        // if the incoming string/blob should be internally duplicated
        if( cmIsNotFlag(flags,kNoCopyDtFl) )
        {

          // allocate internal space to store the incoming data
          if( (d->tid==kBlobDtId || d->tid == kStrDtId) && cmIsFlag(d->flags,kFreeValueDtFl) )
            d->u.z = cmMemResize(char,d->u.z,byteCnt);
          else
            d->u.z = cmMemAlloc(char,byteCnt);

          // store the source string/blob into the internal memory buffer
          memcpy(d->u.z,vp,byteCnt);

          // be sure the string is zero terminated
          if( tid == kStrDtId )
            d->u.z[byteCnt-1] = 0;

          // by default the system now takes responsibility for freeing this buffer
          d->flags |= kFreeValueDtFl;
        }
        else // the incoming string/blob pointer is simply being assigned w/o duplication
        {
          // free the objects previous value ...
          if((rc = _cmDataFreeData(d)) != kOkDtRC )
            return rc;
          
          // and assign the new value (without reallocating the string)
          d->u.z    = vp;

          d->flags  = cmEnaFlag(d->flags,kFreeValueDtFl,cmIsFlag(flags,kFreeValueDtFl));
          d->flags |= kNoCopyDtFl;
        }
      }
      break;
      
    default:
      break;
  }

  // we can't set this above because the string type relies
  // on knowing the previous type of the object
  d->cid = kScalarDtId;
  d->tid = tid;
  d->cnt = byteCnt;
  return rc;
}

cmDtRC_t cmDataSetNull(      cmData_t* d )
{ return cmDataSetScalarValue(d, kNullDtId, NULL, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetBool(      cmData_t* d, bool v )
{ return cmDataSetScalarValue(d, kBoolDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetChar(      cmData_t* d, char v )
{ return cmDataSetScalarValue(d, kCharDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetUChar(     cmData_t* d, unsigned char v )
{ return cmDataSetScalarValue(d, kUCharDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetShort(     cmData_t* d, short v )
{ return cmDataSetScalarValue(d, kShortDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetUShort(    cmData_t* d, unsigned short v )
{ return cmDataSetScalarValue(d, kUShortDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetInt(       cmData_t* d, int v )
{ return cmDataSetScalarValue(d, kIntDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetUInt(      cmData_t* d, unsigned int v )
{ return cmDataSetScalarValue(d, kUIntDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetLong(      cmData_t* d, long v )
{ return cmDataSetScalarValue(d, kLongDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetULong(     cmData_t* d, unsigned long v )
{ return cmDataSetScalarValue(d, kULongDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetFloat(     cmData_t* d, float v )
{ return cmDataSetScalarValue(d, kFloatDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetDouble(    cmData_t* d, double v )
{ return cmDataSetScalarValue(d, kDoubleDtId, &v, 0, kNoFlagsDtFl ); }
cmDtRC_t cmDataSetStr(       cmData_t* d, unsigned flags, cmChar_t* v )
{ return cmDataSetScalarValue(d, kStrDtId, v, v==NULL ? 1 : strlen(v)+1, flags ); }
cmDtRC_t cmDataSetConstStr(  cmData_t* d, unsigned flags, const cmChar_t* v )
{ return cmDataSetScalarValue(d, kStrDtId, (void*)v, v==NULL ? 1 : strlen(v)+1, flags |= kConstValueDtFl ); }
cmDtRC_t cmDataSetStrN(      cmData_t* d, unsigned flags, cmChar_t* v, unsigned charCnt )
{ return cmDataSetScalarValue(d, kStrDtId, (void*)v, v==NULL ? 1 : charCnt+1, flags); }
cmDtRC_t cmDataSetConstStrN( cmData_t* d, unsigned flags, const cmChar_t* v, unsigned charCnt )
{ return cmDataSetScalarValue(d, kStrDtId, (void*)v, v==NULL ? 1 : charCnt+1, flags |= kConstValueDtFl); }
cmDtRC_t cmDataSetBlob(      cmData_t* d, unsigned flags, void* v, unsigned byteCnt )
{ return cmDataSetScalarValue(d, kBlobDtId, v, byteCnt, flags); }
cmDtRC_t cmDataSetConstBlob( cmData_t* d, unsigned flags, const void* v, unsigned byteCnt )
{ return cmDataSetScalarValue(d, kBlobDtId, (void*)v, byteCnt, flags |= kConstValueDtFl); }


cmDtRC_t cmDataBool(      const cmData_t* d, bool* v )
{
  if( d->tid != kBoolDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:bool but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.c;
  return kOkDtRC; 
}

cmDtRC_t cmDataChar(      const cmData_t* d, char* v )
{
  if( d->tid != kCharDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:char but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.c;
  return kOkDtRC; 
}

cmDtRC_t cmDataUChar(     const cmData_t* d, unsigned char* v )
{
  if( d->tid != kUCharDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:uchar but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.uc;
  return kOkDtRC; 
}

cmDtRC_t cmDataShort(     const cmData_t* d, short* v )
{
  if( d->tid != kShortDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:short but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.s;
  return kOkDtRC; 
}

cmDtRC_t cmDataUShort(    const cmData_t* d, unsigned short* v )
{
  if( d->tid != kUShortDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:ushort but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.us;
  return kOkDtRC; 
}

cmDtRC_t cmDataInt(       const cmData_t* d, int* v )
{
  if( d->tid != kIntDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:int but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.i;
  return kOkDtRC; 
}

cmDtRC_t cmDataUInt(      const cmData_t* d, unsigned int* v )
{
  if( d->tid != kUIntDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:uint but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.ui;
  return kOkDtRC; 
}

cmDtRC_t cmDataLong(      const cmData_t* d, long* v )
{
  if( d->tid != kLongDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:long but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.l;
  return kOkDtRC; 
}

cmDtRC_t cmDataULong(     const cmData_t* d, unsigned long* v )
{
  if( d->tid != kULongDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:ulong but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.ul;
  return kOkDtRC; 
}

cmDtRC_t cmDataFloat(     const cmData_t* d, float* v )
{
  if( d->tid != kFloatDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:float but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.f;
  return kOkDtRC; 
}

cmDtRC_t cmDataDouble(    const cmData_t* d, double* v )
{
  if( d->tid != kDoubleDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:double but encountered type:%s.",cmDataTypeToLabel(d->tid));
  *v = d->u.d;
  return kOkDtRC; 
}

cmDtRC_t cmDataStr(       const cmData_t* d, cmChar_t** v )
{
  if( d->tid != kStrDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:string but encountered type:%s.",cmDataTypeToLabel(d->tid));

  if( cmIsFlag(d->flags,kConstValueDtFl) )
    return _cmDtErrMsg(d,kConstErrDtRC,"A const string cannot return as a non-const string.");

  *v = d->u.z;
  return kOkDtRC;
}

cmDtRC_t cmDataConstStr(  const cmData_t* d, const cmChar_t** v )
{
  if( d->tid != kStrDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:string but encountered type:%s.",cmDataTypeToLabel(d->tid));

  *v = d->u.z;
  return kOkDtRC;
}

cmDtRC_t cmDataBlob(       const cmData_t* d, void** v, unsigned* byteCntRef )
{
  if( v != NULL )
    *v = NULL;

  if( byteCntRef != NULL )
    *byteCntRef = 0;

  if( d->tid != kBlobDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:string but encountered type:%s.",cmDataTypeToLabel(d->tid));

  if( v != NULL )
    *v = d->u.z;

  if( byteCntRef != NULL )
    *byteCntRef = d->cnt;

  return kOkDtRC;
}

cmDtRC_t cmDataConstBlob(  const cmData_t* d, const void** v, unsigned* byteCntRef )
{
  if( v != NULL )
    *v = NULL;

  if( byteCntRef != NULL )
    *byteCntRef = 0;

  if( d->tid != kBlobDtId )
    return _cmDtErrMsg(d,kInvalidTypeDtRC,"Expected type:string but encountered type:%s.",cmDataTypeToLabel(d->tid));

  if( v != NULL )
    *v = d->u.z;

  if( byteCntRef != NULL )
    *byteCntRef = d->cnt;

  return kOkDtRC;
}

bool           cmDtBool(      const cmData_t* p )
{
  bool v;
  if( cmDataBool(p,&v) != kOkDtRC )
    v = kInvalidDtBool;
  return v;
}

char           cmDtChar(      const cmData_t* p )
{
  char v;
  if( cmDataChar(p,&v) != kOkDtRC )
    v = kInvalidDtChar;
  return v;
}

unsigned char  cmDtUChar(     const cmData_t* p )
{
  unsigned char v;
  if( cmDataUChar(p,&v) != kOkDtRC )
    v = kInvalidDtChar;
  return v;
}

short          cmDtShort(     const cmData_t* p )
{
  short v;
  if( cmDataShort(p,&v) != kOkDtRC )
    v = kInvalidDtShort;
  return v;
}

unsigned short cmDtUShort(    const cmData_t* p )
{
  unsigned short v;
  if( cmDataUShort(p,&v) != kOkDtRC )
    v = kInvalidDtShort;
  return v;
}

int            cmDtInt(       const cmData_t* p )
{
  int v;
  if( cmDataInt(p,&v) != kOkDtRC )
    v = kInvalidDtInt;
  return v;
}

unsigned       cmDtUInt(      const cmData_t* p )
{
  unsigned v;
  if( cmDataUInt(p,&v) != kOkDtRC )
    v = kInvalidDtInt;
  return v;
}

long           cmDtLong(      const cmData_t* p )
{
  long v;
  if( cmDataLong(p,&v) != kOkDtRC )
    v = kInvalidDtLong;
  return v;
}

unsigned long  cmDtULong(     const cmData_t* p )
{
  unsigned long v;
  if( cmDataULong(p,&v) != kOkDtRC )
    v = kInvalidDtLong;
  return v;
}

float          cmDtFloat(     const cmData_t* p )
{
  float v;
  if( cmDataFloat(p,&v) != kOkDtRC )
    v = kInvalidDtFloat;
  return v;
}

double         cmDtDouble(    const cmData_t* p )
{
  double v;
  if( cmDataDouble(p,&v) != kOkDtRC )
    v = kInvalidDtDouble;
  return v;
}

char*          cmDtStr(       const cmData_t* p )
{
  char* v;
  if( cmDataStr(p,&v) != kOkDtRC )
    v = NULL;
  return v;
}

const char*    cmDtConstStr(  const cmData_t* p )
{
  const char* v;
  if( cmDataConstStr(p,&v) != kOkDtRC )
    v = NULL;
  return v;
}

void*          cmDtBlob(      const cmData_t* p, unsigned* byteCntRef )
{
  void* v;
  if( cmDataBlob(p,&v,byteCntRef) != kOkDtRC )
    v = NULL;
  return v;
}

const void*    cmDtConstBlob( const cmData_t* p, unsigned* byteCntRef )
{
  const void* v;
  if( cmDataConstBlob(p,&v,byteCntRef) != kOkDtRC )
    v = NULL;
  return v;
}

cmDtRC_t  cmDataGetBool( const cmData_t* p, bool* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kBoolDtId:   *vp =       p->u.b;  break;
    case kUCharDtId:  *vp = (bool)p->u.uc; break;
    case kCharDtId:   *vp = (bool)p->u.c;  break;
    case kUShortDtId: *vp = (bool)p->u.us; break;
    case kShortDtId:  *vp = (bool)p->u.s;  break;
    case kUIntDtId:   *vp = (bool)p->u.ui; break;
    case kIntDtId:    *vp = (bool)p->u.i;  break;
    case kULongDtId:  *vp = (bool)p->u.ul; break;
    case kLongDtId:   *vp = (bool)p->u.l;  break;
    case kFloatDtId:  *vp = (bool)p->u.f;  break;
    case kDoubleDtId: *vp = (bool)p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'bool'.",cmDataTypeToLabel(p->tid));
  }
  return kOkDtRC;
}

cmDtRC_t  cmDataGetUChar( const cmData_t* p, unsigned char* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kUCharDtId:  *vp =                p->u.uc; break;
    case kBoolDtId:   *vp = (unsigned char)p->u.b;  break;
    case kCharDtId:   *vp = (unsigned char)p->u.c;  break;
    case kUShortDtId: *vp = (unsigned char)p->u.us; break;
    case kShortDtId:  *vp = (unsigned char)p->u.s;  break;
    case kUIntDtId:   *vp = (unsigned char)p->u.ui; break;
    case kIntDtId:    *vp = (unsigned char)p->u.i;  break;
    case kULongDtId:  *vp = (unsigned char)p->u.ul; break;
    case kLongDtId:   *vp = (unsigned char)p->u.l;  break;
    case kFloatDtId:  *vp = (unsigned char)p->u.f;  break;
    case kDoubleDtId: *vp = (unsigned char)p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'uchar'.",cmDataTypeToLabel(p->tid));
  }
  return kOkDtRC;
}

cmDtRC_t  cmDataGetChar( const cmData_t* p, char* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kBoolDtId:   *vp = (char)p->u.b;  break;
    case kUCharDtId:  *vp = (char)p->u.uc; break;
    case kCharDtId:   *vp =       p->u.c;  break;
    case kUShortDtId: *vp = (char)p->u.us; break;
    case kShortDtId:  *vp = (char)p->u.s;  break;
    case kUIntDtId:   *vp = (char)p->u.ui; break;
    case kIntDtId:    *vp = (char)p->u.i;  break;
    case kULongDtId:  *vp = (char)p->u.ul; break;
    case kLongDtId:   *vp = (char)p->u.l;  break;
    case kFloatDtId:  *vp = (char)p->u.f;  break;
    case kDoubleDtId: *vp = (char)p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'char'.",cmDataTypeToLabel(p->tid));
  }
  return kOkDtRC;
}

cmDtRC_t cmDataGetShort(     const cmData_t* p, short* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kBoolDtId:   *vp = (short)p->u.b;  break;
    case kUCharDtId:  *vp = (short)p->u.uc; break;
    case kCharDtId:   *vp = (short)p->u.c;  break;
    case kUShortDtId: *vp = (short)p->u.us; break;
    case kShortDtId:  *vp =        p->u.s;  break;
    case kUIntDtId:   *vp = (short)p->u.ui; break;
    case kIntDtId:    *vp = (short)p->u.i;  break;
    case kULongDtId:  *vp = (short)p->u.ul; break;
    case kLongDtId:   *vp = (short)p->u.l;  break;
    case kFloatDtId:  *vp = (short)p->u.f;  break;
    case kDoubleDtId: *vp = (short)p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'short'.",cmDataTypeToLabel(p->tid));

  }
  return kOkDtRC;
}


cmDtRC_t  cmDataGetUShort(    const cmData_t* p, unsigned short* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kBoolDtId:   *vp = (unsigned short)p->u.b;  break;
    case kUCharDtId:  *vp = (unsigned short)p->u.uc; break;
    case kCharDtId:   *vp = (unsigned short)p->u.c;  break;
    case kUShortDtId: *vp =                 p->u.us; break;
    case kShortDtId:  *vp = (unsigned short)p->u.s;  break;
    case kUIntDtId:   *vp = (unsigned short)p->u.ui; break;
    case kIntDtId:    *vp = (unsigned short)p->u.i;  break;
    case kULongDtId:  *vp = (unsigned short)p->u.ul; break;
    case kLongDtId:   *vp = (unsigned short)p->u.l;  break;
    case kFloatDtId:  *vp = (unsigned short)p->u.f;  break;
    case kDoubleDtId: *vp = (unsigned short)p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'ushort'.",cmDataTypeToLabel(p->tid));

  }
  return kOkDtRC;
}

cmDtRC_t cmDataGetInt(       const cmData_t* p, int* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kBoolDtId:   *vp = (int)p->u.b;  break;
    case kUCharDtId:  *vp = (int)p->u.uc; break;
    case kCharDtId:   *vp = (int)p->u.c;  break;
    case kUShortDtId: *vp = (int)p->u.us; break;
    case kShortDtId:  *vp = (int)p->u.s;  break;
    case kUIntDtId:   *vp = (int)p->u.ui; break;
    case kIntDtId:    *vp =      p->u.i;  break;
    case kULongDtId:  *vp = (int)p->u.ul; break;
    case kLongDtId:   *vp = (int)p->u.l;  break;
    case kFloatDtId:  *vp = (int)p->u.f;  break;
    case kDoubleDtId: *vp = (int)p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'int'.",cmDataTypeToLabel(p->tid));

  }
  return kOkDtRC;
}

cmDtRC_t    cmDataGetUInt(      const cmData_t* p, unsigned int* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kBoolDtId:   *vp = (unsigned int)p->u.b;  break;
    case kUCharDtId:  *vp = (unsigned int)p->u.uc; break;
    case kCharDtId:   *vp = (unsigned int)p->u.c;  break;
    case kUShortDtId: *vp = (unsigned int)p->u.us; break;
    case kShortDtId:  *vp = (unsigned int)p->u.s;  break;
    case kUIntDtId:   *vp =               p->u.ui; break;
    case kIntDtId:    *vp = (unsigned int)p->u.i;  break;
    case kULongDtId:  *vp = (unsigned int)p->u.ul; break;
    case kLongDtId:   *vp = (unsigned int)p->u.l;  break;
    case kFloatDtId:  *vp = (unsigned int)p->u.f;  break;
    case kDoubleDtId: *vp = (unsigned int)p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'uint'.",cmDataTypeToLabel(p->tid));

  }
  return kOkDtRC;
}

cmDtRC_t  cmDataGetLong( const cmData_t* p, long* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kBoolDtId:   *vp = (long)p->u.b;  break;
    case kUCharDtId:  *vp = (long)p->u.uc; break;
    case kCharDtId:   *vp = (long)p->u.c;  break;
    case kUShortDtId: *vp = (long)p->u.us; break;
    case kShortDtId:  *vp = (long)p->u.s;  break;
    case kUIntDtId:   *vp = (long)p->u.ui; break;
    case kIntDtId:    *vp = (long)p->u.i;  break;
    case kULongDtId:  *vp = (long)p->u.ul; break;
    case kLongDtId:   *vp =       p->u.l;  break;
    case kFloatDtId:  *vp = (long)p->u.f;  break;
    case kDoubleDtId: *vp = (long)p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'long'.",cmDataTypeToLabel(p->tid));

  }
  return kOkDtRC;
}

cmDtRC_t   cmDataGetULong(     const cmData_t* p, unsigned long* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kBoolDtId:   *vp = (unsigned long)p->u.b;  break;
    case kUCharDtId:  *vp = (unsigned long)p->u.uc; break;
    case kCharDtId:   *vp = (unsigned long)p->u.c;  break;
    case kUShortDtId: *vp = (unsigned long)p->u.us; break;
    case kShortDtId:  *vp = (unsigned long)p->u.s;  break;
    case kUIntDtId:   *vp = (unsigned long)p->u.ui; break;
    case kIntDtId:    *vp = (unsigned long)p->u.i;  break;
    case kULongDtId:  *vp =                p->u.ul; break;
    case kLongDtId:   *vp = (unsigned long)p->u.l;  break;
    case kFloatDtId:  *vp = (unsigned long)p->u.f;  break;
    case kDoubleDtId: *vp = (unsigned long)p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'ulong'.",cmDataTypeToLabel(p->tid));

  }
  return kOkDtRC;
}

cmDtRC_t   cmDataGetFloat(     const cmData_t* p, float* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kBoolDtId:   *vp = (float)p->u.b;  break;
    case kUCharDtId:  *vp = (float)p->u.uc; break;
    case kCharDtId:   *vp = (float)p->u.c;  break;
    case kUShortDtId: *vp = (float)p->u.us; break;
    case kShortDtId:  *vp = (float)p->u.s;  break;
    case kUIntDtId:   *vp = (float)p->u.ui; break;
    case kIntDtId:    *vp = (float)p->u.i;  break;
    case kULongDtId:  *vp = (float)p->u.ul; break;
    case kLongDtId:   *vp = (float)p->u.l;  break;
    case kFloatDtId:  *vp =        p->u.f;  break;
    case kDoubleDtId: *vp = (float)p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'float'.",cmDataTypeToLabel(p->tid));

  }
  return kOkDtRC;
}

cmDtRC_t  cmDataGetDouble(    const cmData_t* p, double* vp )
{
  if( p->cid != kScalarDtId )
    return _cmDtErrMsg(p,kInvalidContDtRC,"Cannot convert a non-scalar value to a scalar value.");

  switch( p->tid )
  {
    case kBoolDtId:   *vp = (double)p->u.b;  break;
    case kUCharDtId:  *vp = (double)p->u.uc; break;
    case kCharDtId:   *vp = (double)p->u.c;  break;
    case kUShortDtId: *vp = (double)p->u.us; break;
    case kShortDtId:  *vp = (double)p->u.s;  break;
    case kUIntDtId:   *vp = (double)p->u.ui; break;
    case kIntDtId:    *vp = (double)p->u.i;  break;
    case kULongDtId:  *vp = (double)p->u.ul; break;
    case kLongDtId:   *vp = (double)p->u.l;  break;
    case kFloatDtId:  *vp = (double)p->u.f;  break;
    case kDoubleDtId: *vp =         p->u.d;  break;
    default:
      return _cmDtErrMsg(p,kCvtErrDtRC,"Cannot convert '%s' to 'double'.",cmDataTypeToLabel(p->tid));

  }
  return kOkDtRC;
}


char           cmDtGetChar(      const cmData_t* p )
{
  char v;
  if( cmDataGetChar(p,&v) != kOkDtRC )
    v = kInvalidDtChar;
  return v;
}

unsigned char  cmDtGetUChar(     const cmData_t* p )
{
  unsigned char v;
  if( cmDataGetUChar(p,&v) != kOkDtRC )
    v = kInvalidDtChar;
  return v;
}

short          cmDtGetShort(     const cmData_t* p )
{
  short v;
  if( cmDataGetShort(p,&v) != kOkDtRC )
    v = kInvalidDtShort;
  return v;
}

unsigned short cmDtGetUShort(    const cmData_t* p )
{
  unsigned short v;
  if( cmDataGetUShort(p,&v) != kOkDtRC )
    v = kInvalidDtShort;
  return v;
}

int            cmDtGetInt(       const cmData_t* p )
{
  int v;
  if( cmDataGetInt(p,&v) != kOkDtRC )
    v = kInvalidDtInt;
  return v;
}

unsigned       cmDtGetUInt(      const cmData_t* p )
{
  unsigned v;
  if( cmDataGetUInt(p,&v) != kOkDtRC )
    v = kInvalidDtInt;
  return v;
}

long           cmDtGetLong(      const cmData_t* p )
{
  long v;
  if( cmDataGetLong(p,&v) != kOkDtRC )
    v = kInvalidDtLong;
  return v;
}

unsigned long  cmDtGetULong(     const cmData_t* p )
{
  unsigned long v;
  if( cmDataGetULong(p,&v) != kOkDtRC )
    v = kInvalidDtLong;
  return v;
}

float          cmDtGetFloat(     const cmData_t* p )
{
  float v;
  if( cmDataGetFloat(p,&v) != kOkDtRC )
    v = kInvalidDtFloat;
  return v;
}

double         cmDtGetDouble(    const cmData_t* p )
{
  double v;
  if( cmDataGetDouble(p,&v) != kOkDtRC )
    v = kInvalidDtDouble;
  return v;
}


cmDtRC_t cmDataNewArray( cmData_t* parent, cmDataTypeId_t tid, void* vp, unsigned eleCnt, unsigned flags, cmData_t** ref )
{
  cmDtRC_t rc;

  if( ref != NULL )
    *ref = NULL;

  // create a new 'null' object
  cmData_t* d = _cmDataNew(parent, kScalarDtId, kNullDtId );

  // assign the value
  if((rc = cmDataSetArrayValue(d,tid,vp,eleCnt,flags)) != kOkDtRC )
    return rc;

  // set the flags for the new object
  d->flags = cmSetFlag(d->flags, flags & (kConstValueDtFl | kConstObjDtFl | kNoCopyDtFl));

  if( ref != NULL )
    *ref = d;

  return rc;

}

cmDtRC_t cmDataNewBoolArray(   cmData_t* parent, bool* v,  unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kBoolDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewCharArray(   cmData_t* parent, char* v,  unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kCharDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewUCharArray(  cmData_t* parent, unsigned char* v,  unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kUCharDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewShortArray(  cmData_t* parent, short* v,          unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kShortDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewUShortArray( cmData_t* parent, unsigned short* v, unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kUShortDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewIntArray(    cmData_t* parent, int* v,            unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kIntDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewUIntArray(   cmData_t* parent, unsigned int* v,   unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kUIntDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewLongArray(   cmData_t* parent, long* v,           unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kLongDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewULongArray(  cmData_t* parent, unsigned long* v,  unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kULongDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewFloatArray(  cmData_t* parent, float* v,          unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kFloatDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewDoubleArray( cmData_t* parent, double* v,         unsigned eleCnt, unsigned flags, cmData_t** ref )
{ return cmDataNewArray(parent, kDoubleDtId, v, eleCnt, flags, ref ); }
cmDtRC_t cmDataNewStrArray(    cmData_t* parent, cmChar_t** v,       unsigned eleCnt, unsigned flags, cmData_t** ref )
{  return cmDataNewArray(parent, kStrDtId, v, eleCnt, flags, ref );  }
cmDtRC_t cmDataNewConstStrArray( cmData_t* parent, const cmChar_t** v,unsigned eleCnt, unsigned flags, cmData_t** ref )
{  return cmDataNewArray(parent, kStrDtId, (cmChar_t**)v, eleCnt, flags, ref );  }


cmDtRC_t cmDataSetArrayValue( cmData_t* d, cmDataTypeId_t tid, void* vp, unsigned eleCnt, unsigned flags )
{
  cmDtRC_t rc = kOkDtRC;

  // if the type of the object is changing
  if( d->tid != tid || d->cid != kScalarDtId )
  {
    // verify that it is legal to change the type of the object
    if( cmIsFlag(d->flags,kConstObjDtFl) )
      return _cmDtErrMsg(d,kConstErrDtRC,"Const object violation.");

    // convert this to a scalar null object.
    if((rc = _cmDataFreeData(d)) != kOkDtRC )
      return rc;
  }

  // verify that it is legal to change the value of this object
  if( cmIsFlag(d->flags,kConstValueDtFl) )
    return _cmDtErrMsg(d,kConstErrDtRC,"Const value violation.");


  // if the array should be reallocated 
  if( cmIsNotFlag(flags,kNoCopyDtFl) )
  {
    unsigned byteCnt = cmDataByteWidth(tid) * eleCnt;

    // reallocate a new string
    if( d->cid == kArrayDtId && cmIsFlag(d->flags,kFreeValueDtFl) )
      d->u.vp = cmMemResize(char,d->u.z,byteCnt);
    else
      d->u.vp = cmMemAlloc(char,byteCnt);

    memcpy(d->u.z,vp,byteCnt);

    d->flags |= kFreeValueDtFl;
  }
  else
  {
    // free the previous value ...
    if((rc = _cmDataFreeData(d)) != kOkDtRC )
      return rc;
    
    // and assign the new value (without reallocating the array)
    d->u.vp    = vp;

    d->flags  = cmEnaFlag(d->flags,kFreeValueDtFl,cmIsFlag(flags,kFreeValueDtFl));

  }

  // we can't set this above because the string type relies
  // on knowing the previous type of the object
  d->cid    = kArrayDtId;
  d->tid    = tid;
  d->cnt    = eleCnt;

  return rc;

}

cmDtRC_t cmDataSetBoolArray(   cmData_t* d, bool* v,           unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kBoolDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetCharArray(   cmData_t* d, char* v,           unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kCharDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetUCharArray(  cmData_t* d, unsigned char* v,  unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kUCharDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetShortArray(  cmData_t* d, short* v,          unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kShortDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetUShortArray( cmData_t* d, unsigned short* v, unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kUShortDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetIntArray(    cmData_t* d, int* v,            unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kIntDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetUIntArray(   cmData_t* d, unsigned int* v,   unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kUIntDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetLongArray(   cmData_t* d, long* v,           unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kLongDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetULongArray(  cmData_t* d, unsigned long* v,  unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kULongDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetFloatArray(  cmData_t* d, float* v,          unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kFloatDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetDoubleArray( cmData_t* d, double* v,         unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kDoubleDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetStrArray(    cmData_t* d, cmChar_t** v,      unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kStrDtId, v, eleCnt, flags ); }
cmDtRC_t cmDataSetConstStrArray(cmData_t* d,const cmChar_t** v,unsigned eleCnt, unsigned flags )
{ return cmDataSetArrayValue(d, kStrDtId, (cmChar_t**)v, eleCnt, flags ); }


unsigned cmDataArrayEleCount( const cmData_t* d )
{ return d->cid==kArrayDtId ? d->cnt : 0; }

cmDtRC_t cmDataBoolArray(      const cmData_t* d, bool** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (bool*)d->u.vp;
  return kOkDtRC;
}

cmDtRC_t cmDataCharArray(      const cmData_t* d, char** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (char*)d->u.vp;
  return kOkDtRC;
}

cmDtRC_t cmDataUCharArray(     const cmData_t* d, unsigned char** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (unsigned char*)d->u.vp;
  return kOkDtRC;
}

cmDtRC_t cmDataShortArray(     const cmData_t* d, short** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (short*)d->u.vp;
  return kOkDtRC;
}

cmDtRC_t cmDataUShortArray(    const cmData_t* d, unsigned short** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (unsigned short*)d->u.vp;
  return kOkDtRC;
}

cmDtRC_t cmDataIntArray(       const cmData_t* d, int** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (int*)d->u.vp;
  return kOkDtRC;
}

cmDtRC_t cmDataUIntArray(      const cmData_t* d, unsigned int** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (unsigned int*)d->u.vp;
  return kOkDtRC;
}

cmDtRC_t cmDataLongArray(      const cmData_t* d, long** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (long*)d->u.vp;
  return kOkDtRC;
}

cmDtRC_t cmDataULongArray(     const cmData_t* d, unsigned long** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (unsigned long*)d->u.vp;
  return kOkDtRC;
}

cmDtRC_t cmDataFloatArray(     const cmData_t* d, float** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (float*)d->u.vp;
  return kOkDtRC;
}

cmDtRC_t cmDataDoubleArray(    const cmData_t* d, double** v )
{
  if( d->cid != kArrayDtId )
    return _cmDtErrMsg(d,kInvalidContDtRC,"Cannot return an array base for a %s container.",cmDataContainerIdToLabel(d->cid));
  *v = (double*)d->u.vp;
  return kOkDtRC;
}

bool*           cmDtBoolArray(      const cmData_t* d )
{
  bool* v = NULL;
  if( cmDataBoolArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}

char*           cmDtCharArray(      const cmData_t* d )
{
  char* v = NULL;
  if( cmDataCharArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}

unsigned char*  cmDtUCharArray(     const cmData_t* d )
{
  unsigned char* v = NULL;
  if( cmDataUCharArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}

short*          cmDtShortArray(     const cmData_t* d )
{
  short* v = NULL;
  if( cmDataShortArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}

unsigned short* cmDtUShortArray(    const cmData_t* d )
{
  unsigned short* v = NULL;
  if( cmDataUShortArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}

int*            cmDtIntArray(       const cmData_t* d )
{
  int* v = NULL;
  if( cmDataIntArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}

unsigned*   cmDtUIntArray(      const cmData_t* d )
{
  unsigned* v = NULL;
  if( cmDataUIntArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}

long*           cmDtLongArray(      const cmData_t* d )
{
  long* v = NULL;
  if( cmDataLongArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}

unsigned long*  cmDtULongArray(     const cmData_t* d )
{
  unsigned long* v = NULL;
  if( cmDataULongArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}

float*          cmDtFloatArray(     const cmData_t* d )
{
  float* v = NULL;
  if( cmDataFloatArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}

double*         cmDtDoubleArray(    const cmData_t* d )
{
  double* v = NULL;
  if( cmDataDoubleArray(d,&v) != kOkDtRC )
    return NULL;
  return v;
}



//----------------------------------------------------------------------------
// Structure related functions
//


void  cmDataFree( cmData_t* p )
{
  _cmDataFree(p);
}

cmData_t* cmDataUnlink( cmData_t* p )
{
  if( p->parent == NULL )
    return p;

  assert( cmDataIsStruct(p->parent) );

  cmData_t* cp = p->u.child;
  cmData_t* pp = NULL;
  for(; cp!=NULL; cp=cp->sibling)
    if( cp == p )
    {
      if( pp == NULL )
        p->parent->u.child = p->sibling;
      else
        pp->sibling = cp->sibling;
    }
  return p;
}

void cmDataUnlinkAndFree( cmData_t* p )
{
  cmDataUnlink(p);
  cmDataFree(p);
}

cmDtRC_t _cmDataDupl( const cmData_t* d, cmData_t* parent, cmData_t** ref )
{
  cmDtRC_t rc  = kOkDtRC;
  cmData_t* rp = NULL;

  *ref = NULL;

  switch( d->cid )
  {
    case kScalarDtId:
      if( d->tid == kBlobDtId || d->tid == kStrDtId )
        rc = cmDataNewScalar(parent, d->tid, d->flags, d->u.vp, d->cnt, &rp );
      else
        rc = cmDataNewScalar(parent, d->tid, d->flags, d->u.vp, 0, &rp );
      break;

    case kArrayDtId:
      rc = cmDataNewArray(parent, d->tid, d->u.vp, d->cnt, d->flags, &rp );
      break;

    case kListDtId:
    case kPairDtId:
    case kRecordDtId:
      {
        rp  = _cmDataNew(parent,d->cid,d->tid);
        const cmData_t* cp  = d->u.child;
        for(; cp!=NULL; cp=cp->sibling)
        {
          cmData_t* ncp = NULL;
          // duplicate the child (ncp) and append it to the parent (rp)
          if((rc = _cmDataDupl(cp,rp,&ncp)) == kOkDtRC )
            cmDataAppendChild(rp,ncp);
        }
      }

      break;

    default:
      assert(0);
      break;
  }

  if( rp != NULL )
    *ref = rp;

  return rc;
}

cmRC_t cmDataDupl( const cmData_t* p, cmData_t** ref )
{ return _cmDataDupl(p,NULL,ref);  }

cmData_t* cmDataReplace( cmData_t* dst, cmData_t* src )
{
  if( dst->parent == NULL )
  {
    cmDataUnlinkAndFree(dst);
    src->parent = NULL;
    return src;
  }

  cmData_t* parent = dst->parent;
  cmData_t* cp     = parent->u.child;
  cmData_t* pp     = NULL;
  unsigned  i      = 0;
  unsigned  n      = cmDataChildCount(parent);

  // locate dst's right sibling
  for(i=0; i<n; ++i,cp=cp->sibling)
  {
    
    if( cp == dst )
    {
      // link in 'src' in place of 'dst'
      src->sibling = dst->sibling;

      // free dst
      cmDataUnlinkAndFree(dst);

      // update the sibling link to 
      if( pp == NULL )
        parent->u.child = src;
      else
        pp->sibling = src;

      src->parent = parent;
      break;
    }
    pp = cp;
  }
 
  return src;
}


unsigned cmDataChildCount( const cmData_t* p )
{
  if( !cmDataIsStruct(p) )
    return 0;

  unsigned n = 0;
  const cmData_t* cp = p->u.child;
  for(; cp!=NULL; cp=cp->sibling)
    ++n;

  return n;
}

cmData_t* cmDataChild( cmData_t* p, unsigned index )
{
  if( !cmDataIsStruct(p) )
    return NULL;

  unsigned  n  = 0;
  cmData_t* cp = p->u.child;
  for(; cp!=NULL; cp=cp->sibling)
  {
    if( n == index )
      break;
    ++n;
  }

  return cp;
}

cmData_t* cmDataPrependChild(cmData_t* parent, cmData_t* p )
{
  assert( cmDataIsStruct(p) );
  

  cmDataUnlink(p);

  p->u.child    = parent->u.child;
  parent->u.child = p;
  p->parent = parent;
  return p;
}

cmData_t* cmDataAppendChild( cmData_t* parent, cmData_t* p )
{
  assert( cmDataIsStruct(parent) );
  assert( parent->cid != kRecordDtId || (parent->cid == kRecordDtId && p->cid==kPairDtId));

  cmDataUnlink(p);

  cmData_t* cp = parent->u.child;
  if( cp == NULL )
    parent->u.child = p;
  else
  {
    for(; cp!=NULL; cp=cp->sibling)
      if( cp->sibling == NULL )
      {
        cp->sibling = p;
        break;
      }
  }

  p->parent  = parent;
  p->sibling = NULL;
  return p;
}

cmData_t* cmDataInsertChild( cmData_t* parent, unsigned index, cmData_t* p )
{
  if( !cmDataIsStruct(parent) )
    return NULL;

  cmDataUnlink(p);

  unsigned  n  = 0;
  cmData_t* cp = parent->u.child;
  cmData_t* pp = NULL;
  for(; cp!=NULL; cp=cp->sibling)
  {
    if( n == index )
    {
      if( pp == NULL )
      {
        parent->u.child = p;
        p->sibling = NULL;
      }
      else
      {
        p->sibling  = pp->sibling;
        pp->sibling = p;
      }
      break;
        
    }
    ++n;
  }

  p->parent = parent;

  return p;
  
}


//----------------------------------------------------------------------------

bool _cmDataPairIsValid( const cmData_t* p )
{
  assert( p->cid == kPairDtId && p->tid==kStructDtId );

  bool fl = p->u.child == NULL || p->u.child->sibling == NULL || p->u.child->sibling->sibling!=NULL;
  return !fl;
}

// Get the key/value of a pair
cmData_t* cmDataPairKey(          cmData_t* p )
{
  assert( _cmDataPairIsValid(p) );
  return p->u.child;
}

unsigned  cmDataPairKeyId(    cmData_t* p )
{
  assert( _cmDataPairIsValid(p) );
  unsigned id = cmInvalidId;
  cmDataGetUInt(p->u.child,&id);
  return id;
}

const cmChar_t* cmDataPairKeyLabel( cmData_t* p )
{
  assert( _cmDataPairIsValid(p) );
  const cmChar_t* label = NULL;
  cmDataConstStr(p->u.child,&label);
  return label;
}


cmData_t* cmDataPairValue(        cmData_t* p )
{
  assert( _cmDataPairIsValid(p) );
  return p->u.child->sibling;
}
    
  // Set the key or value of an existing pair node. 
cmData_t* cmDataPairSetValue( cmData_t* p, cmData_t* value )
{
  assert( _cmDataPairIsValid(p) );
  cmDataReplace( cmDataPairValue(p), value );
  return p;
}


cmData_t* cmDataPairSetKey(   cmData_t* p, cmData_t* key )
{
  assert( _cmDataPairIsValid(p) );
  cmDataReplace( cmDataPairValue(p), key );
  return p;
}

cmData_t* cmDataPairSetKeyId( cmData_t* p, unsigned id )
{
  assert( _cmDataPairIsValid(p) );
  cmDataSetUInt(p->u.child,id);
  return p;
}

cmData_t* cmDataPairSetKeyLabel(  cmData_t* p, const cmChar_t* label )
{
  assert( _cmDataPairIsValid(p) );
  cmDataSetConstStr(p->u.child,kNoFlagsDtFl,label);
  return p;  
}


cmData_t* cmDataPairMake( cmData_t* parent, cmData_t* p, cmData_t* key, cmData_t* value )
{
  cmDataUnlink(p);
  _cmDataFreeData(p);
  p->tid    = kStructDtId;
  p->cid    = kPairDtId;
  p->parent = parent;
  p->flags   = cmIsFlag(p->flags,kFreeObjDtFl) ? kFreeObjDtFl : 0;
  p->u.child = NULL;
  cmDataAppendChild(p,key);
  cmDataAppendChild(p,value);
  return p;
}

// Dynamically allocate a pair node 
cmRC_t cmDataPairAlloc(  cmData_t* parent, const cmData_t* key, const cmData_t* value, cmData_t** ref )
{
  cmRC_t    rc = kOkDtRC;
  cmData_t* p  = _cmDataNew(parent,kPairDtId,kStructDtId);
  cmData_t* kp = NULL;
  cmData_t* vp = NULL;

  if( ref != NULL )
    *ref = NULL;

  if((rc = cmDataDupl(key,&kp)) != kOkDtRC )
    goto errLabel;

  if((rc = cmDataDupl(value,&vp)) != kOkDtRC )
    goto errLabel;

  cmDataPrependChild(p,vp);
  cmDataPrependChild(p,kp);

  if( ref != NULL )
    *ref = p;

 errLabel:
  if( rc != kOkDtRC )
  {
    cmDataFree(kp);
    cmDataFree(vp);
    cmDataFree(p);
  }

  return rc;
}

cmRC_t cmDataPairAllocId(cmData_t* parent, unsigned  keyId,  cmData_t* value, cmData_t** ref )
{
  cmRC_t rc = kOkDtRC;
  if( ref != NULL )
    *ref = NULL;

  cmData_t* p  = _cmDataNew(parent,kPairDtId,kStructDtId);

  if((rc = cmDataNewUInt(p,kNoFlagsDtFl,keyId,NULL)) != kOkDtRC )
    goto errLabel;

  cmDataAppendChild(p,value);

  if( ref != NULL )
    *ref = p;
  
 errLabel:
  if( rc != kOkDtRC )
    cmDataFree(p);

  return rc;
}

cmRC_t cmDataPairAllocLabel( cmData_t* parent, const cmChar_t *label, cmData_t* value, cmData_t** ref )
{
  cmRC_t    rc = kOkDtRC;
  cmData_t* p  = _cmDataNew(parent,kPairDtId,kStructDtId);

  if( ref != NULL )
    *ref = NULL;

  if((rc = cmDataNewConstStr(p,kNoFlagsDtFl,label,NULL)) != kOkDtRC )
    goto errLabel;

  cmDataAppendChild(p,value);

  if( ref != NULL )
    *ref = p;

 errLabel:

  if( rc != kOkDtRC )
    cmDataFree(p);

  return rc;
}

cmRC_t cmDataPairAllocLabelN(cmData_t* parent, const cmChar_t* label, unsigned charCnt, cmData_t* value, cmData_t** ref)
{
  cmRC_t rc = kOkDtRC;

  if( ref != NULL )
    *ref = NULL;

  cmData_t* p  = _cmDataNew(parent,kPairDtId,kStructDtId);

  if((rc = cmDataNewConstStrN(p,kNoFlagsDtFl,label,charCnt,NULL)) != kOkDtRC )
    goto errLabel;

  cmDataAppendChild(p,value);

  if( ref != NULL )
    *ref = p;

 errLabel:
  if( rc != kOkDtRC )
    cmMemFree(p);

  return rc;
}

  
//----------------------------------------------------------------------------

unsigned  cmDataListCount(const cmData_t* p )
{ return cmDataChildCount(p);  }

cmData_t* cmDataListEle(    cmData_t* p, unsigned index )
{ return cmDataChild(p,index); }

cmData_t* cmDataListMake(  cmData_t* parent, cmData_t* p )
{
  cmDataUnlink(p);
  _cmDataFreeData(p);
  p->parent  = parent;
  p->cid     = kListDtId;
  p->tid     = kStructDtId;
  p->flags   = cmIsFlag(p->flags,kFreeObjDtFl) ? kFreeObjDtFl : 0;
  p->u.child = NULL;
  return p;
}

cmData_t* cmDataListAlloc( cmData_t* parent)
{ return _cmDataNew(parent,kListDtId,kStructDtId); }

cmDtRC_t  _cmDataParseArgV( cmData_t* parent, cmErr_t* err, cmRC_t errRC, va_list vl, cmData_t** vpp )
{
  cmDtRC_t            rc    = kOkDtRC;
  cmData_t*           vp    = NULL;
  cmDataTypeId_t      tid   = va_arg(vl,unsigned);
  cmDataContainerId_t cid   = tid & kContainerDtMask;
  unsigned            flags = tid & kFlagsDtMask;

  tid = cmClrFlag(tid,kContainerDtMask | kFlagsDtMask);

  // if no container flag was given assume a scalar container
  if( cid == 0 )
    cid = kScalarDtId;
    
  switch(cid)
  {
    case kScalarDtId:
      switch(tid)
      {
        case kInvalidTypeDtId:    
          rc = kEolDtRC; 
          break;

        case kNullDtId:       rc = cmDataNewNull(     parent,flags,&vp);                           break;
        case kBoolDtId:       rc = cmDataNewBool(     parent,flags,va_arg(vl,int),&vp);            break;
        case kUCharDtId:      rc = cmDataNewUChar(    parent,flags,va_arg(vl,int),&vp);            break;
        case kCharDtId:       rc = cmDataNewChar(     parent,flags,va_arg(vl,int),&vp);            break;
        case kUShortDtId:     rc = cmDataNewUShort(   parent,flags,va_arg(vl,int),&vp);            break;
        case kShortDtId:      rc = cmDataNewShort(    parent,flags,va_arg(vl,int),&vp);            break;
        case kUIntDtId:       rc = cmDataNewUInt(     parent,flags,va_arg(vl,unsigned int),&vp);   break;
        case kIntDtId:        rc = cmDataNewInt(      parent,flags,va_arg(vl,int),&vp);            break;
        case kULongDtId:      rc = cmDataNewULong(    parent,flags,va_arg(vl,unsigned long),&vp);  break;
        case kLongDtId:       rc = cmDataNewLong(     parent,flags,va_arg(vl,long),&vp);           break;
        case kFloatDtId:      rc = cmDataNewFloat(    parent,flags,va_arg(vl,double),&vp);         break;
        case kDoubleDtId:     rc = cmDataNewDouble(   parent,flags,va_arg(vl,double),&vp);         break;
        case kStrDtId:        rc = cmDataNewStr(      parent,flags,va_arg(vl,cmChar_t*),&vp);      break;
        default:
          if( err != NULL )
            cmErrMsg(err,errRC,"Unknown scalar data type id (%i) when parsing new object var-args list.",tid);
          else
            rc = kInvalidTypeDtRC;
          break;
      }
      break;

    case kArrayDtId:
      {
        void*    svp  = va_arg(vl,void*);
        unsigned cnt = va_arg(vl,unsigned);

        switch(tid)
        {
          case kBoolDtId:       rc = cmDataNewBoolArray(     parent,(bool*)svp,          cnt,flags,&vp); break;
          case kUCharDtId:      rc = cmDataNewUCharArray(    parent,(unsigned char*)svp, cnt,flags,&vp); break;
          case kCharDtId:       rc = cmDataNewCharArray(     parent,(char*)svp,          cnt,flags,&vp); break;
          case kUShortDtId:     rc = cmDataNewUShortArray(   parent,(unsigned short*)svp,cnt,flags,&vp); break;
          case kShortDtId:      rc = cmDataNewShortArray(    parent,(short*)svp,         cnt,flags,&vp); break;
          case kUIntDtId:       rc = cmDataNewUIntArray(     parent,(unsigned int*)svp,  cnt,flags,&vp); break;
          case kIntDtId:        rc = cmDataNewIntArray(      parent,(int*)svp,           cnt,flags,&vp); break;
          case kULongDtId:      rc = cmDataNewULongArray(    parent,(unsigned long*)svp, cnt,flags,&vp); break;
          case kLongDtId:       rc = cmDataNewLongArray(     parent,(long*)svp,          cnt,flags,&vp); break;
          case kFloatDtId:      rc = cmDataNewFloatArray(    parent,(float*)svp,         cnt,flags,&vp); break;
          case kDoubleDtId:     rc = cmDataNewDoubleArray(   parent,(double*)svp,        cnt,flags,&vp); break;
          case kStrDtId:        rc = cmDataNewStrArray(      parent,(cmChar_t**)svp,     cnt,flags,&vp); break;
          default:
            if( err != NULL )
              cmErrMsg(err,errRC,"Unknown array data type id (%i) when parsing new object var-args list.",tid);
            else
              rc = kInvalidTypeDtRC;
            break;
        }
      }
      break;

    case kListDtId:
    case kPairDtId:
    case kRecordDtId:
      cmDataAppendChild(parent,va_arg(vl,cmData_t*));
      break;

    default:
      if( err != NULL )
        cmErrMsg(err,errRC,"Unknown container type id (%i) when parsing new object var-args list.",cid);
      else
        rc = kInvalidContDtRC;
      break;
  }

  *vpp = vp;

  return rc;
}

cmDtRC_t  _cmDataListParseV(cmData_t* parent, cmErr_t* err, cmRC_t errRC, va_list vl )
{
  cmDtRC_t  rc = kOkDtRC;
  bool      contFl = true;

  while( contFl )
  {  
    cmData_t* vp;

    rc = _cmDataParseArgV(parent,err,errRC,vl,&vp);
    
    if(rc != kOkDtRC )    
    {
      contFl = false;
      
      if( rc == kEolDtRC )
        rc = kOkDtRC;
    }
  }

  return rc;
}

cmRC_t  cmDataListAllocV(cmData_t* parent, cmErr_t* err, cmRC_t errRC, cmData_t** ref, va_list vl )
{
  cmData_t* p = cmDataListAlloc(parent);

  cmRC_t rc;

  if((rc = _cmDataListParseV(p,err,errRC,vl )) != kOkDtRC )
  {
    cmDataUnlinkAndFree(p);
    return rc;
  }

  if( ref != NULL )
    *ref = p;

  return rc;
}

cmRC_t cmDataListAllocA(cmData_t* parent, cmErr_t* err, cmRC_t errRC, cmData_t** ref,  ... )
{
  va_list vl;
  va_start(vl,ref);
  cmRC_t rc  = cmDataListAllocV(parent,err,errRC,ref,vl);
  va_end(vl);
  return rc;
}
  

cmData_t* cmDataListAppendEle( cmData_t* p, cmData_t* ele )
{ 
  assert(p->cid == kListDtId && p->tid==kStructDtId);
  return cmDataAppendChild(p,ele);
}

cmData_t* cmDataListAppendEleN(cmData_t* p, cmData_t* ele[], unsigned n )
{
  assert(p->cid == kListDtId && p->tid==kStructDtId);

  cmData_t* rp = NULL;
  unsigned i;
  for(i=0; i<n; ++i)
  {
    cmData_t* ep = cmDataAppendChild(p,ele[i]);
    if( rp == NULL )
      rp = ep;
  }
  return rp;
}

cmDtRC_t  cmDataListAppendV(   cmData_t* p, cmErr_t* err, cmRC_t errRC, va_list vl )
{
  cmDtRC_t rc;
  if((rc = _cmDataListParseV(p, err, errRC, vl )) != kOkDtRC )
    return rc;

  return kOkDtRC;
}

cmDtRC_t  cmDataListAppend(    cmData_t* p, cmErr_t* err, cmRC_t errRC, ... )
{
  va_list vl;  
  va_start(vl,errRC);
  cmDtRC_t rc = cmDataListAppendV(p,err,errRC,vl); 
  va_end(vl);
  return rc;
}

cmData_t* cmDataListInsertEle( cmData_t* p, unsigned index, cmData_t* ele )
{ return cmDataInsertChild(p,index,ele); }

cmData_t* cmDataListInsertEleN(cmData_t* p, unsigned index, cmData_t* ele[], unsigned n )
{
  unsigned i;
  for(i=0; i<n; ++i)
    cmDataListInsertEle(p,index+i,ele[i]);
  return p;
}


//----------------------------------------------------------------------------
unsigned       cmDataRecdCount(    const cmData_t* p )
{ 
  assert( p->cid == kRecordDtId && p->tid==kStructDtId );
  return cmDataChildCount(p);
}

cmData_t*       cmDataRecdEle(      cmData_t* p, unsigned index )
{
  assert( p->cid == kRecordDtId && p->tid==kStructDtId );
  cmData_t* cp = cmDataChild(p,index);
  assert( p->cid == kPairDtId );
  return cp;
}

cmData_t*       cmDataRecdValueFromIndex(    cmData_t* p, unsigned index )
{
  assert( p->cid == kRecordDtId && p->tid==kStructDtId );
  cmData_t* cp =  cmDataChild(p,index);
  assert( p->cid == kPairDtId && p->tid==kStructDtId );
  return cmDataPairValue(cp);
}

cmData_t*       cmDataRecdValueFromId(    cmData_t* p, unsigned id )
{
  assert( p->cid == kRecordDtId || p->tid==kStructDtId );
  cmData_t* cp =  p->u.child;
  for(; cp!=NULL; cp=cp->sibling)
    if( cmDataPairKeyId(cp) == id )
      break;
      
  assert( cp!=NULL && cp->cid==kPairDtId && cp->tid==kStructDtId);

  return cmDataPairValue(cp);
}

cmData_t*       cmDataRecdValueFromLabel(    cmData_t* p, const cmChar_t* label )
{
  assert( p->cid == kRecordDtId && p->tid==kStructDtId);
  cmData_t* cp =  p->u.child;
  for(; cp!=NULL; cp=cp->sibling)
  {
    const cmChar_t* lp = cmDataPairKeyLabel(cp);

    if( lp!=NULL && strcmp(lp,label)==0 )
      break;
  }    
  assert( cp!=NULL && cp->cid==kPairDtId && cp->tid==kStructDtId);

  return cmDataPairValue(cp);
}

cmData_t*       cmDataRecdKey(      cmData_t* p, unsigned index )
{
  assert( p->cid == kRecordDtId && p->tid==kStructDtId );
  cmData_t* cp =  cmDataChild(p,index);
  assert( cp->cid == kPairDtId && cp->tid==kStructDtId);
  return cmDataPairKey(cp);
}

unsigned        cmDataRecdKeyId(    cmData_t* p, unsigned index )
{
  cmData_t* kp = cmDataRecdKey(p,index);
  unsigned id = cmInvalidId;
  cmDataGetUInt(kp,&id);
  return id;
}

const cmChar_t* cmDataRecdKeyLabel( cmData_t* p, unsigned index )
{
  cmData_t* kp = cmDataRecdKey(p,index);
  const cmChar_t* label = NULL;
  cmDataConstStr(kp,&label);
  return label;
}
  
cmData_t*       cmDataRecdMake( cmData_t* parent, cmData_t* p )
{
  cmDataUnlink(p);
  _cmDataFreeData(p);
  p->parent  = parent;
  p->cid     = kRecordDtId;
  p->tid     = kStructDtId;
  p->flags   = cmIsFlag(p->flags,kFreeObjDtFl) ? kFreeObjDtFl : 0;
  p->u.child = NULL;
  return p;
}

cmData_t*       cmDataRecdAlloc(cmData_t* parent)
{ return _cmDataNew(parent,kRecordDtId,kStructDtId); }

cmData_t* cmDataRecdAppendPair( cmData_t* p, cmData_t* pair )
{
  assert( p!=NULL && p->cid==kRecordDtId && p->tid==kStructDtId);
  cmDataAppendChild(p,pair);
  return p;
}


cmDtRC_t  _cmDataRecdParseInputV(cmData_t* parent, cmErr_t* err, cmRC_t errRC, unsigned idFl, va_list vl )
{
  assert( parent != NULL && parent->cid == kRecordDtId && parent->tid == kStructDtId );
  bool      contFl = true;
  cmDtRC_t  rc     = kOkDtRC;

  // for each record field
  while( contFl )
  {  
    cmData_t*       vp    = NULL;
    unsigned        id    = cmInvalidId;
    const cmChar_t* label = NULL;

    // parse the field idenfier
    if( idFl )
      id    = va_arg(vl,unsigned);     // numeric field identifier
    else
      label = va_arg(vl,const char*);  // text field label identifier
    
    // validate the field identifier
    if( (idFl && id==cmInvalidId) || (!idFl && label==NULL) )
      break;

    // parse the field data
    if((rc =_cmDataParseArgV( NULL, err, errRC, vl, &vp )) != kOkDtRC )
    {
      contFl = false;
    }
    else
    {
      // create the field pair
      if( idFl )
        rc = cmDataPairAllocId(parent,id,vp,NULL);
      else
        rc = cmDataPairAllocLabel(parent,label,vp,NULL);
    }
  }
  return rc;
}

cmDtRC_t cmDataRecdAllocLabelV( cmData_t* parent, cmErr_t* err, cmRC_t errRC, cmData_t** ref, va_list vl )
{
  cmData_t* p = cmDataRecdAlloc(parent);
  if( ref != NULL )
    *ref = NULL;

  cmDtRC_t rc = _cmDataRecdParseInputV(p, err, errRC, false, vl );
  if( rc != kOkDtRC )
  {
    cmDataFree(p);
    p = NULL;
  }

  if( ref != NULL )
    *ref = p;

  return rc;
}

cmDtRC_t cmDataRecdAllocLabelA( cmData_t* parent, cmErr_t* err, cmRC_t errRC, cmData_t** ref, ... )
{
  va_list vl;
  va_start(vl,ref);
  cmDtRC_t rc = cmDataRecdAllocLabelV(parent,err,errRC,ref,vl);
  va_end(vl);
  return rc;
}

cmDtRC_t cmDataRecdAllocIdV( cmData_t* parent, cmErr_t* err, cmRC_t errRC, cmData_t** ref, va_list vl )
{
  cmData_t* p = cmDataRecdAlloc(parent);
  if( ref != NULL )
    *ref = NULL;

  cmDtRC_t rc = _cmDataRecdParseInputV(p, err, errRC, true, vl );
  if( rc != kOkDtRC )
  {
    cmDataFree(p);
    p = NULL;
  }

  if( ref != NULL )
    *ref = p;

  return rc;
}

cmDtRC_t cmDataRecdAllocIdA( cmData_t* parent, cmErr_t* err, cmRC_t errRC, cmData_t** ref, ... )
{
  va_list vl;
  va_start(vl,ref);
  cmRC_t rc = cmDataRecdAllocIdV(parent,err,errRC,ref,vl);
  va_end(vl);
  return rc;
}

cmDtRC_t _cmDataRecdParseErrV( cmErr_t* err, unsigned rc, unsigned id, const cmChar_t* label, const cmChar_t* fmt, va_list vl )
{
  if( err != NULL )
  {
    cmChar_t* s0 = NULL;
    cmChar_t* s1 = NULL;

    if( label == NULL )
      s0 = cmTsPrintfP(s0,"Error parsing field id=%i.",id);
    else
      s0 = cmTsPrintfP(s0,"Error parsing field '%s'.",label);

    s1 = cmTsVPrintfP(s1,fmt,vl);

    cmErrMsg(err,rc,"%s %s",s0,s1);

    cmMemFree(s0);
    cmMemFree(s1);
      
  }
  return rc;
}

cmDtRC_t _cmDataRecdParseErr( cmErr_t* err, unsigned rc, unsigned id, const cmChar_t* label, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  rc = _cmDataRecdParseErrV(err,rc,id,label,fmt,vl);
  va_end(vl);
  return rc;
}

cmDtRC_t _cmDataRecdParseV(cmData_t* p, bool idFl, cmErr_t* err, unsigned errRC, va_list vl )
{
  bool      contFl = true;
  cmDtRC_t  rc     = kOkDtRC;
  
  while( contFl && rc==kOkDtRC)
  {
    unsigned    id = cmInvalidId;
    const char* label = NULL;

    // parse the field idenfier
    if( idFl )
      id    = va_arg(vl,unsigned);     // numeric field identifier
    else
      label = va_arg(vl,const char*);  // text field label identifier
   
    // validate the field identifier
    if( (idFl && id==cmInvalidId) || (!idFl && label==NULL) )
      break;

    cmDataContainerId_t cid    = va_arg(vl,unsigned);
    cmDataTypeId_t      typeId = va_arg(vl,unsigned);
    void*               v      = va_arg(vl,void*);
    cmData_t*           np     = NULL;
    bool                optFl  = cmIsFlag(typeId,kOptArgDtFl);
    cmDtRC_t            rc0    = kOkDtRC;
    unsigned*           cntPtr = NULL;

    // arrays have an additional arg - the element count pointer
    if( cid == kArrayDtId )
      cntPtr = va_arg(vl,unsigned*);

    // clear the optArgFl from the type id
    typeId = cmClrFlag(typeId,kOptArgDtFl);

    // attempt to locate the field
    if( idFl )
      np = cmDataRecdValueFromLabel( p, label );
    else
      np = cmDataRecdValueFromId( p, id );

    // if the field was not found
    if( np == NULL )
    {
      if(optFl)
        continue;

      _cmDataRecdParseErr(err, errRC, id, label, "The required field was missing." );
      rc = kMissingFieldDtRC;
      continue;
    }

    // validate the container type
    if( cid != np->cid )
    {
      _cmDataRecdParseErr(err, errRC, id, label, "The container for the field is a '%s' but the arg. list indicated it should be a '%s'.",cmStringNullGuard(cmDataContainerIdToLabel(np->cid)),cmStringNullGuard(cmDataContainerIdToLabel(cid)));
      rc = kInvalidContDtRC;
      continue;
    }


    switch(cid)
    {
      case kScalarDtId:
        
        switch(typeId)
        {
          case kNullDtId:
            break;

          case kBoolDtId:
            rc0 = cmDataGetBool(np,(bool*)v);
            break;

          case kUCharDtId: 
            rc0 = cmDataGetUChar(np,(unsigned char*)v);
            break;

          case kCharDtId:
            rc0 = cmDataGetChar(np,(char*)v);
            break;

          case kUShortDtId:
            rc0 = cmDataGetUShort(np,(unsigned short*)v);
            break;

          case kShortDtId:
            rc0 = cmDataGetShort(np,(short*)v);
            break;

          case kUIntDtId:
            rc0 = cmDataGetUInt(np,(unsigned int*)v);        
            break;

          case kIntDtId:
            rc0 = cmDataGetInt(np,(int*)v);
            break;

          case kULongDtId:
            rc0 = cmDataGetULong(np,(unsigned long*)v);
            break;

          case kLongDtId:
            rc0 = cmDataGetLong(np,(long*)v);
            break;

          case kFloatDtId:
            rc0 = cmDataGetFloat(np,(float*)v);
            break;

          case kDoubleDtId:
            rc0 = cmDataGetDouble(np,(double*)v);
            break;

          case kStrDtId:
            rc0 = cmDataStr(np,(char**)v);
            break;
            
          default:
            _cmDataRecdParseErr(err, errRC, id, label, "An invalid data type id=%i was encountered.",typeId);
            rc = kInvalidTypeDtRC;
        }

        if( rc0 != kOkDtRC)
        {
          _cmDataRecdParseErr(err, errRC, id, label,"Unable to convert the field value to the requested type:'%s'.",cmStringNullGuard(cmDataTypeToLabel(typeId)));

          rc = kCvtErrDtRC;
        }

        break;

      case kArrayDtId:
        {

          switch(typeId)
          {
            case kNullDtId:
              break;

            case kBoolDtId:
            case kUCharDtId: 
            case kCharDtId:
            case kUShortDtId:
            case kShortDtId:
            case kUIntDtId:
            case kIntDtId:
            case kULongDtId:
            case kLongDtId:
            case kFloatDtId:
            case kDoubleDtId:
              *(void**)v = np->u.vp;
              *cntPtr =  cmDataArrayEleCount(np);
              break;

            default:
              _cmDataRecdParseErr(err, errRC, id, label,"Invalid array type identifier (%s).", cmStringNullGuard(cmDataTypeToLabel(typeId)));
              rc = kInvalidTypeDtRC;
          }
        }
        break;

      case kListDtId:
      case kPairDtId:
      case kRecordDtId:
        *(cmData_t**)v = np;
        break;

      default:
        {
          _cmDataRecdParseErr(err, errRC, id, label,"An invalid cotainer id=%i was encountered.",cid);
          rc = kInvalidContDtRC;
        }
    }

  }  
  return rc;
}

cmDtRC_t cmDataRecdParseLabelV(cmData_t* p, cmErr_t* err, unsigned errRC, va_list vl )
{ return _cmDataRecdParseV(p,false,err,errRC,vl); }

cmDtRC_t cmDataRecdParseLabel(cmData_t* p, cmErr_t* err, unsigned errRC, ... )
{
  va_list vl;
  va_start(vl,errRC);
  cmDtRC_t rc = cmDataRecdParseLabelV(p,err,errRC,vl);
  va_end(vl);
  return rc;
}

cmDtRC_t cmDataRecdParseIdV(cmData_t* p, cmErr_t* err, unsigned errRC, va_list vl )
{ return _cmDataRecdParseV(p,true,err,errRC,vl); }

cmDtRC_t cmDataRecdParseId(cmData_t* p, cmErr_t* err, unsigned errRC, ... )
{
  va_list vl;
  va_start(vl,errRC);
  cmDtRC_t rc = cmDataRecdParseIdV(p,err,errRC,vl);
  va_end(vl);
  return rc;
}



//============================================================================
//============================================================================
//============================================================================
#ifdef NOT_DEF
unsigned _cmDataSerializeNodeByteCount( const cmData_t* p )
{
  unsigned n = 0;

  // all serialized data ele's begin with a cmDataFmtId_t
  n += sizeof(cmDataFmtId_t);  

  // arrays then have a count of bytes and structures have a child count
  if( cmDataIsPtr(p) || cmDataIsStruct(p) )
    n += sizeof(unsigned);

  // then the data itself takes a variable number of bytes
  n += _cmDataByteCount(p);

  return n;
}

unsigned cmDataSerializeByteCount( const cmData_t* p )
{
  unsigned bn = 0;

  // if this data type has a child then calculate it's size
  if( kMinStructDtId <= p->tid && p->tid <= kMaxStructDtId && p->u.child != NULL )
    bn = cmDataSerializeByteCount(p->u.child);
  
  // if this data type has siblings get their type
  cmData_t* dp = p->u.child;
  for(; dp != NULL; dp=dp->sibling )
    bn += cmDataSerializeByteCount(dp->sibling);
    

  // 
  return  bn + _cmDataSerializeNodeByteCount(p);
}

char* _cmDataSerializeWriteArray( cmData_t* np, char* dp, const char* ep )
{
  unsigned byteCnt = _cmDataByteCount(np);

  *((unsigned*)dp) = byteCnt;
  dp += sizeof(unsigned);

  memcpy(dp,np->u.vp,byteCnt);
  dp += byteCnt;

  return dp;
}

char* _cmDataSerializeWriteStruct( cmData_t* np, char* dp, const char* ep )
{
  *((unsigned*)dp) = cmDataChildCount(np);
  dp += sizeof(unsigned);

  return dp;
}


char* _cmDataSerializeWrite( cmData_t* np, char* dp, const char* ep )
{
  assert( dp + _cmDataSerializeNodeByteCount(np) <= ep );

  *((cmDataFmtId_t*)dp) = np->tid;
  dp += sizeof(cmDataFmtId_t);

  switch( np->tid )
  {
    case kNullDtId:   break;
    case kBoolDtId:   *((bool*)dp)           = cmDataBool(np);   dp+=sizeof(bool);           break;
    case kUCharDtId:  *((unsigned char*)dp)  = cmDataUChar(np);  dp+=sizeof(unsigned char);  break;
    case kCharDtId:   *((char*)dp)           = cmDataChar(np);   dp+=sizeof(char);           break;
    case kUShortDtId: *((unsigned short*)dp) = cmDataUShort(np); dp+=sizeof(unsigned short); break;
    case kShortDtId:  *((short*)dp)          = cmDataShort(np);  dp+=sizeof(short);          break;
    case kUIntDtId:   *((unsigned int*)dp)   = cmDataUInt(np);   dp+=sizeof(unsigned int);   break;
    case kIntDtId:    *((int*)dp)            = cmDataInt(np);    dp+=sizeof(int);            break;
    case kULongDtId:  *((unsigned long*)dp)  = cmDataULong(np);  dp+=sizeof(unsigned long);  break;
    case kLongDtId:   *((long*)dp)           = cmDataLong(np);   dp+=sizeof(long);           break;
    case kFloatDtId:  *((float*)dp)          = cmDataFloat(np);  dp+=sizeof(float);          break;
    case kDoubleDtId: *((double*)dp)         = cmDataDouble(np); dp+=sizeof(double);         break;
    case kStrDtId:       dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kConstStrDtId:  dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kUCharPtrDtId:  dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kCharPtrDtId:   dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kUShortPtrDtId: dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kShortPtrDtId:  dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kUIntPtrDtId:   dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kIntPtrDtId:    dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kULongPtrDtId:  dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kLongPtrDtId:   dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kFloatPtrDtId:  dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kDoublePtrDtId: dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kVoidPtrDtId:   dp = _cmDataSerializeWriteArray(np,dp,ep); break;
    case kListDtId:      dp = _cmDataSerializeWriteStruct(np,dp,ep); break;
    case kPairDtId:      dp = _cmDataSerializeWriteStruct(np,dp,ep); break;
    case kRecordDtId:    dp = _cmDataSerializeWriteStruct(np,dp,ep); break;
    default:
      { assert(0); }
  }
  return dp;
}

char* _cmDataSerialize( const cmData_t* p, char* buf, const char* ep )
{
  /*
  buf = _cmDataSerializeWrite(p,buf,ep);

  // if this data type has a child then write the child
  if( kMinStructDtId <= p->tid && p->tid <= kMaxStructDtId && p->u.child != NULL )
    buf = _cmDataSerialize(p->u.child,buf,ep);
  
  // if this data type has siblings then write sibings
  cmData_t* dp = p->u.child;
  for(; dp != NULL; dp=dp->sibling )
    buf = cmDataSerialize(dp->sibling,buf,ep);

  return buf;
  */
  return NULL;
}

cmDtRC_t cmDataSerialize( const cmData_t* p, void* buf, unsigned bufByteCnt )
{
  /*
  const char* ep = (char*)p + bufByteCnt;
  buf = _cmDataSerialize(p,buf,bufByteCnt);
  assert( buf <= ep );
  */
  return kOkDtRC;
}

cmDtRC_t cmDataDeserialize( const void* buf, unsigned bufByteCnt, cmData_t** pp )
{
  return kOkDtRC;
}
#endif

//============================================================================
//============================================================================
//============================================================================
enum
{
  kLCurlyLexTId = kUserLexTId + 1,
  kRCurlyLexTId,
  kLParenLexTId,
  kRParenLexTId,
  kLBrackLexTId,
  kRBrackLexTId,
  kColonLexTId,
  kCommaLexTId,
};

typedef struct
{
  unsigned        id;
  const cmChar_t* label;
} cmDtToken_t;

cmDtToken_t _cmDtTokenArray[] = 
{
  { kLCurlyLexTId, "{" },
  { kRCurlyLexTId, "}" },
  { kLBrackLexTId,  "[" },
  { kRBrackLexTId,  "]" },
  { kLParenLexTId,  "(" },
  { kRParenLexTId,  ")" },
  { kColonLexTId,  ":" },
  { kCommaLexTId,  "," },
  { kErrorLexTId,""}
  
};

typedef struct
{
  cmErr_t    err;
  cmLexH    lexH;
  cmStackH_t stH;

} cmDataParser_t;


cmDataParserH_t cmDataParserNullHandle = cmSTATIC_NULL_HANDLE;

cmDataParser_t* _cmDataParserHandleToPtr( cmDataParserH_t h )
{
  cmDataParser_t* p = (cmDataParser_t*)h.h;
  assert( p!= NULL );
  return p;
}

cmDtRC_t _cmDataParserDestroy( cmDataParser_t* p )
{
  if( cmLexFinal(&p->lexH) != kOkLexRC )
    cmErrMsg(&p->err,kLexFailDtRC,"Lexer release failed.");

  if( cmStackFree(&p->stH) != kOkStRC )
    cmErrMsg(&p->err,kParseStackFailDtRC,"The data object parser stack release failed.");

  cmMemFree(p);

  return kOkDtRC;
}

cmDtRC_t cmDataParserCreate( cmCtx_t* ctx, cmDataParserH_t* hp )
{
  cmDtRC_t rc;
  unsigned i;

  if((rc = cmDataParserDestroy(hp)) != kOkDtRC )
    return rc;

  cmDataParser_t* p = cmMemAllocZ(cmDataParser_t,1);

  cmErrSetup(&p->err,&ctx->rpt,"Data Parser");

  if(cmLexIsValid(p->lexH = cmLexInit(NULL,0,0,&ctx->rpt))==false)
  {
    rc = cmErrMsg(&p->err, kLexFailDtRC, "The data object parser lexer create failed.");
    goto errLabel;
  }

  for(i=0; _cmDtTokenArray[i].id != kErrorLexTId; ++i)
    if( cmLexRegisterToken(p->lexH, _cmDtTokenArray[i].id, _cmDtTokenArray[i].label) != kOkLexRC )
    {
      rc = cmErrMsg(&p->err,kLexFailDtRC,"The data object parser lexer could not register the '%s' token.",_cmDtTokenArray[i].label);
      goto errLabel;
    }

  if( cmStackAlloc(ctx, &p->stH, 1024, 1024, sizeof(cmData_t*)) != kOkStRC )
  {
    rc = cmErrMsg(&p->err,kParseStackFailDtRC,"The data object parser stack create failed.");
    goto errLabel;
  }

  hp->h = p;

 errLabel:
  if( rc != kOkDtRC )
    _cmDataParserDestroy(p);

  return kOkDtRC;
}

cmDtRC_t cmDataParserDestroy( cmDataParserH_t* hp )
{
  cmDtRC_t rc=kOkDtRC;

  if( hp==NULL || cmDataParserIsValid(*hp)==false )
    return rc;

  cmDataParser_t* p = _cmDataParserHandleToPtr(*hp);

  if((rc = _cmDataParserDestroy(p)) != kOkDtRC )
    return rc;

  hp->h = NULL;

  return kOkDtRC;
}


bool     cmDataParserIsValid( cmDataParserH_t h )
{ return h.h != NULL; }



// {
//  id0 : scalar_value
//  id1 : ( heterogenous, array, value )
//  id2 : [ homogeneous array values ]
//  id3 : 
// }


// flags describing the expected next token
enum
{
  kValueExpFl = 0x01,
  kIdExpFl    = 0x02,
  kColonExpFl = 0x04,
  kCommaExpFl = 0x08
};

typedef struct
{
  cmData_t* dp;
} cmDataStEle_t;

typedef struct
{
  cmDataParser_t*  p;
  cmData_t*        cnp;
  unsigned         flags;
  cmChar_t*        tmpStr;

  unsigned         arrayCnt;
  void*            arrayMem;
  
} cmDataParserCtx_t;

cmDtRC_t _cmDpSyntaxErrV( cmDataParserCtx_t* c, const cmChar_t* fmt, va_list vl )
{
  cmChar_t* s0 = NULL;
  cmChar_t* s1 = NULL;
  s0 = cmTsVPrintfP(s0,fmt,vl);
  s1 = cmMemAllocStrN(cmLexTokenText(c->p->lexH),cmLexTokenCharCount(c->p->lexH));
  cmDtRC_t rc = cmErrMsg(&c->p->err,kSyntaxErrDtRC,"Syntax error on line %i column:%i token:'%s'. %s",cmLexCurrentLineNumber(c->p->lexH),cmLexCurrentColumnNumber(c->p->lexH),s1,cmStringNullGuard(s0));
  cmMemFree(s0);
  cmMemFree(s1);
  return rc;
}

cmDtRC_t _cmDpSyntaxErr( cmDataParserCtx_t* c, const cmChar_t* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  cmDtRC_t rc = _cmDpSyntaxErrV(c,fmt,vl);
  va_end(vl);
  return rc;
}

cmDtRC_t _cmDpPopStack(  cmDataParserCtx_t* c, cmData_t** pp )
{
  const void* vp;
  if((vp = cmStackTop(c->p->stH)) == NULL )
    return _cmDpSyntaxErr(c,"Stack underflow.");
  
  if( cmStackPop(c->p->stH,1) != kOkStRC )
    return _cmDpSyntaxErr(c,"Stack pop failed.");

  *pp = *(cmData_t**)vp;

  //printf("pop: %p\n",*pp);

  return kOkDtRC;
}

cmDtRC_t _cmDpPushStack( cmDataParserCtx_t* c, cmData_t* np )
{
  //printf("push:%p\n",np);

  // store the current node
  if( cmStackPush(c->p->stH, &np, 1 ) != kOkStRC )
     return _cmDpSyntaxErr(c,"Parser stack push failed.");
 
  return kOkDtRC;
}

cmDtRC_t _cmDpStoreArrayEle( cmDataParserCtx_t* c, void* dp, unsigned eleByteCnt, cmDataTypeId_t tid )
{
  if( c->cnp->tid == kBlobDtId )
    c->cnp->tid = tid;
  else
    if( c->cnp->tid != tid )
      return _cmDpSyntaxErr(c,"Mixed types were detected in an array list.");

  unsigned newByteCnt = (c->cnp->cnt+1)*eleByteCnt;
  char* vp = cmMemResizeP(char, c->cnp->u.vp, newByteCnt);
  
  memcpy(vp + c->cnp->cnt*eleByteCnt,dp,eleByteCnt);
  c->cnp->u.vp = vp;
  c->cnp->cnt += 1;
  c->cnp->cid  = kArrayDtId;

  c->flags = kValueExpFl | kCommaExpFl;

  return kOkDtRC;
}

cmDtRC_t _cmDataParserOpenPair( cmDataParserCtx_t* c )
{
  cmDtRC_t rc = kOkDtRC;

  assert( c->cnp->cid==kRecordDtId && c->cnp->tid == kStructDtId );

  // create a pair with a 'null' value which will be replaced when the pair's value is parsed
  cmData_t* nnp = NULL;

  if( cmDataNewNull(NULL,0,&nnp) != kOkDtRC )
    return rc;

  cmData_t* pnp = NULL;

  if((rc = cmDataPairAllocLabelN( c->cnp, cmLexTokenText(c->p->lexH), cmLexTokenCharCount(c->p->lexH), nnp, &pnp )) != kOkDtRC )
  {
    cmDataFree(nnp);
    return rc;
  }

  // store the current node
  if((rc = _cmDpPushStack(c,c->cnp)) != kOkDtRC )
    return rc;

  // make the new pair the current node
  c->cnp = pnp;

  // pair openings must be followed by a colon.
  c->flags  = kColonExpFl;
 
  return rc;
}

cmDtRC_t _cmDataParserClosePair( cmDataParserCtx_t* c )
{
  cmDtRC_t rc;

  // make the pair's parent record the current node
  if((rc = _cmDpPopStack(c, &c->cnp )) != kOkDtRC )
    return rc;

  // pairs only occur in records
  if( c->cnp->cid != kRecordDtId )
    return _cmDpSyntaxErr(c,"A 'pair' end was found outside of a 'record'.");

  // pairs must be followed by id's or comma's
  c->flags = kIdExpFl | kCommaExpFl; 

  return rc;
}

cmDtRC_t _cmDpStoreValue( cmDataParserCtx_t* c, cmData_t* np, const cmChar_t* typeLabel )
{
  assert( np != NULL );

  cmDtRC_t rc = kOkDtRC;

  switch( c->cnp->cid )
  {
    case kPairDtId:

      // assign the new node as the value of the pair
      cmDataPairSetValue(c->cnp,np);

      // close the values parent pair
      rc = _cmDataParserClosePair(c);
      break;

    case kListDtId:      
      cmDataAppendChild(c->cnp,np);
      c->flags = kValueExpFl;
      break;

    default:
      rc = _cmDpSyntaxErr(c,"A '%s' value was found outside of a valid container.",typeLabel);

      // Free the new data node because it was not attached and will 
      // otherwise be lost
      cmDataFree(np);
      
  }

  c->flags |= kCommaExpFl;

  return rc;
}

cmDtRC_t _cmDataParserReal( cmDataParserCtx_t* c )
{
  cmDtRC_t rc      = kOkDtRC;
  bool     floatFl = cmLexTokenIsSinglePrecision(c->p->lexH);
  double   dval;
  float    fval;

  if( floatFl )
    fval = cmLexTokenFloat(c->p->lexH);
  else
    dval = cmLexTokenDouble(c->p->lexH);

  
  if( c->cnp->cid == kArrayDtId )
  {
    if( floatFl )
      rc = _cmDpStoreArrayEle(c,&fval,sizeof(fval),kFloatDtId);
    else
      rc = _cmDpStoreArrayEle(c,&dval,sizeof(dval),kDoubleDtId);
  }
  else
  {
    cmData_t* np = NULL;
    if( floatFl )
      rc = cmDataNewFloat(NULL,0,fval,&np);
    else
      rc = cmDataNewDouble(NULL,0,dval,&np);

    if( rc == kOkDtRC )
      rc = _cmDpStoreValue(c,np,"real");
  }
  return rc;
}

cmDtRC_t _cmDataParserInt( cmDataParserCtx_t* c )
{
  cmDtRC_t rc         = kOkDtRC;
  int      val        = cmLexTokenInt(c->p->lexH);
  bool     unsignedFl = cmLexTokenIsUnsigned(c->p->lexH);

  if( c->cnp->cid == kArrayDtId )
    rc = _cmDpStoreArrayEle(c,&val,sizeof(val),unsignedFl ? kUIntDtId : kIntDtId);
  else
  {
    cmData_t* np = NULL;
    if(unsignedFl)
      rc = cmDataNewUInt(NULL,0,val,&np);
    else
      rc = cmDataNewInt(NULL,0,val,&np);

    if( rc == kOkDtRC )
      rc = _cmDpStoreValue(c,np,"int");
  }

  return rc;
}

cmDtRC_t _cmDataParserString( cmDataParserCtx_t* c )
{
  cmDtRC_t rc = kOkDtRC;

  // if we are expecting a pair label
  if( cmIsFlag(c->flags,kIdExpFl) )
    return _cmDataParserOpenPair(c);
    
  // otherwise a 'value' must be expected
  if( cmIsNotFlag(c->flags,kValueExpFl) )
    return _cmDpSyntaxErr(c,"Unexpected string.");

  cmData_t* np = NULL;

  if((rc = cmDataNewConstStrN(NULL,0,cmLexTokenText(c->p->lexH), cmLexTokenCharCount(c->p->lexH),&np)) != kOkDtRC )
    return rc;

  return _cmDpStoreValue(c,np,"string");
}

cmDtRC_t _cmDataParserOpenRecd( cmDataParserCtx_t* c )
{
  cmDtRC_t rc = kOkDtRC;

  // records are values - so we must be expecting a value
  if( cmIsFlag(c->flags,kValueExpFl) == false )
    return _cmDpSyntaxErr(c,"Unexpected '{'.");

  // store the current node
  if((rc = _cmDpPushStack(c,c->cnp)) != kOkDtRC )
    return rc;

  // alloc a new record and make it the current node
  if( (c->cnp = cmDataRecdAlloc(NULL)) == NULL )
    return _cmDpSyntaxErr(c,"'recd' allocate failed.");

  // new records must be followed by an id token.
  c->flags = kIdExpFl;

  return rc;
}

cmDtRC_t _cmDataParserCloseContainer( cmDataParserCtx_t* c, const cmChar_t* typeLabelStr )
{
  cmDtRC_t rc;

  cmData_t* np = c->cnp;

  // make the parent node the new current node
  if((rc = _cmDpPopStack(c,&c->cnp)) != kOkDtRC )
    return rc;

  return _cmDpStoreValue(c,np,typeLabelStr);
}

cmDtRC_t _cmDataParserCloseRecd( cmDataParserCtx_t* c )
{
  assert( c->cnp->cid == kRecordDtId );

  return _cmDataParserCloseContainer(c,"record");
}

cmDtRC_t _cmDataParserOpenList( cmDataParserCtx_t* c )
{
  cmDtRC_t rc = kOkDtRC;

  // lists are values - so we must be expecting a value
  if( cmIsFlag(c->flags,kValueExpFl) == false )
    return _cmDpSyntaxErr(c,"Unexpected '('.");

  // store the current node
  if((rc = _cmDpPushStack(c,c->cnp)) != kOkDtRC )
    return rc;

  // create a new list
  if( (c->cnp = cmDataListAlloc(NULL)) == NULL )
    return _cmDpSyntaxErr(c,"'list' allocate failed.");

  // new lists must be followed by a value
  c->flags = kValueExpFl;

  return rc;
}

cmDtRC_t _cmDataParserCloseList( cmDataParserCtx_t* c )
{
  assert( c->cnp->cid == kListDtId );
  return _cmDataParserCloseContainer(c,"list");
}

cmDtRC_t _cmDataParserOpenArray( cmDataParserCtx_t* c )
{
  cmDtRC_t rc = kOkDtRC;

  // arrays are values - so we must be expecting a value
  if( cmIsFlag(c->flags,kValueExpFl) == false )
    return _cmDpSyntaxErr(c,"Unexpected '['.");

  // store the current node
  if((rc = _cmDpPushStack(c,c->cnp)) != kOkDtRC )
    return rc;

  // create a new array
  c->cnp = NULL;
  if( cmDataNewBlob(NULL, 0, NULL, 0,&c->cnp) != kOkDtRC )
    return _cmDpSyntaxErr(c,"'array' allocate failed.");

  // new arrays must be followed by a value
  c->flags     = kValueExpFl;

  // Idicate that the object is an array by setting c->cnp->cid to  kArrayDtId.
  // (ALTHOUGH the c->cnp->tid==kBlobDtId which is only legal for scalar containers -
  //  but this will be fixed up when the first element is stored in
  //  _cmDpStoreArrayEle(). )
  c->cnp->cid  = kArrayDtId; 
  return rc;

}

cmDtRC_t _cmDataParserCloseArray( cmDataParserCtx_t* c )
{
  assert( c->cnp->cid == kArrayDtId );

  return _cmDataParserCloseContainer(c,"array");
}

cmDtRC_t _cmDataParserOnColon( cmDataParserCtx_t* c )
{
  // colons only follow field identifiers and are always followed by values.
  if( cmIsFlag(c->flags,kColonExpFl) == false )
    return _cmDpSyntaxErr(c,"Unexpected colon.");

  c->flags = kValueExpFl;

  return kOkDtRC;
}

cmDtRC_t _cmDataParserOnComma( cmDataParserCtx_t* c )
{
  // comma's may be found in three places:
  // 1) following field values
  // 2) between list values
  // 3) between  array values
  // comma's are always followed by values
  if( cmIsFlag(c->flags,kCommaExpFl) == false )
    return  _cmDpSyntaxErr(c, "Unexpected comma.");

  c->flags = kValueExpFl;

  return kOkDtRC;      
}


cmDtRC_t cmDataParserExec( cmDataParserH_t h, const cmChar_t* text, cmData_t** pp )
{
  cmDtRC_t          rc = kOkDtRC;
  cmDataParser_t*   p  = _cmDataParserHandleToPtr(h);
  unsigned          tokenId;
  cmDataParserCtx_t ctx;
  cmData_t*         root = cmDataRecdAlloc(NULL);
  ctx.cnp   = root;
  ctx.p     = p;
  ctx.flags = kIdExpFl;

  if( cmLexSetTextBuffer(p->lexH,text,strlen(text)) != kOkLexRC )
    return cmErrMsg(&p->err,kLexFailDtRC,"The data object lexer failed during reset.");

  cmStackClear(p->stH,false);

  while(rc==kOkDtRC && (tokenId = cmLexGetNextToken(p->lexH)) != kEofLexTId )
  {
    switch(tokenId)
    {
      case kRealLexTId:     // real number (contains a decimal point or is in scientific notation) 
        rc = _cmDataParserReal(&ctx);
        break;

      case kIntLexTId:      // decimal integer
      case kHexLexTId:      // hexidecimal integer
        rc = _cmDataParserInt(&ctx);
        break;

      case kIdentLexTId:    // identifiers are treated as strings
      case kQStrLexTId:     // quoted string
        rc = _cmDataParserString(&ctx);
        break;

      case kLCurlyLexTId:    // a new record is starting
        rc = _cmDataParserOpenRecd(&ctx);
        break;

      case kRCurlyLexTId:   // the current record is finished
        rc = _cmDataParserCloseRecd(&ctx);
        break;

      case kLParenLexTId:   // a list is starting
        rc = _cmDataParserOpenList(&ctx);
        break;

      case kRParenLexTId:   // a list is finished
        rc = _cmDataParserCloseList(&ctx);
        break;

      case kLBrackLexTId:   // an  array is starting
        rc = _cmDataParserOpenArray(&ctx);
        break;

      case kRBrackLexTId:   // an array is ending
        rc = _cmDataParserCloseArray(&ctx);
        break;

      case kColonLexTId:    // the previous id was a field id 
        rc = _cmDataParserOnColon(&ctx);
        break;

      case kCommaLexTId:    // comma sep. for array or fields
        rc = _cmDataParserOnComma(&ctx);
        break;

      case kBlockCmtLexTId: // block comment
      case kLineCmtLexTId:  // line comment
      case kErrorLexTId:    // the lexer was unable to identify the current token
      case kUnknownLexTId:  // the token is of an unknown type (only used when kReturnUnknownLexFl is set)
      case kEofLexTId:      // the lexer reached the end of input
      case kSpaceLexTId:    // white space
        {
          rc = cmErrMsg(&p->err,kLexFailDtRC,"The data object lexer failed with an unexpected token '%s' on line '%i'.",cmLexIdToLabel(p->lexH,tokenId),cmLexCurrentLineNumber(p->lexH));
          goto errLabel;
        }
    }
  }

 errLabel:

  if( rc == kOkDtRC )
    *pp = ctx.cnp;
  else
  {
    if( ctx.cnp != root )
      cmDataUnlinkAndFree(ctx.cnp);

    cmDataUnlinkAndFree(root);
  }
  return rc;
}

//============================================================================
//============================================================================
//============================================================================

#define parr(rpt,fmt,arr,n) do{int i=0; cmRptPrintf(rpt,"[ "); for(;i<n;++i) cmRptPrintf(rpt,fmt,arr[i]); cmRptPrintf(rpt," ]"); }while(0) 

void _cmDataPrintIndent( cmRpt_t* rpt, unsigned indent )
{
  unsigned j=0;
  for(; j<indent; ++j)
    cmRptPrintf(rpt," ");
}

void     _cmDataPrint( const cmData_t* p, cmRpt_t* rpt, unsigned indent )
{
  cmData_t* cp;

  //_cmDataPrintIndent(rpt,indent);

  switch(p->cid)
  {
    case kScalarDtId:
      switch(p->tid)
      {
        case kNullDtId:       cmRptPrintf(rpt,"<null>"); break;
        case kBoolDtId:       cmRptPrintf(rpt,"%i", cmDtBool(p));   break;
        case kUCharDtId:      cmRptPrintf(rpt,"%c ",cmDtUChar(p));  break;
        case kCharDtId:       cmRptPrintf(rpt,"%c ",cmDtChar(p));   break;
        case kUShortDtId:     cmRptPrintf(rpt,"%i ",cmDtUShort(p)); break;
        case kShortDtId:      cmRptPrintf(rpt,"%i ",cmDtShort(p));  break;
        case kUIntDtId:       cmRptPrintf(rpt,"%i ",cmDtUInt(p));   break;
        case kIntDtId:        cmRptPrintf(rpt,"%i ",cmDtInt(p));    break;
        case kULongDtId:      cmRptPrintf(rpt,"%i ",cmDtULong(p));  break;
        case kLongDtId:       cmRptPrintf(rpt,"%i ",cmDtLong(p));   break;
        case kFloatDtId:      cmRptPrintf(rpt,"%f ",cmDtFloat(p));  break;
        case kDoubleDtId:     cmRptPrintf(rpt,"%f ",cmDtDouble(p)); break;
        case kStrDtId:        cmRptPrintf(rpt,"%s ",cmDtConstStr(p)); break;
        default:
          {assert(0); }
      }
      break;

    case kArrayDtId:
      {
        switch(p->tid)
        {
          case kBoolDtId:   parr(rpt,"%i ", cmDtBoolArray(p), p->cnt ); break;
          case kUCharDtId:  parr(rpt,"%c ", cmDtCharArray(p), p->cnt ); break;
          case kCharDtId:   parr(rpt,"%c ", cmDtCharArray(p), p->cnt ); break;
          case kUShortDtId: parr(rpt,"%i ", cmDtUShortArray(p), p->cnt ); break;
          case kShortDtId:  parr(rpt,"%i ", cmDtShortArray(p), p->cnt ); break; 
          case kUIntDtId:   parr(rpt,"%i ", cmDtUIntArray(p), p->cnt ); break;
          case kIntDtId:    parr(rpt,"%i ", cmDtIntArray(p), p->cnt ); break;
          case kULongDtId:  parr(rpt,"%i ", cmDtULongArray(p), p->cnt ); break;
          case kLongDtId:   parr(rpt,"%i ", cmDtLongArray(p), p->cnt ); break;
          case kFloatDtId:  parr(rpt,"%f ", cmDtFloatArray(p), p->cnt ); break;
          case kDoubleDtId: parr(rpt,"%f ", cmDtDoubleArray(p), p->cnt ); break;

          default:
            {assert(0);}
            
        }

      }
      break;

    case kPairDtId:
      _cmDataPrint(p->u.child,rpt,indent);
      cmRptPrintf(rpt," : ");
      _cmDataPrint(p->u.child->sibling,rpt,indent);
      cmRptPrintf(rpt,"\n");
      break;

    case kListDtId:
      cmRptPrintf(rpt,"(\n");
      indent += 2;
      cp = p->u.child;
      for(; cp!=NULL; cp=cp->sibling)
      {
        _cmDataPrintIndent(rpt,indent);
        _cmDataPrint(cp,rpt,indent);
        cmRptPrintf(rpt,"\n");
      }
      indent -= 2;
      _cmDataPrintIndent(rpt,indent);
      cmRptPrintf(rpt,")\n");
      break;

    case kRecordDtId:
      cmRptPrintf(rpt,"{\n");
      indent += 2;
      cp = p->u.child;
      for(; cp!=NULL; cp=cp->sibling)
      {
        _cmDataPrintIndent(rpt,indent);
        _cmDataPrint(cp,rpt, indent);        
      }
      indent -= 2;
      _cmDataPrintIndent(rpt,indent);
      cmRptPrintf(rpt,"}\n");
      break;

    default:
      break;
  }
}

void cmDataPrint( const cmData_t* p, cmRpt_t* rpt )
{ _cmDataPrint(p,rpt,0); }


cmDtRC_t cmDataParserTest( cmCtx_t* ctx )
{
  cmDtRC_t        rc = kOkDtRC;
  cmDataParserH_t h  = cmDataParserNullHandle;
  cmErr_t         err;
  cmData_t*       dp = NULL;

  const cmChar_t text[] =
  {
    //0         1         2         3
    //0123456789012345678901234567890123 
    "f0:1.23 f1:\"hey\" f2:( a b c ) f3:[ 0f 1f 2f ]"
    //"f0:1.23 f1:\"hey\""
  };

  cmErrSetup(&err,&ctx->rpt,"Data Parser Tester");

  if((rc = cmDataParserCreate(ctx, &h )) != kOkDtRC )
  {
    rc = cmErrMsg(&err,rc,"Data parser create failed.");
    goto errLabel;
  }

  if( cmDataParserExec(h,text,&dp) != kOkDtRC )
    rc = cmErrMsg(&err,rc,"Data parser exec failed.");
  else
    if( dp != NULL )
      cmDataPrint(dp,&ctx->rpt);


 errLabel:
  if( cmDataParserDestroy( &h ) != kOkDtRC )
  {
    rc = cmErrMsg(&err,rc,"Data parser destroy failed.");
    goto errLabel;
  }

  cmDataFree(dp);

  return rc;
}


void     cmDataTest( cmCtx_t* ctx )
{
  
  float farr[] = { 1.23, 45.6, 7.89 };

  if( cmDataParserTest(ctx) != kOkDtRC )
    return;

  cmData_t* d0 = NULL;

  cmDataRecdAllocLabelA(NULL,&ctx->err,0,&d0,
    "name", kStrDtId,"This is a string.",
    "id",  kUIntDtId,    21,
    "real",kFloatDtId, 1.23,
    "arr", kArrayDtId |  kFloatDtId, farr, 3,
    NULL);
  
  //cmDataPrint(d0,&ctx->rpt);
 

 cmData_t* d1 = NULL;
  cmDataListAllocA(NULL,&ctx->err,0,&d1,
    kUIntDtId, 53,
    kStrDtId, "Blah blah",
    kArrayDtId | kFloatDtId, farr, 3,
    kRecordDtId, d0, 
    kInvalidCntDtId );


  cmDataPrint(d1,&ctx->rpt);
  cmDataFree(d1);
 


  cmRptPrintf(&ctx->rpt,"Done!.\n");
}
