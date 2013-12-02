#ifndef cmData_h
#define cmData_h

#ifdef __cplusplus
extern "C" {
#endif

  /*
    TODO:
    0) Figure out an error handling scheme that does not rely on
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
    Whereever a function may be implemented using 
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

  enum
  {
    kOkDtRC = cmOkRC,
    kAssertErrDtRC,
    kConstErrDtRC,
    kCvtErrDtRC,
    kInvalidContDtRC,
    kInvalidTypeDtRC,
    kMissingFieldDtRC,
    kLexFailDtRC,
    kParseStackFailDtRC,
    kSyntaxErrDtRC,
    kEolDtRC
  };

  typedef unsigned cmDtRC_t;

  typedef enum
  {
    kInvalidTypeDtId,// 0
    kNullDtId,       // 1 the data object exists but it has no data
    kUCharDtId,      // 2
    kCharDtId,       // 3
    kUShortDtId,     // 4 
    kShortDtId,      // 5
    kUIntDtId,       // 6
    kIntDtId,        // 7
    kULongDtId,      // 8
    kLongDtId,       // 9
    kFloatDtId,      // 10 
    kDoubleDtId,     // 11 
    kStrDtId,        // 12 zero terminated string
    kBlobDtId,       // 13 application defined raw memory object
    kStructDtId,      // 14 node is a pair,list, or recd

    kOptArgDtFl = 0x10000000
  } cmDataTypeId_t;


  typedef enum
  {
    kInvalidCntDtId, // 0 
    kScalarDtId,     // 1
    kArrayDtId,      // 2
    kPairDtId,       // 3
    kListDtId,       // 4
    kRecordDtId      // 5
  } cmDataContainerId_t;

  enum
  {
    kNoFlagsDtFl = 0x00,

    // Indicate that the memory used by the data object
    // was dynamically allocated and should be released
    // by cmDataFree().
    kFreeObjDtFl   = 0x01,

    // Indicate that the memory used by strings, blobs 
    // and arrays should be freed by cmDataFree().
    kFreeValueDtFl = 0x02, 

    // Indicate that the value of the object cannot be changed.
    // (but the object could be reassigned as a new type).
    kConstValueDtFl = 0x04,

    // Indicate that the type of the object cannot be changed.
    // (but the value may be changed).
    kConstObjDtFl  = 0x08,

    // Indicate that the array or string should not be 
    // internally reallocated but rather the source pointer
    // should be taken as the new value of the object.
    kNoCopyDtFl = 0x10, 


  };

  // The kInvalidDtXXX constants  are used to indicate an error when returned
  // from the cmDtXXX() functions below.
#define   kInvalidDtFloat  FLT_MAX
#define   kInvalidDtDouble DBL_MAX

  enum
  {
    kInvalidDtChar = 0xff,
    kInvalidDtShort = 0xffff,
    kInvalidDtInt   = 0xffffffff,
    kInvalidDtLong  = kInvalidDtInt,
  };


  typedef struct cmData_str
  {
    cmDataTypeId_t      tid;       // data format id
    cmDataContainerId_t cid;       // container id
    unsigned            flags;     // 
    struct cmData_str*  parent;    // this childs parent
    struct cmData_str*  sibling;   // this childs left sibling
    unsigned            cnt;       // byte cnt for strings/blobs and ele count for arrays

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

      void*             vp;

      struct cmData_str* child; // first child (list,record,pair)
    } u;
  
  } cmData_t;

  extern cmData_t cmDataNull;

  const cmChar_t*  cmDataTypeToLabel( cmDataTypeId_t tid );
  cmDataTypeId_t   cmDataLabelToType( const cmChar_t* typeLabelStr );

  // Returns 1 for kStrDtId.
  // Returns cmInvalidCnt if tid is not recognized.
  unsigned         dmDataByteWidth( cmDataTypeId_t tid );

  const cmChar_t*      cmDataContainerIdToLabel( cmDataContainerId_t tid );
  cmDataContainerId_t  cmDataLabelToContainerId( const cmChar_t* contLabelStr );

  bool cmDataIsConstObj( const cmData_t* d );
  void cmDataEnableConstObj( cmData_t* d, bool enaFl );

  bool cmDataIsConstValue( const cmData_t* d );
  void cmDataEnableConstValue( cmData_t* d, bool enaFl );

  bool cmDataIsFreeValue( const cmData_t* d );
  void cmDataEnableFreeValue( cmData_t* d, bool enaFl );

  // Returns true if this is a scalar or array node.
  bool cmDataIsLeaf( const cmData_t* d);

  // Return true if this is NOT a scalar or array node.
  bool cmDataIsStruct( const cmData_t* d );
  
  

  //----------------------------------------------------------------------------
  // Scalar related functions
  //

  // Dynamically allocate a scalar object and set it's value.
  // The 'flags' argument may include kConstValueDtFl and kConstObjDtFl.
  // The string and blob constructors may also use the
  // kNoCopyDtFl and the kFreeValueDtFl.

  // Generic:
  // 'byteCnt' is ignored for all types other than strings and blobs.
  cmDtRC_t cmDataNewScalar( cmData_t* parent, cmDataTypeId_t tid, unsigned flags, void* vp, unsigned byteCnt, cmData_t** ref );

  // Type specific
  cmDtRC_t cmDataNewNull(     cmData_t* parent, unsigned flags,                   cmData_t** ref );
  cmDtRC_t cmDataNewChar(     cmData_t* parent, unsigned flags, char v,           cmData_t** ref );
  cmDtRC_t cmDataNewUChar(    cmData_t* parent, unsigned flags, unsigned char v,  cmData_t** ref );
  cmDtRC_t cmDataNewShort(    cmData_t* parent, unsigned flags, short v,          cmData_t** ref );
  cmDtRC_t cmDataNewUShort(   cmData_t* parent, unsigned flags, unsigned short v, cmData_t** ref );
  cmDtRC_t cmDataNewInt(      cmData_t* parent, unsigned flags, int v,            cmData_t** ref );
  cmDtRC_t cmDataNewUInt(     cmData_t* parent, unsigned flags, unsigned int v,   cmData_t** ref );
  cmDtRC_t cmDataNewLong(     cmData_t* parent, unsigned flags, long v,           cmData_t** ref );
  cmDtRC_t cmDataNewULong(    cmData_t* parent, unsigned flags, unsigned long v,  cmData_t** ref );
  cmDtRC_t cmDataNewFloat(    cmData_t* parent, unsigned flags, float v,          cmData_t** ref );
  cmDtRC_t cmDataNewDouble(   cmData_t* parent, unsigned flags, double v,         cmData_t** ref );
  cmDtRC_t cmDataNewStr(      cmData_t* parent, unsigned flags, cmChar_t* str,       cmData_t** ref );
  cmDtRC_t cmDataNewConstStr( cmData_t* parent, unsigned flags, const cmChar_t* str, cmData_t** ref );
  cmDtRC_t cmDataNewStrN(     cmData_t* parent, unsigned flags, cmChar_t* str,       unsigned charCnt, cmData_t** ref );
  cmDtRC_t cmDataNewConstStrN(cmData_t* parent, unsigned flags, const cmChar_t* str, unsigned charCnt, cmData_t** ref );
  cmDtRC_t cmDataNewBlob(     cmData_t* parent, unsigned flags, void* vp,            unsigned byteCnt, cmData_t** ref );
  cmDtRC_t cmDataNewConstBlob(cmData_t* parent, unsigned flags, const void* vp,      unsigned byteCnt, cmData_t** ref );
 


  // Set the value and type of an existing scalar object.
  // These functions begin by releasing any resources held by *p
  // prior to resetting the type and value of the object.
  // The 'flags' argument to cmDataSetStr() and cmDataSetConstStr() 
  // may use the kNoCopyDtFl and the kFreeValueDtFl
  cmDtRC_t  cmDataSetScalarValue( cmData_t* d, cmDataTypeId_t tid, void* vp, unsigned byteCnt, unsigned flags );
  
  cmDtRC_t cmDataSetNull(      cmData_t* p );
  cmDtRC_t cmDataSetChar(      cmData_t* p, char v );
  cmDtRC_t cmDataSetUChar(     cmData_t* p, unsigned char v );
  cmDtRC_t cmDataSetShort(     cmData_t* p, short v );
  cmDtRC_t cmDataSetUShort(    cmData_t* p, unsigned short v );
  cmDtRC_t cmDataSetInt(       cmData_t* p, int v );
  cmDtRC_t cmDataSetUInt(      cmData_t* p, unsigned int v );
  cmDtRC_t cmDataSetLong(      cmData_t* p, long v );
  cmDtRC_t cmDataSetULong(     cmData_t* p, unsigned long v );
  cmDtRC_t cmDataSetFloat(     cmData_t* p, float v );
  cmDtRC_t cmDataSetDouble(    cmData_t* p, double v );
  cmDtRC_t cmDataSetStr(       cmData_t* p, unsigned flags, cmChar_t* s );
  cmDtRC_t cmDataSetConstStr(  cmData_t* p, unsigned flags, const cmChar_t* s );
  cmDtRC_t cmDataSetStrN(      cmData_t* p, unsigned flags, cmChar_t* s, unsigned charCnt );
  cmDtRC_t cmDataSetConstStrN( cmData_t* p, unsigned flags, const cmChar_t* s, unsigned charCnt );
  cmDtRC_t cmDataSetBlob(      cmData_t* p, unsigned flags, void* v, unsigned byteCnt );
  cmDtRC_t cmDataSetConstBlob( cmData_t* p, unsigned flags, const void* v, unsigned byteCnt );

  // Get the value of an object. No conversion is applied the
  // type must match exactly or an error is generated.
  cmDtRC_t cmDataChar(      const cmData_t* p, char* v );
  cmDtRC_t cmDataUChar(     const cmData_t* p, unsigned char* v );
  cmDtRC_t cmDataShort(     const cmData_t* p, short* v );
  cmDtRC_t cmDataUShort(    const cmData_t* p, unsigned short* v );
  cmDtRC_t cmDataInt(       const cmData_t* p, int* v );
  cmDtRC_t cmDataUInt(      const cmData_t* p, unsigned int* v );
  cmDtRC_t cmDataLong(      const cmData_t* p, long* v );
  cmDtRC_t cmDataULong(     const cmData_t* p, unsigned long* v );
  cmDtRC_t cmDataFloat(     const cmData_t* p, float* v );
  cmDtRC_t cmDataDouble(    const cmData_t* p, double* v );
  cmDtRC_t cmDataStr(       const cmData_t* p, cmChar_t** v );
  cmDtRC_t cmDataConstStr(  const cmData_t* p, const cmChar_t** v );
  cmDtRC_t cmDataBlob(      const cmData_t* p, void** v, unsigned* byteCntRef );
  cmDtRC_t cmDataConstBlob( const cmData_t* p, const void** v, unsigned* byteCntRef );

  // Functions in this group which return pointers will return NULL
  // on error.  The other function indicate an error by returning 
  // kInvalidDtXXX depending on their type.
  // Note that there is no guarantee, except as determined by the 
  // application, that one of the kInvalidDtXXX is not in fact a legal return value.
  // These function are simple wrappers around calls to cmDataXXX() and
  // therefore do NOT do any type conversion.
  char           cmDtChar(      const cmData_t* p );
  unsigned char  cmDtUChar(     const cmData_t* p );
  short          cmDtShort(     const cmData_t* p );
  unsigned short cmDtUShort(    const cmData_t* p );
  int            cmDtInt(       const cmData_t* p );
  unsigned       cmDtUInt(      const cmData_t* p );
  long           cmDtLong(      const cmData_t* p );
  unsigned long  cmDtULong(     const cmData_t* p );
  float          cmDtFloat(     const cmData_t* p );
  double         cmDtDouble(    const cmData_t* p );
  char*          cmDtStr(       const cmData_t* p );
  const char*    cmDtConstStr(  const cmData_t* p );
  void*          cmDtBlob(      const cmData_t* p, unsigned* byteCntRef );
  const void*    cmDtConstBlob( const cmData_t* p, unsigned* byteCntRef );

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

  // Functions in this group which return pointers will return NULL
  // on error.  The other function indicate an error by returning 
  // kInvalidDtXXX depending on their type.
  // Note that there is no guarantee, except as determined by the 
  // application that one of the kInvalidDtXXX is not in fact a legal return value.
  // These function are simple wrappers around calls to cmDataGetXXX() and
  // therefore do type conversion.
  char           cmDtGetChar(      const cmData_t* p );
  unsigned char  cmDtGetUChar(     const cmData_t* p );
  short          cmDtGetShort(     const cmData_t* p );
  unsigned short cmDtGetUShort(    const cmData_t* p );
  int            cmDtGetInt(       const cmData_t* p );
  unsigned       cmDtGetUInt(      const cmData_t* p );
  long           cmDtGetLong(      const cmData_t* p );
  unsigned long  cmDtGetULong(     const cmData_t* p );
  float          cmDtGetFloat(     const cmData_t* p );
  double         cmDtGetDouble(    const cmData_t* p ); 

  //----------------------------------------------------------------------------
  // Array related functions
  //

  // Notes:
  // 1) string arrays are arrays of string pointers.
  // 2) blob arrays (array of void pointers) are not supported because
  //    there is no direct way to determine the length of each blob
  //    and therefore they cannot be internally duplicated - a special scheme
  //    could be devised (length goes in first 4 bytes) to make this 
  //    work but we will defer that until the need arises.

  //
  // Dynamically allocate a new array data object.
  //
  // eleCnt referes to the number of elements in the array pointed
  // to by 'vp'.  The number of bytes pointed to by 'vp' is then
  // cmDataByteWidth(tid)*eleCnt.
  //
  // If no flags are set then the array pointed to by 'vp' is reallocated
  // and kDataFreeDtFl is set.
  //
  // If kFreeValueDtFl is set then the object will take responsibility for
  // releasing the memory pointed to by 'vp' when the object is destroyed
  // or the array is reassigned.  
  //
  // If kNoCopyDtFl is set then 'vp' becomes the internal array
  // value (vp[cnt]) is NOT reallocated). In this case the client is
  // responsibile for eventually releasing the associated memory - when
  // the data object is no longer valid.
  cmDtRC_t cmDataNewArray(  cmData_t* parent, cmDataTypeId_t tid, void* vp, unsigned eleCnt, unsigned flags, cmData_t** ref );

  cmDtRC_t cmDataNewCharArray(     cmData_t* parent, char* v,           unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewUCharArray(    cmData_t* parent, unsigned char* v,  unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewShortArray(    cmData_t* parent, short* v,          unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewUShortArray(   cmData_t* parent, unsigned short* v, unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewIntArray(      cmData_t* parent, int* v,            unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewUIntArray(     cmData_t* parent, unsigned int* v,   unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewLongArray(     cmData_t* parent, long* v,           unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewULongArray(    cmData_t* parent, unsigned long* v,  unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewFloatArray(    cmData_t* parent, float* v,          unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewDoubleArray(   cmData_t* parent, double* v,         unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewStrArray(      cmData_t* parent, cmChar_t** v,      unsigned eleCnt, unsigned flags, cmData_t** ref );
  cmDtRC_t cmDataNewConstStrArray( cmData_t* parent, const cmChar_t** v,unsigned eleCnt, unsigned flags, cmData_t** ref );

  // Set the value and type of an existing scalar object.
  //
  // These functions begin by releasing any resources held by *p
  // prior to resetting the type and value of the object.
  // The 'flags' argument may include kConstValueDtFl, kConstObjDtFl,
  // kNoCopyDtFl and the kFreeValueDtFl.

  // Generic set array functions. 'vp' is assumed to point to an array 
  // of the type defined by 'tid'.
  cmDtRC_t cmDataSetArrayValue(  cmData_t* dt, cmDataTypeId_t tid, void* vp, unsigned eleCnt, unsigned flags );

  // Type sepctific set array functions.
  cmDtRC_t cmDataSetCharArray(   cmData_t* d, char* v,           unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetUCharArray(  cmData_t* d, unsigned char* v,  unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetShortArray(  cmData_t* d, short* v,          unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetUShortArray( cmData_t* d, unsigned short* v, unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetIntArray(    cmData_t* d, int* v,            unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetUIntArray(   cmData_t* d, unsigned int* v,   unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetLongArray(   cmData_t* d, long* v,           unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetULongArray(  cmData_t* d, unsigned long* v,  unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetFloatArray(  cmData_t* d, float* v,          unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetDoubleArray( cmData_t* d, double* v,         unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetStrArray(    cmData_t* d, cmChar_t** v,      unsigned eleCnt, unsigned flags );
  cmDtRC_t cmDataSetConstStrArray(cmData_t* d,const cmChar_t** v,unsigned eleCnt, unsigned flags );

  // Return the count of elements in a n array.
  unsigned cmDataArrayEleCount( const cmData_t* d );

  // Get a pointer to the base of an array. 
  // The type must match exactly or an error is generated.
  // Use cmDataEleCount() to determine the number of elements in the array.
  cmDtRC_t cmDataCharArray(      const cmData_t* d, char** v );
  cmDtRC_t cmDataUCharArray(     const cmData_t* d, unsigned char** v );
  cmDtRC_t cmDataShortArray(     const cmData_t* d, short** v );
  cmDtRC_t cmDataUShortArray(    const cmData_t* d, unsigned short** v );
  cmDtRC_t cmDataIntArray(       const cmData_t* d, int** v );
  cmDtRC_t cmDataUIntArray(      const cmData_t* d, unsigned int** v );
  cmDtRC_t cmDataLongArray(      const cmData_t* d, long** v );
  cmDtRC_t cmDataULongArray(     const cmData_t* d, unsigned long** v );
  cmDtRC_t cmDataFloatArray(     const cmData_t* d, float** v );
  cmDtRC_t cmDataDoubleArray(    const cmData_t* d, double** v );

  // This group of functions is a wrapper around calls to the same named
  // cmDataXXXArray() functions above. On error they return NULL.
  char*           cmDtCharArray(      const cmData_t* d );
  unsigned char*  cmDtUCharArray(     const cmData_t* d );
  short*          cmDtShortArray(     const cmData_t* d );
  unsigned short* cmDtUShortArray(    const cmData_t* d );
  int*            cmDtIntArray(       const cmData_t* d );
  unsigned*       cmDtUIntArray(      const cmData_t* d );
  long*           cmDtLongArray(      const cmData_t* d );
  unsigned long*  cmDtULongArray(     const cmData_t* d );
  float*          cmDtFloatArray(     const cmData_t* d );
  double*         cmDtDoubleArray(    const cmData_t* d );
  
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
  cmData_t* cmDataPairMake(       cmData_t* parent, cmData_t* p, cmData_t* key, cmData_t* value );

  // Dynamically allocate a pair node. Both the key and value nodes are reallocated.
  cmData_t* cmDataPairAlloc(      cmData_t* parent, const cmData_t* key,  const cmData_t* value );

  // Dynamically allocate the id but link (w/o realloc) the value.
  cmData_t* cmDataPairAllocId(    cmData_t* parent, unsigned  keyId,       cmData_t* value );

  // Dynamically allocate the label but link (w/o realloc) the value.
  cmData_t* cmDataPairAllocLabelN(cmData_t* parent, const cmChar_t* label, unsigned charCnt, cmData_t* value);
  cmData_t* cmDataPairAllocLabel( cmData_t* parent, const cmChar_t* label, cmData_t* value );

  //----------------------------------------------------------------------------
  // List related functions
  //
  
  // Return the count of ele's in the list.
  unsigned  cmDataListCount(  const cmData_t* p );

  // Return the ith element in the list.
  cmData_t* cmDataListEle(    cmData_t* p, unsigned index );

  // 
  cmData_t* cmDataListMake(  cmData_t* parent, cmData_t* p );
  cmData_t* cmDataListAlloc( cmData_t* parent);


  // Var-args fmt:
  // <contId> {<typeId>} <value> {<cnt>}
  // scalar types: <value> is literal,<cnt>   is not included
  //     null has no <value> or <cnt>
  // array  types: <value> is pointer,<cnt>   is element count
  // struct types: <value> is cmData_t, <typeId> and <cnt> is not included
  // Indicate the end of argument list by setting  <typeId> to kInvalidDtId. 
  // The memory for array based data types is dynamically allocated.
  cmRC_t cmDataListAllocV(cmData_t* parent, cmData_t** ref, va_list vl );
  cmRC_t  cmDataListAllocA(cmData_t* parent,  cmData_t** ref, ... );
  
  // Returns a ptr to 'ele'.
  cmData_t* cmDataListAppendEle( cmData_t* p, cmData_t* ele );
  cmDtRC_t  cmDataListAppendV(   cmData_t* p, va_list vl );
  cmDtRC_t  cmDataListAppend(    cmData_t* p, ... );

  // Return  'p'.
  cmData_t* cmDataListInsertEle( cmData_t* p, unsigned index, cmData_t* ele );
  cmData_t* cmDataListInsertEleN(cmData_t* p, unsigned index, cmData_t* ele[], unsigned n );
 
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
  // <label|id> {<cid>} <typeId>  <value> {<cnt>}
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
  // Var-args format:
  // (label | id) <cid> <typeId> <ptr> {cnt_ptr}
  // The var-args list must be NULL terminated.
  // The <'id' | 'label'> identify a pair.  
  // The <cid> indicates the type of the target container.
  // The <typeId> indicates the C type of 'pointer'.
  // If <cid> is kArrayDtId then the <cnt_ptr} must be include to receive the
  // count of elements in the array.
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
  extern cmDataParserH_t cmDataParserNullHandle;

  cmDtRC_t cmDataParserCreate( cmCtx_t* ctx, cmDataParserH_t* hp );
  cmDtRC_t cmDataParserDestroy( cmDataParserH_t* hp );
  bool     cmDataParserIsValid( cmDataParserH_t h );

  // Parse a text representation into a 'record' type. 
  // Note that the text is wrapped with implied curly braces 
  // (e.g. "{ text }").  The contents of the text should therefore
  // fit the record syntax (e.g. the first token should be a 
  // 'pair' label.
  cmDtRC_t cmDataParserExec(   cmDataParserH_t  h, const cmChar_t* text, cmData_t** pp );
  //-----------------------------------------------------------------------------
  
  void     cmDataPrint( const cmData_t* p, cmRpt_t* rpt );
  
  void     cmDataTest( cmCtx_t* ctx );


#ifdef __cplusplus
}
#endif

#endif
