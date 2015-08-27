#ifdef cmVectOpsTemplateCode_h

void          VECT_OP_FUNC(VPrint)( cmRpt_t* rpt, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);

  if( rpt != NULL )
    cmRptVPrintf(rpt,fmt,vl);
  else
    vprintf(fmt,vl);

  va_end(vl);
}

void          VECT_OP_FUNC(Printf)( cmRpt_t* rpt, unsigned rowCnt, unsigned colCnt, const VECT_OP_TYPE* sbp, int fieldWidth, int decPlCnt, const char* fmt, unsigned flags )
{
  unsigned cci;
  unsigned outColCnt = 10;

  if( fieldWidth < 0 )
    fieldWidth = 10;

  if( decPlCnt < 0 )
    decPlCnt = 4;

  if( outColCnt == -1 )
    outColCnt = colCnt;

  for(cci=0; cci<colCnt; cci+=outColCnt)
  {
    unsigned ci0 = cci;
    unsigned cn  = cci + outColCnt;
    unsigned ri;

    if(cn > colCnt)
      cn = colCnt;

    if( colCnt > outColCnt )
    {
      if( cmIsFlag(flags,cmPrintMatlabLabelsFl) )
        VECT_OP_FUNC(VPrint)(rpt,"Columns:%i to %i\n",ci0,cn-1);
      else
      if( cmIsFlag(flags,cmPrintShortLabelsFl) )
        VECT_OP_FUNC(VPrint)(rpt,"%3i: ",ci0);  
    }

    if( rowCnt > 1 )
      VECT_OP_FUNC(VPrint)(rpt,"\n");

    for(ri=0; ri<rowCnt; ++ri)
    {
      unsigned ci;

      for(ci=ci0; ci<cn; ++ci )
        VECT_OP_FUNC(VPrint)(rpt,fmt,fieldWidth,decPlCnt,sbp[ (ci*rowCnt) + ri ]);

      if( cn > 0 )
        VECT_OP_FUNC(VPrint)(rpt,"\n");
    }
  }
}

void          VECT_OP_FUNC(Print)( cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* sbp )
{ VECT_OP_FUNC(Printf)(rpt,rn,cn,sbp,cmDefaultFieldWidth,cmDefaultDecPlCnt,"%*.*f ",cmPrintShortLabelsFl); }

void          VECT_OP_FUNC(PrintE)( cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* sbp )
{ VECT_OP_FUNC(Printf)(rpt,rn,cn,sbp,cmDefaultFieldWidth,cmDefaultDecPlCnt,"%*.*e ",cmPrintShortLabelsFl); }

void          VECT_OP_FUNC(PrintLf)( const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* dbp, unsigned fieldWidth, unsigned decPlCnt, const char* fmt )
{ 
  VECT_OP_FUNC(VPrint)( rpt, "%s\n", label );
  VECT_OP_FUNC(Printf)( rpt, rn, cn, dbp, fieldWidth, decPlCnt,fmt,cmPrintShortLabelsFl );
}

void          VECT_OP_FUNC(PrintL)(  const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* dbp )
{  
  VECT_OP_FUNC(VPrint)( rpt, "%s\n", label );
  VECT_OP_FUNC(Printf)(  rpt, rn, cn, dbp, cmDefaultFieldWidth,cmDefaultDecPlCnt,"%*.*f ",cmPrintShortLabelsFl );
}

void          VECT_OP_FUNC(PrintLE)( const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* dbp )
{ 
  VECT_OP_FUNC(VPrint)( rpt, "%s\n", label );
  VECT_OP_FUNC(Printf)( rpt, rn, cn, dbp, cmDefaultFieldWidth,cmDefaultDecPlCnt,"%*.*e ",cmPrintShortLabelsFl );
}


VECT_OP_TYPE* VECT_OP_FUNC(NormalizeProbabilityVV)(VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp)
{
  VECT_OP_TYPE sum = VECT_OP_FUNC(Sum)(sbp,dn);  

  if( sum == 0 )
    sum = 1;

  return VECT_OP_FUNC(DivVVS)(dbp,dn,sbp,sum);
}

VECT_OP_TYPE* VECT_OP_FUNC(NormalizeProbability)(VECT_OP_TYPE* dbp, unsigned dn)
{ return VECT_OP_FUNC(NormalizeProbabilityVV)(dbp,dn,dbp); }

VECT_OP_TYPE* VECT_OP_FUNC(NormalizeProbabilityN)(VECT_OP_TYPE* dbp, unsigned dn, unsigned stride)
{
  VECT_OP_TYPE sum = VECT_OP_FUNC(SumN)(dbp,dn,stride);
  
  if( sum == 0 )
    return dbp;


  VECT_OP_TYPE* dp = dbp;
  VECT_OP_TYPE* ep = dp + (dn*stride);
  for(;  dp < ep; dp+=stride )
    *dp /= sum;
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(StandardizeRows)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, VECT_OP_TYPE* uV, VECT_OP_TYPE* sdV )
{
  bool uFl = false;
  bool sFl = false;
  unsigned i;

  if( uV == NULL )
  {
    uV = cmMemAllocZ(VECT_OP_TYPE,drn);
    uFl = true;
  }

  if( sdV == NULL )
  {
    sdV = cmMemAllocZ(VECT_OP_TYPE,drn);
    sFl = true;    
  }

  VECT_OP_FUNC(MeanM)(uV, dbp, drn, dcn, 1 );
  
  VECT_OP_FUNC(VarianceM)(sdV, dbp, drn, dcn, uV, 1 );

  for(i=0; i<dcn; ++i)
  {
    VECT_OP_FUNC(SubVV)(dbp + i * drn, drn,  uV );
    VECT_OP_FUNC(DivVV)(dbp + i * drn, drn, sdV );
  }

  if(uFl)
    cmMemFree(uV);

  if(sFl)
    cmMemFree(sdV);

  return dbp;
  
}

VECT_OP_TYPE* VECT_OP_FUNC(StandardizeCols)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, VECT_OP_TYPE* uV, VECT_OP_TYPE* sdV )
{
  bool uFl = false;
  bool sFl = false;
  unsigned i;

  if( uV == NULL )
  {
    uV = cmMemAllocZ(VECT_OP_TYPE,dcn);
    uFl = true;
  }

  if( sdV == NULL )
  {
    sdV = cmMemAllocZ(VECT_OP_TYPE,dcn);
    sFl = true;    
  }

  VECT_OP_FUNC(MeanM)(uV, dbp, drn, dcn, 0 );
  
  VECT_OP_FUNC(VarianceM)(sdV, dbp, drn, dcn, uV, 0 );

  for(i=0; i<drn; ++i)
  {
    VECT_OP_FUNC(SubVVNN)(dbp + i, dcn,  drn,  uV, 1 );
    VECT_OP_FUNC(DivVVNN)(dbp + i, dcn,  drn, sdV, 1 );
  }

  if(uFl)
    cmMemFree(uV);

  if(sFl)
    cmMemFree(sdV);

  return dbp;
}


VECT_OP_TYPE* VECT_OP_FUNC(HalfWaveRectify)(VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp )
{
  VECT_OP_TYPE* dp = dbp;
  VECT_OP_TYPE* ep = dbp + dn;
  for(; dp < ep; ++dp,++sp )
    *dp = *sp < 0 ? 0 : *sp;

  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(CumSum)(VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp)
{
  VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE*  rp = dbp;
  VECT_OP_TYPE sum = 0;
  while( dbp < dep )
  {
    sum += *sbp++;
    *dbp++ = sum;

  }
  return rp;
}

VECT_OP_TYPE  VECT_OP_FUNC(Mean)( const VECT_OP_TYPE* bp, unsigned n )
{  return VECT_OP_FUNC(Sum)(bp,n)/n; }

VECT_OP_TYPE  VECT_OP_FUNC(MeanN)( const VECT_OP_TYPE* bp, unsigned n, unsigned stride )
{  return VECT_OP_FUNC(SumN)(bp,n,stride)/n; }

VECT_OP_TYPE*  VECT_OP_FUNC(MeanM)(    VECT_OP_TYPE* dp, const VECT_OP_TYPE* sp, unsigned srn, unsigned scn, unsigned dim )
{
  unsigned i;
  unsigned cn     = dim == 0 ? scn : srn;
  unsigned rn     = dim == 0 ? srn : scn;
  unsigned inc    = dim == 0 ? srn : 1;
  unsigned stride = dim == 0 ? 1   : srn;
  unsigned d0     = 0;

  for(i=0; i<cn; ++i, d0+=inc)
    dp[i] = VECT_OP_FUNC(MeanN)(sp + d0, rn, stride );

  return dp;
}

VECT_OP_TYPE*  VECT_OP_FUNC(MeanM2)(    VECT_OP_TYPE* dp, const VECT_OP_TYPE* sp, unsigned srn, unsigned scn, unsigned dim, unsigned cnt )
{
  unsigned i;
  unsigned cn     = dim == 0 ? scn : srn;
  unsigned rn     = dim == 0 ? srn : scn;
  unsigned inc    = dim == 0 ? srn : 1;
  unsigned stride = dim == 0 ? 1   : srn;
  unsigned d0     = 0;

  for(i=0; i<cn; ++i, d0+=inc)
    dp[i] = VECT_OP_FUNC(MeanN)(sp + d0, cmMin(rn,cnt), stride );

  return dp;
}

VECT_OP_TYPE*  VECT_OP_FUNC(Mean2)(   VECT_OP_TYPE* dp, const VECT_OP_TYPE* (*srcFuncPtr)(void* arg, unsigned idx ), unsigned D, unsigned N, void* argPtr )
{
  unsigned i,n;
  const VECT_OP_TYPE* sp;

  VECT_OP_FUNC(Zero)(dp,D);

  if( N > 1 )
  {
    n = 0;

    for(i=0; i<N; ++i)
      if((sp = srcFuncPtr(argPtr,i)) != NULL )
      {
        VECT_OP_FUNC(AddVV)(dp,D,sp);
        ++n;
      }

    VECT_OP_FUNC(DivVS)(dp,D,n);
  }

  return dp;
}

VECT_OP_TYPE  VECT_OP_FUNC(Variance)(  const VECT_OP_TYPE* sp, unsigned sn, const VECT_OP_TYPE* avgPtr )
{ return VECT_OP_FUNC(VarianceN)(sp,sn,1,avgPtr); }

VECT_OP_TYPE  VECT_OP_FUNC(VarianceN)(  const VECT_OP_TYPE* sp, unsigned sn, unsigned stride, const VECT_OP_TYPE* meanPtr )
{
  VECT_OP_TYPE mean = 0;

  if( sn <= 1 )
    return 0;

  if( meanPtr == NULL )
    mean = VECT_OP_FUNC(MeanN)( sp, sn, stride );
  else
    mean = *meanPtr;

  const VECT_OP_TYPE* ep = sp + (sn*stride);
  VECT_OP_TYPE sum = 0;
  for(; sp < ep; sp += stride )
    sum += (*sp-mean) * (*sp-mean);

  return sum / (sn-1);  
}

VECT_OP_TYPE*  VECT_OP_FUNC(VarianceM)(VECT_OP_TYPE* dp,  const VECT_OP_TYPE* sp, unsigned srn, unsigned scn, const VECT_OP_TYPE* avgPtr, unsigned dim )
{
  unsigned i;
  unsigned cn     = dim == 0 ? scn : srn;
  unsigned rn     = dim == 0 ? srn : scn;
  unsigned inc    = dim == 0 ? srn : 1;
  unsigned stride = dim == 0 ? 1   : srn;
  unsigned d0     = 0;

  for(i=0; i<cn; ++i, d0+=inc)
    dp[i] = VECT_OP_FUNC(VarianceN)(sp + d0, rn, stride, avgPtr==NULL ? NULL : avgPtr+i );

  return dp;  
}

unsigned  VECT_OP_FUNC(NormToMax)(    VECT_OP_TYPE* dp, unsigned dn )
{
  unsigned i = VECT_OP_FUNC(MaxIndex)(dp,dn,1);

  if( i != cmInvalidIdx )
  {  
    VECT_OP_TYPE v = dp[i];
    VECT_OP_FUNC(DivVS)(dp,dn,v);
  }

  return i;
}

unsigned VECT_OP_FUNC(NormToAbsMax)(   VECT_OP_TYPE* dp, unsigned dn, VECT_OP_TYPE fact )
{
  if( dn == 0 )
    return cmInvalidIdx;

  unsigned     i  = 0;
  unsigned     mi = 0;
  VECT_OP_TYPE mx = cmAbs(dp[0]);

  for(i=1; i<dn; ++i)
    if( cmAbs(dp[i])>mx )
    {
      mi = i;
      mx = cmAbs(dp[i]);
    }

  VECT_OP_FUNC(MultVS)(dp,dn,fact/mx);
  
  return mi;
  
}


VECT_OP_TYPE  VECT_OP_FUNC(AlphaNorm)(const VECT_OP_TYPE* sp, unsigned sn, VECT_OP_TYPE alpha )
{
  double sum = 0;
  const VECT_OP_TYPE* bp = sp;
  const VECT_OP_TYPE* ep = sp + sn;
  while( bp < ep )
    sum += pow(fabs(*bp++),alpha);

  return (VECT_OP_TYPE)pow(sum/sn,1.0/alpha);
}

void  VECT_OP_FUNC(GaussCovariance)(VECT_OP_TYPE* yM, unsigned D, const VECT_OP_TYPE* xM, unsigned xN, const VECT_OP_TYPE* uV, const unsigned* selIdxV, unsigned selKey )
{
  unsigned i,j,k,n = 0;
  VECT_OP_TYPE tV[ D ];
  
  VECT_OP_FUNC(Fill)(yM,D*D,0);

  // if the mean was not given - then calculate it
  if( uV == NULL )
  {
    VECT_OP_FUNC(Fill)(tV,D,0);

    // sum each row of xM[] into uM[]
    for(i=0; i<D; ++i)
    {
      n = 0;
      for(j=0; j<xN; ++j)
        if( selIdxV==NULL || selIdxV[j]==selKey )
        {
          tV[i] += xM[ (j*D) + i ];
          ++n;
        }
    }
    // form an average from the sum in tV[]
    VECT_OP_FUNC(DivVS)(tV,D,n);

    uV = tV;
  }

  for(i=0; i<D; ++i)
    for(j=i; j<D; ++j)
    {
      n = 0;

      for(k=0; k<xN; ++k)
        if( selIdxV==NULL || selIdxV[k]==selKey)
        {
          unsigned yi = (i*D)+j;
 
          yM[ yi ] += ((xM[ (k*D)+j ]-uV[j]) * (xM[ (k*D) + i ]-uV[i]));
          if( i != j )
            yM[ (j*D)+i ] = yM[ yi ];

          ++n;
        }
    }
  
  if( n>1 )
    VECT_OP_FUNC(DivVS)( yM, D*D, n-1 );

}

void  VECT_OP_FUNC(GaussCovariance2)(VECT_OP_TYPE* yM, unsigned D, const VECT_OP_TYPE* (*srcFunc)(void* userPtr, unsigned idx), unsigned xN, void* userPtr, const VECT_OP_TYPE* uV, const unsigned* selIdxV, unsigned selKey )
{
  unsigned i,j,k = 0,n;
  VECT_OP_TYPE tV[ D ];
  const VECT_OP_TYPE* sp;
  
  VECT_OP_FUNC(Fill)(yM,D*D,0);

  // if the mean was not given - then calculate it
  if( uV == NULL )
  {
    VECT_OP_FUNC(Fill)(tV,D,0);
    
    n = 0;

    // sum each row of xM[] into uM[]
    for(i=0; i<xN; ++i)
      if( (selIdxV==NULL || selIdxV[i]==selKey) && ((sp=srcFunc(userPtr,i))!=NULL) )
      {
        VECT_OP_FUNC(AddVV)(tV,D,sp);
        ++n;
      }

    // form an average from the sum in tV[]
    VECT_OP_FUNC(DivVS)(tV,D,n);

    uV = tV;
  }

  for(i=0; i<xN; ++i)
    if( selIdxV==NULL || selIdxV[i]==selKey )
    {
      // get a pointer to the ith data point
      const VECT_OP_TYPE* sV = srcFunc(userPtr,i);

      // note: this algorithm works because when a data point element (scalar) 
      // is multiplied by another data point element those two elements
      // are always part of the same data point (vector).  Two elements
      // from different data points are never multiplied.
      
      if( sV != NULL )
        for(j=0; j<D; ++j)
          for(k=j; k<D; ++k)
            yM[j + k*D] += (sV[j]-uV[j]) * (sV[k]-uV[k]);
    }

  if( n > 1 )
    VECT_OP_FUNC(DivVS)( yM, D*D, n-1 );

  // fill in the lower triangle
  for(j=0; j<D; ++j)
    for(k=j; k<D; ++k)
      yM[k + j*D] = yM[j + k*D];

}


bool          VECT_OP_FUNC(Equal)(    const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn )
{
  const VECT_OP_TYPE* ep = s0p + sn;
  while( s0p < ep )
    if( *s0p++ != *s1p++ )
      return false;
  return true;
}

bool          VECT_OP_FUNC(IsNormal)( const VECT_OP_TYPE* sp, unsigned sn )
{
  const VECT_OP_TYPE* ep = sp + sn;
  for(; sp<ep; ++sp)
    if( !isnormal(*sp) )
      return false;

  return true;
}

bool          VECT_OP_FUNC(IsNormalZ)(const VECT_OP_TYPE* sp, unsigned sn )
{
  const VECT_OP_TYPE* ep = sp + sn;
  for(; sp<ep; ++sp)
    if( (*sp != 0) && (!isnormal(*sp)) )
      return false;

  return true;
}

unsigned      VECT_OP_FUNC(FindNonNormal)( unsigned* dp, unsigned dn, const VECT_OP_TYPE* sbp )
{
  const VECT_OP_TYPE* sp = sbp;
  const VECT_OP_TYPE* ep = sp + dn;
  unsigned            n  = 0;

  for(; sp<ep; ++sp)
    if( !isnormal(*sp) )
      dp[n++] = sp - sbp;

  return n;

}

unsigned      VECT_OP_FUNC(FindNonNormalZ)( unsigned* dp, unsigned dn, const VECT_OP_TYPE* sbp )
{
  const VECT_OP_TYPE* sp = sbp;
  const VECT_OP_TYPE* ep = sp + dn;
  unsigned            n  = 0;

  for(; sp<ep; ++sp)
    if( (*sp!=0) && (!isnormal(*sp)) )
      dp[n++] = sp - sbp;

  return n;
}



unsigned      VECT_OP_FUNC(ZeroCrossCount)( const VECT_OP_TYPE* bp, unsigned bn, VECT_OP_TYPE* delaySmpPtr)
{
  unsigned n = delaySmpPtr != NULL ? ((*delaySmpPtr >= 0) != (*bp >= 0)) : 0 ;
  const VECT_OP_TYPE* ep = bp + bn;
  for(; bp<ep-1; ++bp)
    if( (*bp >= 0) != (*(bp+1) >= 0) )
      ++n;

  if( delaySmpPtr != NULL )
    *delaySmpPtr = *bp;

  return n;
}

VECT_OP_TYPE VECT_OP_FUNC(SquaredSum)( const VECT_OP_TYPE* bp, unsigned bn )
{
  VECT_OP_TYPE        sum = 0;
  const VECT_OP_TYPE* ep  = bp + bn;

  for(; bp < ep; ++bp )
    sum += *bp * *bp;
  return sum;
}

VECT_OP_TYPE  VECT_OP_FUNC(RMS)( const VECT_OP_TYPE* bp, unsigned bn, unsigned wndSmpCnt )
{
  const VECT_OP_TYPE* ep = bp + bn;

  if( bn==0 )
    return 0;

  assert( bn <= wndSmpCnt );

  double sum = 0;
  for(; bp < ep; ++bp )
    sum += *bp * *bp;

  return (VECT_OP_TYPE)sqrt(sum/wndSmpCnt);
}

VECT_OP_TYPE* VECT_OP_FUNC(RmsV)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sp, unsigned sn, unsigned wndSmpCnt, unsigned hopSmpCnt )
{
  const VECT_OP_TYPE* dep       = dp + dn;
  const VECT_OP_TYPE* sep       = sp + sn;
  VECT_OP_TYPE*       rp        = dp;

  for(; dp<dep && sp<sep; sp+=hopSmpCnt)
        *dp++ = VECT_OP_FUNC(RMS)( sp, cmMin(wndSmpCnt,sep-sp), wndSmpCnt );
  

  VECT_OP_FUNC(Zero)(dp,dep-dp);

  return rp;
}

