#ifndef cmProc3_h
#define cmProc3_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct
  {
    double      wi;
    double      xi;
    int         yn;
    cmSample_t* y;
    int         yii;
    int         yoi;
    int         ynn;
  
  } cmPitchShiftOsc_t;


  typedef struct
  {
    cmObj       obj;
    unsigned    procSmpCnt;
    double      srate;
  
    int               outN;       // procSmpCnt
    int               wn;         //  
    int               xn;         //
    int               bn;         // 
    cmSample_t*       b;          // b[bn]
    cmSample_t*       x;          // x[xn] 
    cmSample_t*       wnd;        // wnd[wn]
    cmSample_t*       outV;       // outV[outN];
    int               xni;        // 
    bool              cubeFl;     //
    cmPitchShiftOsc_t osc[2];     //


  } cmPitchShift;

  cmPitchShift* cmPitchShiftAlloc( cmCtx* c, cmPitchShift* p, unsigned procSmpCnt, cmReal_t srate );
  cmRC_t        cmPitchShiftFree( cmPitchShift** pp );
  cmRC_t        cmPitchShiftInit( cmPitchShift* p, unsigned procSmpCnt, cmReal_t srate );
  cmRC_t        cmPitchShiftFinal(cmPitchShift* p );
  cmRC_t        cmPitchShiftExec( cmPitchShift* p, const cmSample_t* x, cmSample_t* y, unsigned n, double shiftRatio, bool bypassFl );

  //=======================================================================================================================

  typedef struct
  {
    double     xi;  // index into xV[]
    double     wi;  // index into wV[]
    cmSample_t u;   // cross-fade window polarity 
  } cmLoopRecdOsc;

  typedef struct
  {
    cmLoopRecdOsc osc[2];       // 
    cmSample_t*   xV;           // xV[ xN ]
    int           xN;           // maxRecdSmpCnt
    int           xfN;          // cross-fade sample count
    int           xii;          // xV[] recording input index
    cmSample_t*   wV;           // wV[ xN ] window function (actually contains xii values after recording)
  } cmLoopRecdBuf;

  typedef struct
  {
    cmObj          obj;
    cmSample_t*    bufMem;
    unsigned       maxRecdSmpCnt;
    cmLoopRecdBuf* bufArray;
    unsigned       bufArrayCnt;
    cmSample_t*    outV;
    unsigned       outN;
    unsigned       procSmpCnt;
    unsigned       xfadeSmpCnt;
    unsigned       recdBufIdx;
    unsigned       playBufIdx;
    bool           recdFl;
    bool           playFl;
    
  } cmLoopRecord;

  cmLoopRecord* cmLoopRecordAlloc( cmCtx* c, cmLoopRecord* p, unsigned procSmpCnt, unsigned maxRecdSmpCnt, unsigned xfadeSmpCnt );
  cmRC_t        cmLoopRecordFree( cmLoopRecord** pp );
  cmRC_t        cmLoopRecordInit( cmLoopRecord* p, unsigned procSmpCnt, unsigned maxRecdSmpCnt, unsigned xfadeSmpCnt );
  cmRC_t        cmLoopRecordFinal( cmLoopRecord* p );
  // rgain=recorder output gain, pgain=pass through gain
  cmRC_t        cmLoopRecordExec( cmLoopRecord* p, const cmSample_t* x, cmSample_t* y, unsigned xn, bool bypassFl, bool recdFl, bool playFl, double ratio, double pgain, double rgain );


  //=======================================================================================================================
  typedef struct
  {
    cmObj       obj;
    unsigned    rmsN;
    cmSample_t* rmsV;          // rmsV[rmsN] previous rmsN values. rmsV[rmsN-1] == rms        
    cmSample_t  rms;           // RMS of last procSmpCnt samples
    unsigned    wndN;
    cmSample_t* wndV;
    cmSample_t  mean;
    cmSample_t  d0;
    unsigned    durSmpCnt;     // duration of the current gate polarity in samples
    bool        gateFl;
    bool        deltaFl;  
    cmReal_t    onThreshPct;
    cmReal_t    onThreshDb;
    cmReal_t    offThreshDb;   //
  } cmGateDetect;

  cmGateDetect* cmGateDetectAlloc( cmCtx* c, cmGateDetect* p, unsigned procSmpCnt, cmReal_t onThreshPct, cmReal_t onThreshDb, cmReal_t offThreshDb );
  cmRC_t       cmGateDetectFree( cmGateDetect** p );
  cmRC_t       cmGateDetectInit( cmGateDetect* p, unsigned procSmpCnt, cmReal_t onThreshPct, cmReal_t onThreshDb, cmReal_t offThreshDb );
  cmRC_t       cmGateDetectFinal(cmGateDetect* p );
  cmRC_t       cmGateDetectExec( cmGateDetect* p, const cmSample_t* x, unsigned xn );
  

  //=======================================================================================================================
  typedef struct
  {
    unsigned medCnt;       // length of the median filter
    unsigned avgCnt;       // length of the (rms - med) moving avg. filter
    unsigned suprCnt;      // length of the supression window
    unsigned offCnt;       // length of the offset detection window
    cmReal_t suprCoeff;    // supression signal shape coeff
    cmReal_t onThreshDb;   // onset threshold
    cmReal_t offThreshDb;  // offset threshold
  } cmGateDetectParams;
  
  typedef struct
  {
    cmObj              obj;
    cmGateDetectParams args;

    cmSample_t*        medV;   // medV[medCnt] 
    unsigned           medIdx; 
    cmSample_t*        avgV;   // avgV[avgCnt]
    unsigned           avgIdx;
    cmSample_t*        fcofV;  // fcofV[medCnt]
    cmSample_t*        fdlyV;  // fdlyV[medCnt]
    cmSample_t*        suprV;  // suprV[suprCnt]
    unsigned           suprIdx;
    cmSample_t*        pkV;    // pkV[3]
    cmSample_t*        offV;   // offV[offCnt]
    unsigned           offIdx;

    unsigned           pkFl;     //
    bool               gateFl;   // set by onset, cleared by offset
    bool               onFl;     // set if an onset was detected
    bool               offFl;    // set if an offset was detected

    cmReal_t           onThresh;
    cmReal_t           offThresh;

    cmSample_t         rms;   // RMS
    cmSample_t         med;   // median RMS over last medCnt exec's
    cmSample_t         dif;   // max(0, RMS - median_RMS)
    cmSample_t         avg;   // avg dif's over last avgCnt exec's
    cmSample_t         ons;   // dif - avg
    cmSample_t         flt;   // filtered(ons)
    cmSample_t         sup;   // flt w/ suppression
  } cmGateDetect2;


  cmGateDetect2* cmGateDetectAlloc2( cmCtx* c, cmGateDetect2* p, unsigned procSmpCnt, const cmGateDetectParams* args );
  cmRC_t       cmGateDetectFree2( cmGateDetect2** p );
  cmRC_t       cmGateDetectInit2( cmGateDetect2* p, unsigned procSmpCnt, const cmGateDetectParams* args );
  cmRC_t       cmGateDetectFinal2(cmGateDetect2* p );
  cmRC_t       cmGateDetectExec2( cmGateDetect2* p, const cmSample_t* x, unsigned xn );
  void       cmGateDetectSetOnThreshDb2( cmGateDetect2* p, cmReal_t db );
  void       cmGateDetectSetOffThreshDb2( cmGateDetect2* p, cmReal_t db );

  //=======================================================================================================================
  
  //
  // Calculate a set of automatic gain adjustments for a set of audio channels.
  //
  // 1)  Call cmAutoGainInit() to reset the object.
  // 2)  Call  cmAutoGainStartCh() and provide an id to identify the channel.
  // 3)  Call  cmAutoGainProcCh() with audio from the channel identified in 2)
  // 4)  Repeat 2) and 3) for for all channels.  
  // 5)  Call cmAutoGainCalcGains() to set the value of cmAutoGainCh.gain.
  // 
  // The gain coefficents are set to balance the overall gain. 
  // (i.e. Loud channels are decreased and quiet channels are increased.)

  typedef struct
  {
    unsigned id;         // channel id
    cmReal_t gain;       // gain adjustment coefficient
    unsigned onCnt;      // count of onsets detected 
    unsigned offCnt;     // count of offsets detected
    cmReal_t gateMaxAvg; // average of the max values for each detected event
  } cmAutoGainCh;

  typedef struct
  {
    cmObj          obj;
    cmShiftBuf*    sbp;
    cmGateDetect2* gdp;
    cmAutoGainCh*  chArray; // 
    unsigned       chCnt;


    cmAutoGainCh* chp;
    unsigned      gateCnt;
    cmReal_t      gateSum;
    cmReal_t      gateMax;
    cmReal_t      minRms;
    
  } cmAutoGain;

  cmAutoGain* cmAutoGainAlloc( cmCtx* c, cmAutoGain* p, unsigned procSmpCnt, cmReal_t srate, cmReal_t hopMs, unsigned chCnt, const cmGateDetectParams* gd_args );
  cmRC_t      cmAutoGainFree( cmAutoGain** p );
  cmRC_t      cmAutoGainInit( cmAutoGain* p, unsigned procSmpCnt, cmReal_t srate, cmReal_t hopMs, unsigned chCnt, const cmGateDetectParams* gd_args );
  cmRC_t      cmAutoGainFinal( cmAutoGain* p );

  // Notify the object that the following calls to cmAutoGainProcCh()
  // should be associed with the channel 'id'.
  cmRC_t      cmAutoGainStartCh( cmAutoGain* p, unsigned id );

  // Examine the signal arriving from the channel specified in the previous
  // call to cmAutoGainProcCh() and determine the max RMS value for each 
  // event contained in the signal. 
  cmRC_t      cmAutoGainProcCh( cmAutoGain* p, const cmSample_t* x, unsigned xn );

  // Calculate the cmAutoGainCh.gain coefficient for each channel.
  cmRC_t      cmAutoGainCalcGains( cmAutoGain* p );

  void        cmAutoGainPrint( cmAutoGain* p, cmRpt_t* rpt );

  //=======================================================================================================================
  typedef struct
  {
    unsigned        ch;
    unsigned        ssi;
    const cmChar_t* pitchStr;
    unsigned        midi;
    cmReal_t        gain;
    bool            nsFl;  // noise shaper channel
    bool            cdFl;  // chord detector channel
  } cmChCfgCh;

  typedef struct
  {
    cmObj           obj;
    cmChCfgCh*      chArray;
    unsigned        chCnt;
    unsigned        nsChCnt; // count of noise-shaper channels
    const cmChar_t* fn;
    cmJsonH_t       jsH;
    cmJsonNode_t*   cap;
  } cmChCfg;

  cmChCfg* cmChCfgAlloc( cmCtx* c, cmChCfg* p, cmCtx_t* ctx, const cmChar_t* fn );
  cmRC_t   cmChCfgFree(  cmChCfg** pp );
  cmRC_t   cmChCfgInit(  cmChCfg*  p, cmCtx_t* ctx, const cmChar_t* fn );
  cmRC_t   cmChCfgFinal( cmChCfg*  p );
  cmRC_t   cmChCfgWrite( cmChCfg* p );
  void     cmChCfgPrint( cmChCfg* p, cmRpt_t* rpt );
  unsigned cmChCfgChannelCount( cmCtx_t* ctx, const cmChar_t* fn, unsigned* nsChCntPtr );
  unsigned cmChCfgChannelIndex( cmCtx_t* ctx, const cmChar_t* fn, unsigned chIdx );

  //=======================================================================================================================

  typedef struct 
  {
    bool     readyFl;     // This channel has received an offset since it was last a candidate.
    bool     candFl;      // This channel is a chord candidate
    unsigned candSmpTime; // Time that this channel became a candidate
    cmReal_t candRMS;     // RMS when this channel became a candidate
    bool     chordFl;     // This channel is part of the current chord
    bool     gateFl;      // Previous gate state
  } cmChordDetectCh;

  typedef struct
  {
    cmObj            obj;
    unsigned         maxTimeSpanSmpCnt; // maximum time between onsets of first and last note of chord
    unsigned         minNotesPerChord;  // the min. number of notes required to form a chord
    unsigned         chCnt;             // count of channels
    cmChordDetectCh* chArray;           // channel state array
    bool             detectFl;          // true when a new chord has been detected
    unsigned         timeSmp;           // current time
    cmReal_t         srate;
  } cmChordDetect;

  cmChordDetect* cmChordDetectAlloc( cmCtx*c, cmChordDetect* p, cmReal_t srate, unsigned chCnt, cmReal_t maxTimeSpanMs, unsigned minNotesPerChord  );
  cmRC_t         cmChordDetectFree(  cmChordDetect** pp );
  cmRC_t         cmChordDetectInit(  cmChordDetect* p, cmReal_t srate, unsigned chCnt, cmReal_t maxTimeSpanMs, unsigned minNotesPerChord );
  cmRC_t         cmChordDetectFinal( cmChordDetect* p );
  cmRC_t         cmChordDetectExec(  cmChordDetect* p, unsigned procSmpCnt, const bool* gateV, const cmReal_t* rmsV, unsigned chCnt );
  cmRC_t         cmChordDetectSetSpanMs( cmChordDetect* p, cmReal_t maxTimeSpanMs );

  //=======================================================================================================================
  // This object is not really a cross-fader.  It is really just a multichannel
  // fader - which just calculates the fade gain but does not actually apply it
  // to the audio signal - unless you use cmXfaderExecAudio()
  typedef struct
  {
    cmReal_t gain;
    bool     gateFl;
  } cmXfaderCh;

  typedef struct
  {
    cmObj       obj;
    unsigned    chCnt;
    cmXfaderCh* chArray;
    unsigned    fadeSmpCnt;    
    cmReal_t    srate;
    bool        gateFl;  // true if any channels are on
    bool        onFl;    // true on cycle where gate transitions to 'on'.
    bool        offFl;   // true on cycle where gate transitions to 'off'.
  } cmXfader;

  cmXfader* cmXfaderAlloc( cmCtx*c, cmXfader* p, cmReal_t srate, unsigned chCnt, cmReal_t fadeTimeMs  );
  cmRC_t    cmXfaderFree(  cmXfader** pp );
  cmRC_t    cmXfaderInit(  cmXfader* p, cmReal_t srate, unsigned chCnt, cmReal_t fadeTimeMs  );
  cmRC_t    cmXfaderFinal(  cmXfader* p );
  cmRC_t    cmXfaderExec(  cmXfader* p, unsigned procSmpCnt, const bool* gateV, unsigned chCnt );
  cmRC_t    cmXfaderExecAudio( cmXfader* p, unsigned procSmpCnt, const bool* gateV, unsigned chCnt, const cmSample_t* x[], cmSample_t* y );
  void      cmXfaderSetXfadeTime( cmXfader* p, cmReal_t fadeTimeMs );
  // Set all gates to false except chIdx.
  void      cmXfaderSelectOne( cmXfader* p, unsigned chIdx );
  void      cmXfaderAllOff( cmXfader* p );

  //=======================================================================================================================
  // This fader object accepts a gate signal. When the gate is high it increments
  // the gain until it reaches 1.0. When the gate is low it decrements the gain
  // until it reaches 0.0.  The fade time is the lenght of time the gain will take
  // to transition from 0.0 to 1.0 or 1.0 to 0.0.
  typedef struct
  {
    cmObj       obj;
    unsigned    fadeSmpCnt;  // time to fade from 0->1 or 1->0   
    cmReal_t    srate;
    cmReal_t    gain;
  } cmFader;

  cmFader*  cmFaderAlloc( cmCtx*c, cmFader* p, cmReal_t srate, cmReal_t fadeTimeMs  );
  cmRC_t    cmFaderFree(  cmFader** pp );
  cmRC_t    cmFaderInit(  cmFader* p, cmReal_t srate, cmReal_t fadeTimeMs  );
  cmRC_t    cmFaderFinal( cmFader* p );
  cmRC_t    cmFaderExec(  cmFader* p, unsigned procSmpCnt, bool gateFl, bool mixFl, const cmSample_t* x, cmSample_t* y );
  void      cmFaderSetFadeTime( cmFader* p, cmReal_t fadeTimeMs );

  //=======================================================================================================================
  struct cmIDelay_str;
  typedef struct
  {
    cmObj     obj;
    cmReal_t  srate;         // system sample rate
    bool      feedbackFl;    // set if this is a feedback comb filter
    cmReal_t  minHz;         // lowest comb frequency this comb filter can support
    cmReal_t  hz;            // location of first comb
    cmReal_t  alpha;         // filter coeff.
    cmReal_t  dN;            // max length of the the cf delay line
    unsigned  dn;            // current length of cf delay line
    cmReal_t* d;             // d[dn] filter delay line
    cmReal_t* b;             // b[dn] feedforward coeff's
    cmReal_t* a;             // a[dn] feedback coeff's
    cmReal_t  b0;            // feedforward coeff 0
    bool      bypassFl;      // bypass enable flag
    struct cmIDelay_str* idp;

  } cmCombFilt;

  cmCombFilt* cmCombFiltAlloc( cmCtx* c, cmCombFilt* p, cmReal_t srate, bool feedbackFl, cmReal_t minHz, cmReal_t alpha, cmReal_t hz, bool bypassFl );
  cmRC_t      cmCombFiltFree(  cmCombFilt** pp);
  cmRC_t      cmCombFiltInit(  cmCombFilt* p, cmReal_t srate, bool feedbackFl, cmReal_t minHz, cmReal_t alpha, cmReal_t hz, bool bypassFl );
  cmRC_t      cmCombFiltFinal( cmCombFilt* p );
  cmRC_t      cmCombFiltExec(  cmCombFilt* p, const cmSample_t* x, cmSample_t* y, unsigned n );
  void        cmCombFiltSetAlpha( cmCombFilt* p, cmReal_t alpha );
  cmRC_t      cmCombFiltSetHz( cmCombFilt* p, cmReal_t hz );

  //=======================================================================================================================

  typedef struct
  {
    cmObj    obj;
    cmReal_t d[2]; //
    cmReal_t b[1]; // 
    cmReal_t a[1]; // a[dn] feedback coeff's
    cmReal_t b0;                 // feedforward coeff 0
    bool     bypassFl;
  } cmDcFilt;

  cmDcFilt* cmDcFiltAlloc( cmCtx* c, cmDcFilt* p, bool bypassFl );
  cmRC_t    cmDcFiltFree(  cmDcFilt** pp);
  cmRC_t    cmDcFiltInit(  cmDcFilt* p, bool bypassFl );
  cmRC_t    cmDcFiltFinal( cmDcFilt* p );
  cmRC_t    cmDcFiltExec(  cmDcFilt* p, const cmSample_t* x, cmSample_t* y, unsigned n );

  //=======================================================================================================================
  
  // interpolating delay - used by the comb filter
  
  typedef struct cmIDelay_str
  {
    cmObj       obj;
    cmSample_t* d;  //  d[dn] delay line
    int         dn; //  sizeo of delay  
    cmSample_t* m;  //  memory buffer
    int         mn; //  size of memory bufer (dn+3)    
    int         ii; //  input index
    unsigned    tn; //  count of taps 
    cmReal_t*   ti; //  ti[tn] tap locations (fractional delay from t->ii)
    cmReal_t*   tff; // tg[tn] tap out gain
    cmReal_t*   tfb;//  tfb[tn] tap feedback gain
    cmReal_t    srate;
  } cmIDelay;

  cmIDelay* cmIDelayAlloc( cmCtx* c, cmIDelay* p, cmReal_t srate, cmReal_t maxDelayMs, const cmReal_t* tapMs, const cmReal_t* tapFfGain, const cmReal_t* tapFbGain, unsigned tapCnt );
  cmRC_t    cmIDelayFree( cmIDelay** pp );
  cmRC_t    cmIDelayInit( cmIDelay* p, cmReal_t srate, cmReal_t maxDelayMs, const cmReal_t* tapMs, const cmReal_t* tapFfGain, const cmReal_t* tapFbGain, unsigned tapCnt );
  cmRC_t    cmIDelayFinal(cmIDelay* p );
  cmRC_t    cmIDelayExec( cmIDelay* p, const cmSample_t* x, cmSample_t* y, unsigned n );
  cmRC_t    cmIDelaySetTapMs( cmIDelay* p, unsigned tapIdx, cmReal_t tapMs );
  
  
  //=======================================================================================================================
  
  // This object sequentially assigns channels to groups when their gates go high.
  // 'chsPerGroup' channels will be assigned to each group.  No channel will be
  // assigned to any group unless there are at least 'chsPerGroup' available
  // (unassigned) channels.
  // Channels are released from groups when one of the member channels gates goes low.
  //

  typedef struct
  {
    cmReal_t rms;       // current rms of this input channel
    bool     gateFl;    // current gate state of this input channel
    bool     readyFl;   // this channel is available to be assigned to a group
    bool     offsetFl;  // the gate went low during this cycle  (cleared on exec)
    unsigned groupIdx;  // group this channel is assigned to or cmInvalidIdx if it is not assigned to any group 
  } cmGroupSelCh;

  typedef struct
  {
    unsigned* chIdxArray; // chIdxArray[p->chCnt] array of indexes to channels assigned to this group
    unsigned  chIdxCnt;   // count indexes in chIdxArray[] or 0 if the channel is not in use
    bool      releaseFl;  // true during the cycle that this group was released on
    bool      createFl;   // true during the cycle that this group was created on
  } cmGroupSelGrp;

  typedef struct
  {
    cmObj obj;
    

    cmGroupSelCh*  chArray;     // chArray[chCnt]
    unsigned       chCnt;       // count of channels
    cmGroupSelGrp* groupArray;  // groupArray[groupCnt]
    unsigned       groupCnt;    // count of groups - must be <= chCnt - can be changed at any time 
    unsigned       chsPerGroup; // channels per group
    bool           updateFl;    // set during exec if channels were assigned or released 
    
  } cmGroupSel;
  
  cmGroupSel* cmGroupSelAlloc( cmCtx* c, cmGroupSel* p, unsigned chCnt, unsigned groupCnt, unsigned chsPerGroup );
  cmRC_t      cmGroupSelFree( cmGroupSel** pp );
  cmRC_t      cmGroupSelInit( cmGroupSel* p, unsigned chCnt, unsigned groupCnt, unsigned chsPerGroup );
  cmRC_t      cmGroupSelFinal( cmGroupSel* p );
  cmRC_t      cmGroupSetChannelGate( cmGroupSel* p, unsigned chIdx, bool gateFl );
  cmRC_t      cmGroupSetChannelRMS(  cmGroupSel* p, unsigned chIdx, cmReal_t rms );
  
  // After exec if the p->updateFl is set then iterate through
  // p->groupArray[]. Groups that have been created will have their 'createFl' set
  // and groups that will be removed on the next cycle have their 'releaseFl' set.
  cmRC_t      cmGroupSelExec( cmGroupSel* p );
  
  //=======================================================================================================================
  
  // Route N of M input channels to N output channels.
  // The N channels are selected from the first N gates to go high.

  typedef struct cmAudioNofM_In_str
  {
    bool        gateFl;
    bool        onsetFl;
    bool        offsetFl;
    unsigned    outChIdx;
    cmFader*    fader;
    struct cmAudioNofM_In_str* link;
  } cmAudioNofM_In;

  typedef struct
  {
    struct cmAudioNofM_In_str* list;
  } cmAudioNofM_Out;

  typedef struct
  {
    cmObj            obj;
    unsigned         iChCnt;      // (M) input channel count 
    cmAudioNofM_In*  inArray;     // chArray[ M ] - input channel array
    unsigned         oChCnt;      // (N) output channel count
    cmAudioNofM_Out* outArray;    // outArray[N] - output channel array
    unsigned         nxtOutChIdx; // ch assoc;d with the next onset gate will be assined to this output channel
  } cmAudioNofM;

  cmAudioNofM* cmAudioNofMAlloc( cmCtx* c, cmAudioNofM* p, cmReal_t srate, unsigned iChCnt, unsigned oChCnt, cmReal_t fadeTimeMs );
  cmRC_t       cmAudioNofMFree( cmAudioNofM** pp );
  cmRC_t       cmAudioNofMInit( cmAudioNofM* p, cmReal_t srate, unsigned iChCnt, unsigned oChCnt, cmReal_t fadeTimeMs );
  cmRC_t       cmAudioNofMFinal( cmAudioNofM* p );
  cmRC_t       cmAudioNofMSetChannelGate( cmAudioNofM* p, unsigned inChIdx, bool gateFl );

  // Sum the audio contained in x[inChCnt][n] into y[outChCnt][n] according 
  // to the state of the object.
  // Notes
  // 1) y[n] should be zeroed by the caller as the output is summed into this buffer.
  // 2) inChCnt should equal p->iChCnt and outChCnt should equal p->oChCnt
  cmRC_t       cmAudioNofMExec( cmAudioNofM* p, const cmSample_t* x[], unsigned inChCnt, cmSample_t* y[], unsigned outChCnt, unsigned n );
  cmRC_t       cmAudioNofMSetFadeMs( cmAudioNofM* p, cmReal_t fadeTimeMs );


  //=======================================================================================================================

  enum { kDlyAdsrId, kAtkAdsrId, kDcyAdsrId, kSusAdsrId, kRlsAdsrId, kDoneAdsrId };

  typedef struct
  {
    cmObj    obj;
    cmReal_t srate;
    bool     trigModeFl; // gate on triggers start, gate-off ignored
    cmReal_t levelMin;
    cmReal_t scaleDur;   // 

    int      dlySmp;
    int      atkSmp;
    cmReal_t atkLevel;
    int      dcySmp;
    int      susSmp;            // only used in trigger mode
    cmReal_t susLevel;
    int      rlsSmp;




    unsigned state;             // current state
    int      durSmp;            // time in current state
    cmReal_t level;             // current level 
    bool     gateFl;            // last gate state

    cmReal_t atkBegLevel;       // attack starting level
    cmReal_t atkDurSmp;         // attack duration 

    cmReal_t rlsLevel;          // release starting level
    cmReal_t rlsDurSmp;         // release duration 

    cmReal_t actAtkLevel;
    cmReal_t actSusLevel;
    

  } cmAdsr;

  cmAdsr*   cmAdsrAlloc( cmCtx* c, cmAdsr* p, cmReal_t srate, bool trigFl, cmReal_t minL, cmReal_t dlyMs, cmReal_t atkMs, cmReal_t atkL, cmReal_t dcyMs, cmReal_t susMs, cmReal_t susL, cmReal_t rlsMs ); 
  cmRC_t    cmAdsrFree(  cmAdsr** pp );
  cmRC_t    cmAdsrInit( cmAdsr* p, cmReal_t srate, bool trigFl, cmReal_t minL, cmReal_t dlyMs, cmReal_t atkMs, cmReal_t atkL, cmReal_t dcyMs, cmReal_t susMs, cmReal_t susL, cmReal_t rlsMs );
  cmRC_t    cmAdsrFinal( cmAdsr* p );
  cmReal_t  cmAdsrExec( cmAdsr* p, unsigned procSmpCnt, bool gateFl, cmReal_t tscale, cmReal_t ascale );
  void      cmAdsrSetTime( cmAdsr* p, cmReal_t ms, unsigned id );
  void      cmAdsrSetLevel( cmAdsr* p, cmReal_t level, unsigned id );
  void      cmAdsrReport( cmAdsr* p, cmRpt_t* rpt );

  //=======================================================================================================================
  enum { kAtkCompId, kRlsCompId };

  typedef struct
  {
    cmObj       obj;
    cmReal_t    srate;          // system sample rate
    unsigned    procSmpCnt;     // samples per exec cycle
    cmReal_t    inGain;         // input gain
    cmReal_t    threshDb;       // threshold in dB (max:100 min:0) 
    cmReal_t    ratio_num;      // numerator of the ratio
    unsigned    atkSmp;         // time to reduce   the signal by 10.0 db
    unsigned    rlsSmp;         // time to increase the signal by 10.0 db
    cmReal_t    outGain;        // makeup gain
    bool        bypassFl;       // bypass enable

    cmSample_t* rmsWnd;         // rmsWnd[rmsWndAllocCnt]
    unsigned    rmsWndAllocCnt; // 
    unsigned    rmsWndCnt;      // current RMS window size (rmsWndCnt must be <= rmsWndAllocCnt)
    unsigned    rmsWndIdx;      // next RMS window input index

    unsigned    state;          // env. state
    cmReal_t    rmsDb;          // current incoming signal RMS (max:100 min:0)
    cmReal_t    gain;           // current compressor  gain

    cmReal_t    timeConstDb;    // the atk/rls will incr/decr by 'timeConstDb' per atkMs/rlsMs.
    cmReal_t    pkDb;           //
    cmReal_t    accumDb;        //

  } cmCompressor;

  cmCompressor* cmCompressorAlloc( cmCtx* c, cmCompressor* p, cmReal_t srate, unsigned procSmpCnt, cmReal_t inGain, cmReal_t rmsWndMaxMs, cmReal_t rmsWndMs, cmReal_t threshDb, cmReal_t ratio, cmReal_t atkMs, cmReal_t rlsMs, cmReal_t outGain, bool bypassFl );
  cmRC_t  cmCompressorFree( cmCompressor** pp );
  cmRC_t  cmCompressorInit( cmCompressor* p, cmReal_t srate, unsigned procSmpCnt, cmReal_t inGain, cmReal_t rmsWndMaxMs, cmReal_t rmsWndMs, cmReal_t threshDb, cmReal_t ratio, cmReal_t atkMs, cmReal_t rlsMs, cmReal_t outGain, bool bypassFl );  
  cmRC_t  cmCompressorFinal( cmCompressor* p );
  cmRC_t  cmCompressorExec( cmCompressor* p, const cmSample_t* x, cmSample_t* y, unsigned n );
  void    cmCompressorSetAttackMs( cmCompressor* p, cmReal_t ms );
  void    cmCompressorSetReleaseMs( cmCompressor* p, cmReal_t ms );
  void    cmCompressorSetThreshDb(  cmCompressor* p, cmReal_t thresh );
  void    cmCompressorSetRmsWndMs( cmCompressor* p, cmReal_t ms );

  //=======================================================================================================================

  // BiQuad Audio Eq's based on Robert Bristow-Johnson's recipes.
  // http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
  // See filter_rbj.m for equivalent octave code.

  enum
  {
    kLpfBqId,
    kHpFBqId,
    kBpfBqId,
    kNotchBqId,
    kAllpassBqId,
    kPeakBqId,
    kLowShelfBqId,
    kHighShelfBqId
  };

  typedef struct
  {
    cmObj obj;

    cmReal_t srate;
    unsigned mode;
    cmReal_t f0Hz;
    cmReal_t Q;
    cmReal_t gainDb;

    cmReal_t d[4];
    cmReal_t b[3];
    cmReal_t a[3];
    
    bool bypassFl;

  } cmBiQuadEq;

  cmBiQuadEq* cmBiQuadEqAlloc( cmCtx* c, cmBiQuadEq* p, cmReal_t srate, unsigned mode, cmReal_t f0Hz, cmReal_t Q, cmReal_t gainDb, bool bypassFl );
  cmRC_t      cmBiQuadEqFree( cmBiQuadEq** pp );
  cmRC_t      cmBiQuadEqInit( cmBiQuadEq* p, cmReal_t srate, unsigned mode, cmReal_t f0Hz, cmReal_t Q, cmReal_t gainDb, bool bypassFl );  
  cmRC_t      cmBiQuadEqFinal( cmBiQuadEq* p );
  cmRC_t      cmBiQuadEqExec( cmBiQuadEq* p, const cmSample_t* x, cmSample_t* y, unsigned n );
  void        cmBiQuadEqSet( cmBiQuadEq* p, unsigned mode, cmReal_t f0Hz, cmReal_t Q, cmReal_t gainDb );


  //=======================================================================================================================
  typedef struct
  {
    cmObj      obj;
    cmReal_t   srate;
    cmReal_t   downSrate;
    cmReal_t   bits;
    bool       rectFl;
    bool       fullFl;
    cmReal_t   clipDb;
    cmReal_t   inGain;
    cmReal_t   outGain;
    bool       bypassFl;

    double     fracIdx;
    cmSample_t lastVal;

    cmSample_t lastY;
    cmSample_t lastX;
  } cmDistDs;

  cmDistDs* cmDistDsAlloc( cmCtx* c, cmDistDs* p, cmReal_t srate, cmReal_t inGain, cmReal_t downSrate, cmReal_t bits, bool rectFl, bool fullFl, cmReal_t clipDb, cmReal_t outGain, bool bypassFl );
  cmRC_t    cmDistDsFree( cmDistDs** p );
  cmRC_t    cmDistDsInit( cmDistDs* p, cmReal_t srate, cmReal_t inGain, cmReal_t downSrate, cmReal_t bits, bool rectFl, bool fullFl, cmReal_t clipDb, cmReal_t outGain, bool bypassFl );
  cmRC_t    cmDistDsFinal( cmDistDs* p );
  cmRC_t    cmDistDsExec(  cmDistDs* p, const cmSample_t* x, cmSample_t* y, unsigned n );

  //=======================================================================================================================
  /*
  typedef struct
  {
    cmObj obj;
  } cmUnitDelay;

  cmUnitDelay* cmUnitDelayAlloc( cmCtx* c, cmUnitDelay* p, cmReal_t srate, unsigned smpCnt, unsigned inCnt, unsigned outCnt, const cmReal_t delayMsV, unsigned delayCnt );
  cmRC_t       cmUnitDelayFree(  cmUnitDelay* p );
  cmRC_t       cmUnitDelayInit(  cmUnitDelay* p, cmReal_t srate, unsigned smpCnt, braunsigned inCnt, unsigned outCnt, const cmReal_t delayMsV, unsigned delayCnt );
  cmRC_t       cmUnitDelayFinal( cmUnitDelay* p );
  cmRC_t       cmUnitDelayExec(  cmUnitDelay* p, const cmSample_t** x, unsigned inChCnt, cmSample_t** y, unsigned outChCnt, unsigned smpCnt );
  */

#ifdef __cplusplus
}
#endif


#endif
