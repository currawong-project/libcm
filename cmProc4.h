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
  kSaMinIdx, 
  kSaSubIdx, // 'substitute' - may or may not match
  kSaDelIdx, // 'delete'     - delete a MIDI note 
  kSaInsIdx, // 'insert'     - insert a space in the score 
  kSaCnt 
};

typedef struct
{
  unsigned v[kSaCnt];
  bool     matchFl;   // if this is a substitute; is it also a match?
  bool     transFl;   // if this is a substitute; is this the second element in a reversed pair?
} cmScAlignVal_t;

typedef struct cmScAlignPath_str
{
  unsigned                  code;
  unsigned                  ri;
  unsigned                  ci;
  bool                      matchFl;
  bool                      transFl;
  unsigned                  locIdx;
  struct cmScAlignPath_str* next;
} cmScAlignPath_t;

typedef struct
{
  unsigned pitch;
  unsigned scEvtIdx;
  bool     matchFl;
} cmScAlignScEvt_t;

typedef struct
{
  unsigned          evtCnt;         // 
  cmScAlignScEvt_t* evtV;           // evtV[evtCnt]
  unsigned          scLocIdx;       // scH score location index
  int               barNumb;        // bar number of this location
} cmScAlignLoc_t;

typedef struct
{
  unsigned locIdx; // location assoc'd with this MIDI evt (cmInvalidIdx if not a positive-match)
  unsigned cbCnt;  // count of times this event has been sent via the callback
  unsigned mni;    // unique identifier for this event since previous call to cmScAlignReset().
  unsigned smpIdx; // time stamp of this event
  unsigned pitch;  // MIDI note pitch
  unsigned vel;    //  "    "   velocity
} cmScAlignMidiEvt_t;

typedef struct
{
  unsigned locIdx;    // loc[] sync. location    
  unsigned smpIdx;    // 
  unsigned mni;       // MIDI event unique index  
  unsigned pitch;     // MIDI event pitch which may not match the score event pitch
  unsigned vel;       //  "     "   velocity
  bool     matchFl;
  bool     transFl;
  bool     foundFl; 
} cmScAlignResult_t;

// 
typedef void (*cmScAlignCb_t)( void* cbArg, unsigned scLocIdx, unsigned mni, unsigned pitch, unsigned vel );

typedef struct
{
  cmObj               obj;
  cmScAlignCb_t       cbFunc;   // 
  void*               cbArg;    // 
  cmScH_t             scH;      // 
  double              srate;    // 
  unsigned            locN;     // length of loc[]
  cmScAlignLoc_t*     loc;      // loc[locN] score array
  unsigned            rn;       // length of midiBuf[]    (mn+1)
  unsigned            cn;       // length of score window (scWndN+1)
  unsigned            mn;       // length of  midiBuf[] (rn-1)
  cmScAlignMidiEvt_t* midiBuf;  // midiBuf[ mn ]
  unsigned            mbi;      // index of first element in midiBuf[] - this is required because the MIDI buf fills from the end to the beginning
  unsigned            mni;      // index of event in midiBuf[p->mn] - increments on each note inserted into midiBuf[] - zeroed by cmScAlignReset().

  cmScAlignVal_t*     m;        // m[rn,cn]   
  unsigned            pn;       // rn+cn
  cmScAlignPath_t*    p_mem;    // pmem[ 2*pn ];
  cmScAlignPath_t*    p_avl;    // available path record linked list
  cmScAlignPath_t*    p_cur;    // current path linked list
  cmScAlignPath_t*    p_opt;    // p_opt[pn] current best alignment 
  double              s_opt;    // score of the current best alignment  
  unsigned            esi;      // loc[] index of latest positive match
  unsigned            missCnt;  // count of consecutive trailing MIDI events without positive matches
  unsigned            scanCnt;

  bool                printFl;

  unsigned            begScanLocIdx; // begin the search at this score locations scWnd[begScanLocIdx:begScanLocIdx+p->cn-1]

  unsigned            resN;  // count of records in res[] == 2*cmScoreEvtCount()
  cmScAlignResult_t*  res;   // res[resN]
  unsigned            ri;    // 

  int                 stepCnt;    // count of loc[] locations to step ahead/back during a cmScAlignStep() operation.
  int                 maxStepMissCnt;  // max. consecutive trailing non-positive matches before a scan takes place.


} cmScAlign;

cmScAlign* cmScAlignAlloc( cmCtx* ctx, cmScAlign* p, cmScAlignCb_t cbFunc, void* cbArg, cmReal_t srate, cmScH_t scH, unsigned midiN, unsigned scWndN );
cmRC_t     cmScAlignFree( cmScAlign** pp );
cmRC_t     cmScAlignInit( cmScAlign* p, cmScAlignCb_t cbFunc, void* cbArg, cmReal_t srate, cmScH_t scH, unsigned midiN, unsigned scWndN );
cmRC_t     cmScAlignFinal( cmScAlign* p );
void       cmScAlignReset( cmScAlign* p, unsigned begScanLocIdx );

bool       cmScAlignExec(  cmScAlign* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 );

bool       cmScAlignInputMidi(  cmScAlign* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 );

// Scan from p->begScanLocIdx to the end of the score looking
// for the best match to p->midiBuf[].
// Returns the score location index which best matches the 
// first note p->midiBuf[]. The score associated
// with this match is stored in s_opt.
unsigned   cmScAlignScan( cmScAlign* p, unsigned scanCnt );

// Step forward/back by p->stepCnt from p->esi.
// If more than p->maxStepMissCnt consecutive MIDI events are 
// missed then automatically run cmScAlignScan().
bool cmScAlignStep(  cmScAlign* p );

unsigned   cmScAlignScanToTimeLineEvent( cmScAlign* p, cmTlH_t tlH, cmTlObj_t* top, unsigned endSmpIdx );

// Given a score, a time-line, and a marker on the time line scan the
// entire score looking for the best match between the first 'midiN'
// notes in each marker region and the score. 
void       cmScAlignScanMarkers(  cmRpt_t* rpt, cmTlH_t tlH, cmScH_t scH );

#ifdef __cplusplus
}
#endif

#endif
