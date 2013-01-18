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
  unsigned     code;
  unsigned     ri;
  unsigned     ci;
  bool         matchFl;
  bool         transFl;
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
} cmScMatchVal_t;

// List record used to track a path through the DP matrix p->m[,]
typedef struct cmScMatchPath_str
{
  unsigned                  code;    // kSmXXXIdx
  unsigned                  ri;      // matrix row index
  unsigned                  ci;      // matrix col index
  unsigned                  flags;   // cmSmMatchFl | cmSmTransFl
  unsigned                  locIdx;  // p->loc index or cmInvalidIdx
  struct cmScMatchPath_str* next;    //
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
  cmObj            obj;         // 
  cmScH_t          scH;         // cmScore handle
  unsigned         locN;        //
  cmScMatchLoc_t*  loc;         // loc[locN] 
  unsigned         mrn;         // max row count (midi)
  unsigned         rn;          // cur row count
  unsigned         mcn;         // max column count (score)
  unsigned         cn;          // cur column count
  unsigned         mmn;         // max length of midiBuf[]    (mrn-1)
  unsigned         msn;         // max length of score window (mcn-1)
  cmScMatchVal_t*  m;           // m[mrn,mcn]  DP matrix
  unsigned         pn;          // mrn+mcn
  cmScMatchPath_t* p_mem;       // pmem[ 2*pn ];
  cmScMatchPath_t* p_avl;       // available path record linked list
  cmScMatchPath_t* p_cur;       // current path linked list
  cmScMatchPath_t* p_opt;       // p_opt[pn] current best alignment 
  double           opt_cost;    // p_opt cost set by cmScMatchExec() 
} cmScMatch;

cmScMatch* cmScMatchAlloc( cmCtx* c, cmScMatch* p, cmScH_t scH, unsigned maxScWndN, unsigned maxMidiWndN );
cmRC_t     cmScMatchFree(  cmScMatch** pp );
cmRC_t     cmScMatchInit(  cmScMatch* p, cmScH_t scH, unsigned maxScWndN, unsigned maxMidiWndN );
cmRC_t     cmScMatchFinal( cmScMatch* p );

// Returns cmEofRC if locIdx + locN > p->locN - note that this is not necessarily an error.
// The optimal path p_opt[] will only be updated if the edit cost is less than min_cost.
// Set min_cost to DBL_MAX to force p_opt[] to be updated.
cmRC_t     cmScMatchExec(  cmScMatch* p, unsigned locIdx, unsigned locN, const unsigned* midiPitchV, unsigned midiPitchN, double min_cost );

 //=======================================================================================================================

 typedef struct
 {
   unsigned locIdx; // location assoc'd with this MIDI evt (cmInvalidIdx if not a positive-match)
   //unsigned cbCnt;  // count of times this event has been sent via the callback
   unsigned mni;    // unique identifier for this event since previous call to cmScAlignReset().
   unsigned smpIdx; // time stamp of this event
   unsigned pitch;  // MIDI note pitch
   unsigned vel;    //  "    "   velocity
 } cmScMatcherMidi_t;

 typedef struct
 {
   unsigned locIdx;
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
   cmScMatcherMidi_t*   midiBuf;  // midiBuf[mn]

   cmScMatcherResult_t* res;    // res[rn]
   unsigned             rn;     // length of res[]
   unsigned             ri;     // next avail res[] recd.

   double               s_opt;          // 
   unsigned             missCnt;        // count of consecutive trailing non-matches
   unsigned             esi;            // index into loc[] of the last positive match. 
   unsigned             mni;            // track the count of MIDI events since the last call to cmScMatcherReset()
   unsigned             mbi;            // index of oldest MIDI event in midiBuf[]; 0 when the buffer is full.
   unsigned             begSyncLocIdx;  // start of score window, in mp->loc[], of best match in previous scan
   unsigned             stepCnt;        // count of forward/backward score loc's to examine for a match during cmScMatcherStep().
   unsigned             maxMissCnt;     // max. number of consecutive non-matches during step prior to executing a scan.
   unsigned             scanCnt;        // count of time scan was executed inside cmScMatcherStep()
} cmScMatcher;



cmScMatcher* cmScMatcherAlloc( cmCtx* c, cmScMatcher* p, double srate, cmScH_t scH, unsigned scWndN, unsigned midiWndN );
cmRC_t       cmScMatcherFree(  cmScMatcher** pp );
cmRC_t       cmScMatcherInit(  cmScMatcher* p, double srate, cmScH_t scH, unsigned scWndN, unsigned midiWndN );
cmRC_t       cmScMatcherFinal( cmScMatcher* p );
void         cmScMatcherReset( cmScMatcher* p );
bool         cmScMatcherInputMidi( cmScMatcher* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 );

// Slide a score window scanCnt times, beginning at 'bsi',
// looking for the best match to p->midiBuf[].  The score window
// contain scWndN (p->mp->mcn-1) score locations.
// Returns the index into p->mp->loc[] of the start of the best
// match score window. The score associated
// with this match is stored in s_opt.
unsigned   cmScMatcherScan( cmScMatcher* p, unsigned bsi, unsigned scanCnt );

// Step forward/back by p->stepCnt from p->esi.
// p->esi must therefore be valid prior to calling this function.
// If more than p->maxMissCnt consecutive MIDI events are 
// missed then automatically run cmScAlignScan().
// Return cmEofRC if the end of the score is encountered.
// Return cmSubSysFailRC if an internal scan resync. failed.
cmRC_t     cmScMatcherStep(  cmScMatcher* p );

cmRC_t     cmScMatcherExec(  cmScMatcher* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 );


//=======================================================================================================================

typedef struct
{
  unsigned mni;
  unsigned scEvtIdx;
  unsigned locIdx;
  unsigned smpIdx;
  unsigned pitch;
  unsigned vel;
} cmScMeasMidi_t;


typedef struct
{
  unsigned      bsei;  // begin score event index  
  unsigned      esei;  // end   score event index

  unsigned      bsli;  //
  unsigned      esli;  // end score loc index

  unsigned      bli;   //
  unsigned      eli;   //

} cmScMeasSet_t;

typedef struct
{
  cmObj            obj;
  cmScMatch*       mp;
  unsigned         mi;       // next avail recd in midiBuf[]
  unsigned         mn;       // length of of midiBuf[]
  cmScMeasMidi_t*  midiBuf;  // midiBuf[mn]

  unsigned         sn;       // length of set[]
  cmScMeasSet_t*   set;      // set[sn]  

  unsigned         nsi;       // next set index
  unsigned         nsli;      // next score location index
  

} cmScMeas;

cmScMeas* cmScMeasAlloc( cmCtx* c, cmScMeas* p, cmScH_t scH );
cmRC_t    cmScMeasFree(  cmScMeas** pp );
cmRC_t    cmScMeasInit(  cmScMeas* p, cmScH_t scH );
cmRC_t    cmScMeasFinal( cmScMeas* p );
cmRC_t    cmScMeasExec(  cmScMeas* p, unsigned mni,  unsigned locIdx, unsigned smpIdx, unsigned pitch, unsigned vel );


//=======================================================================================================================

unsigned   cmScAlignScanToTimeLineEvent( cmScMatcher* p, cmTlH_t tlH, cmTlObj_t* top, unsigned endSmpIdx );

// Given a score, a time-line, and a marker on the time line scan the
// entire score looking for the best match between the first 'midiN'
// notes in each marker region and the score. 
void       cmScAlignScanMarkers(  cmRpt_t* rpt, cmTlH_t tlH, cmScH_t scH );

#ifdef __cplusplus
}
#endif

#endif
