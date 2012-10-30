#ifndef cmGrPage_h
#define cmGrPage_h

#ifdef __cplusplus
extern "C" {
#endif
  
  enum
  {
    kHashMarkGrFl = 0x10,
    kHashLabelGrFl= 0x20
  };

  typedef cmHandle_t cmGrPgH_t;
  typedef cmHandle_t cmGrVwH_t;
  typedef cmHandle_t cmGrAxH_t;

  extern cmGrPgH_t   cmGrPgNullHandle;
  extern cmGrVwH_t   cmGrVwNullHandle;
  extern cmGrAxH_t   cmGrAxNullHandle;

  // Create a cmGrPage object.
  cmGrRC_t cmGrPageCreate( cmCtx_t* ctx, cmGrPgH_t* hp, cmGrCbFunc_t cbFunc, void* cbArg );

  // Destroy and release the resources assoc'd with a cmGrPage object;
  cmGrRC_t cmGrPageDestroy( cmGrPgH_t* hp );

  // Return true if the cmGrPage object handle is valid
  bool     cmGrPageIsValid( cmGrPgH_t h );

  // Remove all objects from the page.
  cmGrRC_t cmGrPageClear( cmGrPgH_t h );

  // Intialize the count of rows and columns and setup the default layout.
  cmGrRC_t cmGrPageInit( cmGrPgH_t h, const cmGrPExt_t* r, unsigned rn, unsigned cn, cmGrDcH_t dcH  );

  // Update the position of the views on the page. 
  cmGrRC_t cmGrPageResize( cmGrPgH_t h, const cmGrPExt_t* r, cmGrDcH_t dcH );

  // Return the current page size and loc'n as set by cmGrPageInit() or cmGrPageResize().
  void     cmGrPageRect(   cmGrPgH_t h, cmGrPExt_t* r );

  // Return the count of plot views contained by this page. (rn*cn)
  unsigned cmGrPageViewCount( cmGrPgH_t h );

  // Enable or disable the focus for a given view.
  // Note that the focused view is the view which is the target of controller
  // buttons and scrollbars.  This does not refer to the focused object.
  // Set 'enableFl' if the view is receiving the focus.
  // Clear 'enableFl' if the view is losing focus.
  void      cmGrPageViewFocus( cmGrPgH_t h, unsigned vwIdx, bool enableFl );

  // Return the view which currently has the focus or cmGrVwNullHandle if
  // no view has the focus.
  cmGrVwH_t cmGrPageFocusedView( cmGrPgH_t h );

  // 
  void      cmGrPageLayout( cmGrPgH_t h, cmGrDcH_t dcH );

  // Draw the page.
  void      cmGrPageDraw( cmGrPgH_t h, cmGrDcH_t dcH );

  typedef void (*cmGrLabelFunc_t)( void* arg, cmChar_t* label, unsigned labelCharCnt, cmGrV_t value );
  // Returns id of the new page label function.
  unsigned         cmGrPageLabelFuncRegister( cmGrPgH_t h, cmGrLabelFunc_t func, void* arg, const cmChar_t* label );      
  unsigned         cmGrPageLabelFuncCount(    cmGrPgH_t h );
  unsigned         cmGrPageLabelFuncIndexToId( cmGrPgH_t h, unsigned index );
  unsigned         cmGrPageLabelFuncLabelToId( cmGrPgH_t h, const cmChar_t* label );
  cmGrLabelFunc_t  cmGrPageLabelFunc(         cmGrPgH_t h, unsigned id );
  const cmChar_t*  cmGrPageLabelFuncLabel(    cmGrPgH_t h, unsigned id );
  void*            cmGrPageLabelFuncArg(      cmGrPgH_t h, unsigned id );


  // Get a view handle from the view index.
  cmGrVwH_t cmGrPageViewHandle( cmGrPgH_t h,  unsigned vwIdx );

  // Get a view handle from to cmGrH_t.
  cmGrVwH_t cmGrPageGrHandleToView( cmGrPgH_t h, cmGrH_t grH );

  bool      cmGrViewIsValid( cmGrVwH_t h );

  // Initialize a plot view. title,xLabel, and yLabel are optional.
  cmGrRC_t cmGrViewInit( cmGrVwH_t h, cmGrH_t grH, const cmChar_t* vwTitle, const cmChar_t* xLabel, const cmChar_t* yLabel );

  // Remove all objects from the view.
  cmGrRC_t cmGrViewClear( cmGrVwH_t h );

  // Get the plot views physical extents. This function will return the
  // current view location/size only after a call to cmGrPageDraw().
  // See the implementation note at the top of this file.
  cmGrRC_t cmGrViewPExt( cmGrVwH_t h, cmGrPExt_t* pext );

  bool      cmGrViewHasFocus( cmGrVwH_t h );
  
  // Get the cmGrH_t associated with a view.
  cmGrH_t         cmGrViewGrHandle(      cmGrVwH_t h );

  // kExpandViewGrFl | kSelectHorzGrFl | kSelectVertGrFl
  void            cmGrViewSetCfg(        cmGrVwH_t h, unsigned cfgFlags );
  unsigned        cmGrViewCfg(           cmGrVwH_t h );
  void            cmGrViewSetTitle(      cmGrVwH_t h, const cmChar_t* title );
  const cmChar_t* cmGrViewTitle(         cmGrVwH_t h );
  void            cmGrViewSetFontFamily( cmGrVwH_t h, unsigned id );
  unsigned        cmGrViewFontFamily(    cmGrVwH_t h );
  void            cmGrViewSetFontStyle(  cmGrVwH_t h, unsigned flags );
  unsigned        cmGrViewFontStyle(     cmGrVwH_t h );
  void            cmGrViewSetFontSize(   cmGrVwH_t h, unsigned size );
  unsigned        cmGrViewFontSize(      cmGrVwH_t h );

  // Assign a translation function to be used with cmGrViewValue().
  // cmLeftGrIdx or cmGrRightGrIdx is used to assign y axis translation functions.
  // cmTopGrIdx or cmGrBottomGrIdx is used to assign x axis translation functions.
  // 'pgLabelFuncId' must be a valid page label function id as returned from cmGrPageLabelFuncRegister().
  // or cmGrPageLabelFuncIndexToId().
  void            cmGrViewSetLabelFunc(  cmGrVwH_t h, cmGrAxisIdx_t axisId, unsigned pgLabelFuncId );
  
  typedef enum
  {
    kLocalX_VwId,
    kLocalY_VwId,
    kGlobalX_VwId,
    kGlobalY_VwId,
    kSelX0_VwId,
    kSelY0_VwId,
    kSelX1_VwId,
    kSelY1_VwId,
    kSelW_VwId,
    kSelH_VwId
  } cmGrViewValueId_t;
  const cmChar_t* cmGrViewValue( cmGrVwH_t h, cmGrViewValueId_t id, cmChar_t* buf, unsigned bufCharCnt );

  // Get an axis handle.
  cmGrAxH_t     cmGrViewAxisHandle(    cmGrVwH_t h,  cmGrAxisIdx_t axisIdx  );
  
  bool            cmGrAxisIsValid(            cmGrAxH_t h );
  // kHashMarkGrFl | kHashLabelGrFl
  void            cmGrAxisSetCfg(             cmGrAxH_t h, unsigned cfgFlags );
  unsigned        cmGrAxisCfg(                cmGrAxH_t h );
  void            cmGrAxisSetTitle(           cmGrAxH_t h, const cmChar_t* title );
  const cmChar_t* cmGrAxisTitle(              cmGrAxH_t h  );
  void            cmGrAxisTitleSetFontFamily( cmGrAxH_t h, unsigned id );
  unsigned        cmGrAxisTitleFontFamily(    cmGrAxH_t h );
  void            cmGrAxisTitleSetFontStyle(  cmGrAxH_t h, unsigned flags );
  unsigned        cmGrAxisTitleFontStyle(     cmGrAxH_t h );
  void            cmGrAxisTitleSetFontSize(   cmGrAxH_t h, unsigned size );
  unsigned        cmGrAxisTitleFontSize(      cmGrAxH_t h );

  void            cmGrAxisLabelSetFontFamily( cmGrAxH_t h, unsigned id );
  unsigned        cmGrAxisLabelFontFamily(    cmGrAxH_t h );
  void            cmGrAxisLabelSetFontStyle(  cmGrAxH_t h, unsigned flags );
  unsigned        cmGrAxisLabelFontStyle(     cmGrAxH_t h );
  void            cmGrAxisLabelSetFontSize(   cmGrAxH_t h, unsigned size );
  unsigned        cmGrAxisLabelFontSize(      cmGrAxH_t h );

  // Assign a translation function for the value on this axis.  
  // 'pgLabelFuncId' must be a valid page label function id as returned from cmGrPageLabelFuncRegister().
  // or cmGrPageLabelFuncIndexToId().
  void            cmGrAxisSetLabelFunc(      cmGrAxH_t h, unsigned pgLabelFuncId );

#ifdef __cplusplus
}
#endif

#endif