VECT_OP_TYPE  VECT_OP_FUNC(EuclidNorm)( const VECT_OP_TYPE* sp, unsigned sn )
{ return (VECT_OP_TYPE)sqrt( VECT_OP_FUNC(MultSumVV)(sp,sp,sn)); }

/*
From:http://www.ee.ic.ac.uk/hp/staff/dmb/voicebox/doc/voicebox/distitpf.html
[nf1,p2]=size(pf1);
p1=p2-1;
nf2=size(pf2,1);
nx= min(nf1,nf2);
r = pf1(1:nx,:)./pf2(1:nx,:);
q = r-log(r);                            
s = sum( q(:,2:p1),2) + 0.5 * (q(:,1)+q(:,p2)) 
d= s/p1-1;

*/

VECT_OP_TYPE VECT_OP_FUNC(ItakuraDistance)( const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn )
{
  VECT_OP_TYPE d = 0;

  VECT_OP_TYPE r[ sn ];
  VECT_OP_TYPE q[ sn ];

  // r = pf1(1:nx,:)./pf2(1:nx,:);
  VECT_OP_FUNC(DivVVV)(r,sn,s0p,s1p);

  //q=log(r);  
  VECT_OP_FUNC(LogV)(q,sn,r);

  //r = r - q = r - log(r)
  VECT_OP_FUNC(SubVV)(r,sn,q);

  //r = r - sn = r - log(r) - 1 
  VECT_OP_FUNC(SubVS)(r,sn,sn);

  // d  = sum(r);
  d = VECT_OP_FUNC(Sum)(r,sn);

  return (VECT_OP_TYPE)(d / sn);
  
  //d  = log( VECT_OP_FUNC(Sum)(r,sn) /sn );

  //d -= VECT_OP_FUNC(Sum)(q,sn)/sn;

  return d;
}

VECT_OP_TYPE  VECT_OP_FUNC(CosineDistance)( const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn )
{
  VECT_OP_TYPE d0 = VECT_OP_FUNC(EuclidNorm)(s0p,sn);
  VECT_OP_TYPE d1 = VECT_OP_FUNC(EuclidNorm)(s1p,sn);

  if( d0 == 0 )
    d0 = cmReal_MIN;

  if( d1 == 0 )
    d1 = cmReal_MIN;
      
  return  (VECT_OP_TYPE)(VECT_OP_FUNC(MultSumVV)(s0p,s1p,sn) / (d0 * d1));  
}


VECT_OP_TYPE  VECT_OP_FUNC(EuclidDistance)( const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn )
{
  double d = 0;

  const VECT_OP_TYPE* sep = s0p + sn;
  for(; s0p<sep; ++s0p,++s1p)
    d += (*s0p - *s1p) * (*s0p - *s1p);
  
  return (VECT_OP_TYPE)(sqrt(d));

}

VECT_OP_TYPE  VECT_OP_FUNC(L1Distance)( const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn )
{
  double d = 0;

  const VECT_OP_TYPE* sep = s0p + sn;
  for(; s0p<sep; ++s0p,++s1p)
    d += (VECT_OP_TYPE)fabs(*s0p - *s1p);
  
  return d;
}

VECT_OP_TYPE  VECT_OP_FUNC(MahalanobisDistance)( const VECT_OP_TYPE* x, unsigned D, const VECT_OP_TYPE* u, const VECT_OP_TYPE* invCovM )
{
  VECT_OP_TYPE t[ D ];
  VECT_OP_TYPE d[ D ];

  // t[] = x[] - u[];
  VECT_OP_FUNC(SubVVV)(t,D,x,u);

  // d[1,D] = t[1,D] * covM[D,D]
  VECT_OP_FUNC(MultVVM)( d, D, t, D, invCovM );

  // d = sum(d[].*t[])
  VECT_OP_TYPE dist = VECT_OP_FUNC(MultSumVV)(d,t,D);  

  return (VECT_OP_TYPE)sqrt(dist);
}

VECT_OP_TYPE VECT_OP_FUNC(KL_Distance)( const VECT_OP_TYPE* up, const VECT_OP_TYPE* sp, unsigned sn )
{
  VECT_OP_TYPE v[ sn ];
  VECT_OP_FUNC(DivVVV)(v,sn,up,sp);  // v = up ./ sp
  VECT_OP_FUNC(LogV)(v,sn,v);        // v = log(v)
  VECT_OP_FUNC(MultVV)(v,sn,up);     // v *= up;
  return VECT_OP_FUNC(Sum)(v,sn);    // sum(v)
}

VECT_OP_TYPE VECT_OP_FUNC(KL_Distance2)( const VECT_OP_TYPE* up, const VECT_OP_TYPE* sp, unsigned sn )
{
  VECT_OP_TYPE v0[ sn ];
  VECT_OP_TYPE v1[ sn ];
  VECT_OP_FUNC(NormalizeProbabilityVV)(v0,sn,up);
  VECT_OP_FUNC(NormalizeProbabilityVV)(v1,sn,sp);
  return VECT_OP_FUNC(KL_Distance)(v0,v1,sn);
}

/// If dv[scn] is non NULL then return the Euclidean distance from sv[scn] to each column of sm[srn,scn].
/// The function returns the index of the closest data point (column) in sm[].
unsigned VECT_OP_FUNC(EuclidDistanceVM)( VECT_OP_TYPE* dv, const VECT_OP_TYPE* sv, const VECT_OP_TYPE* sm, unsigned srn, unsigned scn ) 
{

 unsigned             minIdx  = cmInvalidIdx;
 VECT_OP_TYPE         minDist = 0;
 unsigned             i       = 0;

  for(; i<scn; ++i )
  {
    VECT_OP_TYPE dist = VECT_OP_FUNC(EuclidDistance)(sv, sm + (i*srn), srn );

    if( dv != NULL )
     *dv++ = dist;

    if( dist < minDist || minIdx == cmInvalidIdx )
    {
      minIdx  = i;
      minDist = dist;
    }
  }

  return minIdx;
}

void VECT_OP_FUNC(DistVMM)( VECT_OP_TYPE* dM, VECT_OP_TYPE* mvV, unsigned* miV, unsigned rn, const VECT_OP_TYPE* s0M, unsigned s0cn, const VECT_OP_TYPE* s1M, unsigned s1cn, VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* s0V, const VECT_OP_TYPE* s1V, unsigned sn ), void* userPtr )
{
  unsigned i,j,k;

  // for each col in s0M[];
  for(i=0,k=0; i<s0cn; ++i)
  {
    VECT_OP_TYPE min_val = VECT_OP_MAX;
    unsigned     min_idx = cmInvalidIdx;

    // for each col in s1M[]
    for(j=0; j<s1cn; ++j,++k)
    {
      // v = distance(s0M[:,i],s1M[:,j]
      VECT_OP_TYPE v = distFunc( userPtr, s1M + (j*rn), s0M + (i*rn), rn );

      if( dM != NULL )
        dM[k] = v;     // store distance

      // track closest col in s1M[]
      if( v < min_val || min_idx==cmInvalidIdx )
      {
        min_val = v;
        min_idx = j;
      }
    }

    if( mvV != NULL )
      mvV[i] = min_val;

    if( miV != NULL )
      miV[i] = min_idx;
  }
}

