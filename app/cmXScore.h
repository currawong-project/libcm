#ifndef cmXScore_h
#define cmXScore_h

#ifdef __cplusplus
extern "C" {
#endif

  enum
  {
    kOkXsRC = cmOkRC,
    kXmlFailXsRC,
    kLHeapFailXsRC,
    kSyntaxErrorXsRC,
    kCsvFailXsRC,
    kUnterminatedTieXsRC,
    kUnterminatedSlurXsRC,
    kUnterminatedOctaveShiftXsrRC,
    kPedalStateErrorXsRc,
    kMidiFailXsRC,
    kFileFailXsRC,
    kSvgFailXsRC,
    kOverlapWarnXsRC,
    kZeroLengthEventXsRC
  };

  typedef cmRC_t     cmXsRC_t;
  typedef cmHandle_t cmXsH_t;

  extern cmXsH_t cmXsNullHandle;

  // Prepare the MusicXML file:
  //
  // 1) Convert XML to UTF-8:
  //       a. Change: encoding           = 'UTF-16' to encoding='UTF-8'


  //       b. Emacs C-x <RET> f utf-8 <RET>
  //       c. Change: <?xml ... encoding = 'UTF-16' to encoding='UTF-8' ...?>
  //
  // 2) Replace "DoletSibelius Unknown Symbol Index " with "DoletSibelius unknownSymIdx="
  //
  // Steps 1) and 2) can be automated by in emacs by:
  //
  // M-x load-file ~/src/emacs/proc_music_xml.el
  //

  // Initialize an cmXScore object from a Sibelius generated MusicXML file.
  // 'editFn' is used to add additional information to the score.
  // See cmXScoreGenEditFile()
  cmXsRC_t cmXScoreInitialize( cmCtx_t* ctx, cmXsH_t* hp, const cmChar_t* xmlFn, const cmChar_t* editFn );
  cmXsRC_t cmXScoreFinalize( cmXsH_t* hp );

  
  bool     cmXScoreIsValid( cmXsH_t h );

  cmXsRC_t cmXScoreWriteCsv( cmXsH_t h, int beginMeasNumb, const cmChar_t* csvFn );

  void     cmXScoreReport( cmXsH_t h, cmRpt_t* rpt, bool sortFl );

  // Generate a template 'edit file'. This file can be edited by hand to included additional
  // information in the score. See the 'editFn' argument to cmXScoreInitialize() for where
  // this file is used.
  cmXsRC_t cmXScoreGenEditFile( cmCtx_t* ctx, const cmChar_t* xmlFn, const cmChar_t* outFn );

  // Generate the CSV file suitable for use by cmScore.
  //
  // If the file referenced by 'reorderFn' exists then it is used to attach additional
  // score information.  If it does not then a new edit file is created via an
  // internal call to cmXScoreGenEditFile().  This file can then be edited
  // to include the additional score file information and passed back by a later
  // call to this same function.
  // Set reportFl to true to print a report of the score following processing.
  // Set begMeasNumb to the first measure the to be written to the output csv, MIDI and SVG files.
  // Set begBPM to 0 to use the tempo from the score otherwise set it to the tempo at begMeasNumb.
  cmXsRC_t cmXScoreTest( cmCtx_t* ctx, const cmChar_t* xmlFn, const cmChar_t* reorderFn, const cmChar_t* csvOutFn, const cmChar_t* midiOutFn, bool reportFl, int begMeasNumb, int begBPM );
  
#ifdef __cplusplus
}
#endif

#endif
