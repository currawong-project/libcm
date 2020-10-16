//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmArray_h
#define cmArray_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc: "Dynamic array container class." kw:[container] }
  
enum
{
  kOkArRC = cmOkRC,
  kUnderflowArRC
};

  typedef cmRC_t     cmArRC_t;
  typedef cmHandle_t cmArrayH_t;

  extern cmArrayH_t cmArrayNullHandle;

  cmArRC_t    cmArrayAlloc0( cmCtx_t* ctx, cmArrayH_t* hp, unsigned eleByteCnt, unsigned initCnt, unsigned expandCnt );

  // Defaults initCnt and expandCnt to 10.
  cmArRC_t    cmArrayAlloc(    cmCtx_t* ctx, cmArrayH_t* hp, unsigned eleByteCnt );
  cmArRC_t    cmArrayRelease(cmArrayH_t* hp );
  cmArRC_t    cmArrayIsValid(cmArrayH_t h );
  void        cmArraySetExpandCount( cmArrayH_t h, unsigned expandCnt );
  unsigned    cmArrayExpandCount( cmArrayH_t h );
  unsigned    cmArrayCount(  cmArrayH_t h );
  cmArRC_t    cmArrayClear(  cmArrayH_t h, bool releaseFl );

  // Returns a pointer to the first pushed element.
  // Set 'data' to NULL to create 'dataEleCnt' new zeroed elements.
  void*       cmArrayPush(   cmArrayH_t h, const void* data, unsigned dataEleCnt );

  // Decreaese the array count by 'eleCnt'.
  cmArRC_t    cmArrayPop(    cmArrayH_t h, unsigned eleCnt );

  // If 'data' is NULL then array[idx:idx+dataEleCnt] is zeroed.
  // Returns a ptr to the first set element.
  void*       cmArraySet(    cmArrayH_t h, unsigned index, const void* data, unsigned dataEleCnt );
  const void* cmArrayGet(    cmArrayH_t h, unsigned index );



#define cmArrayPtr(t,h,i)     ((t*)cmArrayGet(h,i))

  // Return a ptr to the base of the array.
#define cmArrayBase(t,h)     ((t*)cmArrayGet(h,0))

  // Return a ptr to the ith element
#define cmArrayEle(t,h,i)    (*(t*)cmArrayGet(h,i))

  // Zero the ith element
#define cmArrayClr(t,h,i)    ((t*)cmArraySet(h,i,NULL,1))

  // Zero elements i:i+n-1
#define cmArrayClrN(t,h,i,n) ((t*)cmArraySet(h,i,NULL,n))

  //)
  
#ifdef __cplusplus
}
#endif

#endif