void VECT_OP_FUNC(SelectRandom) ( VECT_OP_TYPE *dM, unsigned* selIdxV, unsigned selIdxN, const VECT_OP_TYPE* sM, unsigned srn, unsigned scn )
{
  bool freeFl = false;
  unsigned i;

  assert( selIdxN != 0 );

  // if no selIdxV[] was given then create one
  if( selIdxV == NULL )
  {
    selIdxV    = cmMemAlloc( unsigned, selIdxN );
    freeFl     = true;
  }

  // select  datapoints at random 
  cmVOU_UniqueRandom(selIdxV,selIdxN,scn);
  
  // copy the data points into the output matrix
  if( dM != NULL )
    for(i=0; i<selIdxN; ++i)
    {
      assert( selIdxV[i] < scn );

      VECT_OP_FUNC(Copy)( dM + (i*srn), srn, sM + selIdxV[i]*srn );
    }

  if( freeFl )
    cmMemPtrFree(&selIdxV);

}

void VECT_OP_FUNC(_SelectDist)( VECT_OP_TYPE *dM, unsigned* selIdxV, unsigned selIdxN, const VECT_OP_TYPE* sM, unsigned srn, unsigned scn, VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* s0V, const VECT_OP_TYPE* s1V, unsigned sn ), void* userPtr, bool avgFl )
{
  unsigned i;
  unsigned dcn    = 0;
  bool     freeFl = false;

  assert( selIdxN > 0 );

  if( dM == NULL )
  {
    dM = cmMemAllocZ( VECT_OP_TYPE, srn*selIdxN );
    freeFl = true;
  }

  // allocate distM[scn,selIdxN] to hold the distances from each selected column to all columns in sM[]
  VECT_OP_TYPE* distM = cmMemAllocZ( VECT_OP_TYPE, scn*selIdxN );

  // sumV[] is a temp vector to hold the summed distances to from the selected columns to each column in sM[]
  VECT_OP_TYPE* sumV  = cmMemAllocZ( VECT_OP_TYPE, scn );

  // select a random point from sM[] and copy it to the first column of dM[]
  cmVOU_Random(&i,1,scn);
  VECT_OP_FUNC(Copy)(dM, srn, sM + (i*srn)); 
  
  if( selIdxV != NULL )
    selIdxV[0] = i;
  
  for(dcn=1; dcn<selIdxN; ++dcn)
  {
    // set distM[scn,dcn] with the dist from dM[dcn,srn] to each column in sM[]
    VECT_OP_FUNC(DistVMM)( distM, NULL, NULL, srn, dM, dcn, sM, scn, distFunc, userPtr );

    // sum the rows of distM[ scn, dcn ] into sumV[scn]
    VECT_OP_FUNC(SumMN)( distM, scn, dcn, sumV );

    if( avgFl )
      VECT_OP_FUNC(DivVS)( sumV, scn, dcn );
   
    // find the point in sM[] which has the greatest combined distance to all previously selected points.
    unsigned maxIdx = VECT_OP_FUNC(MaxIndex)(sumV, scn, 1 );

    // copy the point into dM[]
    VECT_OP_FUNC(Copy)(dM + (dcn*srn), srn, sM + (maxIdx*srn));   

    if( selIdxV != NULL )
      selIdxV[dcn] = maxIdx;
  }

  cmMemPtrFree(&distM);
  cmMemPtrFree(&sumV);

  if( freeFl )
    cmMemPtrFree(&dM);
}

void VECT_OP_FUNC(SelectMaxDist)( VECT_OP_TYPE *dM, unsigned* selIdxV, unsigned selIdxN, const VECT_OP_TYPE* sM, unsigned srn, unsigned scn, VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* s0V, const VECT_OP_TYPE* s1V, unsigned sn ), void* userPtr )
{ VECT_OP_FUNC(_SelectDist)(dM,selIdxV,selIdxN,sM,srn,scn,distFunc,userPtr,false); }

void VECT_OP_FUNC(SelectMaxAvgDist)( VECT_OP_TYPE *dM, unsigned* selIdxV, unsigned selIdxN, const VECT_OP_TYPE* sM, unsigned srn, unsigned scn, VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* s0V, const VECT_OP_TYPE* s1V, unsigned sn ), void* userPtr ) 
{ VECT_OP_FUNC(_SelectDist)(dM,selIdxV,selIdxN,sM,srn,scn,distFunc,userPtr,true); }


#ifdef CM_VECTOP

VECT_OP_TYPE VECT_OP_FUNC(MultSumVV)( const VECT_OP_TYPE* s0p,  const VECT_OP_TYPE* s1p, unsigned sn )
{ return VECT_OP_BLAS_FUNC(dot)(sn, s0p, 1, s1p, 1); }

#else

VECT_OP_TYPE VECT_OP_FUNC(MultSumVV)( const VECT_OP_TYPE* s0p,  const VECT_OP_TYPE* s1p, unsigned sn )
{
  VECT_OP_TYPE sum = 0;
  const VECT_OP_TYPE* sep = s0p + sn;
  
  while(s0p<sep)
    sum += *s0p++ * *s1p++;

  return sum;
}
#endif

VECT_OP_TYPE VECT_OP_FUNC(MultSumVS)( const VECT_OP_TYPE* s0p, unsigned sn, VECT_OP_TYPE s1 )
{
  VECT_OP_TYPE sum = 0;
  const VECT_OP_TYPE* sep = s0p + sn;
  
  while(s0p<sep)
    sum += *s0p++ * s1;

  return sum;
}

#ifdef CM_VECTOP

VECT_OP_TYPE* VECT_OP_FUNC(MultVMV)( VECT_OP_TYPE* dbp, unsigned mrn, const VECT_OP_TYPE* mp, unsigned mcn, const VECT_OP_TYPE* vp )
{
  VECT_OP_BLAS_FUNC(gemv)( CblasColMajor, CblasNoTrans, mrn, mcn, 1.0, mp, mrn, vp, 1, 0.0, dbp, 1 );
  
  return dbp;
}

#else

VECT_OP_TYPE* VECT_OP_FUNC(MultVMV)( VECT_OP_TYPE* dbp, unsigned mrn, const VECT_OP_TYPE* mp, unsigned mcn, const VECT_OP_TYPE* vp )
{
  const VECT_OP_TYPE* dep = dbp + mrn;
  VECT_OP_TYPE*        dp  = dbp;
  const VECT_OP_TYPE*  vep = vp + mcn;

  // for each dest element
  for(; dbp < dep; ++dbp )
  {
    const VECT_OP_TYPE*  vbp = vp;
    const VECT_OP_TYPE*  mbp = mp++;

    *dbp = 0;

    // for each source vector row and src mtx col
    while( vbp < vep )
    {
      *dbp += *mbp * *vbp++;
       mbp += mrn;
    }
  } 

  return dp;
}
#endif


#ifdef CM_VECTOP

VECT_OP_TYPE* VECT_OP_FUNC(MultVVM)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* vp, unsigned vn, const VECT_OP_TYPE* mp )
{
  VECT_OP_BLAS_FUNC(gemv)( CblasColMajor, CblasTrans, vn, dn, 1.0, mp, vn, vp, 1, 0.0, dbp, 1 );
  return dbp;
}

#else

VECT_OP_TYPE* VECT_OP_FUNC(MultVVM)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* vp, unsigned vn, const VECT_OP_TYPE* mp )
{
  unsigned i; 
  for(i=0; i<dn; ++i)
    dbp[i] = VECT_OP_FUNC(MultSumVV)(vp,mp + (i*vn),vn);
  return dbp;
}
#endif


#ifdef CM_VECTOP

VECT_OP_TYPE* VECT_OP_FUNC(MultVMtV)( VECT_OP_TYPE* dbp, unsigned mcn, const VECT_OP_TYPE* mp, unsigned mrn, const VECT_OP_TYPE* vp )
{
  VECT_OP_BLAS_FUNC(gemv)( CblasColMajor, CblasTrans, mrn, mcn, 1.0, mp, mrn, vp, 1, 0.0, dbp, 1 );
  return dbp;
}

#else

VECT_OP_TYPE* VECT_OP_FUNC(MultVMtV)( VECT_OP_TYPE* dbp, unsigned mcn, const VECT_OP_TYPE* mp, unsigned mrn, const VECT_OP_TYPE* vp )
{
  const VECT_OP_TYPE*  dep = dbp + mcn;
  VECT_OP_TYPE*        dp  = dbp;
  const VECT_OP_TYPE*  vep = vp + mrn;

  // for each dest element
  for(; dbp < dep; ++dbp )
  {
    const VECT_OP_TYPE*  vbp = vp;

    *dbp = 0;

    // for each source vector row and src mtx col
    while( vbp < vep )
      *dbp += *mp++ * *vbp++;


  } 

  return dp;
}

#endif

VECT_OP_TYPE* VECT_OP_FUNC(MultDiagVMV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* mp, unsigned mcn, const VECT_OP_TYPE* vp )
{
  VECT_OP_TYPE* rp = dbp;

  const VECT_OP_TYPE* mep = mp + (dn*mcn);

  // for each dest element
  for(; mp < mep; mp += dn+1 )
    *dbp++ = *vp++ * *mp;

  return rp;  
}

/*
  Fortran Doc: http://www.netlib.org/blas/cgemm.f
  
  C Doc:  http://techpubs.sgi.com/library/tpl/cgi-bin/getdoc.cgi?cmd=getdoc&coll=0650&db=man&fname=3%20INTRO_CBLAS

  C = alpha * op(A) * op(B) + beta * C

  cblas_Xgemm(
  order,               enum CBLAS_ORDER {CblasRowMajor=101, CblasColMajor=102};
  transposeA,          enum CBLAS_TRANSPOSE { CblasNoTrans, CblasTrans, CBlasConjTrans }
  transposeB,
  M,                   row op(A) and rows C        (i.e. rows of A 'after' optional transpose)     
  N,                   col op(B) and cols C        (i.e. rows of B 'after' optional transpose)
  K,                   col op(A) and rows op(B)    
  alpha,               A scalar         
  A,                   pointer to source matrix A
  lda,                 number of rows in A as it is stored in memory (assuming col major order)
  B,                   pointer to source matrix B
  ldb,                 number of rows in B as it is stored in memory (assuming col major order)
  beta                 C scalar
  C,                   pointer to destination matrix C
  ldc                  number of rows in C as it is stored in memory  (assuming col major order)
  )  
  
 */

#ifdef CM_VECTOP
VECT_OP_TYPE* VECT_OP_FUNC(MultMMM1)(VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, VECT_OP_TYPE alpha, const VECT_OP_TYPE* m0, const VECT_OP_TYPE* m1, unsigned n, VECT_OP_TYPE beta, unsigned flags )
{
  bool t0fl = cmIsFlag(flags,kTransposeM0Fl);
  bool t1fl = cmIsFlag(flags,kTransposeM1Fl);

  VECT_OP_BLAS_FUNC(gemm)( 
    CblasColMajor, 
    t0fl ? CblasTrans : CblasNoTrans, 
    t1fl ? CblasTrans : CblasNoTrans, 
    drn, dcn, n, 
    alpha, 
    m0,  t0fl ? n   : drn, 
    m1,  t1fl ? dcn : n, 
    beta, 
    dbp, drn );

  return dbp;

}
#else

// Not implemented.

#endif

#ifdef CM_VECTOP
VECT_OP_TYPE* VECT_OP_FUNC(MultMMM2)(VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, VECT_OP_TYPE alpha, const VECT_OP_TYPE* m0, const VECT_OP_TYPE* m1, unsigned n, VECT_OP_TYPE beta, unsigned flags, unsigned dprn, unsigned m0prn, unsigned m1prn )
{

  VECT_OP_BLAS_FUNC(gemm)( 
    CblasColMajor, 
    cmIsFlag(flags,kTransposeM0Fl) ? CblasTrans : CblasNoTrans, 
    cmIsFlag(flags,kTransposeM1Fl) ? CblasTrans : CblasNoTrans, 
    drn, dcn, n, 
    alpha, 
    m0,  m0prn, 
    m1,  m1prn, 
    beta, 
    dbp, dprn );

  return dbp;
}
#else

// Not implemented.

#endif


#ifdef CM_VECTOP

VECT_OP_TYPE* VECT_OP_FUNC(MultMMM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* m0, const VECT_OP_TYPE* m1, unsigned n )
{
  VECT_OP_BLAS_FUNC(gemm)( 
    CblasColMajor, 
    CblasNoTrans, CblasNoTrans, 
    drn, dcn, n, 
    1.0, m0,  drn, 
         m1,  n, 
    0.0, dbp, drn );
  return dbp;
}

#else

VECT_OP_TYPE* VECT_OP_FUNC(MultMMM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* m0, const VECT_OP_TYPE* m1, unsigned m0cn_m1rn )
{
  unsigned i;

  for(i=0; i<dcn; ++i)
    VECT_OP_FUNC(MultVMV)(dbp+(i*drn),drn,m0,m0cn_m1rn,m1+(i*m0cn_m1rn));

  return dbp;
}

#endif

#ifdef CM_VECTOP
VECT_OP_TYPE* VECT_OP_FUNC(MultMMMt)(VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* m0, const VECT_OP_TYPE* m1, unsigned m0cn_m1rn )
{
  VECT_OP_BLAS_FUNC(gemm)( CblasColMajor, CblasNoTrans, CblasTrans, 
    drn, dcn, m0cn_m1rn, 
    1.0, m0, drn, 
         m1, dcn, 
    0.0, dbp, drn );

  return dbp;
}
#else
VECT_OP_TYPE* VECT_OP_FUNC(MultMMMt)(VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* m0, const VECT_OP_TYPE* m1, unsigned m0cn_m1rn )
{
  unsigned i,j,k;
  VECT_OP_FUNC(Zero)(dbp,drn*dcn);

  for(i=0; i<dcn; ++i)
    for(j=0; j<drn; ++j)
      for(k=0; k<m0cn_m1rn; ++k)
        dbp[ i*drn + j ] += m0[ k*drn + j ] * m1[ k*dcn + i ];

  return dbp;
}
#endif

