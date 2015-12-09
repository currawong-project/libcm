

#ifndef cmGlobal_h
#define cmGlobal_h

//( { file_desc:"This is the globally included prefix file for all 'cm' files." kw:[base] }
//
// All operating system dependencies should be resolved in this file via 
// testing for OS_LINUX, OS_OSX, or OS_W32.
//)

//(
#include "config.h" // created by 'configure'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <signal.h>  
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif


#define CM_FLOAT_SMP  1  //< make cmSample_t = float in cmFloatTypes.h
#define CM_FLOAT_REAL 0  //< make cmReal_t = double in cmFloatTypes.h

#ifdef NDEBUG
#define cmDEBUG_FL 0  //< Define cmDEBUG_FL as 0 when building in release mode. See \ref debug_mode.
#else  
#define cmDEBUG_FL 1  //< Define cmDEBUG_FL as 1 when building in debug mode. See \ref debug_mode.
#endif


  // Perform byte swapping on 16 bit values.
#define cmSwap16(x) \
  (((((unsigned short)(x)) & 0x00ff) << 8) | ((((unsigned short)(x)) & 0xff00) >> 8))

#ifdef OS_LINUX
#include <byteswap.h>  // gcc specific
#include <unistd.h>

  // Perform byte swapping on 32 bit values on systems were <byteswap.h> is available.
#define cmSwap32(x) (bswap_32(x))

  // Perform byte swapping on 64 bit values on systems were <byteswap.h> is  available.
#define cmSwap64(x) (bswap_64(x))


#endif


#ifdef OS_OSX
#include <unistd.h>

  // Perform byte swapping on 32 bit values on systems were <byteswap.h> is not available.
#define cmSwap32(x)                             \
  ((((unsigned)((x) & 0x000000FF)) << 24) |   \
    (((unsigned)((x) & 0x0000FF00)) << 8) |   \
    (((unsigned)((x) & 0x00FF0000)) >> 8) |   \
    (((unsigned)((x) & 0xFF000000)) >> 24))

  // Perform byte swapping on 64 bit values on systems were <byteswap.h> is not available.
#define cmSwap64(x)                                 \
  (((((unsigned long long)(x))<<56) & 0xFF00000000000000ULL)  |   \
    ((((unsigned long long)(x))<<40) & 0x00FF000000000000ULL)  |  \
    ((((unsigned long long)(x))<<24) & 0x0000FF0000000000ULL)  |  \
    ((((unsigned long long)(x))<< 8) & 0x000000FF00000000ULL)  |  \
    ((((unsigned long long)(x))>> 8) & 0x00000000FF000000ULL)  |  \
    ((((unsigned long long)(x))>>24) & 0x0000000000FF0000ULL)  |  \
    ((((unsigned long long)(x))>>40) & 0x000000000000FF00ULL)  |  \
    ((((unsigned long long)(x))>>56) & 0x00000000000000FFULL))

#endif

#define cmAllFlags(f,m) (((f) & (m)) == (m))                   //< Test if all of a group 'm' of binary flags in 'f' are set.
#define cmIsFlag(f,m)  (((f) & (m)) ? true : false)            //< Test if any one of a the bits in 'm' is also set in 'f'. 
#define cmIsNotFlag(f,m) (cmIsFlag(f,m)==false)                //< Test if none of the bits in 'm' are set in 'f'.
#define cmSetFlag(f,m) ((f) | (m))                             //< Return 'f' with the bits in 'm' set.
#define cmClrFlag(f,m) ((f) & (~(m)))                          //< Return 'f' with the bits in 'm' cleared.
#define cmTogFlag(f,m) ((f)^(m))                               //< Return 'f' with the bits in 'm' toggled.
#define cmEnaFlag(f,m,b) (b) ? cmSetFlag(f,m) : cmClrFlag(f,m) //< \brief Set or clear bits in 'f' based on bits in 'm' and the state of 'b'.
                                                               //<
                                                               //< If 'b' == 0 then return 'f' with the bits in 'm' cleared.
                                                               //< otherwise return 'f' with the bits in 'm' set.


#define cmMin(v0,v1) ((v0)<(v1) ? (v0) : (v1)) //< Return the minimum arg.
#define cmMax(v0,v1) ((v0)>(v1) ? (v0) : (v1)) //< Return the maximum arg.


#define cmStringNullGuard(p) ((p)==NULL?"<null>":(p)) //< If 'p'==NULL return the static string "<null>" otherwise return 'p'.
#define cmStringLen(s)       ((s)==NULL? 0 : strlen(s))

  // Default return code indicating successful function completion.
#define cmOkRC (0)                  

  // Default directory separator character for unix based systems.
#define cmPathSeparatorChar ("/")

#define cmInvalidIdx (0xffffffff)     //< cm wide value indicating a invalid or NULL index.
#define cmInvalidId  (cmInvalidIdx)   //< cm wide value indicating an invalid or NULL numeric id.
#define cmInvalidCnt (cmInvalidIdx)   //< cm wide value indicating a invalid or NULL count of items.

#define cmSTATIC_NULL_HANDLE {NULL}   //< Default NULL value for cmHandle_t

  // Generic data type for implementing opaque object handles.
  /*
  typedef struct cmHandle_str
  {
    void* h;
  } cmHandle_t; 
  */

#define cmHandle_t struct { void* h; } 

#define cmHandlesAreEqual(    a, b ) ((a).h == (b).h)  //< Test if two cmHandle_t values are equivalent.
#define cmHandlesAreNotEqual( a, b ) (!cmHandlesAreEqual(a,b)) //< Test if two cmHandle_t value are not equivalent.

  // Data type commonly used as a function return value. Functions returning cmRC_t values 
  // return cmOkRC (0) to indicate successful completion or some other value to indicate 
  // some kind of exceptional conidtion.  In general the condition indicates an unexpected condition
  // such as resource exhaution, or a missing file. 
  typedef unsigned cmRC_t; 
  
  // A data type which indicates a system dependent error.  This is generally an abstraction for an 'errno'
  // like code.
  typedef int      cmSysErrCode_t;  // same as errno

  
  // cmChar_t is a data type used to indicate that a char is being used to hold human readable
  // text.  Eventually this type will be used to locate and handle unicode based strings.
  typedef char   cmChar_t;
  

  typedef unsigned int   cmUInt32_t;  //< This typedef is used to indicate that the type must be an  unsigned 32 bit integer.
  typedef unsigned short cmUInt16_t;  //< This typedef is used to indicate that hte type must be an  unsigned 16 bit integer.

#ifdef __cplusplus
}
#endif

//)

#endif
