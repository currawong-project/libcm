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
  unsigned  pitchCnt; // 
  unsigned* pitchV;   // pitchV[pitchCnt]
  unsigned  scIdx;    // index of the score event (into cmScoreEvt[]) at this location 
  int       barNumb;  // bar number of this location
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
  

#ifdef __cplusplus
}
#endif

#endif