VECT_OP_TYPE* VECT_OP_FUNC(PowVS)(  VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE expo )
{
  VECT_OP_TYPE* dp = dbp;
  VECT_OP_TYPE* ep = dp + dn;
  for(; dp < ep; ++dp )
    *dp = (VECT_OP_TYPE)pow(*dp,expo);

  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(PowVVS)(  VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp, VECT_OP_TYPE expo )
{
  VECT_OP_TYPE* dp = dbp;
  VECT_OP_TYPE* ep = dp + dn;
  for(; dp < ep; ++dp,++sp )
    *dp = (VECT_OP_TYPE)pow(*sp,expo);

  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(LogV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp )
{
  VECT_OP_TYPE* dp = dbp;
  VECT_OP_TYPE* ep = dp + dn;
  for(; dp <ep; ++dp,++sbp)
    *dp = (VECT_OP_TYPE)log(*sbp);
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(AmplToDbVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE minDb )
{
  VECT_OP_TYPE  minVal = pow(10.0,minDb/20.0);
  VECT_OP_TYPE* dp     = dbp;
  VECT_OP_TYPE* ep     = dp + dn;

  for(; dp<ep; ++dp,++sbp)
    *dp = *sbp<minVal ? minDb : 20.0 * log10(*sbp);
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(DbToAmplVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp)
{
  VECT_OP_TYPE* dp = dbp;
  VECT_OP_TYPE* ep = dp + dn;
  for(; dp<ep; ++dp,++sbp)
    *dp = pow(10.0,*sbp/20.0);
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(PowToDbVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE minDb )
{
  VECT_OP_TYPE  minVal = pow(10.0,minDb/10.0);
  VECT_OP_TYPE* dp     = dbp;
  VECT_OP_TYPE* ep     = dp + dn;

  for(; dp<ep; ++dp,++sbp)
    *dp = *sbp<minVal ? minDb : 10.0 * log10(*sbp);
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(DbToPowVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp)
{
  VECT_OP_TYPE* dp = dbp;
  VECT_OP_TYPE* ep = dp + dn;
  for(; dp<ep; ++dp,++sbp)
    *dp = pow(10.0,*sbp/10.0);
  return dbp;
}


VECT_OP_TYPE* VECT_OP_FUNC(RandSymPosDef)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE* t )
{
  unsigned i,j;

  bool fl = t == NULL;
  
  if( fl )
    t = cmMemAlloc( VECT_OP_TYPE , dn*dn );

  do
  {
    // intialize t[] as a square symetric matrix with random values
    for(i=0; i<dn; ++i)
      for(j=i; j<dn; ++j)
      {
        VECT_OP_TYPE v = (VECT_OP_TYPE)rand()/RAND_MAX;

        t[ (i*dn) + j ] = v;

        if( i != j )
          t[ (j*dn) + i ] = v;
      }


    // square t[] to force the eigenvalues to be positive
    VECT_OP_FUNC(MultMMM)(dbp,dn,dn,t,t,dn);

    VECT_OP_FUNC(Copy)(t,dn*dn,dbp);

    // test that func is positive definite
  }while( VECT_OP_FUNC(Chol)(t,dn)==NULL );


  if( fl )
    cmMemFree(t);

  return dbp;  
}


// Calculate the determinant of a matrix previously factored by
// the lapack function dgetrf_()
VECT_OP_TYPE VECT_OP_FUNC(LUDet)( const VECT_OP_TYPE* lu, const int_lap_t* ipiv, int rn )
{
  VECT_OP_TYPE  det1 = 1;
  int           det2 = 0;
  int           i;

  for(i=0; i<rn; ++i)
  {
    if( ipiv != NULL && ipiv[i] != (i+1) )
      det1 = -det1;

    det1 = lu[ (i*rn) + i ] * det1;

    if( det1 == 0 )
      break;

    while( fabs(det1) <= 1 )
    {
      det1 *= 10;
      det2 -= 1;
    }
    //continue;

    while( fabs(det1) >= 10 )
    {
      det1 /= 10;
      det2 += 1;
    }      
  }

  // Here's where underflow or overflow might happen.
  // Enable floating point exception handling to trap.
  det1 *=  pow(10.0,det2);

  return det1;
}

// take the inverse of a matrix factored via lapack dgetrf_()
VECT_OP_TYPE* VECT_OP_FUNC(LUInverse)(VECT_OP_TYPE* dp, int_lap_t* ipiv, int drn )
{

  int_lap_t ispec = 1;
  int_lap_t rn    = drn;
  int_lap_t n1    = drn;
  int_lap_t n2    = drn;
  int_lap_t n3    = drn;
  int_lap_t n4    = drn;


  char funcNameStr[] = {"DGETRI"};

  // Calculate the NB factor for LWORK - 
  // The two args are length of string args 'funcNameStr' and ' '.
  // It is not clear how many 'n' args are requred so all are passed set to 'drn'
#ifdef OS_OSX
   int nb = ilaenv_(&ispec, funcNameStr, " ", &n1,&n2,&n3,&n4 );
#else
  int nb = ilaenv_(&ispec, funcNameStr, " ", &n1,&n2,&n3,&n4, strlen(funcNameStr), 1 );
#endif
  
  VECT_OP_TYPE w[drn * nb];    // allocate working memory
  int_lap_t    info;

  // calculate inv(A) base on LU factorization 
  VECT_OP_LAP_FUNC(getri_)(&rn,dp,&rn,ipiv,w,&rn,&info);

  assert(info==0);

  return info ==0 ? dp : NULL;
  
}

VECT_OP_TYPE  VECT_OP_FUNC(DetM)( const VECT_OP_TYPE* sp, unsigned srn )
{
  int_lap_t arn = srn;
  VECT_OP_TYPE A[ arn * arn ];
  int_lap_t ipiv[ arn ];
  int_lap_t info;

  VECT_OP_FUNC(Copy)(A,arn*arn,sp);

  // PLU factor
  VECT_OP_LAP_FUNC(getrf_)(&arn,&arn,A,&arn,ipiv,&info);

  if( info == 0 )
    return VECT_OP_FUNC(LUDet)(A,ipiv,arn);

  return 0;
}

VECT_OP_TYPE  VECT_OP_FUNC(DetDiagM)( const VECT_OP_TYPE* sp, unsigned srn )
{ return VECT_OP_FUNC(LUDet)(sp,NULL,srn); }


VECT_OP_TYPE  VECT_OP_FUNC(LogDetM)( const VECT_OP_TYPE* sp, unsigned srn )
{
  cmReal_t            det    = 0;
  unsigned            ne2    = srn * srn;

  VECT_OP_TYPE        U[ne2];
  const VECT_OP_TYPE* up     = U;
  const VECT_OP_TYPE* ep     = up  + ne2;

  VECT_OP_FUNC(Copy)(U,ne2,sp);
  VECT_OP_FUNC(Chol)(U,srn);

  for(; up<ep; up += (srn+1) )
    det += log(*up);

  return 2*det;
}

VECT_OP_TYPE  VECT_OP_FUNC(LogDetDiagM)( const VECT_OP_TYPE* sp, unsigned srn )
{ return log(VECT_OP_FUNC(DetDiagM)(sp,srn)); }

VECT_OP_TYPE*  VECT_OP_FUNC(InvM)( VECT_OP_TYPE* dp, unsigned drn )
{
  int_lap_t rn = drn;
  int_lap_t ipiv[ rn ];
  int_lap_t info;

  // PLU factor
  VECT_OP_LAP_FUNC(getrf_)(&rn,&rn,dp,&rn,ipiv,&info);

  if( info == 0 )
    return VECT_OP_FUNC(LUInverse)(dp,ipiv,rn );

  return NULL;
}

VECT_OP_TYPE* VECT_OP_FUNC(InvDiagM)(    VECT_OP_TYPE* dp, unsigned drn )
{
  const VECT_OP_TYPE* dep = dp + (drn*drn);
  VECT_OP_TYPE*       rp  = dp;

  for(; dp < dep; dp += drn+1 )
  {
    *dp = 1.0 / *dp;

    // if any element on the diagonal is zero then the 
    // determinant is zero and the matrix is not invertable
    if( *dp == 0 )
      break;
  }

  return dp < dep ? NULL : rp;
}


VECT_OP_TYPE* VECT_OP_FUNC(SolveLS)( VECT_OP_TYPE* A, unsigned an, VECT_OP_TYPE* B, unsigned bcn )
{
  int_lap_t aN   = an;
  int_lap_t bcN  = bcn;
  int_lap_t ipiv[ an ];
  int_lap_t info = 0;

  VECT_OP_LAP_FUNC(gesv_)(&aN,&bcN,(VECT_OP_TYPE*)A,&aN,ipiv,B,&aN,&info); 
  
  return info == 0 ? B : NULL;
}

VECT_OP_TYPE* VECT_OP_FUNC(Chol)(VECT_OP_TYPE* A, unsigned an )
{
  char      uplo = 'U'; 

  int_lap_t N    = an;
  int_lap_t lda  = an;
  int_lap_t info = 0;
  
  VECT_OP_LAP_FUNC(potrf_(&uplo,&N,(VECT_OP_TYPE*)A,&lda,&info));

  return info == 0 ? A : NULL;
}

VECT_OP_TYPE* VECT_OP_FUNC(CholZ)(VECT_OP_TYPE* A, unsigned an )
{
  unsigned i,j;
  VECT_OP_FUNC(Chol)(A,an);

  // zero the lower triangle of A
  for(i=0; i<an; ++i)
    for(j=i+1; j<an; ++j)
      A[ (i*an) + j ] = 0;

  return A;
}

VECT_OP_TYPE VECT_OP_FUNC(FracAvg)( double bi, double ei, const VECT_OP_TYPE* sbp, unsigned sn )
{
  unsigned bii   = cmMax(0,cmMin(sn-1,(unsigned)ceil(bi)));
  unsigned eii   = cmMax(0,cmMin(sn,(unsigned)floor(ei)+1));

  double   begW  = bii - bi;
  double   endW  = eii - floor(ei);
  
  double   cnt   = eii - bii;

  double   sum   = (double)VECT_OP_FUNC(Sum)(sbp+bii,eii-bii);

  if( begW>0 && bii > 0 )
  {
    cnt += begW;
    sum += begW * sbp[ bii-1 ];
  }

  if( endW>0 && eii+1 < sn )
  {
    cnt += endW;
    sum += endW * sbp[ eii+1 ];

  }

  return (VECT_OP_TYPE)(sum / cnt);
  
  
}

VECT_OP_TYPE* VECT_OP_FUNC(DownSampleAvg)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, unsigned sn )
{
  const VECT_OP_TYPE* dep  = dbp + dn;
  VECT_OP_TYPE*       rp   = dbp;
  unsigned            i    = 0;
  double              fact = (double)sn / dn;

  assert( sn >= dn );

  for(i=0; dbp < dep; ++i )
    *dbp++ = VECT_OP_FUNC(FracAvg)( fact*i, fact*(i+1), sbp, sn );

  return rp;
}

VECT_OP_TYPE* VECT_OP_FUNC(UpSampleInterp)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, unsigned sn )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  const VECT_OP_TYPE* sep = sbp + sn;
  VECT_OP_TYPE* rp   = dbp;
  double        fact = (double)sn / dn;
  double        phs  = 0;

  assert( sn <= dn );
		
  while( dbp<dep )
  {
    if( sbp < sep )
      *dbp++ =  (VECT_OP_TYPE)((*sbp) + (phs * ((*(sbp+1)) - (*sbp))));
    else
      *dbp++ = (*(sep-1));
			
    phs += fact;
			
    while( phs > 1.0 )
    {
      phs -= 1.0;
      sbp++;
    }
  }		

  return rp;
}

VECT_OP_TYPE* VECT_OP_FUNC(FitToSize)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, unsigned sn )
{
  if( dn == sn )
    return VECT_OP_FUNC(Copy)(dbp,dn,sbp);

  if( dn < sn )
    return VECT_OP_FUNC(DownSampleAvg)(dbp,dn,sbp,sn);
  
  return VECT_OP_FUNC(UpSampleInterp)(dbp,dn,sbp,sn);
}

VECT_OP_TYPE* VECT_OP_FUNC(LinearMap)(VECT_OP_TYPE* dV, unsigned dn, VECT_OP_TYPE* sV, unsigned sn )
{
  if( dn == sn )
  {
    memcpy(dV,sV,dn*sizeof(VECT_OP_TYPE));
    return dV;
  }
  
  unsigned     i,j,k;

  // if stretching
  if( dn > sn )
  {
    VECT_OP_TYPE f_n  = (VECT_OP_TYPE)dn/sn;
    VECT_OP_TYPE f_nn = f_n;
    unsigned     i_n  = floor(f_n);

    k = 0;
    i = 0;
  
    // for each set of ceiling(dn/sn) dst values
    while(1)
    {
      // repeat floor(dn/sn) src val into dst
      for(j=0; j<i_n; ++j,++i)
        dV[i] = sV[k];

      if( k + 1 == sn )
        break;

      // interpolate between the cur and nxt source value
      VECT_OP_TYPE w = f_nn - floor(f_nn);
      dV[i] = sV[k] + w * (sV[k+1]-sV[k]);
      ++i;
      ++k;

      i_n   = floor(f_n - (1.0-w));
      f_nn += f_n;
    }
  } 
  else // if shrinking
  {
    VECT_OP_TYPE f_n  = (VECT_OP_TYPE)sn/dn;
    VECT_OP_TYPE f_nn = f_n;
    unsigned     i_n  = floor(f_n);

    k = 0;
    i = 0;

    VECT_OP_TYPE acc = 0;
  
    // for each seq of ceil(sn/dn) src values
    while(1)
    {
      // accum first floor(sn/dn) src values
      for(j=0; j<i_n; ++j,++i)
        acc += sV[i];

      if( k == dn-1 )
      {
        dV[k] = acc/f_n;
        break;
      }

      // interpolate frac of last src value
      VECT_OP_TYPE w = f_nn - floor(f_nn);

      // form avg
      dV[k] = (acc + (w*sV[i]))/f_n;


      // reload acc with inverse frac of src value
      acc = (1.0-w) * sV[i];

      ++i;
      ++k;

      i_n   = floor(f_n-(1.0-w));
      f_nn += f_n;

    }
  }

  return dV;
}


VECT_OP_TYPE* VECT_OP_FUNC(Random)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE minVal, VECT_OP_TYPE maxVal )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp =dbp;
  double fact = (maxVal - minVal)/RAND_MAX;

  while( dbp < dep )
    *dbp++ = fact *  rand() + minVal;

  return dp;
}

