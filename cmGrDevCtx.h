#ifndef cmGrDevCtx_h
#define cmGrDevCtx_h

#ifdef __cplusplus
extern "C" {
#endif
  //( { file_desc:"Device independent graphics context object used by cmGr." kw:[plot]}
  
  enum
  {
    kOkGrDcRC = cmOkRC,
    kStackFaultGrDcRC,
    kDevDrvFailGrDcRC
  };

  typedef cmRC_t     cmGrDcRC_t;

  extern cmGrDcH_t cmGrDcNullHandle;

  enum
  {
    kSolidLsGrFl = 0x01,
    kDashLsGrFl  = 0x02,
    kDotLsGrFl   = 0x04
  };

  enum
  {
    kHelveticaFfGrId,
    kTimesFfGrId,
    kCourierFfGrId,

    kFontFfCnt
  };

  enum
  {
    kNormalFsGrFl = 0x00,
    kBoldFsGrFl   = 0x01,
    kItalicFsGrFl = 0x02
  };


  typedef struct cmGrDev_str
  {
    // return true on success
    bool            (*create)( void* arg, unsigned w, unsigned h );
    void            (*destroy)( void* arg );

    void            (*begin_draw)( void* arg );
    void            (*end_draw)(   void* arg );
    void            (*draw)(       void* arg, int x, int y );


    void            (*set_color)(  void* arg, const cmGrColor_t c  );
    void            (*get_color)(  void* arg, cmGrColor_t* c );

    // Return false if the 'font' label is not recognized.
    void            (*set_font_family)(void* arg, unsigned fontId );
    unsigned        (*get_font_family)(void* arg );

    void            (*set_font_style)( void* arg, unsigned styleFLags );
    unsigned        (*get_font_style)( void* arg );

    void            (*set_font_size)(  void* arg, unsigned size );
    unsigned        (*get_font_size)(  void* arg );

    void            (*set_pen_style)(  void* arg, unsigned styleFlags );
    unsigned        (*get_pen_style)(  void* arg );

    void            (*set_pen_width)(  void* arg, unsigned w );
    unsigned        (*get_pen_width)(  void* arg );

    void (*draw_line)(     void* arg, int x, int y, int x1, int y1 );
    void (*draw_rect)(     void* arg, int x, int y, unsigned w, unsigned h );
    void (*fill_rect)(     void* arg, int x, int y, unsigned w, unsigned h );

    // Draw an ellipse, diamond or triangle inside the rectangle formed by l,t,w,h.
    void (*draw_ellipse)(  void* arg, int x, int y, unsigned w, unsigned h );
    void (*fill_ellipse)(  void* arg, int x, int y, unsigned w, unsigned h );
    void (*draw_diamond)(  void* arg, int x, int y, unsigned w, unsigned h );
    void (*fill_diamond)(  void* arg, int x, int y, unsigned w, unsigned h );
    void (*draw_triangle)( void* arg, int x, int y, unsigned w, unsigned h, unsigned dirFlag );
    void (*fill_triangle)( void* arg, int x, int y, unsigned w, unsigned h, unsigned dirFlag );

    // x,y identifies the left,lower text edge
    void (*draw_text)(    void* arg, const char* text, int x, int y );
    void (*draw_text_rot)(void* arg, const char* text, int x, int y, int angle );
    void (*measure_text)( void* arg, const char* text, unsigned* w, unsigned* h );

    // Fill p[w*h*3] with RGB data. 
    void (*read_image)( void* arg,       unsigned char* p, int x, int y, unsigned w, unsigned h );
    void (*draw_image)( void* arg, const unsigned char* p, int x, int y, unsigned w, unsigned h );

  } cmGrDev_t;

  cmGrDcRC_t      cmGrDevCtxCreate( cmCtx_t* ctx, cmGrDcH_t* hp, cmGrDev_t* dd, void* ddArg, int x, int y, int w, int h );
  cmGrDcRC_t      cmGrDevCtxDestroy(  cmGrDcH_t* hp );
  bool            cmGrDevCtxIsValid(  cmGrDcH_t h );

  cmGrDcRC_t      cmGrDevCtxResize( cmGrDcH_t h, int x, int y, int ww, int hh );
  void            cmGrDevCtxSize(   cmGrDcH_t h, cmGrPExt_t* pext );

  void            cmGrDevCtxBeginDraw( cmGrDcH_t h );
  void            cmGrDevCtxEndDraw(   cmGrDcH_t h );
  void            cmGrDevCtxDraw(      cmGrDcH_t h );


  void            cmGrDcPushCtx(      cmGrDcH_t h );
  void            cmGrDcPopCtx(       cmGrDcH_t h );

  unsigned        cmGrDcColor(        cmGrDcH_t h );
  void            cmGrDcSetColorRgb(  cmGrDcH_t h, unsigned char r, unsigned char g, unsigned char b );
  void            cmGrDcSetColor(     cmGrDcH_t h, cmGrColor_t color );

  unsigned        cmGrDcFontFamily(    cmGrDcH_t h );
  void            cmGrDcSetFontFamily( cmGrDcH_t h, unsigned fontId );

  unsigned        cmGrDcFontStyle(      cmGrDcH_t h );
  void            cmGrDcSetFontStyle(   cmGrDcH_t h, unsigned style );
  
  unsigned        cmGrDcFontSize(      cmGrDcH_t h );
  void            cmGrDcSetFontSize(   cmGrDcH_t h, unsigned size );

  unsigned        cmGrDcPenWidth(      cmGrDcH_t h );
  void            cmGrDcSetPenWidth(   cmGrDcH_t h, unsigned width );

  unsigned        cmGrDcPenStyle(      cmGrDcH_t h );
  void            cmGrDcSetPenStyle(   cmGrDcH_t h, unsigned style );

  void            cmGrDcDrawLine(     cmGrDcH_t h, int x,  int y, int x1, int y1 );
  // x,y is the upper,left.
  void            cmGrDcDrawRect(     cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh );
  void            cmGrDcDrawRectPExt( cmGrDcH_t h, const cmGrPExt_t* pext );

  void            cmGrDcFillRect(     cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh );
  void            cmGrDcDrawEllipse(  cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh );
  void            cmGrDcFillEllipse(  cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh );

  void            cmGrDcDrawDiamond(  cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh );
  void            cmGrDcFillDiamond(  cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh );

  // Set 'dirFlag' to kTopGrFl,kBottomGrFl,kRightGrFl,kLeftGrFl to indicate 
  // the direction the triangle is pointeed. 
  void            cmGrDcDrawTriangle( cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh, unsigned dirFlag );
  void            cmGrDcFillTriangle( cmGrDcH_t h, int x,  int y,  unsigned ww, unsigned hh, unsigned dirFlag );

  void            cmGrDcMeasure(     cmGrDcH_t h, const cmChar_t* text, cmGrPSz_t* sz );  
  void            cmGrDcDrawText(    cmGrDcH_t h, const cmChar_t* text, int x, int y );
  void            cmGrDcDrawTextRot( cmGrDcH_t h, const cmChar_t* text, int x, int y, int angle );

  void            cmGrDcReadImage(   cmGrDcH_t h,       unsigned char* p, const cmGrPExt_t* pext );
  void            cmGrDcDrawImage(   cmGrDcH_t h, const unsigned char* p, const cmGrPExt_t* pext );

  //
  // Composite Functions
  //

  void            cmGrDcSetFont( cmGrDcH_t h, unsigned fontId, unsigned size, unsigned style );
  void            cmGrDcFontSetAndMeasure(cmGrDcH_t h, unsigned fontId, unsigned size, unsigned style, const cmChar_t* text, cmGrPSz_t* sz );

  enum 
  { 
    kLeftJsGrFl    = 0x001, 
    kRightJsGrFl   = 0x002, 
    kTopJsGrFl     = 0x004, 
    kBottomJsGrFl  = 0x008, 
    kHorzCtrJsGrFl = 0x010, 
    kVertCtrJsGrFl = 0x020, 

    kNorthJsGrFl   = 0x040,
    kEastJsGrFl    = 0x080,
    kSouthJsGrFl   = 0x100,
    kWestJsGrFl    = 0x200
  };

  // Use compass (NSEW) flags to select the draw point. Defaults to center for both dir's.
  // Use TBLF flags to select the text justification relative to the point.
  // In effect the TBLF flags select the corner of the text to place at the location of
  // the point selected by the NSEW flags.
  // If neither NS flag is set then the vertical point is set to the vertical center.
  // If neither EW flags is set then the horizontal point is set to the horzontal center.
  void            cmGrDcDrawTextJustify(    cmGrDcH_t h, unsigned fontId, unsigned size, unsigned style, const cmChar_t* text, const cmGrPExt_t* pext, unsigned flags );

  // Use LBLF to set the justification - the text corner to match to the given point.
  // If neither TL  flag is given then the point is matched to the vertical center of the text.
  // If neither RL  flag is given then the point is matched to the horizontal center of the text.
  void            cmGrDcDrawTextJustifyPt(  cmGrDcH_t h, unsigned fontId, unsigned size, unsigned style, const cmChar_t* text, unsigned flags, int x, int y );

  // Return the rectangle around the text but do not display the text.
  void            cmGrDcDrawTextJustifyRect( cmGrDcH_t h, unsigned fontId, unsigned size, unsigned style, const cmChar_t* text, unsigned flags, int x, int y, cmGrPExt_t* pext );

  // Is the point x,y visible in this drawing context.
  bool            cmGrDcPointIsVisible( cmGrDcH_t h, int x, int y );

  // Is any of the rectangle visible in this drawing context.
  bool            cmGrDcRectIsVisible(  cmGrDcH_t h, const cmGrPExt_t* r );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
