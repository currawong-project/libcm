//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmSdb_h
#define cmSdb_h

#ifdef __cplusplus
extern "C" {
#endif

  /*( { file_desc:"Musical instrument sample database manager and synthetic sequence generator." kw:[audio] }

    The CSV file used to initialize a SDB object has the  following column syntax.
    
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
    kFileSysFailSdbRC,
    kAudioFileFailSdbRC,
    kSyntaxErrSdbRC,
    kInvalidRspIdxSdbRC,
    kInvalidSeqIdxSdbRC,
    kChPairNotFoundSdbRC,
    kRspEvtNotFoundSdbRC,
    kAssertFailSdbRC,
    kInvalidArgSdbRC
  };

  typedef cmHandle_t cmSdbH_t;
  typedef cmHandle_t cmSdbResponseH_t;
  typedef cmHandle_t cmSdbSeqH_t;

  typedef cmRC_t     cmSdbRC_t;

  extern cmSdbH_t         cmSdbNullHandle;
  extern cmSdbResponseH_t cmSdbResponseNullHandle;
  extern cmSdbSeqH_t      cmSdbSeqNullHandle;
  
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


  // Create an SDB object.  If 'csvFn' is non-NULL then an internal call is made to cmSdbLoad().
  cmSdbRC_t cmSdbCreate( cmCtx_t* ctx,  cmSdbH_t* hp, const cmChar_t* csvFn, const cmChar_t* audioDir );

  // Release an SDB object previously created via cmSdbCreate().
  cmSdbRC_t cmSdbDestroy( cmSdbH_t* hp );

  bool      cmSdbIsValid( cmSdbH_t h );

  // Iinitialze the internal database from the CSV file 'csvFn'.
  cmSdbRC_t cmSdbLoad( cmSdbH_t h, const cmChar_t* csvFn, const cmChar_t* audioDir );

  // Time align all channel pairs by setting the onset times to 
  // the minimum time among all the pairs and the offset times to
  // the maximum among all the pairs.  This function is applied to all
  // the events contained in the sample database.
  cmSdbRC_t cmSdbSyncChPairs( cmSdbH_t h );

  // Given a sample event unique id return a pointer to the associated record.
  const cmSdbEvent_t* cmSdbEvent( cmSdbH_t h, unsigned uuid );

  //================================================================================================
  // Query Related functions
  //

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
  // pitchV[] contains an array of pitch values to match.
  // The last value in pitchV[] must be kInvalidMidiPitch.
  // If pitchV == NULL then all pitches match.  Note that
  // to match non-pitched instruments set set one element
  // of pitchV[] to -1.
  //
  // The application should release all query response handles 
  // returned from this function via a call to cmSdbResponseFree().  
  // cmSdbDestroy() will automatically release any response
  // handles not previously release by cmSdbReponseFree().
  cmSdbRC_t cmSdbSelect( 
    cmSdbH_t         h,
    double           srate,      // event sample rate or 0 to ignore
    const cmChar_t** instrV,     // array of instrument labels to match
    const cmChar_t** srcV,       // array of 'src' labels to match
    const cmChar_t** notesV,     // array of text 'notes' to match
    const unsigned*  pitchV,     // array of pitches terminated w/ kInvalidMidiPitch
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
    
  //================================================================================================
  // Sequence Related functions
  //
  typedef struct
  {
    unsigned uuid;     // uuid of sample data base envent.
    double   begSec;   // Event start time in seconds.
    double   durSec;   // Event duration in seconds.
    double   gain;     // Event amplitude scaling factor.
    unsigned outChIdx; // Output channel index.
  } cmSdbSeqEvent_t;

  // Generate a random sequence of events with a programmable
  // density of events per second.
  //
  // 'minEvtPerSec' and 'maxEvtPerSec' specify the min and max count of events
  // which may be initiated per second.
  //
  // The application should release all sequence handles 
  // returned from this function via a call to cmSdbSeqFree().  
  // cmSdbDestroy() will automatically release any sequence
  // handles not previously release by cmSdbSeqFree().
  //
  // Note that the event selection is done with replacement.
  // The same event may therefore be selected more than
  // once.
  cmSdbRC_t cmSdbSeqRand( 
    cmSdbResponseH_t rh, 
    unsigned         seqDurSecs, 
    unsigned         seqChCnt, 
    unsigned         minEvtPerSec, 
    unsigned         maxEvtPerSec, 
    cmSdbSeqH_t*     shp );

  // Generate a sequence of serial events w/ gapSec seconds
  // between each event.  Events longer than 'maxEvtDurSec'
  // seconds are truncated to 'maxEvtDurSec'.
  cmSdbRC_t cmSdbSeqSerial(
    cmSdbResponseH_t rh,
    unsigned         seqChCnt,
    double           gapSec,    
    double           maxEvtDurSec,
    cmSdbSeqH_t*     shp );

  // Generate a chord sequence by randomly selecting one event
  // from each response handle. 
  cmSdbRC_t cmSdbSeqChord(
    cmSdbResponseH_t* rhp,           // one rhp[rn] query resonse per chord note
    unsigned          rn,            // count of response handles in rhp[].
    unsigned          seqChCnt,      // output sequence channel count
    unsigned          maxEvtDurSec,  // maximum event duration or 0 to prevent truncation
    cmSdbSeqH_t*      shp );

  // Release a sequence.
  cmSdbRC_t              cmSdbSeqFree( cmSdbSeqH_t* shp );

  // Return the count of sequence events.
  unsigned               cmSdbSeqCount( cmSdbSeqH_t sh );

  // Return a pointer to a specified cmSdbSeqEvent_t record.
  // where 0 <= index < cmSdbSeqCount(sh)
  const cmSdbSeqEvent_t* cmSdbSeqEvent( cmSdbSeqH_t sh, unsigned index );

  // Given a seqence index return the associate cmSdbEvent_t.
  const cmSdbEvent_t* cmSdbSeqSdbEvent( cmSdbSeqH_t sh, unsigned index );

  // Return the total duration of the sequence in seconds.
  double              cmSdbSeqDurSeconds( cmSdbSeqH_t sh );

  // Return the sample rate of the first event in the sequence that
  // has a non-zero sample rate.  There is no guarantee that all
  // of the other events in the sequence have the same sample rate
  // unless this was enforced by the query response that the 
  // sequence was generated from.
  double              cmSdbSeqSampleRate( cmSdbSeqH_t sh );

  // Generate an audio from a sequence and return it in
  // a signal vector.
  cmSdbRC_t cmSdbSeqToAudio( 
    cmSdbSeqH_t  sh, 
    unsigned     decayMs,        // decay rate for truncated events
    double       noiseDb,        // (-70.0) pad signal with white noise to avoid digital silence
    double       evtNormFact,    // normalize each sample event by normFact / abs(max(x[])) or 0 to skip normalization
    cmSample_t** signalRef,      // *signalRef[*sigSmpCntRef * sh.chCnt] returned audio signal
    unsigned*    sigSmpCntRef ); // count of frames in *signalRef

  // Generate an audio file from a sequence vector.
  cmSdbRC_t cmSdbSeqToAudioFn( 
    cmSdbSeqH_t  sh, 
    unsigned     decayMs,     // decay rate for truncated events
    double       noiseDb,     // (-70.0) pad signal with white noise to avoid digital silence
    double       evtNormFact, // normalize each sample event by normFact / abs(max(x[])) or 0 to skip normalization
    double       normFact,    // total signal norm factor or 0.0 to skip normalization
    const cmChar_t* fn,       // write the output signal to this audio file
    unsigned bitsPerSample    // audio file bits per sample
    ); 


  // Print a sequence event listing.
  void                   cmSdbSeqPrint( cmSdbSeqH_t sh, cmRpt_t* rpt );

  cmSdbRC_t cmSdbTest( cmCtx_t* ctx );
  
  //)
  
#ifdef __cplusplus
}
#endif

#endif