unsigned*      VECT_OP_FUNC(WeightedRandInt)( unsigned *dbp, unsigned dn, const VECT_OP_TYPE* wp, unsigned wn )
{
  unsigned i,j;
  VECT_OP_TYPE a[ wn ];

  // form  bin boundaries by taking a cum. sum of the weight values.
  VECT_OP_FUNC(CumSum)(a,wn,wp);

  for(j=0; j<dn; ++j)
  {
    // gen a random number from a uniform distribution betwen 0 and the max value from the cumsum.
    VECT_OP_TYPE rv = (VECT_OP_TYPE)rand() * a[wn-1] / RAND_MAX;

    // find the bin the rv falls into 
    for(i=0; i<wn-1; ++i)
      if( rv <= a[i] )
      {
        dbp[j] = i;
        break;
      }

    if(i==wn-1)
      dbp[j]= wn-1;  
  }

  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(RandomGauss)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE mean, VECT_OP_TYPE var )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* rp = dbp;

  // The code below implements the Box-Muller uniform to 
  // Gaussian distribution transformation.  In rectangular 
  // coordinates this transform is defined as:
  //  y1 = sqrt( - 2.0 * log(x1) ) * cos( 2.0*M_PI*x2 )
  //  y2 = sqrt( - 2.0 * log(x1) ) * sin( 2.0*M_PI*x2 )
  //

  while( dbp < dep )
    *dbp++ = sqrt( -2.0 * log((VECT_OP_TYPE)rand()/RAND_MAX)) * cos(2.0*M_PI*((VECT_OP_TYPE)rand()/RAND_MAX)) * var + mean;

  return rp;
}

VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* varV )
{
  VECT_OP_TYPE* rp = dbp;
  const VECT_OP_TYPE* dep = dbp + dn;
  while( dbp < dep )
    VECT_OP_FUNC(RandomGauss)( dbp++, 1, *meanV++, *varV++ );
  return rp;
}

VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussM)( VECT_OP_TYPE* dbp, unsigned rn, unsigned cn, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* varV )
{
  unsigned i;
  for(i=0; i<cn; ++i)
    VECT_OP_FUNC(RandomGaussV)( dbp+(i*rn), rn, meanV, varV );
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussDiagM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* covarM )
{
  unsigned i,j;
  for(i=0; i<dcn; ++i)
    for(j=0; j<drn; ++j)
      VECT_OP_FUNC(RandomGauss)(dbp + (i*drn)+j, 1, meanV[j], covarM[ (j*drn) + j]);
  return dbp;  
}

VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussNonDiagM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* covarM, VECT_OP_TYPE* t )
{

  bool     fl = t == NULL;
  if( fl  )
    t = cmMemAlloc(VECT_OP_TYPE,  drn * drn );

  VECT_OP_FUNC(Copy)(t,drn*drn,covarM);

  if( VECT_OP_FUNC(CholZ)(t,drn) == NULL )
  {
    // Cholesky decomposition failed - should try eigen analysis next
    // From octave mvnrnd.m
    // [E,Lambda]=eig(Sigma);
	  // if (min(diag(Lambda))<0),error('Sigma must be positive semi-definite.'),end
	  // U = sqrt(Lambda)*E';

    assert(0);
  }
  /*
  unsigned i,j;
  for(i=0; i<drn; ++i)
  {
    for(j=0; j<drn; ++j)
      printf("%f ",t[ (j*drn) + i]);
    printf("\n");
  }     
  */

  VECT_OP_FUNC(RandomGaussNonDiagM2)(dbp,drn,dcn,meanV,t);

  if(fl)
    cmMemFree(t);

  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussNonDiagM2)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* uM )
{  
  unsigned i;

  for(i=0; i<dcn; ++i)
  {
    VECT_OP_TYPE r[ drn ];
    VECT_OP_FUNC(RandomGauss)(r,drn,0,1);             // r = randn(drn,1);
    VECT_OP_FUNC(MultVVM)( dbp+(i*drn),drn,r,drn,uM); // dbp[:i] = r * uM;
    VECT_OP_FUNC(AddVV)(   dbp+(i*drn),drn,meanV);    // dbp[:,i] += meanV;
  }

  return dbp;
}


VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussMM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* meanM, const VECT_OP_TYPE* varM, unsigned K )
{
  unsigned k;
  unsigned D = drn;
  unsigned N = dcn/K;
  for(k=0; k<K; ++k)
    VECT_OP_FUNC(RandomGaussM)( dbp + (k*N*D), drn, N, meanM + (k*D), varM + (k*D) );
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(CircleCoords)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE x, VECT_OP_TYPE y, VECT_OP_TYPE varX, VECT_OP_TYPE varY  )
{
  unsigned i;
  for(i=0; i<dn; ++i)
  {
    double a = 2.0*M_PI*i/(dn-1);

    dbp[ i ]    = (VECT_OP_TYPE)(varX * cos(a) + x);
    dbp[ i+dn ] = (VECT_OP_TYPE)(varY * sin(a) + y);
  }

  return dbp;
}


unsigned      VECT_OP_FUNC(SynthSine)(      VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz )
{
    const VECT_OP_TYPE* dep = dbp + dn;
  double rps = 2.0*M_PI*hz/srate;

  while( dbp < dep )
    *dbp++ = (VECT_OP_TYPE)sin( rps * phase++ );

  return phase;
}

unsigned      VECT_OP_FUNC(SynthCosine)(    VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  double rps = 2.0*M_PI*hz/srate;

  while( dbp < dep )
    *dbp++ = (VECT_OP_TYPE)cos( rps * phase++ );

  return phase;
}

unsigned      VECT_OP_FUNC(SynthSquare)(    VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt )
{
  const VECT_OP_TYPE* dep = dbp + dn;

  if( otCnt > 0 )
  {
    unsigned i;

    // initialize the buffer with the fundamental
    VECT_OP_FUNC(SynthSine)( dbp, dn, phase, srate, hz );

    otCnt *= 2;

    // sum in each additional harmonic
    for(i=3; i<otCnt; i+=2)
    {

      VECT_OP_TYPE* dp  = dbp;      
      double        rps = 2.0 * M_PI * i  * hz / srate;
      unsigned      phs = phase;
      double        g   = 1.0/i;

      while( dp < dep )
        *dp++ += (VECT_OP_TYPE)(g * sin( rps * phs++ ));

    }
  }
  return phase + (dep - dbp);
}

unsigned      VECT_OP_FUNC(SynthTriangle)(  VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  if( otCnt > 0 )
  {
    unsigned i;

    // initialize the buffer with the fundamental
    VECT_OP_FUNC(SynthCosine)( dbp, dn, phase, srate, hz );

    otCnt *= 2;

    // sum in each additional harmonic
    for(i=3; i<otCnt; i+=2)
    {

      VECT_OP_TYPE* dp  = dbp;      
      double        rps = 2.0 * M_PI * i  * hz / srate;
      unsigned      phs = phase;
      double        g   = 1.0/(i*i);
      while( dp < dep )
        *dp++ += (VECT_OP_TYPE)(g * cos( rps * phs++ ));
    }
  }
  return phase + (dep - dbp);
}

unsigned      VECT_OP_FUNC(SynthSawtooth)(  VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  if( otCnt > 0 )
  {
    unsigned i;

    // initialize the buffer with the fundamental
    VECT_OP_FUNC(SynthSine)( dbp, dn, phase, srate, hz );

    // sum in each additional harmonic
    for(i=2; i<otCnt; ++i)
    {

      VECT_OP_TYPE* dp  = dbp;      
      double        rps = 2.0 * M_PI * i  * hz / srate;
      unsigned      phs = phase;
      double        g   = 1.0/i;
      
      while( dp < dep )
        *dp++ += (VECT_OP_TYPE)(g * sin( rps * phs++ ));
    }

    VECT_OP_FUNC(MultVS)(dbp,dn,2.0/M_PI);
  }
  return phase + (dep - dbp);
}

unsigned      VECT_OP_FUNC(SynthPulseCos)(  VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  if( otCnt > 0 )
  {
    unsigned i;


    // initialize the buffer with the fundamental
    VECT_OP_FUNC(SynthCosine)( dbp, dn, phase, srate, hz );

    // sum in each additional harmonic
    for(i=1; i<otCnt; ++i)
    {

      VECT_OP_TYPE* dp  = dbp;      
      double        rps = 2.0 * M_PI * i  * hz / srate;
      unsigned      phs = phase;
      
      while( dp < dep )
        *dp++ += (VECT_OP_TYPE)cos( rps * phs++ );
    }

    VECT_OP_FUNC(MultVS)(dbp,dn,1.0/otCnt);
  }
  return phase + (dep - dbp);
}

unsigned      VECT_OP_FUNC(SynthImpulse)(   VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz )
{
  
  VECT_OP_FUNC(Zero)(dbp,dn);
  unsigned i=0;
  while(1)
  {
    double samplesPerCycle = srate / hz;
    unsigned j = round( (srate * i + phase) / hz);
    if( j >= dn )
      break;

    dbp[j] = 1;
    ++i;
  }

  // Note that returning an integer value here loses precision
  // since j was rounded to the nearest integer.
  return j - dn;
}

VECT_OP_TYPE VECT_OP_FUNC(SynthPinkNoise)( VECT_OP_TYPE* dbp, unsigned n, VECT_OP_TYPE delaySmp )
{
  const VECT_OP_TYPE* dep = dbp + n;
  VECT_OP_TYPE tmp[ n ];
  VECT_OP_FUNC(Random)(tmp,n,-1.0,1.0);
  VECT_OP_TYPE* sp = tmp;
  VECT_OP_TYPE  reg = delaySmp;

  for(; dbp < dep; ++sp)
  {
    *dbp++ = (*sp + reg)/2.0;
    reg = *sp;
  }
  return *sp;
}

VECT_OP_TYPE*  VECT_OP_FUNC(LinSpace)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE base, VECT_OP_TYPE limit )
{
  unsigned i = 0;
  for(; i<dn; ++i)
    dbp[i] = base + i*(limit-base)/(dn-1);
  return dbp;
}


VECT_OP_TYPE* VECT_OP_FUNC(LinearToDb)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp, VECT_OP_TYPE mult )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* rp = dbp;
  while( dbp < dep )
    *dbp++ = (VECT_OP_TYPE)(mult * log10( VECT_OP_EPSILON +  *sp++ ));
  return rp;
}

VECT_OP_TYPE* VECT_OP_FUNC(dBToLinear)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp, VECT_OP_TYPE mult )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* rp = dbp;
  while( dbp < dep )
    *dbp++ =  (VECT_OP_TYPE)pow(10.0, *sp++ / mult );
  return rp;
}

VECT_OP_TYPE* VECT_OP_FUNC(AmplitudeToDb)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp )
{ return VECT_OP_FUNC(LinearToDb)(dbp,dn,sp,20.0); }

VECT_OP_TYPE* VECT_OP_FUNC(PowerToDb)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp )
{ return VECT_OP_FUNC(LinearToDb)(dbp,dn,sp,10.0); }

VECT_OP_TYPE* VECT_OP_FUNC(dBToAmplitude)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp )
{ return VECT_OP_FUNC(dBToLinear)( dbp,dn,sp,20); }

VECT_OP_TYPE* VECT_OP_FUNC(dBToPower)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp )
{ return VECT_OP_FUNC(dBToLinear)( dbp,dn,sp,10); }


unsigned      VECT_OP_FUNC(SynthPhasor)(VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  while( dbp < dep )
    *dbp++ = (VECT_OP_TYPE)fmod( (hz *  phase++)/srate, 1.0 );

  return phase;
}
	

VECT_OP_TYPE VECT_OP_FUNC(KaiserBetaFromSidelobeReject)(  double sidelobeRejectDb )
	{ 
		double beta;
		
		if( sidelobeRejectDb < 13.26 )
			sidelobeRejectDb = 13.26;
		else
		if( sidelobeRejectDb > 120.0)
			sidelobeRejectDb = 120.0; 

		if( sidelobeRejectDb < 60.0 )
			beta = (0.76609 * pow(sidelobeRejectDb - 13.26,0.4))  + (0.09834*(sidelobeRejectDb-13.26));
		else
			beta = 0.12438 * (sidelobeRejectDb + 6.3);
			
		return (VECT_OP_TYPE)beta;
	}
	

VECT_OP_TYPE VECT_OP_FUNC(KaiserFreqResolutionFactor)( double sidelobeRejectDb )
{	return (6.0 * (sidelobeRejectDb + 12.0))/155.0;	}


VECT_OP_TYPE* VECT_OP_FUNC(Kaiser)( VECT_OP_TYPE* dbp, unsigned n, double beta )
{
  bool	   zeroFl 	= false;
  int		   M		    = 0;
  double	 den		  = cmBessel0(beta);	// wnd func denominator
  int		   cnt		  = n;
  int      i;

  assert( n >= 3 );

  // force ele cnt to be odd 
  if( cmIsEvenU(cnt)  )
  {
    cnt--;
    zeroFl = true;
  }

  // at this point cnt is odd and >= 3
		
  // calc half the window length
  M = (int)((cnt - 1.0)/2.0);
		
  double Msqrd = M*M;
		
  for(i=0; i<cnt; i++)
  {		
    double v0 = (double)(i - M);

    double num = cmBessel0(beta * sqrt(1.0 - ((v0*v0)/Msqrd)));
			
    dbp[i] = (VECT_OP_TYPE)(num/den);
  }								 


  if( zeroFl )
    dbp[cnt] = 0.0;  // zero the extra element in the output array

  return dbp;
}
	
