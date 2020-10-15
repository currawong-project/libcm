//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmMath.h"

#ifdef OS_LINUX
#include <cblas.h>
#endif

#include "cmAudioFile.h"
#include "cmFileSys.h"
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmProc.h"
#include "cmVectOps.h"

#define cmVectOpsTemplateCode_h
#define cmVectOpsRICode_h
#include "cmVectOpsTemplateMain.h"

unsigned _cmVOU_Abs( unsigned x ) { return x; }

void cmVOU_VPrint( cmRpt_t* rpt, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  if( rpt == NULL )
    vprintf(fmt,vl);
  else
    cmRptVPrintf(rpt,fmt,vl);
  va_end(vl);
}

void cmVOI_Print( cmRpt_t* rpt, unsigned rn, unsigned cn, const int*      sbp )
{
  unsigned fieldWidth = cmDefaultFieldWidth;

  if( fieldWidth < 0 )
    fieldWidth = 10;

  unsigned ri,ci;
  for(ri=0; ri<rn; ++ri)
  {
    for(ci=0; ci<cn; ++ci )
      cmVOU_VPrint(rpt,"%*i ",fieldWidth,sbp[ (ci*rn) + ri ]);

    if( cn > 0 )
      cmVOU_VPrint(rpt,"\n");
  }
}

void cmVOU_Print( cmRpt_t* rpt, unsigned rn, unsigned cn, const unsigned* sbp )
{
  unsigned fieldWidth = cmDefaultFieldWidth;

  if( fieldWidth < 0 )
    fieldWidth = 10;

  unsigned ri,ci;
  for(ri=0; ri<rn; ++ri)
  {
    for(ci=0; ci<cn; ++ci )
      cmVOU_VPrint(rpt,"%*u ",fieldWidth,sbp[ (ci*rn) + ri ]);

    if( cn > 0 )
      cmVOU_VPrint(rpt,"\n");
  }
}

void cmVOI_PrintL( const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const int*      sp )
{
  cmVOU_VPrint(rpt,"%s ",label);
  return cmVOI_Print(rpt,rn,cn,sp);
}

void cmVOU_PrintL( const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const unsigned* sp )
{
  cmVOU_VPrint(rpt,"%s ",label);
  return cmVOU_Print(rpt,rn,cn,sp);
}

unsigned* cmVOU_Mod( unsigned* dbp, unsigned dn, unsigned modVal )
{
  const unsigned* dep = dbp + dn;
  unsigned* dp = dbp;

  while( dbp < dep )
    *dbp++ %= modVal;

  return dp;
}

unsigned* cmVOU_Hist( unsigned* hbp, unsigned hn, const unsigned* sbp, unsigned sn )
{
  const unsigned* sep = sbp + sn;

  unsigned* rp = hbp;

  memset(hbp,0,hn * sizeof(*hbp));

  while( sbp < sep )
  {
    assert( *sbp < hn );

    ++hbp[ *sbp++ ];
  }
  return rp;
}

unsigned* cmVOU_Random( unsigned* vbp, unsigned vn, unsigned maxValue )
{
  unsigned*       rp  = vbp;
  const unsigned* vep = vbp + vn;
  while( vbp < vep )
    *vbp++ = rand() % (maxValue+1);

  return rp;
}

unsigned* cmVOU_UniqueRandom( unsigned* dV, unsigned dN, unsigned maxValue )
{
  assert( dN < maxValue );

  if( dN == 0 )
    return dV;


  // if maxValue is less than twice dN then use cmVOU_RandomSeq() to
  // generate the random numbers. This is intended to avoid a situation
  // where the attempt to generate a unique random number is confined
  // to an decreasing range of possible target values - as would occur
  // if dN==maxValue. 
  if( maxValue / dN <= 2 )
  {
    unsigned* v = cmMemAllocZ( unsigned, maxValue+1 );
    cmVOU_RandomSeq(v,maxValue+1);
    cmVOU_Copy(dV,dN,v);
    cmMemPtrFree(&v);
  }

  unsigned* tV = cmMemAllocZ( unsigned, dN );
  unsigned  i  = 0;
  unsigned  j  = 0;
  unsigned  n  = dN;

  while(n)
  {
    // generate a set of random integers
    cmVOU_Random(tV,n,maxValue);
    
    // store each unique random int
    for(j=0; j<n; ++j)
      if( cmVOU_Count(dV,i,tV[j]) == 0 )
        dV[i++] = tV[j];
     
    n = dN - i;

  }

  cmMemPtrFree(&tV);
  return dV;
}

int cmVOU_Compare(const void* p0, const void* p1)
{ return *(unsigned*)p0 < *(unsigned*)p1; }

unsigned* cmVOU_RandomSeq( unsigned* vbp, unsigned vn )
{
  unsigned i,j;
  unsigned* r = cmMemAllocZ( unsigned, vn );

  // generate a list of random values
  for(i=0; i<vn; ++i)
  {
    r[i] = rand();
    vbp[i] = r[i];
  }

  // sort the random number list
  qsort(r,vn,sizeof(unsigned),cmVOU_Compare);

  // set vbp[i] to the index of the matching value in r[] 
  for(i=0; i<vn; ++i)
    for(j=0; j<vn; ++j)
      if( vbp[i] == r[j] )
      {
        vbp[i] = j;
        break;
      }
    

  cmMemPtrFree(&r);

  return vbp;
}


cmReal_t cmVOU_Mean(const unsigned* sp, unsigned sn )
{
  cmReal_t sum = (cmReal_t)cmVOU_Sum(sp,sn);
  return sum/sn;
}

cmReal_t cmVOU_Variance(const unsigned* sp, unsigned sn, const cmReal_t* meanPtr)
{
  if( sn <= 1 )
    return 0;

  cmReal_t        sum = 0;
  cmReal_t        mv  = meanPtr==NULL ? cmVOU_Mean(sp,sn) : *meanPtr;
  const unsigned* bp  = sp;
  const unsigned* ep  = sp + sn;
  
  for(; bp < ep; ++bp )
    sum += (((cmReal_t)*bp)-mv) * (((cmReal_t)*bp)-mv);
  
  return sum / (sn-1);
}

cmReal_t cmVOI_Mean(const int* sp, unsigned sn )
{
  cmReal_t sum = (cmReal_t)cmVOI_Sum(sp,sn);
  return sum/sn;  
}

cmReal_t cmVOI_Variance(const int* sp, unsigned sn, const cmReal_t* meanPtr)
{
  if( sn <= 1 )
    return 0;

  cmReal_t   sum = 0;
  cmReal_t   mv  = meanPtr==NULL ? cmVOI_Mean(sp,sn) : *meanPtr;
  const int* bp  = sp;
  const int* ep  = sp + sn;
  
  for(; bp < ep; ++bp )
    sum += (((cmReal_t)*bp)-mv) * (((cmReal_t)*bp)-mv);
  
  return sum / (sn-1);
}



// dbp[1,dn] = v[1,vn] * m[vn,dn]
cmComplexR_t* cmVORC_MultVVM( cmComplexR_t* dbp, unsigned dn, const cmComplexR_t* vp, unsigned vn, const cmComplexR_t* mp )
{
  cmComplexR_t alpha = 1.0 + (I*0);
  cmComplexR_t beta  = 0.0 + (I*0);

  unsigned drn = 1;
  unsigned dcn = dn;
  unsigned n   = vn;
  

  cblas_zgemm( CblasColMajor, CblasNoTrans, CblasNoTrans, drn, dcn, n, &alpha, vp, drn, mp, n, &beta, dbp, drn );
  
  return dbp;
}
