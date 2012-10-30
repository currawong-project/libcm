#ifndef cmJson_h
#define cmJson_h


#ifdef __cplusplus
extern "C" {
#endif
  //{
  //(
  //
  //  Limitations:
  //
  //  1. Accpets two digit hex sequences with 
  //  the \\u escape command. JSON specifies 4 digits.
  //
  //  2. The scientific notation for real numbers is limited to
  //  exponent prefixes: e,E,e-,E-. The prefixes e+ and E+ are
  //  not recognized by cmLex.
  //
  //  Extensions:
  //
  //  1. Will accept C style identifiers where JSON demands 
  //  quoted strings.
  //
  //  2. Will accept C style hex notation (0xddd)  for integer values.
  //
  //)

  //(

  // JSON data type flags
  enum
  {
    kInvalidTId = 0x0000,
    kObjectTId  = 0x0001,   // children are pairs
    kPairTId    = 0x0002,   // children are string : value pairs
    kArrayTId   = 0x0004,   // children may be of any type
    kStringTId  = 0x0008,   // terminal
    kNullTId    = 0x0040,   // terminal
    kIntTId     = 0x0010,   // terminal
    kRealTId    = 0x0020,   // terminal
    kTrueTId    = 0x0080,   // terminal
    kFalseTId   = 0x0100,   // terminal
    
    kMaskTId    = 0x01ff, 

    kOptArgJsFl = 0x0800,  // only used by cmJsonVMemberValues()
    kTempJsFl   = 0x1000,  // used internally

    kNumericTId = kIntTId  | kRealTId | kTrueTId | kFalseTId,
    kBoolTId    = kTrueTId | kFalseTId

  };

  enum
  {
    kOkJsRC,
    kMemAllocErrJsRC,
    kLexErrJsRC,
    kSyntaxErrJsRC,
    kFileOpenErrJsRC,
    kFileCreateErrJsRC,
    kFileReadErrJsRC,
    kFileSeekErrJsRC,
    kFileCloseErrJsRC,
    kInvalidHexEscapeJsRC,
    kSerialErrJsRC,
    kNodeNotFoundJsRC,
    kNodeCannotCvtJsRC,
    kCannotRemoveLabelJsRC,
    kInvalidNodeTypeJsRC,
    kValidateFailJsRC,
    kCsvErrJsRC,
    kBufTooSmallJsRC
  };
 
  typedef unsigned cmJsRC_t;

  typedef cmHandle_t cmJsonH_t;

  // JSON tree node
  typedef struct cmJsonNode_str
  {
    
    unsigned               typeId;      // id of this node
    struct cmJsonNode_str* siblingPtr;  // next ele in array or member list
    struct cmJsonNode_str* ownerPtr;    // parent node ptr

    union
    {
      // childPtr usage:
      // object: first pair
      // array:  first element
      // pair:   string
      struct cmJsonNode_str* childPtr;   
      int                    intVal;     // valid if typeId == kIntTId
      double                 realVal;    // valid if typeId == kRealTId
      char*                  stringVal;  // valid if typeId == kStringTId
      bool                   boolVal;    // valid if typeId == kTrueTId || kFalseTId
    } u;

  } cmJsonNode_t;

  extern cmJsonH_t cmJsonNullHandle;

  // Initialize a json parser/tree object
  cmJsRC_t      cmJsonInitialize( cmJsonH_t* hp, cmCtx_t* ctx );

  // Equivalent to cmJsonInitialize() followed by cmJsonParseFile()
  cmJsRC_t      cmJsonInitializeFromFile( cmJsonH_t* hp, const char* fn, cmCtx_t* ctx );

  // Equivalent to cmJsonInitialize() followed by cmJsonParse(h,buf,cnt,NULL).
  cmJsRC_t      cmJsonInitializeFromBuf( cmJsonH_t* hp, cmCtx_t* ctx, const char* buf, unsigned bufByteCnt );

  // Release all the resources held by the tree.
  cmJsRC_t      cmJsonFinalize(   cmJsonH_t* hp );

  // Returns true if 'h' is a valid cmJsonH_t handle.
  bool          cmJsonIsValid(    cmJsonH_t h );

  // Build the internal tree by parsing a text buffer. 
  // altRootPtr is an optional alternate root ptr which can be used
  // append to an existing tree. Set to altRootPtr to
  // NULL to append the tree to the internal root.
  // If altRootPtr is given it must point ot either an array or
  // object node.
  cmJsRC_t      cmJsonParse(      cmJsonH_t h, const char* buf, unsigned bufCharCnt, cmJsonNode_t* altRootPtr );

  // Fills a text buffer from a file and calls cmJsonParse().
  cmJsRC_t      cmJsonParseFile(  cmJsonH_t h, const char* fn, cmJsonNode_t* altRootPtr );

  // Return the root node of the internal tree.
  cmJsonNode_t* cmJsonRoot(       cmJsonH_t h );

  // Return the tree to the post initialize state by clearing the  internal tree.
  cmJsRC_t      cmJsonClearTree(      cmJsonH_t h );

  // Node type predicates.
  bool cmJsonIsObject( const cmJsonNode_t* np );
  bool cmJsonIsArray(  const cmJsonNode_t* np );
  bool cmJsonIsPair(   const cmJsonNode_t* np );
  bool cmJsonIsString( const cmJsonNode_t* np );
  bool cmJsonIsInt(    const cmJsonNode_t* np );
  bool cmJsonIsReal(   const cmJsonNode_t* np );
  bool cmJsonIsBool(   const cmJsonNode_t* np );


  // Return the count of child nodes of 'np'. 
  // Note that only object,array, and pair nodes have children.
  unsigned      cmJsonChildCount( const cmJsonNode_t* np );

  // Return the node at 'index' from an element array. 
  // 'np must point to an array element.
  cmJsonNode_t* cmJsonArrayElement( cmJsonNode_t* np, unsigned index );
  const cmJsonNode_t* cmJsonArrayElementC( const cmJsonNode_t* np, unsigned index );

  // Return the child value node of a pair with a label node equal to 'label'.
  // Set 'root' to NULL to begin the search at the internal tree root node. 
  // Set 'typeIdMask' with all type flags to match.
  // If 'typeIdMask' is equal to kInvalidTId then all types will match.
  cmJsonNode_t* cmJsonFindValue(    cmJsonH_t h, const char* label, const cmJsonNode_t* root, unsigned typeIdMask );

  // Return the value node of a pair at the end of an object path.
  // 'path' is a '/' seperated list of object names where the final
  // object specifies the pair label for the value to return.
  const cmJsonNode_t* cmJsonFindPathValueC( cmJsonH_t h, const char* path, const cmJsonNode_t* root, unsigned typeIdMask );
  cmJsonNode_t*       cmJsonFindPathValue(  cmJsonH_t h, const char* path, const cmJsonNode_t* root, unsigned typeIdMask );

  // Return the node value. If 'np' does not point to the same type as
  // specified in '*retPtr' then the value is converted if possible.
  // If the value cannot be converted function returns a 'kNodeCannotCvtJsRC'
  // error
  cmJsRC_t cmJsonUIntValue(   const cmJsonNode_t* np, unsigned*       retPtr );
  cmJsRC_t cmJsonIntValue(    const cmJsonNode_t* np, int*            retPtr );
  cmJsRC_t cmJsonRealValue(   const cmJsonNode_t* np, double*         retPtr );
  cmJsRC_t cmJsonBoolValue(   const cmJsonNode_t* np, bool*           retPtr );
  cmJsRC_t cmJsonStringValue( const cmJsonNode_t* np, const char **   retPtrPtr );  
  cmJsRC_t cmJsonPairNode(    const cmJsonNode_t* vp, cmJsonNode_t ** retPtrPtr );
  cmJsRC_t cmJsonArrayNode(   const cmJsonNode_t* vp, cmJsonNode_t ** retPtrPtr );
  cmJsRC_t cmJsonObjectNode(  const cmJsonNode_t* vp, cmJsonNode_t ** retPtrPtr );  


  // Return the label from a pair object.
  const char* cmJsonPairLabel(  const cmJsonNode_t* pairPtr );
  unsigned    cmJsonPairTypeId( const cmJsonNode_t* pairPtr );
  cmJsonNode_t* cmJsonPairValue( cmJsonNode_t* pairPtr );

  // Return values associated with the member values in the object
  // pointed to by object objectNodePtr.
  cmJsRC_t      cmJsonUIntMember(   const cmJsonNode_t* objectNodePtr, const char* label, unsigned* retPtr );
  cmJsRC_t      cmJsonIntMember(    const cmJsonNode_t* objectNodePtr, const char* label, int* retPtr );
  cmJsRC_t      cmJsonRealMember(   const cmJsonNode_t* objectNodePtr, const char* label, double* retPtr );
  cmJsRC_t      cmJsonBoolMember(   const cmJsonNode_t* objectNodePtr, const char* label, bool* retPtr );
  cmJsRC_t      cmJsonStringMember( const cmJsonNode_t* objectNodePtr, const char* label, const char** retPtrPtr );

  // Returns array or object nodes.
  cmJsRC_t      cmJsonNodeMember(   const cmJsonNode_t* objectNodePtr, const char* label, cmJsonNode_t** nodePtrPtr );

  // Returns the value of the member pair named by 'label' or NULL if the 
  // named pair does not exist.
  cmJsonNode_t* cmJsonNodeMemberValue( const cmJsonNode_t* np, const char* label );
  
  // Return values for specified pairs from an object node.
  //
  // The var args syntax is: <label>,<typeId>,<valuePtr>.
  // This functionis implemented in terms of cmJsonXXXMember().
  // 
  // Add kOptArgJsFl to <typeId> if the member may not exist in
  // the object - otherwise the function will fail with a
  // kNodeNotFoundJsRC error and errLabelPtr will have the name of the missing pair.
  //
  // Terminate the var args list with NULL.
  //
  // Object,Array, and Pair members are returned as node pointers.
  // 
  // Since kBoolTId does not exist use kTrueTId or kFalseTId to
  // return bool values.

  cmJsRC_t      cmJsonVMemberValues(const cmJsonNode_t* objectNodePtr, const char** errLabelPtrPtr, va_list vl );
  cmJsRC_t      cmJsonMemberValues( const cmJsonNode_t* objectNodePtr, const char** errLabelPtrPtr, ... );

  // If objectNodePtr is set to NULL then the  tree root is used as the base for the search.
  // pathPrefix may be set to NULL.
  cmJsRC_t cmJsonPathToValueNode( cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, const cmJsonNode_t** nodePtrPtr );
  cmJsRC_t cmJsonPathToBool(   cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, bool* retValPtr );
  cmJsRC_t cmJsonPathToInt(    cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, int* retValPtr );
  cmJsRC_t cmJsonPathToUInt(   cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, unsigned* retValPtr );
  cmJsRC_t cmJsonPathToReal(   cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, double* retValPtr );
  cmJsRC_t cmJsonPathToString( cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, const char** retValPtr );
  cmJsRC_t cmJsonPathToPair(   cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, cmJsonNode_t** retValPtr );
  cmJsRC_t cmJsonPathToArray(  cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, cmJsonNode_t** retValPtr );
  cmJsRC_t cmJsonPathToObject( cmJsonH_t h, const cmJsonNode_t* objectNodePtr, const char* pathPrefix, const char* path, cmJsonNode_t** retValPtr );


  // Same as cmJsonMemberValues() except labels may be paths to a given variable.
  // These paths are equivalent to those used in cmJsonFindPathValue()
  // If objectNodePtr is NULL then the JSON tree root is used as the
  // base reference object.
  // If pathPrefix is non-NULL then it is appended to each of the
  // individual paths.
  cmJsRC_t      cmJsonVPathValues(cmJsonH_t h, const char* pathPrefix, const cmJsonNode_t* objectNodePtr, const char** errLabelPtrPtr, va_list vl );
  cmJsRC_t      cmJsonPathValues( cmJsonH_t h, const char* pathPrefix, const cmJsonNode_t* objectNodePtr, const char** errLabelPtrPtr, ... );


  // Set 'typeId' to the type of the new node.
  // Use 'intVal' for the value of int nodes.
  // Use 'realVal' for the value of real nodes.
  // Use 'stringVal' for the label of pairs and the value of string nodes.
  // 'retNodePtrPtr' is optional
  cmJsRC_t      cmJsonCreate( cmJsonH_t h, cmJsonNode_t* parentPtr, unsigned typeId, const char* stringVal, int intVal, double realVal, cmJsonNode_t** newNodePtrPtr );

  // Insert new nodes in the tree.  If the tree is empty then the first
  // inserted node will become the root (this must be an object or array node.).
  cmJsonNode_t* cmJsonCreateObject( cmJsonH_t h, cmJsonNode_t* parentPtr );
  cmJsonNode_t* cmJsonCreateArray(  cmJsonH_t h, cmJsonNode_t* parentPtr );
  cmJsonNode_t* cmJsonCreatePair(   cmJsonH_t h, cmJsonNode_t* parentPtr, const char* label );
  cmJsRC_t      cmJsonCreateString( cmJsonH_t h, cmJsonNode_t* parentPtr, const char* stringValue );
  cmJsRC_t      cmJsonCreateStringN(cmJsonH_t h, cmJsonNode_t* parentPtr, const char* stringValue, unsigned stringCharCnt );
  cmJsRC_t      cmJsonCreateInt(    cmJsonH_t h, cmJsonNode_t* parentPtr, int value );
  cmJsRC_t      cmJsonCreateReal(   cmJsonH_t h, cmJsonNode_t* parentPtr, double value );
  cmJsRC_t      cmJsonCreateBool(   cmJsonH_t h, cmJsonNode_t* parentPtr, bool value );
  cmJsRC_t      cmJsonCreateNull(   cmJsonH_t h, cmJsonNode_t* parentPtr );

  cmJsRC_t      cmJsonCreateStringArray( cmJsonH_t h, cmJsonNode_t* parentPtr, unsigned n, const char** values );
  cmJsRC_t      cmJsonCreateIntArray(    cmJsonH_t h, cmJsonNode_t* parentPtr, unsigned n, const int* values );
  cmJsRC_t      cmJsonCreateRealArray(   cmJsonH_t h, cmJsonNode_t* parentPtr, unsigned n, const double* values );
  cmJsRC_t      cmJsonCreateBoolArray(   cmJsonH_t h, cmJsonNode_t* parentPtr, unsigned n, const bool* values );


  //--------------------------------------------------------------------------------------------------------------
  //
  // Tree creation helper functiosn
  //

  cmJsRC_t    cmJsonSetInt(    cmJsonH_t h, cmJsonNode_t*  np, int         ival );
  cmJsRC_t    cmJsonSetReal(   cmJsonH_t h, cmJsonNode_t * np, double      rval );
  cmJsRC_t    cmJsonSetBool(   cmJsonH_t h, cmJsonNode_t * np, bool        bval );
  cmJsRC_t    cmJsonSetString( cmJsonH_t h, cmJsonNode_t*  np, const char* sval );

  // Insert a pair with a value indicated by 'typeId'.
  // 'stringVal','intVal' and 'realVal' are used as in cmJsonCreate().
  // Return a pointer to the new pair.
  cmJsonNode_t* cmJsonInsertPair( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned typeId, const char* stringVal, int intVal, double realVal );

  // Create a pair node and the associated label and value nodes and insert the pair in a parent object.
  // The object,array and pair creation functions return pointers to the pair value node.
  // These are helper functions that are implemented in terms of cmJsonCreateXXX() function.
  cmJsonNode_t* cmJsonInsertPairObject( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label );
  cmJsonNode_t* cmJsonInsertPairArray(  cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label );
  cmJsonNode_t* cmJsonInsertPairPair(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, const char* pairLabel );
  cmJsRC_t      cmJsonInsertPairInt(    cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, int intVal );
  cmJsRC_t      cmJsonInsertPairReal(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, double realVal );
  cmJsRC_t      cmJsonInsertPairString( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, const char* string );
  cmJsRC_t      cmJsonInsertPairStringN(cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, const char* string, unsigned stringCharCnt );
  cmJsRC_t      cmJsonInsertPairBool(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, bool boolVal );
  cmJsRC_t      cmJsonInsertPairNull(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label );

  cmJsRC_t      cmJsonInsertPairIntArray(    cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const int* values );
  cmJsRC_t      cmJsonInsertPairRealArray(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const double* values );
  cmJsRC_t      cmJsonInsertPairStringArray( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const char** values );
  cmJsRC_t      cmJsonInsertPairBoolArray(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const bool* values );

  // Returns pair pointer
  cmJsonNode_t* cmJsonInsertPairIntArray2(    cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const int* values );
  cmJsonNode_t* cmJsonInsertPairRealArray2(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const double* values );
  cmJsonNode_t* cmJsonInsertPairStringArray2( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const char** values );
  cmJsonNode_t* cmJsonInsertPairBoolArray2(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned n, const bool* values );

  // Insert a pair (same as cmJsonInsertPair()) or if a pair
  // with a matching label/type already exists then replace the 
  // existing value with a the new value.
  //
  // Set matchTypeMask with the type id's of all pair value types
  // which sould be considered a match.  If matchTypeMask is set to
  // kInvalidTId then all value types will match. 
  //
  // Return a pointer to the new or existing pair.
  //
  // When newTypeId == kObjectTId or kArrayTId then 'nv' may optionally be set to an object or array
  // to be set as the new value node for the selected pair.  If 'nv' is NULL then an empty array or
  // object is created as the pair value node.  
  // 
  // When newTypeId == kPairTId then we are inserting/replacing a pair as the value of the selected pair.
  // In this case sv must be set to the new pair label.  'nv' may be optionally
  // set to a new pair value node. If 'nv' is NULL then the the new pair will have a value of type kNullTId
  cmJsonNode_t* cmJsonInsertOrReplacePair( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, unsigned newTypeId, const char* sv, int iv, double dv, cmJsonNode_t* nv );

  // Returns pointer to object node.
  cmJsonNode_t* cmJsonInsertOrReplacePairObject( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, cmJsonNode_t* newObjNodePtr );
  // Returns pointer to array node.
  cmJsonNode_t* cmJsonInsertOrReplacePairArray(  cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, cmJsonNode_t* newArrayNodePtr );
  // Returns pointer to child pair node
  cmJsonNode_t* cmJsonInsertOrReplacePairPair(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, const char* newPairLabel, cmJsonNode_t* newPairValNodePtr );
  cmJsRC_t      cmJsonInsertOrReplacePairInt(    cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, int intVal );
  cmJsRC_t      cmJsonInsertOrReplacePairReal(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, double realVal );
  cmJsRC_t      cmJsonInsertOrReplacePairString( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, const char* string );
  cmJsRC_t      cmJsonInsertOrReplacePairBool(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, bool boolVal );
  cmJsRC_t      cmJsonInsertOrReplacePairNull(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask );

  // Same as the above cmJsonInsertOrReplaceXXX() functions except
  // the function fails if a matching pair is not found.
  //
  // Replace a pair with the same name/type and return a pointer to the 
  // effected pair. If a pair with the same name and type are not 
  // found then no change is made and the function returns NULL. 
  // For newTypeId=kObjectTId,kArrayTId,kPairTId the replaced pair node is blank,
  // in other words it will have no child nodes.
  cmJsonNode_t* cmJsonReplacePair(       cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, unsigned newTypeId, const char* sv, int iv, double dv, cmJsonNode_t* nv );

  // Returns pointer to object node.
  cmJsonNode_t* cmJsonReplacePairObject( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, cmJsonNode_t* newObjNodePtr );
  // Return  pointer to array node.
  cmJsonNode_t* cmJsonReplacePairArray(  cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, cmJsonNode_t* newArrayNodePtr );
  // Returns pointer to child pair node.
  cmJsonNode_t* cmJsonReplacePairPair(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, const char* newPairLabel, cmJsonNode_t* newPairValueNodePtr );
  cmJsRC_t      cmJsonReplacePairInt(    cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, int intVal );
  cmJsRC_t      cmJsonReplacePairReal(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, double realVal );
  cmJsRC_t      cmJsonReplacePairString( cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, const char* string );
  cmJsRC_t      cmJsonReplacePairBool(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask, bool boolVal );
  cmJsRC_t      cmJsonReplacePairNull(   cmJsonH_t h, cmJsonNode_t* objectNodePtr, const char* label, unsigned matchTypeMask );


  // Insert multiple pairs in a parent object. Terminate the pair sets with NULL.
  // Note that pair,int,real,bool, and string  pairs are specified with 3 args: <label>,<typeId>,<value>
  // all others are specified with 2 args: <label>,<typeId>.
  // The last argument in this function must always be NULL.
  cmJsRC_t      cmJsonVInsertPairs( cmJsonH_t h, cmJsonNode_t* objectNodePtr, va_list vl );
  cmJsRC_t      cmJsonInsertPairs(  cmJsonH_t h, cmJsonNode_t* objectNodePtr, ... );


  // Create an object node, fill it with the specified pairs, and return a pointer to
  // the new object node.
  // This function uses same var args syntax as cmJsonInsertPairs()
  cmJsonNode_t* cmJsonVCreateFilledObject( cmJsonH_t h, cmJsonNode_t* parentPtr, va_list vl );
  cmJsonNode_t* cmJsonCreateFilledObject( cmJsonH_t h, cmJsonNode_t* parentPtr, ... );
  
  //--------------------------------------------------------------------------------------------------------------

  // Remove a node from the tree by unlinking it from its 
  // parent and siblings. 
  // Set the freeFl to true if the node memory should also 
  // be released.
  // 
  // If 'freeFl' is false then np->ownerPtr and 
  // np->siblingPtr remain valid when the function returns.
  // Even with the valid pointer however be careful 
  // not to use the node in a way  that it depends
  // on existing in the tree since the parent and
  // siblings will no longer know about it.
  //
  // If np is the root then the internal tree will be
  // cleared (i.e. cmJsonRoot(h) == NULL).
  //
  // This function will fail if 'np' points to the label value
  // of a pair node.  Pair labels cannot be removed because this
  // would result in an invalid tree (all pairs must have two 
  // child nodes where the first node is a string value). If 'np'
  // points to a pair value node then the value node is replaced
  // with a null node to maintain a valid pair structure.
  cmJsRC_t cmJsonRemoveNode( cmJsonH_t h, cmJsonNode_t* np, bool freeFl );

  //--------------------------------------------------------------------------------------------------------------


  // Duplicate the subtree pointed to by 'np' and attach it as a child
  // of the node pointed to by 'parentPtr'.  This function performs a
  // deep copy of the subtree pointed to by np and returns the pointer
  // to the duplicated subtree.
  //
  // 'parentPtr' must be a legal parent for the sub-tree or NULL to not
  // attach the duplicate tree to any parent.
  //
  // The returned value is a pointer to the new subtree. 
  //
  // If an error occurs the return value is NULL and cmJsonErrorCode()
  // can be used to obtain the code associated with the error.
  cmJsonNode_t* cmJsonDuplicateNode( cmJsonH_t h, const cmJsonNode_t* np, cmJsonNode_t* parentPtr ); 

  //--------------------------------------------------------------------------------------------------------------

  // Copy any pairs not found in the destintaion object from the
  // source object.
  // If an error occurs during merging the destination object is
  // returned unmodified.  The most likely cause of an error is a
  // destination pair with the same name but different type 
  // than a source pair.
  cmJsRC_t  cmJsonMergeObjectNodes( cmJsonH_t h, cmJsonNode_t* destObjNodePtr, const cmJsonNode_t* srcObjNodePtr );

  //--------------------------------------------------------------------------------------------------------------

 
 // Validate the tree.
  cmJsRC_t      cmJsonValidateTree( cmJsonH_t h );

  // Validate the tree beginning with np. Note that this function does
  // not print an error on failure but simply returns kValidateFailJsRC.
  cmJsRC_t      cmJsonValidate( const cmJsonNode_t* np );

  // Get the count of bytes required to serialize the tree rooted at 'np'.  
  unsigned      cmJsonSerialByteCount( const cmJsonNode_t* np );

  // Serialize the tree rooted at 'np' into the buffer buf[bufByteCnt].
  cmJsRC_t      cmJsonSerialize(  const cmJsonNode_t* np, void* buf, unsigned bufByteCnt );

  // Serialize the subtree indicated by 'np' or the entire tree
  // if 'np' is NULL. The buffer created by this call will exist 
  // for the life of 'h' or until the next call to cmJsonSerialize().
  // This function is implemented in terms of cmJsonSerialByteCount()
  // and cmJsonSerializeTree().
  cmJsRC_t      cmJsonSerializeTree( cmJsonH_t h, const cmJsonNode_t* np, void** bufPtrPtr, unsigned* bufByteCntPtr);

  // Recreate the objects previously serialzed via cmJsonSerialize().
  // The tree held in the buffer will be reconstructed as a child of
  // altRootPtr (if it is non-NULL) or the internal root.  
  // If altRootPtr is given then it must point to an array 
  // or object node.
  cmJsRC_t      cmJsonDeserialize( cmJsonH_t h, const void* bufPtr, cmJsonNode_t* altRootPtr );

  // Return a string/int/real/null/bool node as a string value.
  cmJsRC_t      cmJsonLeafToString( const cmJsonNode_t* np, cmChar_t* buf, unsigned bufCharCnt );
  
  // Given a CSV file convert it to an array of JSON objects.
  // The first line of the CSV file must contain a comma seperated lists of types.
  // The type labels must be from the set: 'int','real','string','true','false'.
  // Note that either 'true' or 'false' can be use for boolean columns.
  // The seocnd line contains the field names as comma separated quoted strings.
  // For example "column1","column2","column3"
  // The data is presented as comma separated fields.
  // If parentNodePtr is NULL then the array will be created unattached to 
  // the tree.
  // if arrayNodePtrPtr is non-NULL then the array node ptr will be returned.
  cmJsRC_t      cmJsonFromCSV(  cmJsonH_t h, const char* iFn, cmJsonNode_t* parentPtr, cmJsonNode_t** arrayNodePtrPtr  );

  // Write a CSV file from an array of objects.
  // arrayNodePtr must point to an array of objects.
  cmJsRC_t      cmJsonToCSV(   cmJsonH_t h, const char* oFn, const cmJsonNode_t* arrayNodePtr );

  // Print the subtree using 'np as the root.
  void          cmJsonPrintTree( const cmJsonNode_t* np, cmRpt_t* rpt );

  // Print the subtree using 'np' as the root to a file.
  // 'np' is optional and defaults to cmJsonRoot().
  cmJsRC_t      cmJsonWrite( cmJsonH_t h, const cmJsonNode_t* np, const cmChar_t* fn );

  // Return the code of the last error generated.  This is useful for the
  // the cmJsonCreateXXX() functions which do not return error codes but
  // may still fail.
  cmJsRC_t      cmJsonErrorCode( cmJsonH_t h );
  void          cmJsonClearErrorCode( cmJsonH_t h );

  // Validate the tree and print all the nodes.
  cmJsRC_t      cmJsonReport( cmJsonH_t h );

  // Testing stub.
  cmJsRC_t      cmJsonTest( const char* fn, cmCtx_t* ctx );

  //)
  //}

#ifdef __cplusplus
}
#endif

#endif