VECT_OP_TYPE*	VECT_OP_FUNC(Gaussian)( VECT_OP_TYPE* dbp, unsigned dn, double mean, double variance )
{
  
  int      M		    =  dn-1;
  double   sqrt2pi	= sqrt(2.0*M_PI);
  unsigned i;

  for(i=0; i<dn; i++)
  {
    double arg = ((((double)i/M) - 0.5) * M);

    arg = pow( (double)(arg-mean), 2.0);

    arg = exp( -arg / (2.0*variance));

    dbp[i] = (VECT_OP_TYPE)(arg / (sqrt(variance) * sqrt2pi));
  }
		
  return dbp;
}


VECT_OP_TYPE* VECT_OP_FUNC(Hamming)( VECT_OP_TYPE* dbp, unsigned dn )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp   = dbp;
  double        fact = 2.0 * M_PI / (dn-1);
  unsigned      i;

  for(i=0;  dbp < dep; ++i )
    *dbp++  = (VECT_OP_TYPE)(.54 - (.46 * cos(fact*i)));

  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(Hann)( VECT_OP_TYPE* dbp, unsigned dn )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp   = dbp;
  double        fact = 2.0 * M_PI / (dn-1);
  unsigned      i;

  for(i=0;  dbp < dep; ++i )
    *dbp++  = (VECT_OP_TYPE)(.5 - (.5 * cos(fact*i)));

  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(HannMatlab)( VECT_OP_TYPE* dbp, unsigned dn )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp   = dbp;
  double        fact = 2.0 * M_PI / (dn+1);
  unsigned      i;

  for(i=0;  dbp < dep; ++i )
    *dbp++  = (VECT_OP_TYPE)(0.5*(1.0-cos(fact*(i+1))));

  return dp;
}



VECT_OP_TYPE* VECT_OP_FUNC(Triangle)( VECT_OP_TYPE* dbp, unsigned dn )
{
  unsigned     n    = dn/2;
  VECT_OP_TYPE incr = 1.0/n;

  VECT_OP_FUNC(Seq)(dbp,n,0,incr);

  VECT_OP_FUNC(Seq)(dbp+n,dn-n,1,-incr);

  return dbp;
}

VECT_OP_TYPE*	VECT_OP_FUNC(GaussWin)( VECT_OP_TYPE* dbp, unsigned dn, double arg )	
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* rp = dbp;
  int           N  = (dep - dbp) - 1;
  int           n  = -N/2;
  
  if( N == 0 )
    *dbp = 1.0;
  else
  {
    while( dbp < dep )
    {
      double a = (arg * n++) / (N/2);

      *dbp++ = (VECT_OP_TYPE)exp( -(a*a)/2 );
    }
  }
  return rp;
}


VECT_OP_TYPE* VECT_OP_FUNC(Filter)( 
  VECT_OP_TYPE*       y, 
  unsigned            yn, 
  const VECT_OP_TYPE* x, 
  unsigned            xn, 
  cmReal_t            b0, 
  const cmReal_t*     b, 
  const cmReal_t*     a,  
  cmReal_t*           d, 
  unsigned            dn )
{
  int           i,j;
  VECT_OP_TYPE  y0 = 0;
  unsigned      n = cmMin( yn, xn );

  // This is a direct form II algorithm based on the MATLAB implmentation
  // http://www.mathworks.com/access/helpdesk/help/techdoc/ref/filter.html#f83-1015962

  for(i=0; i<n; ++i)
  {
    y[i] = (x[i] * b0) + d[0];

    y0 = y[i];

    for(j=0; j<dn; ++j)
      d[j] = (b[j] * x[i]) - (a[j] * y0) + d[j+1]; 
   
  }


  // if fewer input samples than output samples - zero the end of the output buffer
  if( yn > xn )
    VECT_OP_FUNC(Fill)(y+i,yn-i,0);

  return y;
  
}


VECT_OP_TYPE*    VECT_OP_FUNC(FilterFilter)(struct cmFilter_str* f, cmRC_t (*func)( struct cmFilter_str* f,  const VECT_OP_TYPE* x, unsigned xn, VECT_OP_TYPE* y, unsigned yn ), const cmReal_t bb[], unsigned bn, const cmReal_t aa[], unsigned an, const VECT_OP_TYPE* x, unsigned xn, VECT_OP_TYPE* y, unsigned yn )
{
  int             i,j;
  int             nfilt = cmMax(bn,an);
  int             nfact = 3*(nfilt-1);
  const cmReal_t* a     = aa;
  const cmReal_t* b     = bb;
  cmReal_t*       m     = NULL;
  cmReal_t*       p;
  unsigned        zn    = (nfilt-1)*(nfilt-1);
  unsigned        mn    = 2*zn; // space  for mtx z0 and z1

  mn += nfilt;   // space for zero padded coeff vector

  mn += 2*nfact; // space for begin/end sequences

  if( nfact >= xn )
  {
    return cmOkRC;
  }

  m = cmMemAllocZ( cmReal_t, mn );
  p = m;

  cmReal_t*   z0 = p;
  p  += zn;

  cmReal_t*   z1 = p;
  p  += zn;

  cmReal_t* s0 = p;  
  p  += nfact;

  cmReal_t* s1 = p;
  p  += nfact;

  // zero pad the shorter coeff vect
  if( bn < nfilt )
  {
    cmVOR_Copy(p,bn,bb);
    b = p;
    p += nfilt;
  }
  else
  if( an < nfilt )
  {
    cmVOR_Copy(p,an,aa);
    a = p;
    p += nfilt;
  }


  // z0=eye(nfilt-1)
  cmVOR_Identity(z0,nfilt-1,nfilt-1);

  // z1=[eye(nfilt-1,nfilt-2); zeros(1,nfilt-1)];
  cmVOR_Identity(z1,nfilt-1,nfilt-2);

  // z0(:,1) -= a(:)
  for(i=0; i<nfilt-1; ++i)
    z0[i] -= -a[i+1];

  // z0(:,2:end) -= z1; 
  for(i=1; i<nfilt-1; ++i)
    for(j=0; j<nfilt-1; ++j)
      z0[ (i*(nfilt-1)) + j ] -= z1[ ((i-1)*(nfilt-1)) + j ];

  // z1 = b - (a * b[0])
  for(i=1; i<nfilt; ++i)
    z1[i-1] = b[i] - (a[i] * b[0]);

  // z1 = z0\z1
  cmVOR_SolveLS(z0,nfilt-1,z1,1);

  // if yn<xn then truncate x.
  xn = cmMin(xn,yn);
  yn = xn;

  // fill in the beginning sequence
  for(i=0; i<nfact; ++i)
    s0[i] = 2*x[0] - x[ nfact-i ];

  // fill in the ending sequence
  for(i=0; i<nfact; ++i)
    s1[i] = 2*x[xn-1] - x[ xn-2-i ];


  cmVOR_MultVVS( z0, nfact, z1, s0[0]);

  unsigned  pn = cmMin(1024,xn);
  //acFilter* f  = cmFilterAlloc(c,NULL,b,bn,a,an,pn,z0);

  cmFilterInit(f,b,bn,a,an,pn,z0);

  const VECT_OP_TYPE* xx = x;

  for(j=0; j<2; ++j)
  {
    unsigned n = pn;

    // filter begining sequence
    cmFilterExecR(f,s0,nfact,s0,nfact); 

    // filter middle sequence
    for(i=0; i<xn; i+=n)
    {
      n = cmMin(pn,xn-i);
      func(f,xx+i,n,y+i,n);
    }

    // filter ending sequence
    cmFilterExecR(f,s1,nfact,s1,nfact);

   
    // flip all the sequences
    cmVOR_Flip(s0,nfact);
    cmVOR_Flip(s1,nfact);
    VECT_OP_FUNC(Flip)(y,yn);    

    if( j==0)
    {

      // swap the begin and end sequences
      cmReal_t* t = s0;
      s0 = s1;
      s1 = t;

      xx = y;

      cmVOR_MultVVS( z0, nfact, z1, s0[0]);

      cmFilterInit(f,b,bn,a,an,pn,z0);

    }
    
  }

  //cmFilterFree(&f);
  cmMemPtrFree(&m);

  return y;
}



VECT_OP_TYPE* VECT_OP_FUNC(LP_Sinc)(VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* wndV, double srate, double fcHz, unsigned flags )
{
  VECT_OP_TYPE* rp = dp;

  int    dM       = dn % 2;                  // dM is used to handle odd length windows
  int    M        = (dn - dM)/2;
  int    Mi       = -M;
  double phsFact  = 2.0 * M_PI * fcHz / srate;
  double sum      = 0;
  VECT_OP_TYPE noWndV[ dn ];

  // if no window was given then create a unity window
  if( wndV == NULL )
  {
    VECT_OP_FUNC(Fill)(noWndV,dn,1);
    wndV = noWndV;
  }

  M += dM; 

  //printf("M=%i Mi=%i sign:%f phs:%f\n",M,Mi,signFact,phsFact);

  for(; Mi<M; ++Mi,++dp,++wndV)
  {
    double phs = phsFact * Mi;
    if( Mi != 0 )
      *dp = *wndV * 0.5 * sin(phs)/phs;
    else
      *dp = *wndV * 0.5;
    
    sum += *dp;
  }

  // normalize the filter to produce unity gain.
  if( cmIsFlag(flags,kNormalize_LPSincFl) )
    VECT_OP_FUNC(DivVS)(rp,dn,fabs(sum));

  // Convert low-pass filter to high-pass filter
  // Note that this can only be done after the filter is normalized.
  if( cmIsFlag(flags,kHighPass_LPSincFl) )
  {
    VECT_OP_FUNC(MultVS)(rp,dn,-1);
    rp[M-1] = 1.0 + rp[M-1];
  }
  
  return rp;
}

VECT_OP_TYPE  VECT_OP_FUNC(ComplexDetect)(const VECT_OP_TYPE* mag0V, const VECT_OP_TYPE* mag1V, const VECT_OP_TYPE* phs0V, const VECT_OP_TYPE* phs1V, const VECT_OP_TYPE* phs2V, unsigned binCnt )
{
  double              sum  = 0;
  const VECT_OP_TYPE*  ep  = mag0V + binCnt;

  unsigned i = 0;

  for(;  mag0V < ep; ++i )
  {
    // calc phase deviation from expected
    double dev_rads    = *phs0V++ - (2 * *phs1V++) + *phs2V++;   

    // map deviation into range: -pi to pi
    //double dev_rads1    = mod(dev_rads0 + M_PI, -2*M_PI ) + M_PI;            

    while( dev_rads > M_PI)
      dev_rads -= 2*M_PI;

    while( dev_rads < -M_PI)
      dev_rads += 2*M_PI;    

    // convert into rect coord's
    double m1r =  *mag1V++;
    double m0r =  *mag0V   * cos(dev_rads);
    double m0i =  *mag0V++ * sin(dev_rads);

    // calc the combined amplitude and phase deviation 
    // sum += hypot( m1 - (m0 * e^(-1*dev_rads)));

    sum += hypot( m1r-m0r, -m0i );

  }

  return (VECT_OP_TYPE)sum; 
}

VECT_OP_TYPE* VECT_OP_FUNC(MelMask)( VECT_OP_TYPE* maskMtx, unsigned filterCnt, unsigned binCnt, double srate, unsigned flags )
{
  unsigned fi,bi;

  double mxh = srate/2.0;                        // nyquist
  double dh  = mxh/(binCnt-1) ;                  // binHz
  double mxm = 1127.0 * log( 1.0 + mxh/700.0);   // max mel value in Hz
  double dm  = mxm / (filterCnt+1);              // avg mel band hz
  double sum = 0;

  for(fi=0; fi<filterCnt; ++fi)
  {
    double   m     =  (fi+1) * dm;

    // calc min/center/max frequencies for this band
    double   minHz =  700.0 * (exp((m-dm)/1127.01048)-1.0);
    double   ctrHz =  700.0 * (exp( m    /1127.01048)-1.0);
    double   maxHz =  700.0 * (exp((m+dm)/1127.01048)-1.0);


    // shift the band min/ctr/max to the nearest bin ctr frequency
    if( cmIsFlag(flags,kShiftMelFl) )
    {
      unsigned i;

      i = (unsigned)floor(minHz/dh);
      minHz =  minHz - (dh*i) <  dh*(i+1) - minHz ? dh*i : dh*(i+1);

      i = (unsigned)floor(ctrHz/dh);
      ctrHz =  ctrHz - (dh*i) <  dh*(i+1) - ctrHz ? dh*i : dh*(i+1);

      i = (unsigned)floor(maxHz/dh);
      maxHz =  maxHz - (dh*i) <  dh*(i+1) - maxHz ? dh*i : dh*(i+1);

    }

    // calc the height of the triangle - such that all bands have equal area
    double   a     =  2.0/(maxHz - minHz);

    for(bi=0; bi<binCnt; ++bi)
    {
      double   h  = bi*dh;
      unsigned mi = bi*filterCnt + fi;

 
      if( h < minHz || h >  maxHz )
        maskMtx[mi] = 0;
      else
      {
        if( h <= ctrHz )
          maskMtx[mi] = a * (h - minHz)/(ctrHz-minHz);   
        else
          maskMtx[mi] = a * (maxHz - h)/(maxHz-ctrHz);

        sum += maskMtx[mi];
      } 

    }
  }

  if( cmIsFlag(flags,kNormalizeMelFl) )
    VECT_OP_FUNC(DivVS)( maskMtx, (filterCnt*binCnt), sum );
    

  return maskMtx;
}

