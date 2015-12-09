#ifndef cmXml_h
#define cmXml_h

#ifdef __cpluspus
extern "C" {
#endif

  enum
  {
    kOkXmlRC = cmOkRC,
    kMemAllocErrXmlRC,
    kLHeapXmlRC
  }; 
  
  typedef struct cmXmlAttr_str
  {
    const cmChar_t*      label;
    const cmChar_t*      value;
    struct cmXmlAttr_str* link;
  } cmXmlAttr_t;
  
  typedef struct cmXmlNode_str
  {
    struct cmXmlNode_str* parent;
    struct cmXmlNode_str* children;
    struct cmXmlNode_str* sibling;
    cmXmlAttr_t*          attr;
  } cmXmlNode_t;

  typedef cmHandle_t cmXmlH_t;
  typedef cmRC_t     cmXmlRC_t;

  extern cmXmlH_t cmXmlNullHandle;

  cmXmlRC_t cmXmlAlloc( cmCtx_t* ctx, cmXmlH_t* hh, const cmChar_t* fn );
  cmXmlRC_t cmXmlFree(  cmXmlH_t* hh );
  bool      cmXmlIsValid( cmXmlH_t h );
  
  cmXmlRC_t cmXmlParse( cmXmlH_t h, const cmChar_t* fn );
  cmXmlRC_t cmXmlClear( cmXmlH_t h );

  
  
#ifdef __cpluspus
}
#endif

#endif
