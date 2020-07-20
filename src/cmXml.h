#ifndef cmXml_h
#define cmXml_h

#ifdef __cpluspus
extern "C" {
#endif

  enum
  {
    kOkXmlRC = cmOkRC,
    kMemAllocErrXmlRC,
    kLHeapXmlRC,
    kSyntaxErrorXmlRC,
    kTestFailXmlRC,
    kInvalidTypeXmlRC,
    kNodeNotFoundXmlRC
  }; 
  
  typedef struct cmXmlAttr_str
  {
    const cmChar_t*       label;
    const cmChar_t*       value;    
    struct cmXmlAttr_str* link;
  } cmXmlAttr_t;

  enum
  {
    kRootXmlFl    = 0x0001,
    kDeclXmlFl    = 0x0002,
    kDoctypeXmlFl = 0x0004,
    kNormalXmlFl  = 0x0008,
    kTypeXmlFlags = kRootXmlFl | kDeclXmlFl | kDoctypeXmlFl | kNormalXmlFl,    
    kClosedXmlFl  = 0x0010
  };
  
  typedef struct cmXmlNode_str
  {
    unsigned              line;     // line number
    unsigned              flags;    // See k???XmlFl
    
    const cmChar_t*       label;    // node label
    const cmChar_t*       dataStr;  // node data string
    
    cmXmlAttr_t*          attr;     // attribute list
    
    struct cmXmlNode_str* parent;   // parent node
    struct cmXmlNode_str* children; // first child node list
    struct cmXmlNode_str* sibling;  // 
    
  } cmXmlNode_t;

  typedef cmHandle_t cmXmlH_t;
  typedef cmRC_t     cmXmlRC_t;

  extern cmXmlH_t cmXmlNullHandle;

  cmXmlRC_t cmXmlAlloc( cmCtx_t* ctx, cmXmlH_t* hh, const cmChar_t* fn );
  cmXmlRC_t cmXmlFree(  cmXmlH_t* hh );
  bool      cmXmlIsValid( cmXmlH_t h );
  
  cmXmlRC_t          cmXmlParse( cmXmlH_t h, const cmChar_t* fn );
  cmXmlRC_t          cmXmlClear( cmXmlH_t h );
  const cmXmlNode_t* cmXmlRoot(  cmXmlH_t h );
  void               cmXmlPrint( cmXmlH_t h, cmRpt_t* rpt );

  const cmXmlNode_t* cmXmlSearch(     const cmXmlNode_t* np, const cmChar_t* label, const cmXmlAttr_t* attrV, unsigned attrN );
  const cmXmlAttr_t* cmXmlFindAttrib( const cmXmlNode_t* np, const cmChar_t* label );

  cmXmlRC_t          cmXmlAttrInt(  const cmXmlNode_t* np, const cmChar_t* attrLabel, int* retRef );
  cmXmlRC_t          cmXmlAttrUInt( const cmXmlNode_t* np, const cmChar_t* attrLabel, unsigned* retRef );
  
  // Return the data value for a node or attributes.
  // Terminate node label list with NULL.
  const cmChar_t*    cmXmlNodeValueV( const cmXmlNode_t* np, va_list vl );
  const cmChar_t*    cmXmlNodeValue( const cmXmlNode_t* np, ... );

  // Terminate node label list with NULL.
  cmXmlRC_t          cmXmlNodeIntV(    const cmXmlNode_t* np, int*      retRef, va_list vl );
  cmXmlRC_t          cmXmlNodeUIntV(   const cmXmlNode_t* np, unsigned* retRef, va_list vl );
  cmXmlRC_t          cmXmlNodeDoubleV( const cmXmlNode_t* np, double*   retRef, va_list vl );

  // Terminate node label list with NULL.
  cmXmlRC_t          cmXmlNodeInt(   const cmXmlNode_t* np, int*      retRef, ... );
  cmXmlRC_t          cmXmlNodeUInt(  const cmXmlNode_t* np, unsigned* retRef, ... );
  cmXmlRC_t          cmXmlNodeDouble(const cmXmlNode_t* np, double*   retRef, ... );

  // Terminate node label list with NULL.
  bool               cmXmlNodeHasChildV(const cmXmlNode_t* np, const cmChar_t* label, va_list vl );
  bool               cmXmlNodeHasChild( const cmXmlNode_t* np, const cmChar_t* label, ... );

  // Last label in list is an attribute label.
  // Terminate the list with NULL.
  bool               cmXmlNodeHasChildWithAttrV( const cmXmlNode_t* np, const cmChar_t* label, va_list vl );
  bool               cmXmlNodeHasChildWithAttr(  const cmXmlNode_t* np, const cmChar_t* label, ... );

  // Last second to last label in the list is an attribute label.
  // THe last label in the list is an attribute value.
  // Terminate the list with NULL.
  bool               cmXmlNodeHasChildWithAttrAndValueV( const cmXmlNode_t* np, const cmChar_t* label, va_list vl );
  bool               cmXmlNodeHasChildWithAttrAndValue(  const cmXmlNode_t* np, const cmChar_t* label, ... );
  
  
  cmXmlRC_t cmXmlTest( cmCtx_t* ctx, const cmChar_t* fn );
  
#ifdef __cpluspus
}
#endif

#endif