unsigned VECT_OP_FUNC(BarkMap)(unsigned* binIdxV, unsigned* cntV, unsigned bandCnt, unsigned binCnt, double srate )
{
  if( bandCnt == 0 )
    return 0;

  //zwicker & fastl: psychoacoustics 1999, page 159
  double bandUprHz[] = { 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500 };

  unsigned hn = sizeof(bandUprHz)/sizeof(double);
  
  unsigned i, bi = 0;

  bandCnt    = cmMin(hn,bandCnt);

  binIdxV[0] = 0;
  cntV[0]    = 1;

  for(i=1; bi < bandCnt  && i<binCnt; ++i)
  {
    double hz =  srate * i / (2 * (binCnt-1));

    if( hz <=  bandUprHz[bi] )
      cntV[bi]++;
    else
    {
      //printf("%i %i %i %f\n",bi,binIdxV[bi],cntV[bi],bandUprHz[bi]);

      ++bi;
      if( bi < bandCnt )
      {
        binIdxV[bi] = i;
        cntV[bi]    = 1;
      }
    }    


  }
  
  return bi;
}

VECT_OP_TYPE* VECT_OP_FUNC(TriangleMask)(VECT_OP_TYPE* maskMtx, unsigned bandCnt, unsigned binCnt, const VECT_OP_TYPE* ctrHzV, VECT_OP_TYPE binHz, VECT_OP_TYPE stSpread, const VECT_OP_TYPE* lfV, const VECT_OP_TYPE* hfV )
{
  unsigned     i,j;
  VECT_OP_TYPE v0[ bandCnt ];
  VECT_OP_TYPE v1[ bandCnt ];

  // if no lower/upper band limits were give use a fixed semitone band width
  if( lfV==NULL || hfV==NULL)
  {

    for(i=0; i<bandCnt; ++i)
    {
      v0[i] = ctrHzV[i] * pow(2.0,-stSpread/12.0);
      v1[i] = ctrHzV[i] * pow(2.0, stSpread/12.0);
    }    

    lfV = v0;
    hfV = v1;

  }

  VECT_OP_FUNC(Zero)(maskMtx,bandCnt*binCnt);

  // for each band 
  for(i=0; i<bandCnt; ++i)
  {
    // calc bin index of first possible bin in this band
    // j = (unsigned)floor(lfV[i] / binHz); 
    
    double binHz_j = 0;

    // for each bin whose ctr frq is  <= the band upper limit
    for(j=0; j<binCnt; ++j)
    {
      double v;

      // if bin[j] is inside the lower leg of the triangle
      if( lfV[i] <= binHz_j && binHz_j <= ctrHzV[i] )
        v =  (binHz_j - lfV[i]) / cmMax(VECT_OP_MIN, ctrHzV[i] - lfV[i] );
      else

      // if bin[j] is inside the upper leg of the triangle
      if( ctrHzV[i] < binHz_j && binHz_j <= hfV[i] )
        v =  (hfV[i] - binHz_j) / cmMax(VECT_OP_MIN, hfV[i] - ctrHzV[i] );
      else
        v = 0;

      maskMtx[ (j*bandCnt)+i ]  = v;

      binHz_j = binHz * (j+1);
      
    }
  }

  return maskMtx;
}

VECT_OP_TYPE* VECT_OP_FUNC(BarkMask)(VECT_OP_TYPE* maskMtx, unsigned bandCnt, unsigned binCnt, double binHz )
{
  //                -1   0  1   2   3   4   5   6   7    8   9    10   11   12   13   14   15   16   17   18   19   20   21   22    23    (23+1)
  VECT_OP_TYPE b[]= {0, 50,150,250,350,450,570,700,840,1000,1170,1370,1600,1850,2150,2500,2900,3400,4000,4800,5800,7000,8500,10500,13500, 15500 };

  bandCnt = cmMin(bandCnt,kDefaultBarkBandCnt);

  VECT_OP_FUNC(TriangleMask)(maskMtx, bandCnt, binCnt, b+1, binHz, 0, b+0, b+2 );
  
  return maskMtx;
}

VECT_OP_TYPE* VECT_OP_FUNC(TerhardtThresholdMask)(VECT_OP_TYPE* maskV, unsigned binCnt, double srate, unsigned flags )
{
  unsigned i;

  double c0 = cmIsFlag(flags,kModifiedTtmFl) ? 0.6 : 1.0;
  double c1 = cmIsFlag(flags,kModifiedTtmFl) ? 0.5 : 6.5;

  maskV[0]=0;

  for(i=0; i<binCnt; ++i)
  {
    double hz =  srate * i / (2 * (binCnt-1));
    maskV[i] = pow(pow(10,(c0 * -3.64* pow(hz/1000,-0.8)  + c1 * exp(-0.6 * pow(hz/1000 - 3.3,2)) - 0.001* pow(hz/1000,4))/20),2);
  }

  return maskV;
}


VECT_OP_TYPE* VECT_OP_FUNC(ShroederSpreadingFunc)(VECT_OP_TYPE* m, unsigned bandCnt, double srate)
{
  int fi,bi;

  for(fi=0; fi<bandCnt; ++fi)
    for(bi=0; bi<bandCnt; ++bi )
      m[ fi + (bi*bandCnt) ] = pow(10,(15.81 + 7.5 * ((fi-bi)+0.474)-17.5*pow(1+pow((fi-bi)+0.474,2),0.5))/10);

  return m;
}


VECT_OP_TYPE* VECT_OP_FUNC(DctMatrix)( VECT_OP_TYPE* dp, unsigned coeffCnt, unsigned filtCnt )
{
  VECT_OP_TYPE* dbp = dp;

  double c0 = 1.0/sqrt(filtCnt/2); // row 1-coeffCnt factor
  double c1 = c0 * sqrt(2)/2;      // row 0 factor

  unsigned i,j;

  // for each column
  for(i=0; i<filtCnt; ++i)
    // for each row
    for(j=0; j<coeffCnt; ++j)
      *dp++ = (j==0 ? c1 : c0) * cos( (0.5 + i) * M_PI * j / filtCnt);

  return dbp;
}

unsigned VECT_OP_FUNC(PeakIndexes)( unsigned* dbp, unsigned dn, const VECT_OP_TYPE* sbp, unsigned sn, VECT_OP_TYPE threshold )
{
  unsigned pkCnt = 0;
  const unsigned*     dep = dbp + dn;
  const VECT_OP_TYPE* sep = sbp + sn;
  const VECT_OP_TYPE* s2p = sbp;
  const VECT_OP_TYPE* s0p = s2p++;
  const VECT_OP_TYPE* s1p = s2p++;


  while( dbp < dep && s2p < sep )
  {
    if( (*s0p < *s1p) && (*s1p > *s2p) && (*s1p >= threshold) )
    {
      *dbp++ = s1p - sbp;
      s0p    = s2p++;
      s1p    = s2p++;
      ++pkCnt;
    }
    else
    {
      s0p = s1p;
      s1p = s2p++;     
    }
  }
  
  return pkCnt;
}

unsigned VECT_OP_FUNC(BinIndex)( const VECT_OP_TYPE* sbp, unsigned sn, VECT_OP_TYPE v )
{
  const VECT_OP_TYPE* sep = sbp + sn;
  const VECT_OP_TYPE* bp = sbp;
  sep--;
  for(; sbp < sep; ++sbp  )
    if( *sbp <= v && v < *(sbp+1) )
      return sbp - bp;
      
  return cmInvalidIdx;
}



unsigned VECT_OP_FUNC(Kmeans)( 
  unsigned*           classIdxV,         // classIdxV[scn] - data point class assignments
  VECT_OP_TYPE*       centroidM,         // centroidM[srn,K] - cluster centroids
  unsigned            K,                 // count of clusters
  const VECT_OP_TYPE* sM,                // sM[srn,scn] source data matrix
  unsigned            srn,               // dimensionality of each data point
  unsigned            scn,               // count of data points
  const unsigned*     selIdxV,           // data subset selection id vector (optional)
  unsigned            selKey,            // data subset selection key       (optional)
  bool                initFromCentroidFl,// true if the starting centroids are in centroidM[]
  VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* s0V, const VECT_OP_TYPE* s1V, unsigned sn ),
  void*               userDistPtr
) 
{
  unsigned D       = srn;   // data dimensionality
  unsigned N       = scn;   // count of data points to cluster
  unsigned iterCnt = 0;
  unsigned ki;
  unsigned i       = 0;
  unsigned selN    = N;

  // if a data point selection vector was given
  if( selIdxV != NULL )
  {
    selN = 0;

    for(i=0; i<N; ++i)
    {
      selN        += selIdxV[i]==selKey;
      classIdxV[i] = K;
    }
  }


  assert(K<=selN);

  // if the numer of datapoints and the number of clusters is the same
  // make the datapoints the centroids and return
  if( K == selN )
  {
    ki = 0;
    for(i=0; i<N; ++i)
      if( selIdxV==NULL || selIdxV[i]==selKey )
      {
        VECT_OP_FUNC(Copy)(centroidM+(ki*D),D,sM+(i*D));
        classIdxV[ki] = ki;
        ++ki;
      }
    return 0;
  }

    
  // if centroidM[] has not been initialized with the starting centroid vectors.
  if( initFromCentroidFl == false )
  {
    unsigned* kiV     = cmMemAlloc( unsigned, N );

    // select K unique datapoints at random as the initial centroids
    cmVOU_RandomSeq(kiV,N);
  
    for(i=0,ki=0; i<N && ki<K; ++i)
    {
      if( selIdxV==NULL || selIdxV[ kiV[i] ]==selKey )
      {
        VECT_OP_FUNC(Copy)( centroidM + (ki*D), D, sM + (kiV[i]*D) );
        ++ki;
      }  
    }
  
    cmMemPtrFree(&kiV);
  }
    
  unsigned* nV = cmMemAllocZ( unsigned,K);

  while(1)
  {
    unsigned changeCnt = 0;

    cmVOU_Zero(nV,K);

    // for each data point - assign data point to a cluster
    for(i=0; i<N; ++i)
      if( selIdxV==NULL || selIdxV[i] == selKey )
      {
        // set ki with the index of the centroid closest to sM[:,i]
        VECT_OP_FUNC(DistVMM)( NULL, NULL, &ki, D, sM + (i*srn), 1, centroidM, K, distFunc, userDistPtr );

        assert(ki<K);

        nV[ki]++;

        changeCnt += ( ki != classIdxV[i] );
        classIdxV[i] = ki;        
      }


    // if no data points change classes then the centroids have converged
    if( changeCnt == 0 )
      break;

    ++iterCnt;

    // zero the centroid matrix
    VECT_OP_FUNC(Fill)(centroidM, D*K, 0 );

    // update the centroids
    for(ki=0; ki<K; ++ki)
    {
      unsigned n = 0;

      // sum the all datapoints belonging to class ki
      for(i=0; i<N; ++i)
        if( classIdxV[i] == ki )
        {
          VECT_OP_FUNC(AddVV)(centroidM + (ki*D), D, sM + (i*srn) );
          ++n;
        }

      // convert the sum to a mean to form the centroid
      if( n > 0 )
        VECT_OP_FUNC(DivVS)(centroidM + (ki*D), D, n );      


    } 
  } 

  cmVOU_PrintL("class cnt:",NULL,1,K,nV);
  cmMemPtrFree(&nV);
  return iterCnt;
}

unsigned VECT_OP_FUNC(Kmeans2)( 
  unsigned*           classIdxV,         // classIdxV[scn] - data point class assignments
  VECT_OP_TYPE*       centroidM,         // centroidM[srn,K] - cluster centroids
  unsigned            K,                 // count of clusters
  const VECT_OP_TYPE* (*srcFunc)(void* userPtr, unsigned frmIdx ),
  unsigned            srn,               // dimensionality of each data point
  unsigned            scn,               // count of data points
  void*               userSrcPtr,        // callback data for srcFunc
  VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* s0V, const VECT_OP_TYPE* s1V, unsigned sn ),
  void*               distUserPtr,
  int                 maxIterCnt,
  int                 deltaStopCnt
) 
{
  unsigned D       = srn;   // data dimensionality
  unsigned N       = scn;   // count of data points to cluster
  unsigned iterCnt = 0;
  unsigned ki;
  unsigned i       = 0;
  const VECT_OP_TYPE* sp;

  assert(K<N);

  deltaStopCnt = cmMax(0,deltaStopCnt);
       
  // nV[K] - class assignment vector
  unsigned* nV  = cmMemAllocZ( unsigned,2*K);  

  // roV[K] - read-only flag centroid 
  // centroids flagged as read-only will not be updated by the clustering routine
  unsigned* roV = nV + K;                      

  // copy the read-only flags into roV[K]
  for(i=0; i<K; ++i)
    roV[i] = classIdxV[i];

  while(1)
  {
    unsigned changeCnt = 0;

    cmVOU_Zero(nV,K);

    // for each data point - assign data point to a cluster
    for(i=0; i<N; ++i)
      if((sp = srcFunc(userSrcPtr,i)) != NULL)
      {
        // set ki with the index of the centroid closest to sM[:,i]
        VECT_OP_FUNC(DistVMM)( NULL, NULL, &ki, D, sp, 1, centroidM, K, distFunc, distUserPtr );

        assert(ki<K);

        // track the number of data points assigned to each centroid
        nV[ki]++;  

        // track the number of data points  which change classes
        changeCnt += ( ki != classIdxV[i] );

        // update the class that this data point belongs to
        classIdxV[i] = ki;        
      }


    // if the count of data points which changed classes is less than deltaStopCnt 
    // then the centroids have converged
    if( changeCnt <= deltaStopCnt )
      break;

    if( maxIterCnt!=-1 && iterCnt>=maxIterCnt )
      break;

    // track the number of interations required to converge
    ++iterCnt;

    fprintf(stderr,"%i:%i (", iterCnt,changeCnt );
    for(i=0; i<K; ++i)
      fprintf(stderr,"%i ",nV[i]);
    fprintf(stderr,") ");
    fflush(stderr);

    // update the centroids
    for(ki=0; ki<K; ++ki)
      if( roV[ki]==0 )
      {
        unsigned n = 0;

        VECT_OP_FUNC(Zero)(centroidM + (ki*D), D );

        // sum the all datapoints belonging to class ki
        for(i=0; i<N; ++i)
          if( classIdxV[i] == ki && ((sp=srcFunc(userSrcPtr,i))!=NULL))
          {
            VECT_OP_FUNC(AddVV)(centroidM + (ki*D), D, sp );
            ++n;
          }

        // convert the sum to a mean to form the centroid
        if( n > 0 )
          VECT_OP_FUNC(DivVS)(centroidM + (ki*D), D, n );      

      } 
  } 

  cmMemPtrFree(&nV);
  return iterCnt;
}


