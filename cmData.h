#ifndef cmData_h
#define cmData_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkDtRC = cmOkRC,
    kCvtErrDtRC
  };

  enum
  {
    kInvalidDtChar   = 0xff,
    kInvalidDtUChar  = 0xff,
    kInvalidDtShort  = 0xffff,
    kInvalidDtUShort = 0xffff,
    kInvalidDtInt    = 0xffffffff,
    kInvalidDtUInt   = 0xffffffff,
    kInvalidDtLong   = 0xffffffff,
    kInvalidDtULong  = 0xffffffff,    
  };

  typedef enum
  {
    kInvalidDtId,

    kNullDtId,

    kUCharDtId,
    kCharDtId,
    kUShortDtId,
    kShortDtId,
    kUIntDtId,
    kIntDtId,
    kULongDtId,
    kLongDtId,
    kFloatDtId,
    kDoubleDtId,

    kStrDtId,
    kConstStrDtId,

    kMinPtrDtId,
    kUCharPtrDtId = kMinPtrDtId,  // cnt=array element count
    kCharPtrDtId,
    kUShortPtrDtId,
    kShortPtrDtId,
    kUIntPtrDtId,
    kIntPtrDtId,
    kULongPtrDtId,
    kLongPtrDtId,
    kFloatPtrDtId,
    kDoublePtrDtId,
    kVoidPtrDtId,
    kMaxPtrDtId = kVoidPtrDtId,

    kMinStructDtId,
    kListDtId = kMinStructDtId, // children nodes are array elements, cnt=child count
    kPairDtId,                  // key/value pairs, cnt=2, first child is key, second is value
    kRecordDtId,                // children nodes are pairs, cnt=pair count
    kMaxStructDtId

  } cmDataFmtId_t;

  enum
  {
    kDynObjDtFl = 0x01,  // object was dynamically allocated
    kDynPtrDtFl = 0x02   // ptr array was dynamically allocated
  };

  typedef struct cmData_str
  {
    cmDataFmtId_t      tid;       // data format id
    unsigned           flags;     // 
    struct cmData_str* parent;    // this childs parent
    struct cmData_str* sibling;   // this childs sibling
    unsigned           allocCnt;  // allocated count
    unsigned           cnt;       // actual count

    union
    {
      char              c;
      unsigned char    uc;
      short             s;
      unsigned short   us;
      int               i;
      unsigned int     ui;
      long              l;
      unsigned long    ul;
      float             f;
      double            d;

      cmChar_t*         z;
      const cmChar_t*  cz;

      void*             vp;

      char*             cp;
      unsigned char*   ucp;
      short*            sp;
      unsigned short*  usp;
      int*              ip;
      unsigned int*    uip;
      long*             lp;
      unsigned long*   ulp;
      float*            fp;
      double*           dp;


      struct cmData_str* child; // first child (array,record,pair)
    } u;
  
  } cmData_t;

  typedef unsigned cmDtRC_t;

  // Get the value of an object without conversion.
  // The data type id must match the return type or the
  // conversion must be an automatic C conversion.
  char            cmDataChar(      const cmData_t* p );
  unsigned char   cmDataUChar(     const cmData_t* p );
  short           cmDataShort(     const cmData_t* p );
  unsigned short  cmDataUShort(    const cmData_t* p );
  int             cmDataInt(       const cmData_t* p );
  unsigned int    cmDataUInt(      const cmData_t* p );
  long            cmDataLong(      const cmData_t* p );
  unsigned long   cmDataULong(     const cmData_t* p );
  float           cmDataFloat(     const cmData_t* p );
  double          cmDataDouble(    const cmData_t* p );
  cmChar_t*       cmDataStr(       const cmData_t* p );
  const cmChar_t* cmDataConstStr(  const cmData_t* p );
  void*           cmDataVoidPtr(   const cmData_t* p );
  char*           cmDataCharPtr(   const cmData_t* p );
  unsigned char*  cmDataUCharPtr(  const cmData_t* p );
  short*          cmDataShortPtr(  const cmData_t* p );
  unsigned short* cmDataUShortPtr( const cmData_t* p );
  int*            cmDataIntPtr(    const cmData_t* p );
  unsigned int*   cmDataUIntPtr(   const cmData_t* p );
  long*           cmDataLongPtr(   const cmData_t* p );
  unsigned long*  cmDataULongPtr(  const cmData_t* p );
  float*          cmDataFloatPtr(  const cmData_t* p );
  double*         cmDataDoublePtr( const cmData_t* p );


  // Get the value of an object with conversion.
  char            cmDataGetChar(      const cmData_t* p );
  unsigned char   cmDataGetUChar(     const cmData_t* p );
  short           cmDataGetShort(     const cmData_t* p );
  unsigned short  cmDataGetUShort(    const cmData_t* p );
  int             cmDataGetInt(       const cmData_t* p );
  unsigned int    cmDataGetUInt(      const cmData_t* p );
  long            cmDataGetLong(      const cmData_t* p );
  unsigned long   cmDataGetULong(     const cmData_t* p );
  float           cmDataGetFloat(     const cmData_t* p );
  double          cmDataGetDouble(    const cmData_t* p );
  cmChar_t*       cmDataGetStr(       const cmData_t* p );
  const cmChar_t* cmDataGetConstStr(  const cmData_t* p );
  void*           cmDataGetVoidPtr(   const cmData_t* p );
  char*           cmDataGetCharPtr(   const cmData_t* p );
  unsigned char*  cmDataGetUCharPtr(  const cmData_t* p );
  short*          cmDataGetShortPtr(  const cmData_t* p );
  unsigned short* cmDataGetUShortPtr( const cmData_t* p );
  int*            cmDataGetIntPtr(    const cmData_t* p );
  unsigned int*   cmDataGetUIntPtr(   const cmData_t* p );
  long*           cmDataGetLongPtr(   const cmData_t* p );
  unsigned long*  cmDataGetULongPtr(  const cmData_t* p );
  float*          cmDataGetFloatPtr(  const cmData_t* p );
  double*         cmDataGetDoublePtr( const cmData_t* p );


  // Set the value of an existing data object.
  cmData_t* cmDataSetChar(      cmData_t* p, char v );
  cmData_t* cmDataSetUChar(     cmData_t* p, unsigned char v );
  cmData_t* cmDataSetShort(     cmData_t* p, short v );
  cmData_t* cmDataSetUShort(    cmData_t* p, unsigned short v );
  cmData_t* cmDataSetInt(       cmData_t* p, int v );
  cmData_t* cmDataSetUInt(      cmData_t* p, unsigned int v );
  cmData_t* cmDataSetLong(      cmData_t* p, long v );
  cmData_t* cmDataSetULong(     cmData_t* p, unsigned long v );
  cmData_t* cmDataSetFloat(     cmData_t* p, float v );
  cmData_t* cmDataSetDouble(    cmData_t* p, double v );
  cmData_t* cmDataSetStr(       cmData_t* p, cmChar_t* s );
  cmData_t* cmDataSetConstStr(  cmData_t* p, const cmChar_t* s );

  // Set the value of an existing data object to an external array.
  // The array is not copied.
  cmData_t* cmDataSetVoidPtr(   cmData_t* p, void* vp,           unsigned cnt );
  cmData_t* cmDataSetCharPtr(   cmData_t* p, char* vp,           unsigned cnt );
  cmData_t* cmDataSetUCharPtr(  cmData_t* p, unsigned char* vp,  unsigned cnt );
  cmData_t* cmDataSetShortPtr(  cmData_t* p, short* vp,          unsigned cnt );
  cmData_t* cmDataSetUShortPtr( cmData_t* p, unsigned short* vp, unsigned cnt );
  cmData_t* cmDataSetIntPtr(    cmData_t* p, int* vp,            unsigned cnt );
  cmData_t* cmDataSetUIntPtr(   cmData_t* p, unsigned int* vp,   unsigned cnt );
  cmData_t* cmDataSetLongPtr(   cmData_t* p, long* vp,           unsigned cnt );
  cmData_t* cmDataSetULongPtr(  cmData_t* p, unsigned long* vp,  unsigned cnt );
  cmData_t* cmDataSetFloatPtr(  cmData_t* p, float* vp,          unsigned cnt );
  cmData_t* cmDataSetDoublePtr( cmData_t* p, double* vp,         unsigned cnt );

  // Set the value of an existing array based data object. 
  // Allocate the internal array and copy the array into it.
  cmData_t* cmDataSetStrAlloc(       cmData_t* p, const cmChar_t* s );
  cmData_t* cmDataSetConstStrAlloc(  cmData_t* p, const cmChar_t* s );
  cmData_t* cmDataSetVoidAllocPtr(   cmData_t* p, const void* vp,           unsigned cnt );
  cmData_t* cmDataSetCharAllocPtr(   cmData_t* p, const char* vp,           unsigned cnt );
  cmData_t* cmDataSetUCharAllocPtr(  cmData_t* p, const unsigned char* vp,  unsigned cnt );
  cmData_t* cmDataSetShortAllocPtr(  cmData_t* p, const short* vp,          unsigned cnt );
  cmData_t* cmDataSetUShortAllocPtr( cmData_t* p, const unsigned short* vp, unsigned cnt );
  cmData_t* cmDataSetIntAllocPtr(    cmData_t* p, const int* vp,            unsigned cnt );
  cmData_t* cmDataSetUIntAllocPtr(   cmData_t* p, const unsigned int* vp,   unsigned cnt );
  cmData_t* cmDataSetLongAllocPtr(   cmData_t* p, const long* vp,           unsigned cnt );

  cmData_t* cmDataSetULongAllocPtr(  cmData_t* p, const unsigned long* vp,  unsigned cnt );
  cmData_t* cmDataSetFloatAllocPtr(  cmData_t* p, const float* vp,          unsigned cnt );
  cmData_t* cmDataSetDoubleAllocPtr( cmData_t* p, const double* vp,         unsigned cnt );
  

  // Dynamically allocate a data object and set it's value.
  cmData_t* cmDataAllocChar(   char v );
  cmData_t* cmDataAllocUChar(  unsigned char v );
  cmData_t* cmDataAllocShort(  short v );
  cmData_t* cmDataAllocUShort( unsigned short v );
  cmData_t* cmDataAllocInt(    int v );
  cmData_t* cmDataAllocUInt(   unsigned int v );
  cmData_t* cmDataAllocLong(   long v );
  cmData_t* cmDataAllocULong(  unsigned long v );
  cmData_t* cmDataAllocFloat(  float v );
  cmData_t* cmDataAllocDouble( double v );
  cmData_t* cmDataAllocStr(    cmChar_t* str );
  cmData_t* cmDataAllocConstStr( const cmChar_t* str );

  // Dynamically allocate a data object and set its array value to an external
  // array. The data is not copied.
  cmData_t* cmDataAllocVoidPtr(   const void* v,           unsigned cnt );
  cmData_t* cmDataAllocCharPtr(   const char* v,           unsigned cnt );
  cmData_t* cmDataAllocUCharPtr(  const unsigned char* v,  unsigned cnt );
  cmData_t* cmDataAllocShortPtr(  const short* v,          unsigned cnt );
  cmData_t* cmDataAllocUShortPtr( const unsigned short* v, unsigned cnt );
  cmData_t* cmDataAllocIntPtr(    const int* v,            unsigned cnt );
  cmData_t* cmDataAllocUIntPtr(   const unsigned int* v,   unsigned cnt );
  cmData_t* cmDataAllocLongPtr(   const long* v,           unsigned cnt );
  cmData_t* cmDataAllocULongPtr(  const unsigned long* v,  unsigned cnt );
  cmData_t* cmDataAllocFloatPtr(  const float* v,          unsigned cnt );
  cmData_t* cmDataAllocDoublePtr( const double* v,         unsigned cnt );


  // Dynamically allocate a data object and its array value.  
  // v[cnt] is copied into the allocated array.
  cmData_t* cmDataVoidAllocPtr(   const void* v,           unsigned cnt );
  cmData_t* cmDataCharAllocPtr(   const char* v,           unsigned cnt );
  cmData_t* cmDataUCharAllocPtr(  const unsigned char* v,  unsigned cnt );
  cmData_t* cmDataShortAllocPtr(  const short* v,          unsigned cnt );
  cmData_t* cmDataUShortAllocPtr( const unsigned short* v, unsigned cnt );
  cmData_t* cmDataIntAllocPtr(    const int* v,            unsigned cnt );
  cmData_t* cmDataUIntAllocPtr(   const unsigned int* v,   unsigned cnt );
  cmData_t* cmDataLongAllocPtr(   const long* v,           unsigned cnt );
  cmData_t* cmDataULongAllocPtr(  const unsigned long* v,  unsigned cnt );
  cmData_t* cmDataFloatAllocPtr(  const float* v,          unsigned cnt );
  cmData_t* cmDataDoubleAllocPtr( const double* v,         unsigned cnt );

  //----------------------------------------------------------------------------
  // Structure related functions
  //

  // Unlink 'p' from its parents and siblings.
  cmData_t* cmDataUnlink( cmData_t* p );

  unsigned  cmDataChildCount( cmData_t* p );
  cmData_t* cmDataChild( cmData_t* p, unsigned index );

  cmData_t* cmDataPrependChild(cmData_t* parent, cmData_t* parent );
  cmData_t* cmDataAppendChild( cmData_t* parent, cmData_t* child );
  cmData_t* cmDataInsertChild( cmData_t* parent, cmData_t* child, unsigned index );


  //----------------------------------------------------------------------------
  // Pair related functions
  //
  
  // Get the key/value of a pair
  cmData_t* cmDataPairKey(          cmData_t* p );
  cmData_t* cmDataPairValue(        cmData_t* p );
  
  // Set the key or value of an existing pair node. 
  cmData_t* cmDataPairSetValue(     cmData_t* p, cmData_t* value );
  cmData_t* cmDataPairAllocValue(   cmData_t* p, const cmData_t* value );

  cmData_t* cmDataPairSetKey(       cmData_t* p, cmData_t* key );
  cmData_t* cmDataPairSetKeyId(     cmData_t* p, unsigned id );
  cmData_t* cmDataPairSetKeyLabel(  cmData_t* p, const cmChar_t* label );
  cmData_t* cmDataPairAllocKey(     cmData_t* p, const cmData_t* key );

  // Dynamically allocate a pair node 
  cmData_t* cmDataAllocPair(      cmData_t* parent, const cmData_t* key,  const cmData_t* value );
  cmData_t* cmDataAllocPairId(    cmData_t* parent, unsigned  keyId,      cmData_t* value );
  cmData_t* cmDataAllocPairLabel( cmData_t* parent, const cmChar_t label, cmData_t* value );

  //----------------------------------------------------------------------------
  // List related functions
  //
  
  // Return the count of ele's in the list.
  cmData_t* cmDataListCount(  const cmData_t* p );

  // Return the ith element in the list.
  cmData_t* cmDataListEle(    const cmData_t* p, unsigned index );

  cmData_t* cmDataListMake(  cmData_t* parent, cmData_t* p );
  cmData_t* cmDataListAlloc( cmData_t* parent);


  // Var-args fmt:
  // <typeId> <value> {<cnt>}
  // scalar types: <value> is literal,<cnt>   is not included
  // ptr    types: <value> is pointer,<cnt>   is element count
  // struct types: <value> is cmData_t, <cnt> is not included
  cmData_t* cmDataListAllocV(cmData_t* parent, va_list vl );
  cmData_t* cmDataListAllocA(cmData_t* parent,  ... );
  

  cmData_t* cmDataListAppendEle( cmData_t* p, cmData_t* ele );
  cmData_t* cmDataListAppendEleN(cmData_t* p, cmData_t* ele[], unsigned n );
  cmDtRC_t  cmDataListAppendV(   cmData_t* p, va_list vl );
  cmDtRC_t  cmDataListAppend(    cmData_t* p, ... );

  cmData_t* cmDataListInsertEle( cmData_t* p, unsigned index, cmData_t* ele );
  cmData_t* cmDataListInsertEleN(cmData_t* p, cmData_t* ele[], unsigned n );
  cmDtRC_t  cmDataListInsertV(   cmData_t* p, va_list vl );
  cmDtRC_t  cmDataListInsert(    cmData_t* p, unsigned index, ... );

 
  cmData_t* cmDataListUnlink( cmData_t* p, unsigned index );
  cmData_t* cmDataListFree(   cmData_t* p, unsigned index );

  //----------------------------------------------------------------------------
  // Record related functions
  //

  // Return count of pairs.
  cmData_t*       cmDataRecdCount(    const cmData_t* p );

  // Return the ith pair.
  cmData_t*       cmDataRecdEle(      const cmData_t* p, unsigned index );

  // Return the ith value.
  cmData_t*       cmDataRecdValue(    const cmData_t* p, unsigned index );

  // Return the ith key
  cmData_t*       cmDataRecdKey(      const cmData_t* p, unsigned index );
  unsigned        cmDataRecdKeyId(    const cmData_t* p, unsigned index );
  const cmChar_t* cmDataRecdKeyLabel( const cmData_t* p, unsigned index );
  
  cmData_t*       cmRecdMake( cmData_t* p );
  cmData_t*       cmRecdAlloc();
  

  

  void cmDataFree( cmData_t* p );

  
  unsigned cmDataSerializeByteCount( const cmData_t* p );
  cmDtRC_t cmDataSerialize( const cmData_t* p, void* buf, unsigned bufByteCnt );
  cmDtRC_t cmDataDeserialize( const void* buf, unsigned bufByteCnt, cmData_t** pp );
  
  void     cmDataPrint( const cmData_t* p, cmRpt_t* rpt );
  
  void     cmDataTest( cmCtx_t* ctx );



#ifdef __cplusplus
}
#endif

#endif
