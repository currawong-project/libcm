#ifndef cmSdb_h
#define cmSdb_h

#ifdef __cplusplus
extern "C" {
#endif

  /*
    The data for this object is stored in a CSV file with the following column syntax.
    
    Column Name     Type  Description
    ------ -------- ----- -----------------------------------------------------
       1   uuid     uint  Unique integer identifier for this event
       2   baseUuid uint  uuid of channel 0 for this event
       3   chIdx    uint  Channel index (stereo: 1=left 2=right)
       4   obi      uint  Outer onset sample index into 'afn'.
       5   ibi      uint  Inner onset sample index into 'afn'.
       6   iei      uint  Inner offset sample index into 'afn'.
       7   oei      uint  Outer offset sample index into 'afn'.
       8   src      text  Source label for this event (e.g. mcgill, ui )       
       9   midi     uint  MIDI pitch number or -1 if the sample is not pitched
      10   instr    text  Instrument label.
      11   srate    uint  Sample rate of audio file reference by 'afn'.
      10   chCnt    uint  Count of channels for this event.
      *    notes    text  0 or more free form double quoted text notes.
      *    afn      text  File name of the audio file this event occurs in.

     Notes:
     #. Each event represents a mono audio signal.  If the event is drawn
        from a multi-channel audio file then the 'chCnt' field will be
        greater than one.  If 'chCnt' is greater than one then the associated
        samples can be found by collecting all events that share the
        same 'baseUuid'.

     #. There may be zero or more columns of 'notes'. If there are no
        notes then the 'afn' field is in column 11.

     #. The index values (chIdx,obi,ibi,iei,oei) as stored in the CSV file
         are 1 based. These values are decreased by 1 by the cmSdb CSV reader
        so that their cmSdb value is zero based.  See cmSdbLoad().
 
   */

  enum
  {
    kOkSdbRC,
    kLHeapFailSdbRC,
    kCsvFailSdbRC,
    kSyntaxErrSdbRC,
    kInvalidRspIdxSdbRC,
    kChPairNotFoundSdbRC
  };

  typedef cmHandle_t cmSdbH_t;
  typedef cmHandle_t cmSdbResponseH_t;

  typedef cmRC_t     cmSdbRC_t;
  extern cmSdbH_t cmSdbNullHandle_t;
  extern cmSdbResponseH_t cmSdbResponseNullHandle_t;
  
  typedef struct
  {
    unsigned         uuid;     // unique id of this sample
    unsigned         baseUuid; // uuid of channel 0
    unsigned         chIdx;   // channel index (0=left,1=right)
    unsigned         obi;     // outer onset
    unsigned         ibi;     // inner onset
    unsigned         iei;     // inner offset
    unsigned         oei;     // outer offset
    unsigned         midi;    // MIDI pitch or -1 for unpitched instruments
    unsigned         srate;   // sample rate
    unsigned         chCnt;   // source channel count
    const cmChar_t*  src;     // sample source (e.g. mcgill, ui )
    const cmChar_t*  instr;   // instrument label
    const cmChar_t*  afn;     // audio file name 
    const cmChar_t** notesV;  // NULL terminated list of terms describing the event.
  } cmSdbEvent_t;


  cmSdbRC_t cmSdbCreate( cmCtx_t* ctx,  cmSdbH_t* hp, const cmChar_t* audioDir, const cmChar_t* csvFn );
  cmSdbRC_t cmSdbDestroy( cmSdbH_t* hp );

  bool      cmSdbIsValid( cmSdbH_t h );

  cmSdbRC_t cmSdbLoad( cmSdbH_t h, const cmChar_t* csvFn );

  // Select a set of events from the sample database.
  //
  // The possible selection criteria are:
  //   sample rate
  //   instrument label
  //   source label
  //   notes labels
  //   event duration
  //
  // In order to match an event all active criteria
  // must match.  In other words the match implies a
  // logical AND operation on each match criteria.
  // Each of the criteria can be made inactive by 
  // specifying particular key values.
  //   sample rate      = 0
  //   instrument label = NULL
  //   source label     = NULL
  //   notes labels     = NULL
  //   event duration   = minDurSec=0 maxDurSec=0
  //
  // For the text array arguments (instrV,srcV,notesV) 
  // each element of the array is a key which is attempts to
  // match the associated field in each event record.
  // By default a match is triggered if the key text is identical to the 
  // event field text.  The match algorithm can be modified by
  // specifying a '*' as the first character in the key field.
  // In this case a the key need only be a substring of the
  // event field to trigger a match.  For example "*viol" 
  // will return events that match both "violin" and "viola".
  // 
  // To specify a mismatch as a successful match 
  // (i.e. to return events which do not match the key text) 
  // prefix the key with a '!' character. 
  // 
  // Note that it is legal to specify both '!' and '*'. In
  // which case a match will be triggered by fields where
  // the key text is not a substring of the field text.
  //
  // All query response handles returned from this function
  // should eventualy be released by the application via a call to
  // cmSdbResponseFree().  
  cmSdbRC_t cmSdbSelect( 
    cmSdbH_t         h,
    double           srate,      // event sample rate or 0 to ignore
    const cmChar_t** instrV,     // array of instrument labels to match
    const cmChar_t** srcV,       // array of 'src' labels to match
    const cmChar_t** notesV,     // array of text 'notes' to match
    double           minDurSec,  // min event duration
    double           maxDurSec,  // max event duration or 0 to ignore
    unsigned         minChCnt,   // min ch cnt or 0 to ignore
    cmSdbResponseH_t* rhp );

  // Given the event 'ep' locate the channel pairs associated with that event.
  // The response handle returned from this function must be released
  // by a call to cmSdbResponseFree().
  cmSdbRC_t cmSdbSelectChPairs( cmSdbH_t h, const cmSdbEvent_t* ep, cmSdbResponseH_t* rhp );
  
  // Return the count of events in a query response.
  unsigned            cmSdbResponseCount(   cmSdbResponseH_t  rh );

  // Return the event at 'index' in from a query response.
  // Legal 'index' range: 0<=index<=cmSdbResponseCount().
  const cmSdbEvent_t* cmSdbResponseEvent(   cmSdbResponseH_t  rh, unsigned index );

  // Return true if the 'rh' is a non-NULL query response handle.
  bool                cmSdbResponseIsValid( cmSdbResponseH_t  rh );

  // Release the resource held by a query response.  
  cmSdbRC_t           cmSdbResponseFree(    cmSdbResponseH_t* rhp );
  void                cmSdbResponsePrint(   cmSdbResponseH_t  rh, cmRpt_t* rpt );
    
  // Time align all channel pairs by setting the onset times to 
  // the minimum time among all the pairs and the offset times to
  // the maximum among all the pairs.
  cmSdbRC_t cmSdbSyncChPairs( cmSdbH_t h );

  cmSdbRC_t cmSdbTest( cmCtx_t* ctx );

#ifdef __cplusplus
}
#endif

#endif