VECT_OP_TYPE* VECT_OP_FUNC(GaussPDF)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE mean, VECT_OP_TYPE stdDev )
{
  VECT_OP_TYPE*       rp    = dbp;
  const VECT_OP_TYPE* dep   = dbp + dn;
  VECT_OP_TYPE        var   = stdDev * stdDev;
  VECT_OP_TYPE        fact0 = 1.0/sqrt(2*M_PI*var);
  VECT_OP_TYPE        fact1 = 2.0 * var;

  for(; dbp < dep; ++sbp )
    *dbp++ = fact0 * exp( -((*sbp-mean)*(*sbp-mean))/ fact1 );

  return rp;
}

/// Evaluate a multivariate normal distribution defined by meanV[D] and covarM[D,D]
/// at the data points held in the columns of xM[D,N]. Return the evaluation
/// results in the vector yV[N]. 
bool VECT_OP_FUNC(MultVarGaussPDF)( VECT_OP_TYPE* yV, const VECT_OP_TYPE* xM, const VECT_OP_TYPE* meanV,  const VECT_OP_TYPE* covarM, unsigned D, unsigned N, bool diagFl )
{
  VECT_OP_TYPE det0;

  // calc the determinant of the covariance matrix
  if( diagFl )
    // kpl 1/16/11 det0  = VECT_OP_FUNC(LogDetDiagM)(covarM,D);
    det0  = VECT_OP_FUNC(DetDiagM)(covarM,D);
  else
    // kpl 1/16/11 det0 = VECT_OP_FUNC(LogDetM)(covarM,D);
    det0 = VECT_OP_FUNC(DetM)(covarM,D);

  assert(det0 != 0 );

  if( det0 == 0 )
    return false;

  // calc the inverse of the covariance matrix
  VECT_OP_TYPE icM[D*D];
  VECT_OP_FUNC(Copy)(icM,D*D,covarM);
  
  VECT_OP_TYPE* r;
  if( diagFl )
    r = VECT_OP_FUNC(InvDiagM)(icM,D);
  else
    r = VECT_OP_FUNC(InvM)(icM,D);

  if( r == NULL )
      return false;
  
  VECT_OP_FUNC(MultVarGaussPDF2)( yV, xM, meanV, icM, det0, D, N, diagFl );

  return true;
}

VECT_OP_TYPE* VECT_OP_FUNC(MultVarGaussPDF2)( VECT_OP_TYPE* yV, const VECT_OP_TYPE* xM, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* icM, VECT_OP_TYPE logDet, unsigned D, unsigned N, bool diagFl )
{
  unsigned i;

  double fact = (-(cmReal_t)D/2) * log(2.0*M_PI) - 0.5*logDet;

  for(i=0; i<N; ++i)
  {
    VECT_OP_TYPE dx[D];
    VECT_OP_TYPE t[D];

    // dx[] difference between mean and ith data point
    VECT_OP_FUNC(SubVVV)(dx,D, xM + (i*D), meanV);

    // t[] = dx[] * inv(covarM);
    if( diagFl )
      VECT_OP_FUNC(MultDiagVMV)(t,D,icM,D,dx);
    else
      VECT_OP_FUNC(MultVMV)(t,D,icM,D,dx);

    // dist = sum(dx[] * t[])
    cmReal_t dist = VECT_OP_FUNC(MultSumVV)(t,dx,D);

    yV[i] = exp( fact - (0.5*dist) );

  }

  return yV;
}


VECT_OP_TYPE* VECT_OP_FUNC(MultVarGaussPDF3)( 
  VECT_OP_TYPE*       yV, 
  const VECT_OP_TYPE* (*srcFunc)(void* funcDataPtr, unsigned frmIdx ),
  void*               funcDataPtr,
  const VECT_OP_TYPE* meanV, 
  const VECT_OP_TYPE* icM, 
  VECT_OP_TYPE        logDet, 
  unsigned            D, 
  unsigned            N, 
  bool                diagFl )
{
  unsigned i;

  double fact = (-(cmReal_t)D/2) * log(2.0*M_PI) - 0.5*logDet;

  for(i=0; i<N; ++i)
  {
    VECT_OP_TYPE dx[D];
    VECT_OP_TYPE t[D];

    const VECT_OP_TYPE* xV = srcFunc( funcDataPtr, i );

    if( xV == NULL )
      yV[i] = 0;
    else
    {
      // dx[] difference between mean and ith data point
      VECT_OP_FUNC(SubVVV)(dx, D, xV, meanV);

      // t[] = dx[] * inv(covarM);
      if( diagFl )
        VECT_OP_FUNC(MultDiagVMV)(t,D,icM,D,dx);
      else
        VECT_OP_FUNC(MultVMV)(t,D,icM,D,dx);

      // dist = sum(dx[] * t[])
      cmReal_t dist = VECT_OP_FUNC(MultSumVV)(t,dx,D);

      yV[i] = exp( fact - (0.5*dist) );
    }
  }

  return yV;
}


/// stateV[timeN]
/// a[stateN,stateN], 
/// b[stateN,timeN]
/// phi[stateN].
void VECT_OP_FUNC(DiscreteViterbi)(unsigned* stateV, unsigned tN, unsigned sN, const VECT_OP_TYPE* phi, const VECT_OP_TYPE* a, const VECT_OP_TYPE* b )
{
  unsigned*     psiM   = cmMemAlloc( unsigned,     sN*tN );  // psi[sN,tN]
  VECT_OP_TYPE* dV     = cmMemAlloc( VECT_OP_TYPE, 2*sN );
  VECT_OP_TYPE* d0V    = dV;
  VECT_OP_TYPE* d1V    = dV + sN;

  int     t,i,j;

  // calc the prob of starting in each state given the observations
  VECT_OP_FUNC(MultVVV)( d0V, sN, phi, b );
  VECT_OP_FUNC(NormalizeProbability)( d0V, sN ); // scale to prevent underflow

  // for each time step
  for(t=1; t<tN; ++t)
  {
    // for each possible next state 
    for(j=0; j<sN; ++j)
    {
      VECT_OP_TYPE mv = 0;
      unsigned     mi = 0;

      // The following loop could be replaced with these vector op's:
      // VECT_OP_TYPE tV[ sN ];
      // VECT_OP_TYPE(MultVVV)(tV,sN,d0V,a + (j*sN));
      // mi = VECT_OP_TYPE(MaxIndex)(tV,sN);
      // mv = tV[mi];

      // for each possible prev state 
      for(i=0; i<sN; ++i)
      {
        // calc prob of having ended in state i and transitioning to state j
        VECT_OP_TYPE v =  d0V[i] * a[ i + (j*sN) ];

        // track the most likely transition ending in state j
        if( v > mv )
        {
          mv = v;
          mi = i;
        }
      }  

      // scale the prob of the most likely state by the prob of the obs given that state
      d1V[j] = mv * b[ (t*sN) + j ];

      // store the most likely previous state given that the current state is j
      // (this is the key to understanding the backtracking step below)
      psiM[ (t*sN) + j ] = mi;
    }

    VECT_OP_FUNC(NormalizeProbability)( d1V, sN ); // scale to prevent underflow

    // swap d0V and d1V
    VECT_OP_TYPE* tmp = d0V;
    d0V = d1V;
    d1V = tmp;
  }

  // store the most likely ending state
  stateV[tN-1] = VECT_OP_FUNC(MaxIndex)( d0V, sN, 1 );

  // given the most likely next step select the most likely previous step
  for(t=tN-2; t>=0; --t)
    stateV[t] = psiM[ ((t+1)*sN) + stateV[t+1] ];


  cmMemPtrFree( &psiM   );
  cmMemPtrFree( &dV );
}

bool VECT_OP_FUNC(ClipLine2)( VECT_OP_TYPE x0, VECT_OP_TYPE y0, VECT_OP_TYPE x1, VECT_OP_TYPE y1, VECT_OP_TYPE xMin, VECT_OP_TYPE yMin, VECT_OP_TYPE xMax, VECT_OP_TYPE yMax, VECT_OP_TYPE* t0, VECT_OP_TYPE* t1 )
{

  VECT_OP_TYPE dx = x1 - x0;
  VECT_OP_TYPE dy = y1 - y0;

  VECT_OP_TYPE p=0,q=0,r=0;

  *t0 = 0.0;
  *t1 = 1.0;

  unsigned i;
  for(i=0; i<4; ++i)
  {
    switch(i)
    {
      case 0: p=-dx; q=-(xMin - x0); break; // left
      case 1: p= dx; q= (xMax - x0); break; // right
      case 2: p=-dy; q=-(yMin - y0); break; // bottom 
      case 3: p= dy; q= (yMax - y0); break; // top        
    }

    // if parallel to edge i
    if( p == 0 )
    {
      // if entirely outside of window
      if( q < 0 )
        return false;

      continue;
    }

    r = p/q;

    // if travelling right/up
    if( p < 0 )
    {
      // travelling away from x1,y1 
      if( r > *t1 )
        return false;

      // update distance on line to point of intersection 
      if( r > *t0 )
        *t0 = r;
    }
    else // if travelling left/down
    {
      // travelling away from x1,y1
      if( r < *t0 )
        return false;

      // update distance on line to point of intersection 
      if( r < *t1 )
        *t1 = r;
    }
   
  }


  return true;

}


/// (Uses the Laing-Barsky clipping algorithm)
/// From: http://www.skytopia.com/project/articles/compsci/clipping.html
bool VECT_OP_FUNC(ClipLine)( VECT_OP_TYPE* x0, VECT_OP_TYPE* y0, VECT_OP_TYPE* x1, VECT_OP_TYPE* y1, VECT_OP_TYPE xMin, VECT_OP_TYPE yMin, VECT_OP_TYPE xMax, VECT_OP_TYPE yMax )
{
  VECT_OP_TYPE t0;
  VECT_OP_TYPE t1;

  if( VECT_OP_FUNC(ClipLine2)(*x0,*y0,*x1,*y1,xMin,yMin,xMax,yMax,&t0,&t1) )
  {
    VECT_OP_TYPE dx = *x1 - *x0;
    VECT_OP_TYPE dy = *y1 - *y0;

    *x0 = *x0 + t0*dx;
    *x1 = *x0 + t1*dx;

    *y0 = *y0 + t0*dy;
    *y1 = *y0 + t1*dy;

    return true;
  }

  return false;

}

bool VECT_OP_FUNC(IsLineInRect)( VECT_OP_TYPE x0, VECT_OP_TYPE y0, VECT_OP_TYPE x1, VECT_OP_TYPE y1, VECT_OP_TYPE xMin, VECT_OP_TYPE yMin, VECT_OP_TYPE xMax, VECT_OP_TYPE yMax )
{
  VECT_OP_TYPE t0;
  VECT_OP_TYPE t1;

  return VECT_OP_FUNC(ClipLine2)(x0,y0,x1,y1,xMin,yMin,xMax,yMax,&t0,&t1);

}

VECT_OP_TYPE VECT_OP_FUNC(PtToLineDistance)( VECT_OP_TYPE x0, VECT_OP_TYPE y0, VECT_OP_TYPE x1, VECT_OP_TYPE y1, VECT_OP_TYPE px, VECT_OP_TYPE py)
{
  // from:http://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
  double normalLength = sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));

  if( normalLength <= 0 )
    return 0;

  return (VECT_OP_TYPE)fabs((px - x0) * (y1 - y0) - (py - y0) * (x1 - x0)) / normalLength;
}                              

void VECT_OP_FUNC(Lsq1)(const VECT_OP_TYPE* x, const VECT_OP_TYPE* y, unsigned n, VECT_OP_TYPE* b0, VECT_OP_TYPE* b1 )
{
  VECT_OP_TYPE sx = 0;
  VECT_OP_TYPE sy = 0;
  VECT_OP_TYPE sx_2 = 0;
  VECT_OP_TYPE sxy  = 0;
  unsigned i;  

  if( x == NULL )
  {
    for(i=0; i<n; ++i)
    {
      VECT_OP_TYPE xx = i;
      sx   += xx;
      sx_2 += xx * xx;
      sxy  += xx * y[i];
      sy   += y[i];
    }
  }
  else
  {
    for(i=0; i<n; ++i)
    {
      sx   += x[i];
      sx_2 += x[i] * x[i];
      sxy  += x[i] * y[i];
      sy   += y[i];
    }
  }

  *b1 = (sxy * n - sx * sy) / (sx_2 * n - sx*sx);
  *b0 = (sy - (*b1) * sx) / n;      

}



void VECT_OP_FUNC(Interp1)(VECT_OP_TYPE* y1, const VECT_OP_TYPE* x1, unsigned xy1N, const VECT_OP_TYPE* x0, const VECT_OP_TYPE* y0, unsigned xy0N )
{
  unsigned i,j;

  // for each output value
  for(i=0,j=0; i<xy1N; ++i)
  {
    // x1[] and x0[] are increasing monotonic therefore j should never
    // have to decrease
    for(; j<xy0N-1; ++j)
    {
      // if x1[i] is between x0[j] and x0[j+1]
      if( x0[j] <= x1[i] && x1[i] < x0[j+1] )
      {
        // interpolate y0[j] based on the distance beteen x0[j] and x1[i].
        y1[i] = y0[j] + (y0[j+1]-y0[j]) * ((x1[i] - x0[j]) / (x0[j+1] - x0[j]));
        break;
      }
    }

    if( j == xy0N-1 )
      y1[i] = y0[xy0N-1];
    
  }
}

#endif


