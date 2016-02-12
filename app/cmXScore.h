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
    kUnterminatedSlurXsRC
  };

  typedef cmRC_t     cmXsRC_t;
  typedef cmHandle_t cmXsH_t;

  extern cmXsH_t cmXsNullHandle;

  // Prepare the MusicXML file:
  //
  // 1) Convert XML to UTF-8:
  //       a. Change: encoding='UTF-16' to encoding='UTF-8'
  //       b. Emacs C-x <RET> f utf-8 <RET>
  //       c. Change: <?xml ... encoding='UTF-16' to encoding='UTF-8' ...?>
  //
  // 2) Replace "DoletSibelius Unknown Symbol Index" with "DoletSibelius unknownSymIdx"
  //
  // 3) How to assigned dynamic markings (they are not attached to notes). (from MIDI file?)
  // 4) Tempo syntax is inconsistent (only a problem in full part2 score)     
  // 5) Heel is being parsed but not used. 
  // 6) Sostenuto pedal events are not being parsed because they are not pedal events.
  // 7) What is a 'pedal-change' event vs. a 'pedal-stop' event.
  // 8) Verify the colors. (done)
  // 9) Remove blank bars at end (done in xml - do in score)
  //10) Need to assign section targets (always default to next section)
  //11) Mark tied notes for skip. (done)
  //12) Determine note off locations based on ties and slurs - defer 'pedal' to player
  
 
  cmXsRC_t cmXScoreInitialize( cmCtx_t* ctx, cmXsH_t* hp, const cmChar_t* xmlFn );
  cmXsRC_t cmXScoreFinalize( cmXsH_t* hp );

  bool     cmXScoreIsValid( cmXsH_t h );

  cmXsRC_t cmXScoreWriteCsv( cmXsH_t h, const cmChar_t* csvFn );

  void     cmXScoreReport( cmXsH_t h, cmRpt_t* rpt, bool sortFl );

  cmXsRC_t cmXScoreTest( cmCtx_t* ctx, const cmChar_t* fn );
  
#ifdef __cplusplus
}
#endif

#endif
