//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmGrTimeLine_h
#define cmGrTimeLine_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Device indenpendent, multi-axis, interactive, plotting system based on cmGrPage, cmGrAxis and cmGr." kw:[plot]}
  
  enum
  {
    kOkGrPlRC,
    kObjNotFoundGrPlRC,
    kGrFailGrPlRC,
    kRsrcFailGrPlRC
  };

  // Graphic object type id's
  typedef enum
  {
    kInvalidPlId,
    kRectGrPlId,
    kHLineGrPlId,
    kVLineGrPlId,
    kLineGrPlId,
    kEllipseGrPlId,
    kDiamondGrPlId,
    kUTriGrPlId,
    kDTriGrPlId,
    kLTriGrPlId,
    kRTriGrPlId,
    kStarGrPlId,
    kCrossGrPlId,
    kPlusGrPlId,
  } cmGrPlObjTypeId_t;

  // Object cfg. flags
  enum
  {
    kSymbolGrPlFl   = 0x0001,  // This object is a symbol
    kNoSelectGrPlFl = 0x0002,  // The clicking with the mouse will not select this object
    kNoDragGrPlFl   = 0x0004,  // Dragging with the mouse will not move this object
    kNoFocusGrPlFl  = 0x0008,  // This object cannot receive focus.
    kNoDrawGrPlFl   = 0x0010,  // Do not draw this object (is is invisible and disabled)
    kNoFillGrPlFl   = 0x0020,  // Do not draw the fill area of this object
    kNoBorderGrPlFl = 0x0040,  // Do not draw the border of this object
    kNoLabelGrPlFl  = 0x0080,  // Do not draw the label for this object
    kBorderSelGrPlFl= 0x0100,  // This object is selected by clicking near it's border
  };

  // object state flags
  enum
  {
    kVisibleGrPlFl = 0x0001,  //  
    kEnabledGrPlFl = 0x0002,  // Enabled obj's must be visible. 
    kSelectGrPlFl  = 0x0004,  // This object is selected - multiple objects may be selected
    kFocusGrPlFl   = 0x0008   // Focused obj's are also selected - only one object can have the focus.
  };

  typedef enum
  {
    kFocusPlGrId, 
    kSelectPlGrId, 
    kEnablePlGrId, 
    kDisablePlGrId, 

    kMaxPlGrId
  } cmGrPlStateId_t; 

  enum // TODO: change these into cmGrPlotH_t user settable variables
  {
    kDefaultSymW = 9,
    kDefaultSymH = 9
  };

  typedef cmHandle_t cmGrPlH_t;  
  typedef cmHandle_t cmGrPlObjH_t;
  typedef cmRC_t     cmGrPlRC_t;

  // Plot object callback id's
  typedef enum
  {
    kCreatedCbSelGrPlId,
    kDestroyedCbSelGrPlId,
    kPreEventCbSelGrPlId,
    kEventCbSelGrPlId,
    kStateChangeGrPlId,          // See Note with cmGrPlotCbFunc_t below. 
  } cmGrPlCbSelId_t;
  

  typedef struct
  {
    cmCtx_t*        ctx;          // Global program context.
    void*           cbArg;        // User pointer set in cmGrPlotObjSetCb().
    cmGrPlCbSelId_t selId;        // Callback selector id. See cmGrPlCbSeId_t.
    cmGrPlObjH_t    objH;         // The plot object handle.
    unsigned        eventFlags;   // Event flags from canvas callback (See cmGrEvent() flags)
    cmGrKeyCodeId_t eventKey;     // Event keys (See the cmGrEvent() keys)
    int             eventX;       // Mouse X,Y location when event was generated (Same as cmGrEvent()) 
    int             eventY;       // 
    unsigned        deltaFlags;   // Event which caused an object state change (See kXXXGrPlFl flags) 
  } cmGrPlotCbArg_t;


  // Return 'false' from kPreEventCbGrPlSelId events to prevent default processing.
  // Note:
  // When this callback is made with the 'kStateChangeGrPlId' the state of
  // the object has not yet been changed.  This may be confusing because if 
  // the state of the object is queried inside the callback it will have the 
  // pre-change state - but this state will be automatically toggled when the 
  // callback returns  'true'.  Examine the arg->deltaFlags to determine the
  // state attribute which is changing.
  typedef bool (*cmGrPlotCbFunc_t)( cmGrPlotCbArg_t* arg );

  extern cmGrPlH_t    cmGrPlNullHandle;
  extern cmGrPlObjH_t cmGrPlObjNullHandle;

  // Notes:
  // 1) Set kSymbolGrPlFl to create a symbol.
  // 2) If kSymbolGrPlFl is set then w and h are taken as the physical size 
  //    of the symbol. Set w and h to 0 to use the default symbols size
  //    kDefaultSymW, kDefaultSymH
  
  cmGrPlRC_t      cmGrPlotObjCreate( 
    cmGrPlH_t         plH,          // Owner Plot Object Manager. See cmGrPlotCreate().
    cmGrH_t           grH,          // The canvas this object will be drawn on.
    cmGrPlObjH_t*     ohp,          // Pointer to the new objects handle (optional)
    unsigned          id,           // User defined identifier.
    cmGrPlObjH_t      parentObjH,   // Containing parent object.
    cmGrPlObjH_t      xAnchorObjH,  // x is taken as an offset from this obj's x coord (optional).
    cmGrPlObjH_t      yAnchorObjH,  // y is taken as an offset from this obj's y coord (optional).
    cmGrPlObjTypeId_t typeId,       // See cmGrPlObjTypeId_t
    unsigned          cfgFlags,     // 
    cmReal_t          x,            // Coord's within the parent's world coord system.
    cmReal_t          y,            //
    cmReal_t          w,            // 
    cmReal_t          h,            // 
    const cmChar_t*   label,        // Object text string (optional) 
    const cmGrVExt_t* wext );       // This objects internal world extents (optional)

  cmGrPlRC_t        cmGrPlotObjDestroy(       cmGrPlObjH_t* ohp );
  bool              cmGrPlotObjIsValid(       cmGrPlObjH_t oh );

  cmGrPlH_t         cmGrPlotObjMgrHandle(     cmGrPlObjH_t oh );
  cmGrObjH_t        cmGrPlotObjHandle(        cmGrPlObjH_t oh );

  cmGrPlObjH_t      cmGrPlotObjParent(        cmGrPlObjH_t oh );
  cmGrPlObjH_t      cmGrPlotObjXAnchor(       cmGrPlObjH_t oh );
  cmGrPlObjH_t      cmGrPlotObjYAnchor(       cmGrPlObjH_t oh );
  
  void              cmGrPlotObjSetId(         cmGrPlObjH_t oh, unsigned id );
  unsigned          cmGrPlotObjId(            cmGrPlObjH_t oh );

  void              cmGrPlotObjSetUserPtr(    cmGrPlObjH_t oh, void* userPtr );
  void              cmGrPlotObjAllocUser(     cmGrPlObjH_t oh, const void* data, unsigned byteCnt );
  void*             cmGrPlotObjUserPtr(       cmGrPlObjH_t oh );

  void              cmGrPlotObjSetLabel(      cmGrPlObjH_t oh, const cmChar_t* label );
  const cmChar_t*   cmGrPlotObjLabel(         cmGrPlObjH_t oh );
  // Set flags to kXXXJsGrFl values.  See cmGrDrawTextJustify for their meaning.
  // 'color' is optional
  void              cmGrPlotObjSetLabelAttr(  cmGrPlObjH_t oh, unsigned flags, int angle, const cmGrColor_t color );
  unsigned          cmGrPlotObjLabelFlags(    cmGrPlObjH_t oh );
  int               cmGrPlotObjLabelAngle(    cmGrPlObjH_t oh );
  cmGrColor_t       cmGrPlotObjLabelColor(  cmGrPlObjH_t oh );
  

  void              cmGrPlotObjSetStateFlags( cmGrPlObjH_t oh, unsigned flags );
  unsigned          cmGrPlotObjStateFlags(    cmGrPlObjH_t oh  );

  void              cmGrPlotObjSetCfgFlags(   cmGrPlObjH_t oh, unsigned flags );
  void              cmGrPlotObjClrCfgFlags(   cmGrPlObjH_t oh, unsigned flags );
  void              cmGrPlotObjTogCfgFlags(   cmGrPlObjH_t oh, unsigned flags );
  unsigned          cmGrPlotObjCfgFlags(      cmGrPlObjH_t oh );

  cmGrPlRC_t        cmGrPlotObjSetPhysExt(    cmGrPlObjH_t oh, int  loffs, int  toffs, int  roffs, int  boffs );
  void              cmGrPlotObjPhysExt(       cmGrPlObjH_t oh, int* loffs, int* toffs, int* roffs, int* boffs  );

  void              cmGrPlotObjVExt(          cmGrPlObjH_t oh, cmGrVExt_t* vext  );

  void              cmGrPlotObjSetFontFamily( cmGrPlObjH_t h, unsigned id );
  unsigned          cmGrPlotObjFontFamily(    cmGrPlObjH_t h );
  void              cmGrPlotObjSetFontSize(   cmGrPlObjH_t h, unsigned size );
  unsigned          cmGrPlotObjFontSize(      cmGrPlObjH_t h );
  void              cmGrPlotObjSetFontStyle(  cmGrPlObjH_t h, unsigned flags );
  unsigned          cmGrPlotObjFontStyle(     cmGrPlObjH_t h );
  void              cmGrPlotObjSetFont(       cmGrPlObjH_t h, unsigned id, unsigned size, unsigned style );

  void              cmGrPlotObjSetLineColor(  cmGrPlObjH_t h, cmGrPlStateId_t id, const cmGrColor_t c );
  cmGrColor_t       cmGrPlotObjLineColor(     cmGrPlObjH_t h, cmGrPlStateId_t id );
  cmGrColor_t       cmGrPlotObjCurLineColor(  cmGrPlObjH_t h );

  void              cmGrPlotObjSetFillColor(  cmGrPlObjH_t h, cmGrPlStateId_t id, const cmGrColor_t c );
  cmGrColor_t       cmGrPlotObjFillColor(     cmGrPlObjH_t h, cmGrPlStateId_t id );
  cmGrColor_t       cmGrPlotObjCurFillColor(  cmGrPlObjH_t h );

  void              cmGrPlotObjSetCb(        cmGrPlObjH_t h, cmGrPlotCbFunc_t func, void* arg );
  cmGrPlotCbFunc_t  cmGrPlotObjCbFunc(       cmGrPlObjH_t h );
  void*             cmGrPlotObjCbArg(        cmGrPlObjH_t h );

  
  // Draw aH above bH in the z-order.
  void            cmGrPlotObjDrawAbove(     cmGrPlObjH_t bH, cmGrPlObjH_t aH );

  //----------------------------------------------------------------------------
  // Plot Object Manager Functions
  //----------------------------------------------------------------------------
  cmGrPlRC_t   cmGrPlotCreate(  cmCtx_t* ctx, cmGrPlH_t* hp );
  cmGrPlRC_t   cmGrPlotDestroy( cmGrPlH_t* hp );
  bool         cmGrPlotIsValid( cmGrPlH_t h );
  cmGrPlRC_t   cmGrPlotClear(   cmGrPlH_t h ); // destroy all objects
  cmErr_t*     cmGrPlotErr(     cmGrPlH_t h );
  cmRpt_t*     cmGrPlotRpt(     cmGrPlH_t h );
  
  // Return the count of plot objects.
  unsigned     cmGrPlotObjectCount(   cmGrPlH_t h );

  // Return the handle of the ith object (0<=index<cmGrPlotObjectCount())
  cmGrPlObjH_t cmGrPlotObjectIndexToHandle( cmGrPlH_t h, unsigned index );

  // Given a plot object id return the associated object handle.
  cmGrPlObjH_t cmGrPlotObjectIdToHandle( cmGrPlH_t h, unsigned id );

  // Callback func(arg,objH) for every object. 
  typedef void (*cmGrPlotObjCbFunc_t)( void* arg, cmGrPlObjH_t oh );
  void         cmGrPlotObjCb( cmGrPlH_t h, cmGrPlotObjCbFunc_t func, void* arg );

  // Pass a keyboard event to the plot system.
  void         cmGrPlotKeyEvent(   cmGrPlH_t h, cmGrH_t grH, unsigned eventFlags, cmGrKeyCodeId_t keycode );
  
  // Set the default object callback and arg.
  void         cmGrPlotSetCb( cmGrPlH_t h, cmGrPlotCbFunc_t func, void* arg );

  //)

#ifdef __cplusplus
}
#endif

#endif


/*
Plot Object Attributes:

Location: x,y
Size:     w,h

Shape:
  rectangle:
  ellipse:
  line:
  hline:
  vline:
  symbol:

Parent: Defines the world coordinate system in which this object is drawn.
Children are always fully contained by their parent and may not be dragged
outside of their parent.

Label: 
Border Color: One per state (enabled,selected,focused)
Fill Color:   One per state (enabled,selected,focused)
State:
  Visible:  Draw this object.
  Enabled:  Disabled objects cannot be selected,focused, or dragged.
  Selected: Multiple objects may be selected. 
  Focused:  Only one object may be focused.

Physical Offsets: Physical offsets which expand the size of the object.
Font Family:
Font Style:
Font Size:
Cfg Flags:
  No Drag:   Do not allow dragging.
  No Select: Do not allow selection.
  No Focus:  Do not allow focusing.
  No Draw:   Do not draw this object (automatically disabled the object)
  No Fill:   Do not draw fill color.
  No Border: Do not draw border.
  No Label:  Do not draw label.  

 */
