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
    kLexErrXmlRC,
    kSyntaxErrorXmlRC
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
    kClosedXmlFl  = 0x0010
  };
  
  typedef struct cmXmlNode_str
  {
    unsigned              flags;
    
    const cmChar_t*       label;
    const cmChar_t*       dataStr;
    
    cmXmlAttr_t*          attr;
    
    struct cmXmlNode_str* parent;
    struct cmXmlNode_str* children;
    struct cmXmlNode_str* sibling;
    
    
  } cmXmlNode_t;

  typedef cmHandle_t cmXmlH_t;
  typedef cmRC_t     cmXmlRC_t;

  extern cmXmlH_t cmXmlNullHandle;

  cmXmlRC_t cmXmlAlloc( cmCtx_t* ctx, cmXmlH_t* hh, const cmChar_t* fn );
  cmXmlRC_t cmXmlFree(  cmXmlH_t* hh );
  bool      cmXmlIsValid( cmXmlH_t h );
  
  cmXmlRC_t cmXmlParse( cmXmlH_t h, const cmChar_t* fn );
  cmXmlRC_t cmXmlClear( cmXmlH_t h );
  void      cmXmlPrint( cmXmlH_t h , cmRpt_t* rpt );

  cmXmlRC_t cmXmlTest( cmCtx_t* ctx, const cmChar_t* fn );
  
#ifdef __cpluspus
}
#endif

#endif
