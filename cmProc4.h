#ifndef cmProc4_h
#define cmProc4_h

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct
  {
    unsigned     smpIdx;  // time tag sample index for val
    cmMidiByte_t val;     //
    bool         validFl; //  
  } cmScFolBufEle_t;

  typedef struct
  {
    unsigned pitch;
    unsigned scEvtIdx;
  } cmScFolEvt_t;

  typedef struct
  {
    unsigned      evtCnt; // 
    cmScFolEvt_t* evtV;   // pitchV[pitchCnt]
    unsigned      scIdx;    // index of the score loc (into cmScoreEvt[]) at this location 
    int           barNumb;  // bar number of this location
  } cmScFolLoc_t;


  typedef struct
  {
    cmObj            obj;
    cmReal_t         srate;      //
    cmScH_t          scH;        // score handle
    unsigned         bufN;       // event buffer count 
    cmScFolBufEle_t* bufV;       // event buffer bufV[bufN] - bufV[bufN-1] newest event, bufV[boi] oldest event
    int              locN;       // count of score locations
    cmScFolLoc_t*    loc;        // score loc[locN]
    unsigned         sbi;        // oldest score window index
    unsigned         sei;        // newest score window index
    unsigned         msln;       // minimum score look ahead count
    unsigned         mswn;       // maximum score window length
    unsigned         forwardCnt; // count of score loc's to look ahead for a match to the current pitch when the optimal edit-dist alignment does not produce a match for the current pitch
    unsigned         maxDist;    // max. dist allowed to still  consider matching
    unsigned         minVel;     // notes < minVel are ignored
    bool             printFl;    // true if pitch tracker reporting should be included
    bool             noBackFl;   // prevent the tracker from going backwards in time
    unsigned*        edWndMtx;

    unsigned         missCnt;    // current consecutive unmatched notes
    unsigned         matchCnt;   // current consecutive matched notes
    unsigned         eventIdx;   // events since reset
    unsigned         skipCnt;    // notes skipped due to velocity
    unsigned         ret_idx;    // last tracked location
  
  } cmScFol;

  cmScFol* cmScFolAlloc( cmCtx* ctx, cmScFol* p, cmReal_t srate, cmScH_t scH, unsigned bufN, unsigned minWndLookAhead, unsigned maxWndCnt, unsigned minVel );
  cmRC_t   cmScFolFree(  cmScFol** pp );
  cmRC_t   cmScFolInit(  cmScFol* p, cmReal_t srate, cmScH_t scH, unsigned bufN, unsigned minWndLookAhead, unsigned maxWndCnt, unsigned minVel );
  cmRC_t   cmScFolFinal( cmScFol* p );

  // Jump to a score location and reset the internal state of the follower.
  cmRC_t   cmScFolReset( cmScFol* p, unsigned scoreIndex );

  // Give the follower a MIDI performance event. Only MIDI note-on events are acted upon;
  // all others are ignored.
  unsigned cmScFolExec(  cmScFol* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 );

  //=======================================================================================================================

  typedef struct
  {
    unsigned pitch;
    unsigned scEvtIdx;
    bool     matchFl;
  } cmScTrkEvt_t;

  typedef struct
  {
    unsigned      evtCnt;         // 
    cmScTrkEvt_t* evtV;           // evtV[evtCnt]
    unsigned      scIdx;          // index of the score event (into cmScoreEvt[]) at this location 
    int           barNumb;        // bar number of this location
  } cmScTrkLoc_t;

  typedef struct
  {
    cmObj         obj;
    cmScFol*      sfp; 
    double        srate;
    cmScH_t       scH;
    unsigned      locN;
    cmScTrkLoc_t* loc;
    unsigned      minVel;
    unsigned      maxWndCnt;
    unsigned      minWndLookAhead;
    bool          printFl;

    int           curLocIdx;
    unsigned      evtIndex;

  } cmScTrk;

  cmScTrk* cmScTrkAlloc( cmCtx* ctx, cmScTrk* p, cmReal_t srate, cmScH_t scH, unsigned bufN, unsigned minWndLookAhead, unsigned maxWndCnt, unsigned minVel );
  cmRC_t   cmScTrkFree(  cmScTrk** pp );
  cmRC_t   cmScTrkInit(  cmScTrk* p, cmReal_t srate, cmScH_t scH, unsigned bufN, unsigned minWndLookAhead, unsigned maxWndCnt, unsigned minVel );
  cmRC_t   cmScTrkFinal( cmScTrk* p );

  // Jump to a score location and reset the internal state of the follower.
  cmRC_t   cmScTrkReset( cmScTrk* p, unsigned scoreIndex );

  // Give the follower a MIDI performance event. Only MIDI note-on events are acted upon;
  // all others are ignored.
  unsigned cmScTrkExec(  cmScTrk* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 );

  //=======================================================================================================================  
  //
  // Simplified string alignment function based on Levenshtein edit distance.
  //
  enum { kEdMinIdx, kEdSubIdx, kEdDelIdx, kEdInsIdx, kEdCnt };

  typedef struct
  {
    unsigned v[kEdCnt];
    bool     matchFl;
    bool     transFl;
  } ed_val;

  typedef struct ed_path_str
  {
    unsigned            code;
    unsigned            ri;
    unsigned            ci;
    bool                matchFl;
    bool                transFl;
    struct ed_path_str* next;
  } ed_path;

  /*
    Backtracking:
    m[rn,cn] is organized to indicate the mutation operations 
    on s0[0:rn-1] or s1[0:cn-1] during backtracking.  
    Backtracking begins at cell m[rn-1,cn-1] and proceeds 
    up and left toward m[0,0].  The action to perform during
    backtracking is determined by examinging which values
    int m[].v[1:3] match m[].v[0]. 
    Match                            Next Cell
    Index  Operation                 Location
    -----  ------------------------  ------------------------
    1      Substitute char s0[ri-1]  move diagonally; up-left
    2      Delete char s0[ri-1]      move up.
    3      Delete char s1[ci-1]      move left.
    (same as inserting blank
    into after s[ri-1]

    Note that more than one value in m[].v[1:3] may match
    m[].v[0]. In this case the candidate solution branches
    at this point in the candidate selection processes.
  */
  typedef struct
  {
    const char*    s0;    // forms rows of m[]    - mutate to match s1 - rn=strlen(s0)
    const char*    s1;    // forms columns of m[] - target string      - cn=strlen(s1)
    unsigned       rn;    // length of s0 + 1
    unsigned       cn;    // length of s1 + 1
    ed_val*        m;     // m[rn,cn]   
    unsigned       pn;    // rn+cn
    ed_path*       p_mem; // pmem[ 2*pn ];
    ed_path*       p_avl; // available path record linked list
    ed_path*       p_cur; // current path linked list
    ed_path*       p_opt; // p_opt[pn] current best alignment 
    double         s_opt; // score of the current best alignment

  } ed_r;

  // print the DP matrix ed_r.m[rn,cn].
  void ed_print_mtx( ed_r* r );

  // Initialize ed_r.
  void ed_init( ed_r* r, const char* s0, const char* s1 );

  // Fill in the DP matrix.
  void ed_calc_mtx( ed_r* r );

  // Traverse the possible alignments in the DP matrix and determine the optimal alignment.
  void ed_align( ed_r* r );

  // Print the optimal alignment p_opt[]
  void ed_print_opt( ed_r* r );

  // Free resource allocated by ed_init().
  void ed_free(ed_r* r);

  // Main test function.
  void ed_main();

  //=======================================================================================================================
  enum 
  { 
    kSmMinIdx, // 
    kSmSubIdx, // 'substitute' - may or may not match
    kSmDelIdx, // 'delete'     - delete a MIDI note 
    kSmInsIdx, // 'insert'     - insert a space in the score 
    kSmCnt 
  };

  enum
  {
    kSmMatchFl    = 0x01,
    kSmTransFl    = 0x02,
    kSmTruePosFl  = 0x04,
    kSmFalsePosFl = 0x08,
    kSmBarFl      = 0x10,
    kSmNoteFl     = 0x20
  };

  // Dynamic Programming (DP) matrix element
  typedef struct
  {
    unsigned v[kSmCnt]; // cost for each operation 
    unsigned flags;     // cmSmMatchFl | cmSmTransFl
    unsigned scEvtIdx; 
  } cmScMatchVal_t;

  // List record used to track a path through the DP matrix p->m[,]
  typedef struct cmScMatchPath_str
  {
    unsigned                  code;     // kSmXXXIdx
    unsigned                  ri;       // matrix row index
    unsigned                  ci;       // matrix col index
    unsigned                  flags;    // cmSmMatchFl | cmSmTransFl
    unsigned                  locIdx;   // p->loc index or cmInvalidIdx
    unsigned                  scEvtIdx; // scScore event index
    struct cmScMatchPath_str* next;     //
  } cmScMatchPath_t;

  typedef struct cmScMatchEvt_str
  {
    unsigned pitch;     // 
    unsigned scEvtIdx;  // scScore event index
  } cmScMatchEvt_t;

  // Score location record. 
  typedef struct
  {
    unsigned        evtCnt;       // 
    cmScMatchEvt_t* evtV;         // evtV[evtCnt]
    unsigned        scLocIdx;     // scH score location index
    int             barNumb;      // bar number of this location
  } cmScMatchLoc_t;

  typedef struct
  {
    unsigned mni;    // unique identifier for this MIDI note - used to recognize when the cmScMatcher backtracks.
    unsigned smpIdx; // time stamp of this event
    unsigned pitch;  // MIDI note pitch
    unsigned vel;    //  "    "   velocity
    unsigned locIdx; // location assoc'd with this MIDI evt (cmInvalidIdx if not a  matching or non-matching 'substitute')
    unsigned scEvtIdx; // cmScore event index assoc'd with this event
  } cmScMatchMidi_t;

  typedef struct
  {
    cmObj            obj;         // 
    cmScH_t          scH;         // cmScore handle
    unsigned         locN;        //
    cmScMatchLoc_t*  loc;         // loc[locN] 
    unsigned         mrn;         // max m[] row count (midi)
    unsigned         rn;          // cur m[] row count
    unsigned         mcn;         // max m[] column count (score)
    unsigned         cn;          // cur m[] column count
    unsigned         mmn;         // max length of midiBuf[]    (mrn-1)
    unsigned         msn;         // max length of score window (mcn-1)
    cmScMatchVal_t*  m;           // m[mrn,mcn]  DP matrix
    unsigned         pn;          // mrn+mcn
    cmScMatchPath_t* p_mem;       // pmem[ 2*pn ] - path memory
    cmScMatchPath_t* p_avl;       // available path record linked list
    cmScMatchPath_t* p_cur;       // current path linked list
    cmScMatchPath_t* p_opt;       // p_opt[pn] - current best alignment as a linked list
    double           opt_cost;    // last p_opt cost set by cmScMatchExec() 
  } cmScMatch;

  /*
    1) This matcher cannot handle multiple instances of the same pitch occuring 
    at the same 'location'.

    2) Because each note of a chord is spread out over multiple locations, and 
    there is no way to indicate that a note in the chord is already 'in-use'.  
    If a MIDI note which is part of the chord is repeated, in error, it will 
    apear to be correct (a positive match will be assigned to
    the second (and possible successive notes)). 
  */

  cmScMatch* cmScMatchAlloc( cmCtx* c, cmScMatch* p, cmScH_t scH, unsigned maxScWndN, unsigned maxMidiWndN );
  cmRC_t     cmScMatchFree(  cmScMatch** pp );
  cmRC_t     cmScMatchInit(  cmScMatch* p, cmScH_t scH, unsigned maxScWndN, unsigned maxMidiWndN );
  cmRC_t     cmScMatchFinal( cmScMatch* p );

  // Locate the position in p->loc[locIdx:locIdx+locN-1] which bests 
  // matches midiV[midiN].
  // The result of this function is to update p_opt[] 
  // The optimal path p_opt[] will only be updated if the edit_cost associated 'midiV[midiN]'.
  // with the best match is less than 'min_cost'.
  // Set 'min_cost' to DBL_MAX to force p_opt[] to be updated.
  // Returns cmEofRC if locIdx + locN > p->locN - note that this is not 
  // necessarily an error.
  cmRC_t     cmScMatchExec(  cmScMatch* p, unsigned locIdx, unsigned locN, const cmScMatchMidi_t* midiV, unsigned midiN, double min_cost );

  //=======================================================================================================================


  typedef struct
  {
    unsigned locIdx;
    unsigned scEvtIdx;
    unsigned mni;
    unsigned smpIdx;
    unsigned pitch;
    unsigned vel;
    unsigned flags;
  } cmScMatcherResult_t;

  struct cmScMatcher_str;
  typedef void (*cmScMatcherCb_t)( struct cmScMatcher_str* p, void* arg, cmScMatcherResult_t* rp );

  typedef struct cmScMatcher_str
  {
    cmObj                obj;
    cmScMatcherCb_t      cbFunc;
    void*                cbArg;
    cmScMatch*           mp;
    unsigned             mn;
    cmScMatchMidi_t*     midiBuf;  // midiBuf[mn]

    cmScMatcherResult_t* res;      // res[rn]
    unsigned             rn;       // length of res[] (set to 2*score event count)
    unsigned             ri;       // next avail res[] recd.

    double               s_opt;          // 
    unsigned             missCnt;        // current count of consecutive trailing non-matches
    unsigned             ili;            // index into loc[] to start scan following reset
    unsigned             eli;            // index into loc[] of the last positive match. 
    unsigned             mni;            // current count of MIDI events since the last call to cmScMatcherReset()
    unsigned             mbi;            // index of oldest MIDI event in midiBuf[]; stays at 0 when the buffer is full.
    unsigned             begSyncLocIdx;  // start of score window, in mp->loc[], of best match in previous scan
    unsigned             initHopCnt;     // max window hops during the initial (when the MIDI buffer fills for first time) sync scan 
    unsigned             stepCnt;        // count of forward/backward score loc's to examine for a match during cmScMatcherStep().
    unsigned             maxMissCnt;     // max. number of consecutive non-matches during step prior to executing a scan.
    unsigned             scanCnt;        // current count of times a resync-scan was executed during cmScMatcherStep()
 
    bool                 printFl;
  } cmScMatcher;



  cmScMatcher* cmScMatcherAlloc( 
    cmCtx*          c,        // Program context.
    cmScMatcher*    p,        // Existing cmScMatcher to reallocate or NULL to allocate a new cmScMatcher.
    double          srate,    // System sample rate.
    cmScH_t         scH,      // Score handle.  See cmScore.h.
    unsigned        scWndN,   // Length of the scores active search area. ** See Notes.
    unsigned        midiWndN, // Length of the MIDI active note buffer.    ** See Notes.
    cmScMatcherCb_t cbFunc,   // A cmScMatcherCb_t function to be called to notify the recipient of changes in the score matcher status.
    void*           cbArg );  // User argument to 'cbFunc'.

  // Notes:
  // The cmScMatcher maintains an internal cmScMatch object which is used to attempt to find the
  // best match between the current MIDI active note buffer and the current score search area.
  // 'scWndN' is used to set the cmScMatch 'locN' argument.
  // 'midiWndN' sets the length of the MIDI FIFO which is used to match to the score with
  // each recceived MIDI note.
  // 'midiWndN' must be <= 'scWndN'.

  cmRC_t       cmScMatcherFree(  cmScMatcher** pp );
  cmRC_t       cmScMatcherInit(  cmScMatcher* p, double srate, cmScH_t scH, unsigned scWndN, unsigned midiWndN, cmScMatcherCb_t cbFunc, void* cbArg );
  cmRC_t       cmScMatcherFinal( cmScMatcher* p );

  // 'scLocIdx' is a score index as used by cmScoreLoc(scH) not into p->mp->loc[].
  cmRC_t       cmScMatcherReset( cmScMatcher* p, unsigned scLocIdx );

  // Slide a score window 'hopCnt' times, beginning at 'bli' (an
  // index int p->mp->loc[]) looking for the best match to p->midiBuf[].  
  // The score window contain scWndN (p->mp->mcn-1) score locations.
  // Returns the index into p->mp->loc[] of the start of the best
  // match score window. The score associated
  // with this match is stored in s_opt.
  unsigned   cmScMatcherScan( cmScMatcher* p, unsigned bli, unsigned hopCnt );

  // Step forward/back by p->stepCnt from p->eli.
  // p->eli must therefore be valid prior to calling this function.
  // If more than p->maxMissCnt consecutive MIDI events are 
  // missed then automatically run cmScAlignScan().
  // Return cmEofRC if the end of the score is encountered.
  // Return cmSubSysFailRC if an internal scan resync. failed.
  cmRC_t     cmScMatcherStep(  cmScMatcher* p );

  // This function calls cmScMatcherScan() and cmScMatcherStep() internally.
  // If 'status' is not kNonMidiMdId then the function returns without changing the
  // state of the object. In other words the matcher only recognizes MIDI note-on messages.
  // If the MIDI note passed by the call results in a successful match then
  // p->eli will be updated to the location in p->mp->loc[] of the latest 
  // match, the MIDI note in p->midiBuf[] associated with this match
  // will be assigned a valid locIdx and scLocIdx values, and *scLocIdxPtr
  // will be set with the matched scLocIdx of the match.
  // If this call does not result in a successful match *scLocIdxPtr is set
  // to cmInvalidIdx.
  // Return:
  // cmOkRC  - Continue processing MIDI events.
  // cmEofRC - The end of the score was encountered.
  // cmInvalidArgRC - scan failed or the object was in an invalid state to attempt a match.
  // cmSubSysFailRC - a scan resync failed in cmScMatcherStep().
  cmRC_t     cmScMatcherExec(  cmScMatcher* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1, unsigned* scLocIdxPtr );

  void cmScMatcherPrint( cmScMatcher* p );

  //=======================================================================================================================

  typedef struct
  {
    cmScoreSet_t* sp;    // ptr to this set in the score

    unsigned      bsei;  // begin score event index  
    unsigned      esei;  // end   score event index

    unsigned      bsli;  // beg score loc index
    unsigned      esli;  // end score loc index

    unsigned      bli;   // 
    unsigned      eli;   //

    double        value; // DBL_MAX if the value has not yet been set
    double        tempo; //
    double        match_cost; // cost of the match to the performance divided by sp->eleCnt


  } cmScMeasSet_t;

  typedef struct
  {
    cmObj            obj;
    double           srate;    // 
    cmScMatch*       mp;       //
    unsigned         mii;      // next avail recd in midiBuf[]
    unsigned         mn;       // length of of midiBuf[]  (init. to 2*cmScoreEvtCount())
    cmScMatchMidi_t* midiBuf;  // midiBuf[mn]

    unsigned         sn;       // length of set[] (init. to cmScoreSetCount())
    cmScMeasSet_t*   set;      // set[sn]  

    unsigned         dn;       // length of dynRef[]
    unsigned*        dynRef;   // dynRef[dn]  

    unsigned         nsi;      // next set index to fill (this is the set[] we are waiting to complete)
    unsigned         nsli;     // next score location index we are expecting to receive

    unsigned         vsi;      // set[vsi:nsi-1] indicates sets with new values following a call to cmScMeasExec()
    unsigned         vsli;     // vsli:nsli-1 indicates cmScore loc's to check for section triggers following a call to cmScMeasExec()
  } cmScMeas;

  //
  // Notes:
  //
  // 1) midiBuf[] stores all MIDI notes for the duration of the performance
  // it is initialized to 2*score_event_count.
  //
  // 2) dynRef[] is the gives the MIDI velocity range for each dynamics
  // category: pppp-fff
  // 
  // 3) See a cmDspKr.c _cmScFolMatcherCb() for an example of how 
  // cmScMeas.vsi and cmScMeas.vsli are used to act on the results of
  // a call to cmMeasExec().

  cmScMeas* cmScMeasAlloc( cmCtx* c, cmScMeas* p, cmScH_t scH, double srate, const unsigned* dynRefArray, unsigned dynRefCnt );
  cmRC_t    cmScMeasFree(  cmScMeas** pp );
  cmRC_t    cmScMeasInit(  cmScMeas* p, cmScH_t scH, double srate, const unsigned* dynRefArray, unsigned dynRefCnt );
  cmRC_t    cmScMeasFinal( cmScMeas* p );

  // Empty MIDI buffer and set the next set nsi and nsli to zero.
  cmRC_t    cmScMeasReset( cmScMeas* p );

  // This function is called for each input MIDI note which is assigned a
  // score location by cmScMatcher. 
  // 'mni' is the MIDI event index which uniquely identifies this MIDI event.
  // 'locIdx' is the location index into cmScMatcher.mp->loc[] associated with 
  // this event.
  cmRC_t    cmScMeasExec(  cmScMeas* p, unsigned mni,  unsigned locIdx, unsigned scEvtIdx, unsigned flags, unsigned smpIdx, unsigned pitch, unsigned vel );

  //=======================================================================================================================

  unsigned   cmScAlignScanToTimeLineEvent( cmScMatcher* p, cmTlH_t tlH, cmTlObj_t* top, unsigned endSmpIdx );

  // Given a score, a time-line, and a marker on the time line scan the
  // entire score looking for the best match between the first 'midiN'
  // notes in each marker region and the score. 
  void       cmScAlignScanMarkers(  cmRpt_t* rpt, cmTlH_t tlH, cmScH_t scH );

  //=======================================================================================================================
  /*
    Syntax: <loc> <mod> <var> <type>   <params>
    <loc> - score location
    <mod> - name of the modulator 
    <var> - variable name 
    <type> - type of operation

    <params>

    <min>  - set a variable min value
    <max>  - set a variable max value
    <rate> - limit how often a variable is transmitted while it is ramping
    <val>  - type dependent value - see 'Types' below.  
    <end>  - ending value for a ramping variable
    <dur>  - determines the length of time to get to the ending value

    The value of parameters may be literal numeric values or may refer to
    variables by their name.

    Types:
    set    = set <var> to <val> which may be a literal or another variable.
    line   = ramp from its current value to <val> over <dur> seconds
    sline  = set <var> to <val> and ramp to <end> over <dur> seconds
    post   = send a 'post' msg after each transmission (can be used to change the cross-fader after each msg)

  */
  enum
  {
    kInvalidModTId,
    kSetModTId,      // set variable to parray[0] at scLocIdx
    kLineModTId,     // linear ramp variable to parray[0] over parray[1] seconds
    kSetLineModTId,  // set variable to parray[0] and ramp to parray[1] over parray[2] seconds
    kPostModTId,     // 
  };

  enum
  {
    kActiveModFl = 0x01,  // this variable is on the 'active' list
    kCalcModFl   = 0x02   // when this variable is used as a parameter it's value must be calculated rather than used directly.
  };

  struct cmScModEntry_str;

  typedef enum
  {
    kInvalidModPId,
    kLiteralModPId,  // this is a literal value
    kSymbolModPId    // 
  } cmScModPId_t;

  typedef struct cmScModParam_str
  {
    cmScModPId_t pid;     // parameter type: literal or symbol
    unsigned     symId;   // symbol of external and internal variables
    double       val;     // value of literals
  } cmScModParam_t;

  typedef struct cmScModVar_str
  {
    unsigned                 flags;    // see kXXXModFl flags above.
    unsigned                 varSymId; // variable name 
    unsigned                 outVarId; // output var id
    double                   value;    // current value of this variable
    double                   v0;       // reserved internal variable
    unsigned                 phase;    // cycle phase since activation  
    double                   min;
    double                   max;
    double                   rate;     // output rate in milliseconds (use
    struct cmScModEntry_str* entry;    // last entry assoc'd with this value
    struct cmScModVar_str*   vlink;    // p->vlist link
    struct cmScModVar_str*   alink;    // p->alist link
  } cmScModVar_t;



  // Each entry gives a time tagged location and some parameters 
  // for an algorthm which is used to set/modulate a value.
  typedef struct cmScModEntry_str
  {
    unsigned       scLocIdx;      // entry start time
    unsigned       typeId;        // variable type
    cmScModParam_t beg;           // parameter values
    cmScModParam_t end;           //
    cmScModParam_t dur;           //
    cmScModParam_t min;           // min value for this variable
    cmScModParam_t max;           // max value for this variable
    cmScModParam_t rate;          // update rate in milliseconds (DBL_MAX to disable)
    cmScModVar_t*  varPtr;        // target variable 
  } cmScModEntry_t;

  typedef void (*cmScModCb_t)( void* cbArg, unsigned varSymId, double value, bool postFl );

  typedef struct
  {
    cmObj           obj;
    cmChar_t*       fn;           // modulator score file
    unsigned        modSymId;     // modulator name
    cmSymTblH_t     stH;          // symbol table used by this modulator
    cmScModCb_t     cbFunc;       // active value callback function
    void*           cbArg;        // first arg to cbFunc()
    unsigned        samplesPerCycle; // interval in samples between calls to cmScModulatorExec()
    double          srate;        // system sample rate
    cmScModEntry_t* earray;       // earray[en] - entry array sorted on ascending cmScModEntry_t.scLocIdx
    unsigned        en;           // count 
    cmScModVar_t*   vlist;        // variable list
    cmScModVar_t*   alist;        // active variable list
    cmScModVar_t*   elist;        // last element on the active list
    unsigned        nei;          // next entry index
    unsigned        outVarCnt;    // count of unique vars that are targets of entry recds
    bool            postFl;       // send a 'post' msg after each transmission
  } cmScModulator;


  cmScModulator* cmScModulatorAlloc( cmCtx* c, cmScModulator* p, cmCtx_t* ctx, cmSymTblH_t stH, double srate, unsigned samplesPerCycle, const cmChar_t* fn, const cmChar_t* modLabel, cmScModCb_t cbFunc, void* cbArg );
  cmRC_t         cmScModulatorFree(  cmScModulator** pp );
  cmRC_t         cmScModulatorInit(  cmScModulator* p, cmCtx_t* ctx, cmSymTblH_t stH, double srate, unsigned samplesPerCycle, const cmChar_t* fn, const cmChar_t* modLabel, cmScModCb_t cbFunc, void* cbArg );
  cmRC_t         cmScModulatorFinal( cmScModulator* p );

  // Return count of variables.
  unsigned       cmScModulatorOutVarCount( cmScModulator* p );

  // Return a pointer to the variable at vlist[idx].
  cmScModVar_t*  cmScModulatorOutVar( cmScModulator* p, unsigned idx ); 

  cmRC_t         cmScModulatorSetValue( cmScModulator* p, unsigned varSymId, double value, double min, double max );

  cmRC_t         cmScModulatorReset( cmScModulator* p, cmCtx_t* ctx, unsigned scLocIdx );
  cmRC_t         cmScModulatorExec(  cmScModulator* p, unsigned scLocIdx );
  cmRC_t         cmScModulatorDump(  cmScModulator* p );

  //=======================================================================================================================
  //
  // Record fragments of audio, store them, and play them back at a later time.
  //

  typedef struct cmRecdPlayFrag_str
  {
    unsigned                   labelSymId; // this fragments label
    cmSample_t**               chArray;    // record buffer chArray[cmRecdPlay.chCnt][allocCnt]
    unsigned                   allocCnt;   // count of samples allocated to each channel
    unsigned                   playIdx;    // index of next sample to play
    unsigned                   recdIdx;    // index of next sample to receieve audio (count of full samples)
    double                     fadeDbPerSec; // fade rate in dB per second
    unsigned                   fadeSmpIdx; 
    struct cmRecdPlayFrag_str* rlink;      // cmRecdPlay.rlist link
    struct cmRecdPlayFrag_str* plink;      // cmRecdPlay.plist link
  } cmRecdPlayFrag;

  typedef struct
  {
    cmObj            obj;
    cmRecdPlayFrag*  frags;         // frags[fragCnt] fragment array
    unsigned         fragCnt;       // count of fragments 
    double           srate;         // system sample rate
    unsigned         chCnt;         // count of input and output audio channels
    double           initFragSecs;  // size initial memory allocated to each frag in seconds
    unsigned         maxLaSmpCnt;   // samples allocated to each channel of the look-ahead buffers.
    unsigned         curLaSmpCnt;   // current look-ahead time in samples (curLaSmpCnt<=maxLaSmpCnt)
    cmSample_t**     laChs;         // laChs[chCnt][maxLaSmpCnt] - look-ahead buffers
    int              laSmpIdx;      // next look-ahead buffer index to receive a sample
    cmRecdPlayFrag*  plist;         // currently playing frags
    cmRecdPlayFrag*  rlist;         // currently recording frags
  } cmRecdPlay;


  // srate        - system sample rate
  // fragCnt      - total count of samples to record
  // chCnt        - count of input and output audio channels.
  // initFragSecs - amount of memory to pre-allocate for each fragment.
  // maxLaSecs    -  maximum value for curLaSecs
  // curLaSecs    -  current duration of look-ahead buffer 
  //
  // The look-ahead buffer is a circular buffer which hold the previous 'curLaSecs' seconds
  // of incoming audio.  When recording is enabled with via cmRecdPlayBeginRecord() the
  // look ahead buffer is automatically prepended to the fragment.

  cmRecdPlay*    cmRecdPlayAlloc( cmCtx* c, cmRecdPlay* p, double srate, unsigned fragCnt, unsigned chCnt, double initFragSecs, double maxLaSecs, double curLaSecs  );
  cmRC_t         cmRecdPlayFree(  cmRecdPlay** pp );
  cmRC_t         cmRecdPlayInit(  cmRecdPlay* p, double srate, unsigned flagCnt, unsigned chCnt, double initFragSecs, double maxLaSecs, double curLaSecs  );
  cmRC_t         cmRecdPlayFinal( cmRecdPlay* p );

  cmRC_t         cmRecdPlayRegisterFrag( cmRecdPlay* p, unsigned fragIdx, unsigned labelSymId );

  cmRC_t         cmRecdPlaySetLaSecs( cmRecdPlay* p, double curLaSecs );

  // Deactivates all active recorders and players, zeros the look-ahead buffer and
  // rewinds all fragment play positions.  This function does not clear the audio from 
  // frabments that have already been recorded.
  cmRC_t         cmRecdPlayRewind(      cmRecdPlay* p );

  cmRC_t         cmRecdPlayBeginRecord( cmRecdPlay* p, unsigned labelSymId );
  cmRC_t         cmRecdPlayEndRecord(   cmRecdPlay* p, unsigned labelSymId );
  cmRC_t         cmRecdPlayInsertRecord(cmRecdPlay* p, unsigned labelSymId, const cmChar_t* wavFn );

  cmRC_t         cmRecdPlayBeginPlay(   cmRecdPlay* p, unsigned labelSymId );
  cmRC_t         cmRecdPlayEndPlay(     cmRecdPlay* p, unsigned labelSymId );

  // Begin fading out the specified fragment at a rate deteremined by 'dbPerSec'.
  cmRC_t         cmRecdPlayBeginFade(   cmRecdPlay* p, unsigned labelSymId, double fadeDbPerSec );

  cmRC_t         cmRecdPlayExec(        cmRecdPlay* p, const cmSample_t** iChs, cmSample_t** oChs, unsigned chCnt, unsigned smpCnt );

  //=======================================================================================================================
  // Goertzel Filter
  //

  typedef struct
  {
    double s0;
    double s1;
    double s2;
    double coeff;
    double hz;
  } cmGoertzelCh;

  struct cmShiftBuf_str;

  typedef struct cmGoertzel_str
  {
    cmObj                  obj;
    cmGoertzelCh*          ch;
    unsigned               chCnt;
    double                 srate;
    struct cmShiftBuf_str* shb;
    cmSample_t*            wnd;
  } cmGoertzel;

  cmGoertzel* cmGoertzelAlloc( cmCtx* c, cmGoertzel* p, double srate, const double* fcHzV, unsigned chCnt, unsigned procSmpCnt, unsigned hopSmpCnt, unsigned wndSmpCnt );
  cmRC_t cmGoertzelFree( cmGoertzel** pp );
  cmRC_t cmGoertzelInit( cmGoertzel* p, double srate, const double* fcHzV, unsigned chCnt, unsigned procSmpCnt, unsigned hopSmpCnt, unsigned wndSmpCnt );
  cmRC_t cmGoertzelFinal( cmGoertzel* p );
  cmRC_t cmGoertzelSetFcHz( cmGoertzel* p, unsigned chIdx, double hz );
  cmRC_t cmGoertzelExec( cmGoertzel* p, const cmSample_t* in, unsigned procSmpCnt,  double* outV, unsigned chCnt );


#ifdef __cplusplus
}
#endif

#endif
