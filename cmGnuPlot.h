#ifndef cmGnuPlot_h
#define cmGnuPlot_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Interface to GNU Plot." kw:[plot]}
  
  enum
  {
    kOkPlRC,
    kSignalFailedPlRC,
    kPipeFailedPlRC,
    kForkFailedPlRC,
    kExecFailedPlRC,
    kPipeCloseFailedPlRC,
    kPlotNotFoundPlRC,
    kNoCurPlotPlRC,
    kPlotDataFileFailPlRC,
    kFopenFailedPlRC,
    kFcloseFailedPlRC
  };

  enum
  {
    kInvalidPlotPtId    = 0x000,
    kPlusPlotPtId       = 0x001,
    kXPlotPtId          = 0x002,
    kAsteriskPlotPtId   = 0x003,
    kSquarePlotPtId     = 0x004,
    kFillSquarePlotPtId = 0x005,
    kCirclePlotPtId     = 0x006,
    kFillCirclePlotPtId = 0x007,
    kTriPlotPtId        = 0x008,
    kFillTriPlotPtId    = 0x009,
    kInvTriPlotPtId     = 0x00a,
    kInvFillTriPlotPtId = 0x00b,
    kDiamondPlotPtId    = 0x00c,
    kFillDiamonPlotPtId = 0x00d,
    kPlotPtMask         = 0x00f,
  };

  enum
  {
    kInvalidPlotLineId   = 0x000,  // -2 after translation
    kSolidPlotLineId     = 0x010,  // -1 after translation
    kDashPlotLineId      = 0x020,  //  0 after translation
    kPlotLineMask        = 0x0f0,
    kPlotLineShift       = 4
  };

  enum 
  {
    kImpulsePlotFl       = 0x100
  };

  /// Set terminal to NULL to use the default terminal.
  cmRC_t cmPlotInitialize( const char* terminalStr );

  // Combines initializaion and setup in a single call.
  cmRC_t cmPlotInitialize2( const char* terminalStr, const char* title, unsigned rowCnt, unsigned colCnt );

  cmRC_t cmPlotFinalize();

  /// Setup the plot page
  cmRC_t cmPlotSetup( const char* title, unsigned rowCnt, unsigned colCnt );

  /// Select sub-plot to apply subsequent commands to
  cmRC_t cmPlotSelectSubPlot( unsigned ri, unsigned ci );

  /// Clear the current current subplot 
  cmRC_t cmPlotClear();

  /// Set the labels on the current sub-plot
  cmRC_t cmPlotSetLabels( const char* title, const char* xLabelStr, const char* yLabelStr, const char* zLabelStr ); 

  /// Set the default ranges for the x, y and z axes. To leave the ranges at their current values set the min and max to -1.
  /// The range values are used to form data sets when data is not explicitely given.
  cmRC_t cmPlotSetRange( double xMin, double xMax, double yMin, double yMax, double zMin, double zMax ); 

  /// If x or y is given as NULL then the values will be taken from the range settings xMin:xMax or yMin:yMax.
  /// Use the gnuplot command:'test' to see the valid lineType and pointType values for a given terminal
  /// Color string may be any of the predefined color labels: show palette colornames or and rgb value: e.g. #FF00FF
  cmRC_t cmPlotLineF( const char* legendStr, const float*  x, const float*  y, const float*  z, unsigned n, const char* colorStr, unsigned styleFlags );
  cmRC_t cmPlotLineD( const char* legendStr, const double* x, const double* y, const double* z, unsigned n, const char* colorStr, unsigned styleFlags );

  cmRC_t cmPlotLineMD( const double* x, const double* y, const double* z, unsigned rn, unsigned cn, unsigned styleFlags );


#if CM_FLOAT_SMP == 1
#define cmPlotLineS cmPlotLineF
#else
#define cmPlotLineS cmPlotLineD
#endif

#if CM_FLOAT_REAL == 1
#define cmPlotLineR cmPlotLineF
#else
#define cmPlotLineR cmPlotLineD
#endif


  cmRC_t cmPlotDraw();
  cmRC_t cmPlotPrint( bool printDataFl );
  cmRC_t cmPlotDrawAndPrint( bool printDataFl );

  //)
  
#ifdef __cplusplus
}
#endif

#endif
