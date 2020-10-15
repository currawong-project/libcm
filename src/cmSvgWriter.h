#ifndef cmSvgWriter_h
#define cmSvgWriter_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"SVG file writer." kw[file plot] }
  

enum
{
  kOkSvgRC = cmOkRC,
  kFileFailSvgRC,
  kPrintFailSvgRC,
  kLHeapFailSvgRC
};

  typedef cmRC_t     cmSvgRC_t;
  typedef cmHandle_t cmSvgH_t;

  extern cmSvgH_t cmSvgNullHandle;
  
  cmSvgRC_t cmSvgWriterAlloc( cmCtx_t* ctx, cmSvgH_t* hp );
  cmSvgRC_t cmSvgWriterFree(    cmSvgH_t* hp );
  bool      cmSvgWriterIsValid( cmSvgH_t h );
  
  cmSvgRC_t cmSvgWriterRect( cmSvgH_t h, double  x, double y,  double ww, double hh, const cmChar_t* cssClassLabel );
  cmSvgRC_t cmSvgWriterLine( cmSvgH_t h, double x0, double y0, double x1, double y1, const cmChar_t* cssClassLabel );
  cmSvgRC_t cmSvgWriterText( cmSvgH_t h, double  x, double y,  const cmChar_t* text, const cmChar_t* cssClassLabel );

  // Write the SVG file.  Note that header on this file references the CSS file 'cssFn'
  // and the Javascript file svg-pan-zoom.min.js from https://github.com/ariutta/svg-pan-zoom.
  // Both the CSS file and svg-pan-zoom.min.js should therefore be in the same directory
  // as the output HTML file.
  cmSvgRC_t cmSvgWriterWrite( cmSvgH_t h, const cmChar_t* cssFn, const cmChar_t* outFn, bool standaloneFl, bool panZoomFl );

  //)
  
#ifdef __cplusplus
}
#endif


#endif
