#ifndef cmData_h
#define cmData_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkDtRC = cmOkRC,
    kCvtErrDtRC,
    kVarArgErrDtRC,
    kMissingFieldDtRC,
    kLexFailDtRC,
    kParseStackFailDtRC,
    kSyntaxErrDtRC,
    kEolDtRC
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

    kMinValDtId,

    kNullDtId = kMinValDtId,
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
    kMaxValDtId = kConstStrDtId,

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
    kPairDtId,                   // key/value pairs, cnt=2, first child is key, second is value
    kRecordDtId,                 // children nodes are pairs, cnt=pair count
    kMaxStructDtId,

    kOptArgDtFl = 0x80000000
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
    struct cmData_str* sibling;   // this childs left sibling
    unsigned           cnt;       // array ele count

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

  extern cmData_t cmDataNull;

  bool cmDataIsValue(  const cmData_t* p );
  bool cmDataIsPtr(    const cmData_t* p );
  bool cmDataIsStruct( const cmData_t* p ); // is a pair,list or record

  /*
    TODO:
    0) Figure out a error handling scheme that does not rely on
    a global errno.  This is not useful in multi-thread environments.
    It might be ok to go with an 'all errors are fatal' model
    (except in the var-args functions).
    Consider the use of a context object for use with functions 
    that can have runtime errors or need to allocate memory.

    1) Implement the canConvert and willTruncate functions.

    2) Make a set of cmDataAllocXXXPtr() functions which take
    a flag indicating whether or not to dynamically allocate
    the array space. This will allow dynamic allocattion to
    occur at runtime.  Make var args functions for list and
    record objects which also take this flag.
    Where ever a function may be implemented using 
    static/dynamic allocation this flag should be present.
    (e.g. string allocation for pair labels)
    This choice is common enough that it may be worth
    suffixing function names with a capital letter to
    be clear what the functions memory policy is.

    3) Come up with a var-args format which allows a 
    hierchy of records to be defined in one line.

    4) Implement the serialization functions.

    5) Implement an ascii string/parse format for writing/reading.

    6) Implement fast lookup of record fields.

    7) Allow for user defined types.  For example a 'matrix'
    data type. This might be as simple as adding an extra 'user_tid' 
    field to cmData_t.

    8) Implement type specific cmDataGetRecordValueXXX() functions.

    9) Implement cmDataIsEqual(), cmDataIsLtE(), ...

   */

  bool canConvertType( cmDataFmtId_t srcId, cmDataFmtId_t dstId );
  bool willTruncate(   cmDataFmtId_t srcId, cmDataFmtId_t dstId );
  bool canConvertObj(  const cmData_t* srcObj, cmData_t* dstObj );
  bool willTruncateObj(const cmData_t* srcObj, cmData_t* dstObj );
    
  

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
  cmDtRC_t cmDataGetChar(      const cmData_t* p, char* v );
  cmDtRC_t cmDataGetUChar(     const cmData_t* p, unsigned char* v );
  cmDtRC_t cmDataGetShort(     const cmData_t* p, short* v );
  cmDtRC_t cmDataGetUShort(    const cmData_t* p, unsigned short* v );
  cmDtRC_t cmDataGetInt(       const cmData_t* p, int* v );
  cmDtRC_t cmDataGetUInt(      const cmData_t* p, unsigned int* v );
  cmDtRC_t cmDataGetLong(      const cmData_t* p, long* v );
  cmDtRC_t cmDataGetULong(     const cmData_t* p, unsigned long* v );
  cmDtRC_t cmDataGetFloat(     const cmData_t* p, float* v );
  cmDtRC_t cmDataGetDouble(    const cmData_t* p, double* v );

  // Returns the pointer - does not copy the data.
  cmDtRC_t cmDataGetStr(       const cmData_t* p, char** v );
  cmDtRC_t cmDataGetConstStr(  const cmData_t* p, const char** v );
  cmDtRC_t cmDataGetVoidPtr(   const cmData_t* p, void** v );
  cmDtRC_t cmDataGetCharPtr(   const cmData_t* p, char** v );
  cmDtRC_t cmDataGetUCharPtr(  const cmData_t* p, unsigned char** v );
  cmDtRC_t cmDataGetShortPtr(  const cmData_t* p, short** v );
  cmDtRC_t cmDataGetUShortPtr( const cmData_t* p, unsigned short** v );
  cmDtRC_t cmDataGetIntPtr(    const cmData_t* p, int** v );
  cmDtRC_t cmDataGetUIntPtr(   const cmData_t* p, unsigned int** v );
  cmDtRC_t cmDataGetLongPtr(   const cmData_t* p, long** v );
  cmDtRC_t cmDataGetULongPtr(  const cmData_t* p, unsigned long** v );
  cmDtRC_t cmDataGetFloatPtr(  const cmData_t* p, float** v );
  cmDtRC_t cmDataGetDoublePtr( const cmData_t* p, double** v );


  // Set the value and type of an existing scalar object. 
  // These functions begin by releasing any resources held by *p
  // prior to resetting the type and value of the object.
  cmData_t* cmDataSetNull(      cmData_t* p );
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

  // Set the type and value of an existing data object to an external array.
  // These functions begin by releasing any resources help by *p.
  // The array pointed to by 'vp' is not copied or duplicated.
  // 'vp' is simply assigned as the data space for the object and therefore must remain
  // valid for the life of the object.
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
  // These functions begin by releasing any resources help by *p
  // and then dynamically allocate the internal array and copy 
  // the array data into it.
  cmData_t* cmDataSetStrAllocN(      cmData_t* p, const cmChar_t* s, unsigned charCnt );
  cmData_t* cmDataSetStrAlloc(       cmData_t* p, const cmChar_t* s );
  cmData_t* cmDataSetConstStrAllocN( cmData_t* p, const cmChar_t* s, unsigned charCnt );
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
  cmData_t* cmDataAllocNull(   cmData_t* parent );
  cmData_t* cmDataAllocChar(   cmData_t* parent, char v );
  cmData_t* cmDataAllocUChar(  cmData_t* parent, unsigned char v );
  cmData_t* cmDataAllocShort(  cmData_t* parent, short v );
  cmData_t* cmDataAllocUShort( cmData_t* parent, unsigned short v );
  cmData_t* cmDataAllocInt(    cmData_t* parent, int v );
  cmData_t* cmDataAllocUInt(   cmData_t* parent, unsigned int v );
  cmData_t* cmDataAllocLong(   cmData_t* parent, long v );
  cmData_t* cmDataAllocULong(  cmData_t* parent, unsigned long v );
  cmData_t* cmDataAllocFloat(  cmData_t* parent, float v );
  cmData_t* cmDataAllocDouble( cmData_t* parent, double v );

  // Dynamically allocate a data object and set its array value to an external
  // array. v[cnt] is assigned as the internal data space for the object and 
  // therefore must remain valid for the life of the object. 
  // See the cmDataXXXAlocPtr() for equivalent functions which dynamically 
  // allocate the intenal data space.
  cmData_t* cmDataAllocStr(       cmData_t* parent, cmChar_t* str );
  cmData_t* cmDataAllocConstStr(  cmData_t* parent, const cmChar_t* str );
  cmData_t* cmDataAllocCharPtr(   cmData_t* parent, char* v,           unsigned cnt );
  cmData_t* cmDataAllocUCharPtr(  cmData_t* parent, unsigned char* v,  unsigned cnt );
  cmData_t* cmDataAllocShortPtr(  cmData_t* parent, short* v,          unsigned cnt );
  cmData_t* cmDataAllocUShortPtr( cmData_t* parent, unsigned short* v, unsigned cnt );
  cmData_t* cmDataAllocIntPtr(    cmData_t* parent, int* v,            unsigned cnt );
  cmData_t* cmDataAllocUIntPtr(   cmData_t* parent, unsigned int* v,   unsigned cnt );
  cmData_t* cmDataAllocLongPtr(   cmData_t* parent, long* v,           unsigned cnt );
  cmData_t* cmDataAllocULongPtr(  cmData_t* parent, unsigned long* v,  unsigned cnt );
  cmData_t* cmDataAllocFloatPtr(  cmData_t* parent, float* v,          unsigned cnt );
  cmData_t* cmDataAllocDoublePtr( cmData_t* parent, double* v,         unsigned cnt );
  cmData_t* cmDataAllocVoidPtr(   cmData_t* parent, void* v,           unsigned cnt );


  // Dynamically allocate a data object and its array value.  
  // These functions dynamically allocate the internal array data space
  // and copy v[cnt] into it.
  cmData_t* cmDataStrAlloc(       cmData_t* parent, cmChar_t* str );
  cmData_t* cmDataConstStrAlloc(  cmData_t* parent, const cmChar_t* str );
  cmData_t* cmDataConstStrAllocN( cmData_t* parent, const cmChar_t* str, unsigned charCnt );
  cmData_t* cmDataCharAllocPtr(   cmData_t* parent, const char* v,           unsigned cnt );
  cmData_t* cmDataUCharAllocPtr(  cmData_t* parent, const unsigned char* v,  unsigned cnt );
  cmData_t* cmDataShortAllocPtr(  cmData_t* parent, const short* v,          unsigned cnt );
  cmData_t* cmDataUShortAllocPtr( cmData_t* parent, const unsigned short* v, unsigned cnt );
  cmData_t* cmDataIntAllocPtr(    cmData_t* parent, const int* v,            unsigned cnt );
  cmData_t* cmDataUIntAllocPtr(   cmData_t* parent, const unsigned int* v,   unsigned cnt );
  cmData_t* cmDataLongAllocPtr(   cmData_t* parent, const long* v,           unsigned cnt );
  cmData_t* cmDataULongAllocPtr(  cmData_t* parent, const unsigned long* v,  unsigned cnt );
  cmData_t* cmDataFloatAllocPtr(  cmData_t* parent, const float* v,          unsigned cnt );
  cmData_t* cmDataDoubleAllocPtr( cmData_t* parent, const double* v,         unsigned cnt );
  cmData_t* cmDataVoidAllocPtr(   cmData_t* parent, const void* v,           unsigned cnt );

  //----------------------------------------------------------------------------
  // Structure related functions
  //

  // Release an object and any resources held by it.
  // Note the this function does not unlink the object
  // from it's parent.  Use cmDataUnlinkAndFree()
  // to remove a object from it's parent list prior
  // to releasing it.
  void      cmDataFree( cmData_t* p );

  // Unlink 'p' from its parents and siblings.
  // Asserts if parent is not a structure. 
  // Returns 'p'.
  cmData_t* cmDataUnlink( cmData_t* p );

  // Wrapper function to cmDataUnlink() and cmDataFree().
  void      cmDataUnlinkAndFree( cmData_t* p );

  // Replace the 'dst' node with the 'src' node and
  // return 'src'.  This operation does not duplicate
  // 'src' it simply links in 'src' at the location of
  // 'dst' and then unlinks and free's 'dst'.
  cmData_t* cmDataReplace( cmData_t* dst, cmData_t* src );

  // Return the count of child nodes.
  // 1. Array nodes have one child per array element.
  // 2. List nodes have one child pair.
  // 3. Pair nodes have two children.
  // 4. Leaf nodes have 0 children.
  unsigned  cmDataChildCount( const cmData_t* p );

  // Returns the ith child of 'p'.
  // Returns NULL if p has no children or index is invalid.
  cmData_t* cmDataChild( cmData_t* p, unsigned index );

  // Prepend 'p' to 'parents' child list.
  // The source node 'p' is not duplicated  it is simply linked in.
  // 'p' is automatically unlinked prior to being prepended.
  // Returns 'p'. 
  cmData_t* cmDataPrependChild(cmData_t* parent, cmData_t* p );

  // Append 'p' to the end of 'parent' child list.
  // The source node 'p' is not duplicated it is simply linked in.
  // 'p' is automatically unlinked prior to being appended.
  // Returns 'p'. 
  cmData_t* cmDataAppendChild( cmData_t* parent, cmData_t* p );

  // Insert 'p' at index.  Index must be in the range: 
  // 0 to cmDataChildCount(parent).
  // The source node 'p' is not duplicated it is simply linked in.
  // 'p' is automatically unlinked prior to being inserted.
  // Returns 'p'.
  cmData_t* cmDataInsertChild( cmData_t* parent, unsigned index, cmData_t* p );


  //----------------------------------------------------------------------------
  // Pair related functions
  //
  
  // Get the key/value of a pair
  cmData_t*       cmDataPairKey(      cmData_t* p );
  unsigned        cmDataPairKeyId(    cmData_t* p );
  const cmChar_t* cmDataPairKeyLabel( cmData_t* p );
  cmData_t*       cmDataPairValue(    cmData_t* p );
  
  // Set the value of an existing pair node. 
  // 'value' is not duplicated it is simply linked in place of the
  // previous pair value node. The previous pair value node is
  // unlinked and freed.
  // Returns 'p'.
  cmData_t* cmDataPairSetValue(     cmData_t* p, cmData_t* value );

  // Set the key of an existing pair node.
  // The previous key is unlinked and freed.
  cmData_t* cmDataPairSetKey(       cmData_t* p, cmData_t* key );
  cmData_t* cmDataPairSetKeyId(     cmData_t* p, unsigned id );
  // The data space for the 'label' string is dynamically allocated.
  cmData_t* cmDataPairSetKeyLabel(  cmData_t* p, const cmChar_t* label );

  // Create a pair value by assigning a key and value to 'p'.
  // 'p' is unlinked and freed prior to the key value assignment.
  // 'key' and 'value' are simply linked in they are not duplicated or reallocated.
  cmData_t* cmDataMakePair(       cmData_t* parent, cmData_t* p, cmData_t* key, cmData_t* value );

  // Dynamically allocate a pair node. Both the key and value nodes are reallocated.
  cmData_t* cmDataAllocPair(      cmData_t* parent, const cmData_t* key,  const cmData_t* value );

  // Dynamically allocate the id but link (w/o realloc) the value.
  cmData_t* cmDataAllocPairId(    cmData_t* parent, unsigned  keyId,       cmData_t* value );

  // Dynamically allocate the label but link (w/o realloc) the value.
  cmData_t* cmDataAllocPairLabelN(cmData_t* parent, const cmChar_t* label, unsigned charCnt, cmData_t* value);
  cmData_t* cmDataAllocPairLabel( cmData_t* parent, const cmChar_t* label, cmData_t* value );

  //----------------------------------------------------------------------------
  // List related functions
  //
  
  // Return the count of ele's in the list.
  unsigned  cmDataListCount(  const cmData_t* p );

  // Return the ith element in the list.
  cmData_t* cmDataListEle(    cmData_t* p, unsigned index );

  cmData_t* cmDataListMake(  cmData_t* parent, cmData_t* p );
  cmData_t* cmDataListAlloc( cmData_t* parent);


  // Var-args fmt:
  // <typeId> <value> {<cnt>}
  // scalar types: <value> is literal,<cnt>   is not included
  //     null has no <value> or <cnt>
  // ptr    types: <value> is pointer,<cnt>   is element count
  // struct types: <value> is cmData_t, <cnt> is not included
  // Indicate the end of argument list by setting  <typeId> to kInvalidDtId. 
  // The memory for array based data types is dynamically allocated.
  cmData_t* cmDataListAllocV(cmData_t* parent, va_list vl );
  cmData_t* cmDataListAllocA(cmData_t* parent,  ... );
  
  // Returns a ptr to 'ele'.
  cmData_t* cmDataListAppendEle( cmData_t* p, cmData_t* ele );
  cmDtRC_t  cmDataListAppendV(   cmData_t* p, va_list vl );
  cmDtRC_t  cmDataListAppend(    cmData_t* p, ... );

  // Return  'p'.
  cmData_t* cmDataListInsertEle( cmData_t* p, unsigned index, cmData_t* ele );
  cmData_t* cmDataListInsertEleN(cmData_t* p, unsigned index, cmData_t* ele[], unsigned n );
 
  cmData_t* cmDataListUnlink( cmData_t* p, unsigned index );
  cmData_t* cmDataListFree(   cmData_t* p, unsigned index );

  //----------------------------------------------------------------------------
  // Record related functions
  //

  // Return count of pairs.
  unsigned        cmDataRecdCount(    const cmData_t* p );

  // Return the ith pair.
  cmData_t*       cmDataRecdEle(      cmData_t* p, unsigned index );

  // Return the ith value.
  cmData_t*       cmDataRecdValueFromIndex( cmData_t* p, unsigned index );
  cmData_t*       cmDataRecdValueFromId(    cmData_t* p, unsigned id );
  cmData_t*       cmDataRecdValueFromLabel( cmData_t* p, const cmChar_t* label );

  // Return the ith key
  cmData_t*       cmDataRecdKey(      cmData_t* p, unsigned index );
  unsigned        cmDataRecdKeyId(    cmData_t* p, unsigned index );
  const cmChar_t* cmDataRecdKeyLabel( cmData_t* p, unsigned index );
  
  cmData_t*       cmDataRecdMake( cmData_t* parent, cmData_t* p );
  cmData_t*       cmDataRecdAlloc( cmData_t* parent );
  
  // Append a pair node by linking the pair node 'pair' to the record node 'p'.
  // 'pair' is simply linked to 'p' via cmDataAppendChild() no 
  // reallocation or duplicattion takes place.
  cmData_t*       cmDataRecdAppendPair( cmData_t* p, cmData_t* pair );


  // Var-args format:
  // <label|id> <typeId>  <value> {<cnt>}
  // scalar types: <value> is literal,<cnt> is not included
  // null   type: has no <value> or <cnt>
  // ptr    types: <value> is pointer,  <cnt> is element count
  // struct types: <value> is cmData_t, <cnt> is not included
  // Indicate the end of argument list by setting  <typeId> to kInvalidDtId. 
  // The memory for array based data types is dynamically allocated.
  cmData_t*       cmDataRecdAllocLabelV( cmData_t* parent, va_list vl );
  cmData_t*       cmDataRecdAllocLabelA( cmData_t* parent, ... );

  cmData_t*       cmDataRecdAllocIdV( cmData_t* parent, va_list vl );
  cmData_t*       cmDataRecdAllocIdA( cmData_t* parent, ... );
  
  // Extract the data in a record to C variables.
  // The var-args list must be NULL terminated.
  // The <'id' | 'label'> identify a pair.  
  // The <typeId> indicates the C type of 'pointer'.
  // The actual field type must be convertable into this pointer type or the
  // function will fail.
  // 'err' is an application supplied error object to be used if a required
  // field is missing.  'errRC' is the client result code to be passed with 
  // the error report. See cmErrMsg().  Both 'err' and 'errRC' are optional.
  // Set kOptArgDtFl on 'typeId' to indicate that a field is optional.
  // <label|id> (<typeId> | kOptArgDtFl)  <pointer>
  cmDtRC_t cmDataRecdParseLabelV(cmData_t* p, cmErr_t* err, unsigned errRC, va_list vl );
  cmDtRC_t cmDataRecdParseLabel( cmData_t* p, cmErr_t* err, unsigned errRC, ... );
  cmDtRC_t cmDataRecdParseIdV(   cmData_t* p, cmErr_t* err, unsigned errRC, va_list vl );
  cmDtRC_t cmDataRecdParseId(    cmData_t* p, cmErr_t* err, unsigned errRC, ... );
  
  unsigned cmDataSerializeByteCount( const cmData_t* p );
  cmDtRC_t cmDataSerialize(   const cmData_t* p, void* buf, unsigned bufByteCnt );
  cmDtRC_t cmDataDeserialize( const void* buf, unsigned bufByteCnt, cmData_t** pp );

  //-----------------------------------------------------------------------------
  typedef cmHandle_t cmDataParserH_t;
  //static cmDataParserH_t cmDataParserNullHandle;

  cmDtRC_t cmDataParserCreate( cmCtx_t* ctx, cmDataParserH_t* hp );
  cmDtRC_t cmDataParserDestroy( cmDataParserH_t* hp );
  bool     cmDataParserIsValid( cmDataParserH_t h );
  cmDtRC_t cmDataParserExec(   cmDataParserH_t  h, const cmChar_t* text, cmData_t** pp );
  //-----------------------------------------------------------------------------
  
  void     cmDataPrint( const cmData_t* p, cmRpt_t* rpt );
  
  void     cmDataTest( cmCtx_t* ctx );



#ifdef __cplusplus
}
#endif

#endif
