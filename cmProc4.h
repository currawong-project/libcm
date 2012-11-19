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
  unsigned  scIdx;   // index of the score event (into cmScoreEvt[]) at this location 
  int       barNumb;  // bar number of this location
} cmScFolLoc_t;

  /*
typedef struct
{
  cmObj            obj;
  cmReal_t         srate;
  cmScH_t          scH;
  unsigned         maxWndSmp;
  unsigned         wndN;
  cmScFolWndEle_t* wndV;    // wnd[wndN]
  int              wni;     // oldest value in wnd[]
  int              locN;
  cmScFolLoc_t*    loc;
  unsigned         sri;     // last reset score index
  unsigned         sbi;     // first (oldest) score index
  unsigned         sei;     // last (newest) score index

  unsigned         evalWndN; // (dflt:5) count of elements to use for refined match window
  unsigned         allowedMissCnt; // (dflt:1) count of non-match elements in refined match where a match is still signaled

  unsigned*        edWndMtx;
  
} cmScFol1;
  */

typedef struct
{
  cmObj            obj;
  cmReal_t         srate;   //
  cmScH_t          scH;     // score handle
  unsigned         bufN;    // event buffer count 
  cmScFolBufEle_t* bufV;    // event buffer bufV[bufN] - bufV[bufN-1] newest event, bufV[boi] oldest event
  int              locN;    // count of score locations
  cmScFolLoc_t*    loc;     // score loc[locN]
  unsigned         sbi;    // oldest score window index
  unsigned         sei;    // newest score window index
  unsigned         msln;    // minimum score look ahead count
  unsigned         mswn;    // maximum score window length

  //unsigned         evalWndN; // (dflt:5) count of elements to use for refined match window
  //unsigned         allowedMissCnt; // (dflt:1) count of non-match elements in refined match where a match is still signaled

  unsigned*        edWndMtx;
  
} cmScFol;


// wndN = max count of elements in the  score following window.
// wndMs     = max length of the score following window in time

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
