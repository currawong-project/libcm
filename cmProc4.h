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
  cmReal_t         srate;   //
  cmScH_t          scH;     // score handle
  unsigned         bufN;    // event buffer count 
  cmScFolBufEle_t* bufV;    // event buffer bufV[bufN] - bufV[bufN-1] newest event, bufV[boi] oldest event
  int              locN;    // count of score locations
  cmScFolLoc_t*    loc;     // score loc[locN]
  unsigned         sbi;     // oldest score window index
  unsigned         sei;     // newest score window index
  unsigned         msln;    // minimum score look ahead count
  unsigned         mswn;    // maximum score window length
  unsigned         minVel;  // notes < minVel are ignored
  unsigned         missCnt; // current consecutive unmatched notes
  unsigned         matchCnt;// current consecutive matched notes
  bool             printFl; // true if pitch tracker reporting should be included
  unsigned         aheadCnt;// count of score loc's to look ahead for a match to the current pitch when the optimal edit-dist alignment does not produce a match for the current pitch
  unsigned         eventIdx;// events since init
  unsigned*        edWndMtx;
  
} cmScFol;


// bufN   = max count of elements in the event buffer.
// wndMs  = max length of the score following window in time

cmScFol* cmScFolAlloc( cmCtx* ctx, cmScFol* p, cmReal_t srate, unsigned bufN, cmReal_t wndMs, cmScH_t scH );
cmRC_t   cmScFolFree(  cmScFol** pp );
cmRC_t   cmScFolInit(  cmScFol* p, cmReal_t srate, unsigned bufN, cmReal_t wndMs, cmScH_t scH );
cmRC_t   cmScFolFinal( cmScFol* p );
cmRC_t   cmScFolReset( cmScFol* p, unsigned scoreIndex );
unsigned cmScFolExec(  cmScFol* p, unsigned smpIdx, unsigned status, cmMidiByte_t d0, cmMidiByte_t d1 );
  

#ifdef __cplusplus
}
#endif

#endif
