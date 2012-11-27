#ifdef cmVectOpsRICode_h

VECT_OP_TYPE* VECT_OP_FUNC(Col)( VECT_OP_TYPE* m, unsigned ci, unsigned rn, unsigned cn )
{ 
  assert(ci<cn);
  return m + (ci*rn);
}

VECT_OP_TYPE* VECT_OP_FUNC(Row)( VECT_OP_TYPE* m, unsigned ri, unsigned rn, unsigned cn )
{
  assert(ri<rn);
  return m + ri;
}

VECT_OP_TYPE* VECT_OP_FUNC(ElePtr)( VECT_OP_TYPE* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn )
{
  assert(ri<rn && ci<cn);
  return m + (ci*rn) + ri;
}

VECT_OP_TYPE VECT_OP_FUNC(Ele)( VECT_OP_TYPE* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn )
{ return *VECT_OP_FUNC(ElePtr)(m,ri,ci,rn,cn); }

void          VECT_OP_FUNC(Set)( VECT_OP_TYPE* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn, VECT_OP_TYPE v )
{ *(VECT_OP_FUNC(ElePtr)(m,ri,ci,rn,cn)) = v; }

const VECT_OP_TYPE* VECT_OP_FUNC(CCol)( const VECT_OP_TYPE* m, unsigned ci, unsigned rn, unsigned cn )
{
  assert(ci<cn);
  return m + (ci*rn);
}

const VECT_OP_TYPE* VECT_OP_FUNC(CRow)( const VECT_OP_TYPE* m, unsigned ri, unsigned rn, unsigned cn )
{
  assert(ri<rn);
  return m + ri;
}

const VECT_OP_TYPE* VECT_OP_FUNC(CElePtr)( const VECT_OP_TYPE* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn )
{
  assert(ri<rn && ci<cn);
  return m + (ci*rn) + ri;
}

VECT_OP_TYPE VECT_OP_FUNC(CEle)( const VECT_OP_TYPE* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn )
{ return *VECT_OP_FUNC(CElePtr)(m,ri,ci,rn,cn); }




VECT_OP_TYPE* VECT_OP_FUNC(Fill)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE value  )
{
  const VECT_OP_TYPE*  dep = dbp + dn;
  VECT_OP_TYPE*        dp  = dbp;

  if( value == 0 )
    memset(dbp,0,(dep-dbp)*sizeof(VECT_OP_TYPE));
  else
  {
    while( dbp < dep )
      *dbp++ = value;
  }
      return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(Zero)( VECT_OP_TYPE* dbp, unsigned dn  )
{
  memset( dbp, 0, sizeof(VECT_OP_TYPE)*dn);
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(Move)( VECT_OP_TYPE* bp, unsigned bn, const VECT_OP_TYPE* sp )
{
  memmove(bp,sp,sizeof(VECT_OP_TYPE)*bn); 
  return bp;
}

VECT_OP_TYPE* VECT_OP_FUNC(Copy)( VECT_OP_TYPE* bp, unsigned bn, const VECT_OP_TYPE* sp )
{
  memcpy(bp,sp,sizeof(VECT_OP_TYPE)*bn); 
  return bp;
}

VECT_OP_TYPE* VECT_OP_FUNC(CopyN)( VECT_OP_TYPE* bp, unsigned bn, unsigned d_stride, const VECT_OP_TYPE* sp, unsigned s_stride )
{
  VECT_OP_TYPE*       dbp = bp;
  const VECT_OP_TYPE* ep = bp + (bn*d_stride);
  for(; bp < ep; bp += d_stride, sp += s_stride )
    *bp = *sp;
  
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(CopyU)( VECT_OP_TYPE* bp, unsigned bn, const unsigned* sp )
{
  VECT_OP_TYPE*       dbp = bp;
  const VECT_OP_TYPE* ep = bp + bn;
  VECT_OP_TYPE* dp = bp;
  while( dp < ep )
    *dp++ = (VECT_OP_TYPE)*sp++; 
  return dbp;
}


VECT_OP_TYPE* VECT_OP_FUNC(CopyI)( VECT_OP_TYPE* dbp, unsigned dn, const int* sp )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dp < dep )
    *dp++ = (VECT_OP_TYPE)*sp++; 
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(CopyF)( VECT_OP_TYPE* dbp, unsigned dn, const float* sp )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dp < dep )
    *dp++ = (VECT_OP_TYPE)*sp++; 
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(CopyD)( VECT_OP_TYPE* dbp, unsigned dn, const double* sp )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dp < dep )
    *dp++ = (VECT_OP_TYPE)*sp++; 
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(CopyS)( VECT_OP_TYPE* dbp, unsigned dn, const cmSample_t* sp )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dp < dep )
    *dp++ = (VECT_OP_TYPE)*sp++; 
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(CopyR)( VECT_OP_TYPE* dbp, unsigned dn, const cmReal_t* sp )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dp < dep )
    *dp++ = (VECT_OP_TYPE)*sp++; 
  return dbp;
}


VECT_OP_TYPE* VECT_OP_FUNC(CopyStride)(  VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp, unsigned srcStride )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  for(; dp < dep; sp += srcStride )
    *dp++ = *sp; 
  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(Shrink)( VECT_OP_TYPE* s, unsigned sn, const VECT_OP_TYPE* t, unsigned tn )
{
  assert( s <= t && t <= (s+sn) );
  assert( s <= (t+tn) && (t+tn) <= (s+sn));
  //VECT_OP_FUNC(Move)(s,sn - ((t - s) + tn),t+tn);
  VECT_OP_FUNC(Move)((VECT_OP_TYPE*)t,(sn - ((t+tn)-s)) + 1,t+tn);
  return s;
}

VECT_OP_TYPE* VECT_OP_FUNC(Expand)( VECT_OP_TYPE* s, unsigned sn, const VECT_OP_TYPE* t, unsigned tn )                                                            
{
  assert( s <= t && t <= s+sn );
  unsigned i = t - s;
  s = cmMemResizeP(VECT_OP_TYPE,s,sn+tn);
  t = s + i;
  assert( t + tn + sn - i == s + sn + tn );
  VECT_OP_FUNC(Move)(((VECT_OP_TYPE*)t)+tn,sn-i,t);
  return s;  
}

VECT_OP_TYPE* VECT_OP_FUNC(Replace)(VECT_OP_TYPE* s, unsigned* sn, const VECT_OP_TYPE* t, unsigned tn, const VECT_OP_TYPE* u, unsigned un )
{
  // if s is empty and t[tn] is empty
  if( s == NULL && tn == 0 )
  {
    if( un == 0 )
      return s;
    
    s = cmMemAllocZ(VECT_OP_TYPE,un);
    VECT_OP_FUNC(Copy)(s,un,u);

    if( sn != NULL )
      *sn = un;

    return s;
  }    
  

  assert( s!=NULL && t != NULL );
  assert( (u!=NULL && un>0) || (u==NULL && un==0) );

  if( (tn==0 && un==0) || (t==NULL && u==NULL))
    return s;

  // if the area to replace is greater than the area to insert ...
  if( tn > un )
  {
    VECT_OP_FUNC(Shrink)(s,*sn,t+un,tn-un); // ... then shrink the buffer
    *sn -= tn-un;
  }
  else
    // if the area to insert is greater than the area to replace ... 
    if( un > tn )
    {
      unsigned offs = t - s;
      s    = VECT_OP_FUNC(Expand)(s,*sn,t+tn,un-tn); // ... then expand the buffer
      t    = s + offs;
      *sn += un-tn;
    }

  assert(t+un <= s+(*sn));

  if( u!=NULL )
    VECT_OP_FUNC(Copy)((VECT_OP_TYPE*)t,un,u);

  return s;
}



VECT_OP_TYPE* VECT_OP_FUNC(Rotate)( VECT_OP_TYPE* dbp, unsigned dn, int shiftCnt )
{
  VECT_OP_TYPE* dep = dbp + dn;
  int           i   = 0;
  unsigned      k   = 0;
  int           n   = dep - dbp;
  VECT_OP_TYPE  t1  = dbp[i];

  for(k=0; k<n; ++k)
  {
    int          j;  

    j = (i + shiftCnt) % n;

    if( j<0 )
      j += n;

    VECT_OP_TYPE t2 = dbp[j];

    dbp[j] = t1;
    t1     = t2;
    i      = j;
  }

  return dbp;
}

VECT_OP_TYPE* VECT_OP_FUNC(RotateM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* sbp, int rShiftCnt, int cShiftCnt  )
{
  int j;

  while( rShiftCnt < 0 )
    rShiftCnt += drn;

  while( cShiftCnt < 0 )
    cShiftCnt += dcn;

  int m = rShiftCnt % drn;
  int n = cShiftCnt % dcn;


  for(j=0; j<dcn; ++j,++n)
  {
    if(n==dcn)
      n = 0;

    // cnt from dst position to end of column
    unsigned cn = drn - m;

    // copy from top of src col to bottom of dst column
    VECT_OP_FUNC(Copy)(dbp + (n*drn) + m, cn, sbp );
    sbp+=cn;


    if( cn < drn )
    {
      // copy from bottom of src col to top of dst column
      VECT_OP_FUNC(Copy)(dbp + (n*drn), drn-cn, sbp );
      sbp += drn-cn;
    }

  } 
  return dbp;

}


VECT_OP_TYPE* VECT_OP_FUNC(Shift)( VECT_OP_TYPE* dbp, unsigned dn, int shiftCnt, VECT_OP_TYPE fillValue )
{
  VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* rp = dbp;
  unsigned      n  = dep - dbp;

  if( shiftCnt == 0 )
    return dbp;

  if( abs(shiftCnt) >= n )
    return VECT_OP_FUNC(Fill)(dbp,dn,fillValue);

  if( shiftCnt > 0 )
  {
    const VECT_OP_TYPE* sbp = dep - (shiftCnt+1);
    const VECT_OP_TYPE* sep = dbp;
          VECT_OP_TYPE*  dp = dbp + (n-1);

    while( sbp >= sep )
      *dp-- = *sbp--;

    while(dbp <= dp )
      *dbp++ = fillValue;

  }
  else
  {
    const VECT_OP_TYPE* sbp = dbp + abs(shiftCnt);
    while( sbp < dep )
      *dbp++ = *sbp++;

    while(dbp<dep)
      *dbp++ = fillValue;
  }

  return rp;

}


VECT_OP_TYPE* VECT_OP_FUNC(Flip)(  VECT_OP_TYPE* dbp, unsigned dn)
{
  VECT_OP_TYPE* p0 = dbp;
  VECT_OP_TYPE* p1 = dbp + dn - 1;

  while( p0 < p1 )
  {
    VECT_OP_TYPE t = *p0;
    *p0++ = *p1;
    *p1-- = t;
  }
  return dbp;
}


VECT_OP_TYPE* VECT_OP_FUNC(SubVS)( VECT_OP_TYPE* bp, unsigned n, VECT_OP_TYPE v )
{
  const VECT_OP_TYPE* ep = bp + n;
  VECT_OP_TYPE* dp = bp;
  while( dp < ep )
    *dp++ -= v;
  return bp;
}


VECT_OP_TYPE* VECT_OP_FUNC(SubVV)( VECT_OP_TYPE* bp, unsigned n, const VECT_OP_TYPE* v )
{
  const VECT_OP_TYPE* ep = bp + n;
  VECT_OP_TYPE* dp = bp;
  while( dp < ep )
    *dp++ -= *v++;
  return bp;
}

VECT_OP_TYPE* VECT_OP_FUNC(SubVVS)( VECT_OP_TYPE* bp, unsigned n, const VECT_OP_TYPE* v, VECT_OP_TYPE s )
{
  const VECT_OP_TYPE* ep = bp + n;
  VECT_OP_TYPE* dp = bp;
  while( dp < ep )
    *dp++ = *v++ - s;
  return bp;
}


VECT_OP_TYPE* VECT_OP_FUNC(SubVVNN)(VECT_OP_TYPE* dp, unsigned dn, unsigned dnn, const VECT_OP_TYPE* v, unsigned n )
{
  const VECT_OP_TYPE* ep = dp + (dn*dnn);
  VECT_OP_TYPE* dbp = dp;
  for(; dp < ep; dp+=dnn, v+=n )
    *dp -= *v;
  return dbp;  
}

VECT_OP_TYPE* VECT_OP_FUNC(SubVVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p, const VECT_OP_TYPE* sb1p )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ = *sb0p++ - *sb1p++;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(SubVSV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE s0, const VECT_OP_TYPE* sb1p )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ = s0 - *sb1p++;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(AddVS)( VECT_OP_TYPE* bp, unsigned n, VECT_OP_TYPE v )
{
  const VECT_OP_TYPE* ep = bp + n;
  VECT_OP_TYPE* dp = bp;
  while( dp < ep )
    *dp++ += v;
  return bp;
}

VECT_OP_TYPE* VECT_OP_FUNC(AddVV)( VECT_OP_TYPE* bp, unsigned bn, const VECT_OP_TYPE* v )
{
  const VECT_OP_TYPE* ep = bp + bn;
  VECT_OP_TYPE* dp = bp;
  while( dp < ep )
    *dp++ += *v++;
  return bp;
}

VECT_OP_TYPE* VECT_OP_FUNC(AddVVS)( VECT_OP_TYPE* bp, unsigned bn, const VECT_OP_TYPE* v, VECT_OP_TYPE s )
{
  const VECT_OP_TYPE* ep = bp + bn;
  VECT_OP_TYPE* dp = bp;
  while( dp < ep )
    *dp++ = *v++ + s;
  return bp;
}

VECT_OP_TYPE* VECT_OP_FUNC(AddVVNN)(VECT_OP_TYPE* dp, unsigned dn, unsigned dnn, const VECT_OP_TYPE* v, unsigned n )
{
  const VECT_OP_TYPE* ep = dp + (dn*dnn);
  VECT_OP_TYPE* dbp = dp;
  for(; dp < ep; v+=n, dp+=dnn )
    *dp += *v;
  return dbp;  
}

VECT_OP_TYPE* VECT_OP_FUNC(AddVVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p, const VECT_OP_TYPE* sb1p )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ = *sb0p++ + *sb1p++;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(MultVVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p, const VECT_OP_TYPE* sb1p )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ = *sb0p++ * *sb1p++;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(MultVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ *= *sbp++;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(MultVVNN)(VECT_OP_TYPE* dp, unsigned dn, unsigned dnn, const VECT_OP_TYPE* v, unsigned n )
{
  const VECT_OP_TYPE* ep = dp + (dn*dnn);
  VECT_OP_TYPE* dbp = dp;
  for(; dp < ep; v+=n, dp+=dnn )
    *dp *= *v;
  return dbp;  
}

VECT_OP_TYPE* VECT_OP_FUNC(MultVS)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE s )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ *= s;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(MultVVS)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE s )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ = *sbp++ * s;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(MultVaVS)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE s )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ += *sbp++ * s;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(MultSumVVS)(VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE s )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ += *sbp++ * s;
  return dp;
}



VECT_OP_TYPE* VECT_OP_FUNC(DivVVS)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p, VECT_OP_TYPE s1 )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ = *sb0p++ / s1;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(DivVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ /= *sb0p++;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(DivVVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p, const VECT_OP_TYPE* sb1p )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ = *sb0p++ / *sb1p++;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(DivVVNN)(VECT_OP_TYPE* dp, unsigned dn, unsigned dnn, const VECT_OP_TYPE* v, unsigned n )
{
  const VECT_OP_TYPE* ep = dp + (dn*dnn);
  VECT_OP_TYPE* dbp = dp;
  for(; dp < ep; v+=n, dp+=dnn )
    *dp /= *v;
  return dbp;  
}


VECT_OP_TYPE* VECT_OP_FUNC(DivVS)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE s )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ /=  s;
  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(DivVSV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE s0, const VECT_OP_TYPE* sb1p )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  while( dbp < dep )
    *dbp++ = s0 / *sb1p++;
  return dp;
}


VECT_OP_TYPE* VECT_OP_FUNC(DivVVZ)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  for(; dbp < dep; ++sb0p )
    if( *sb0p == 0 )
      *dbp++ = 0;
    else
      *dbp++ /= *sb0p;

  return dp;
}

VECT_OP_TYPE* VECT_OP_FUNC(DivVVVZ)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p, const VECT_OP_TYPE* sb1p )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  VECT_OP_TYPE* dp = dbp;
  for(; dbp < dep; ++sb0p,++sb1p )
    if( *sb1p == 0 )
      *dbp++ = 0;
    else
      *dbp++ = *sb0p / *sb1p;

  return dp;
}



VECT_OP_TYPE*  VECT_OP_FUNC(DivMS)(    VECT_OP_TYPE* dp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* sp )
{
  unsigned i;
  for(i=0; i<dcn; ++i)
    VECT_OP_FUNC(DivVS)( dp + i*drn, drn, sp[i] );
  return dp;
}


VECT_OP_TYPE  VECT_OP_FUNC(Sum)( const VECT_OP_TYPE* bp, unsigned n )
{
  const VECT_OP_TYPE* ep = bp + n;
  VECT_OP_TYPE s = 0;
  while( bp < ep )
    s += *bp++;

  return s;
}

VECT_OP_TYPE  VECT_OP_FUNC(SumN)( const VECT_OP_TYPE* bp, unsigned n, unsigned stride )
{
  const VECT_OP_TYPE* ep = bp + (n*stride);
  VECT_OP_TYPE s = 0;
  for(; bp < ep; bp += stride )
    s += *bp;

  return s;
}

VECT_OP_TYPE*  VECT_OP_FUNC(SumM)(const VECT_OP_TYPE* sp, unsigned srn, unsigned scn, VECT_OP_TYPE* dp )
{
  unsigned i;
  for(i=0; i<scn; ++i)
    dp[i] = VECT_OP_FUNC(Sum)(sp + (i*srn), srn );
  return dp;
}

VECT_OP_TYPE*  VECT_OP_FUNC(SumMN)(const VECT_OP_TYPE* sp, unsigned srn, unsigned scn, VECT_OP_TYPE* dp )
{
  unsigned i;
  for(i=0; i<srn; ++i)
    dp[i] = VECT_OP_FUNC(SumN)(sp + i, scn, srn );
  return dp;
}

VECT_OP_TYPE*   VECT_OP_FUNC(Abs)(   VECT_OP_TYPE* dbp, unsigned dn )
{
  unsigned i;
  for(i=0; i<dn; ++i)
    if( dbp[i]<0 )     
      dbp[i] = -dbp[i];

  return dbp;  
}

// mi is a target value - it holds the number of elements in ap[an] which must be be less than the median value.
// If the initial array contains an even number of values then the median value is formed by averaging the two center values.
// In this case *evenFlPtr is set and used to indicate that the center-upper value must be found during undwinding.
VECT_OP_TYPE VECT_OP_FUNC(MedianSearch)( unsigned mi, const VECT_OP_TYPE* ap, unsigned an, bool* evenFlPtr )
{ 
  VECT_OP_TYPE x = ap[0]; // pick a random value as a potential median value  

  VECT_OP_TYPE a1[ an ];  // values below x
  VECT_OP_TYPE a3[ an ];  // values above x
  unsigned     a1n = 0;
  unsigned     a2n = 0;   // values equal to x
  unsigned     a3n = 0;   


  const VECT_OP_TYPE* abp = ap;
  const VECT_OP_TYPE* aep = abp + an;


  for(; abp < aep; ++abp )
  {
    if( *abp < x )
      a1[a1n++] = *abp;
    else
    {
      if( *abp > x )
        a3[a3n++] = *abp;
      else
        ++a2n;
    }
  }

  //printf("%i : %i %i %i\n",mi,a1n,a2n,a3n);
  
  // there are more values below x (mi remains the target split point)
  if( a1n > mi )
  {
    x = VECT_OP_FUNC(MedianSearch)(mi,a1,a1n,evenFlPtr);
  }
  else
  {
    // the target was located
    if( a1n+a2n >= mi )
    {
    
      // if a1n alone matches mi then  the max value in a1[] holds the median value otherwise x is the median
      if(a1n>=1 && a1n==mi)
      {
        VECT_OP_TYPE mv = VECT_OP_FUNC(Max)(a1,a1n,1);
        x               = *evenFlPtr ? (mv+x)/2 : mv;
        *evenFlPtr      = false;          
      }

      // if the evenFl is set then the closest value above the median (x) must be located
      if( *evenFlPtr )
      {
        // if the next greater value is in a2[]
        if( a2n > 1 && (a1n+a2n) > mi )
          *evenFlPtr = false;
        else
          // if the next greater value is in a3[]
          if( a3n > 1 )
          {
            x          = (x + VECT_OP_FUNC(Min)(a3,a3n,1))/2;
            *evenFlPtr = false;
          }
      }

      // no need for unwind processing - all the possibilities at this level have been exhausted
      return x;
    }  
    else
    {
      // There are more values above x - the median must therefore be in a3[].
      // Reset mi cmcounting for the fact that we know that there are  
      // a1n+a2n values below the lowest value in a3.
      x = VECT_OP_FUNC(MedianSearch)(mi - (a1n+a2n), a3, a3n, evenFlPtr );
    }
  }

  if( *evenFlPtr )
  {

    // find the first value greater than x 
    while( ap < aep && *ap <= x )
      ++ap;

    if( ap < aep )
    {

      VECT_OP_TYPE v  = *ap++;
      
      // find the nearest value greater than x
      for(; ap < aep; ++ap )
        if( *ap > x && ((*ap - x) < (v-x)))
          v  = *ap;

      
      x = (v + x)/2;
      *evenFlPtr = false;
    }
  }
  return x;
}


VECT_OP_TYPE  VECT_OP_FUNC(Median)(   const VECT_OP_TYPE* bp, unsigned n )
{
  bool     evenFl = cmIsEvenU(n);
  unsigned medIdx = evenFl ? n/2 : (n+1)/2;
  return VECT_OP_FUNC(MedianSearch)( medIdx, bp, n, &evenFl );
}


unsigned      VECT_OP_FUNC(MinIndex)( const VECT_OP_TYPE* bp, unsigned n, unsigned stride )
{
  const VECT_OP_TYPE* ep = bp + (n*stride);
  if( bp >= ep )
    return cmInvalidIdx;

  const VECT_OP_TYPE* p = bp;
  const VECT_OP_TYPE* mp = bp;

  bp+=stride;

  for(; bp < ep; bp+=stride )
    if( *bp < *mp )
      mp = bp;

  return (mp - p)/stride;
}

unsigned      VECT_OP_FUNC(MaxIndex)( const VECT_OP_TYPE* bp, unsigned n, unsigned stride )
{
  const VECT_OP_TYPE* ep = bp + (n*stride);

  if( bp >= ep )
    return cmInvalidIdx;

  const VECT_OP_TYPE* p = bp;
  const VECT_OP_TYPE* mp = bp;

  bp+=stride;

  for(; bp < ep; bp+=stride )
    if( *bp > *mp )
      mp = bp;

  return (mp - p)/stride;
}

VECT_OP_TYPE  VECT_OP_FUNC(Min)( const VECT_OP_TYPE* bp, unsigned n, unsigned stride )
{
  unsigned i;

  if((i = VECT_OP_FUNC(MinIndex)(bp,n,stride)) == cmInvalidIdx )
  {
    assert(0);
    return 0;
  }
  return bp[i*stride];
}

VECT_OP_TYPE  VECT_OP_FUNC(Max)( const VECT_OP_TYPE* bp, unsigned n, unsigned stride )
{
  unsigned i;


  if((i = VECT_OP_FUNC(MaxIndex)(bp,n,stride)) == cmInvalidIdx )
  {
    assert(0);
    return 0;
  }
  return bp[i*stride];
} 

VECT_OP_TYPE*  VECT_OP_FUNC(MinVV)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sp )
{
  unsigned i;
  for(i=0; i<dn; ++i)
    if( sp[i] < dp[i] )
      dp[i] = sp[i];
  return dp;
}

VECT_OP_TYPE*  VECT_OP_FUNC(MaxVV)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sp )
{
  unsigned i;
  for(i=0; i<dn; ++i)
    if( sp[i] > dp[i] )
      dp[i] = sp[i];

  return dp;
}

unsigned*  VECT_OP_FUNC(MinIndexM)( unsigned* dp, const VECT_OP_TYPE* sp, unsigned srn, unsigned scn )
{
  unsigned i = 0;
  for(i=0; i<scn; ++i)
    dp[i] = VECT_OP_FUNC(MinIndex)(sp + (i*srn), srn, 1 );
  return dp;
}

unsigned*  VECT_OP_FUNC(MaxIndexM)( unsigned* dp, const VECT_OP_TYPE* sp, unsigned srn, unsigned scn )
{
  unsigned i = 0;
  for(i=0; i<scn; ++i)
    dp[i] = VECT_OP_FUNC(MaxIndex)(sp + (i*srn), srn, 1 );
  return dp;
}


VECT_OP_TYPE  VECT_OP_FUNC(Mode)( const VECT_OP_TYPE* sp, unsigned sn )
{
  unsigned     n[sn];
  VECT_OP_TYPE v[sn];
  unsigned     i,j,k = 0;
  unsigned     n0 = 0;  // idx of most freq occurring ele
  unsigned     n1 = -1; // idx of 2nd most freq occurring ele
  
  for(i=0; i<sn; ++i)
  {
    // find sp[i] in v[]
    for(j=0; j<k; ++j)
      if( sp[i] == v[j] )
      {
        ++n[j]; 
        break;
      }

    // sp[i] was not found in v[]
    if( k == j )
    {
      v[j] = sp[i];
      n[j] = 1;      
      ++k;
    }

    // n[j] holds frq of sp[i]

    // do nothing if j is already most freq
    if( j != n0 )
    {
      // if j is new most freq 
      if( n[j] > n[n0] )
      {
        n1 = n0;
        n0 = j;
      }
      else
        // if j is 2nd most freq
        if( (n1==-1) || (n[j] > n[n1]) )
          n1 = j;
    }

    // if diff between two most freq is greater than remaining ele's
    if( (n1!=-1) && (n[n0]-n[n1]) >= (sn-i) )
      break;

  }


  // if there are no ele's with same count 
  if( n[n0] > n[n1] )
    return v[n0];

  // break tie between ele's with same count be returning min value
  // (this is the same as Matlab tie break criteria)
  j = 0;
  for(i=1; i<k; ++i)
    if( (n[i] > n[j]) || (n[i] == n[j] && v[i] < v[j]) )
      j=i;

  return v[j];
}

unsigned VECT_OP_FUNC(Find)( const VECT_OP_TYPE* sp, unsigned sn, VECT_OP_TYPE key )
{
  const VECT_OP_TYPE* sbp = sp;
  const VECT_OP_TYPE* ep = sp + sn;
  while( sp<ep )
    if( *sp++ == key )
      break;

  if( sp==ep )
    return cmInvalidIdx;

  return (sp-1) - sbp;
}

unsigned VECT_OP_FUNC(Count)( const VECT_OP_TYPE* sp, unsigned sn, VECT_OP_TYPE key )
{
  unsigned            cnt = 0;
  const VECT_OP_TYPE* ep  = sp + sn;
  while( sp<ep )
    if( *sp++ == key )
      ++cnt;

  return cnt;
}

VECT_OP_TYPE* VECT_OP_FUNC(ReplaceLte)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sp, VECT_OP_TYPE lteKeyVal, VECT_OP_TYPE replaceVal )
{
  VECT_OP_TYPE*       rp = dp;
  const VECT_OP_TYPE* ep = dp + dn;

  for(; dp < ep; ++sp )
    *dp++ = *sp <= lteKeyVal ? replaceVal : *sp;

      

  return rp;
}

VECT_OP_TYPE* VECT_OP_FUNC(Diag)( VECT_OP_TYPE* dbp, unsigned n, const VECT_OP_TYPE* sbp )
{
  unsigned i,j;
  for(i=0,j=0; i<n && j<n; ++i,++j)
    dbp[ (i*n) + j ] = sbp[i];

  return dbp;  
}


VECT_OP_TYPE* VECT_OP_FUNC(DiagZ)(VECT_OP_TYPE* dbp, unsigned n, const VECT_OP_TYPE* sbp )
{
  VECT_OP_FUNC(Fill)(dbp,n*n,0);
  return VECT_OP_FUNC(Diag)(dbp,n,sbp);
}


VECT_OP_TYPE* VECT_OP_FUNC(Identity)( VECT_OP_TYPE* dbp, unsigned rn, unsigned cn )
{
  unsigned i,j;
  for(i=0,j=0; i<cn && j<rn; ++i,++j)
      dbp[ (i*rn) + j ] = 1;

  return dbp;
}


VECT_OP_TYPE* VECT_OP_FUNC(IdentityZ)( VECT_OP_TYPE* dbp, unsigned rn, unsigned cn )
{
  VECT_OP_FUNC(Fill)(dbp,rn*cn,0);
  return VECT_OP_FUNC(Identity)(dbp,rn,cn);
}


VECT_OP_TYPE* VECT_OP_FUNC(Transpose)( VECT_OP_TYPE* dbp, const VECT_OP_TYPE* sp, unsigned srn, unsigned scn )
{
  VECT_OP_TYPE*       dp  = dbp;
  const VECT_OP_TYPE* dep = dbp + (srn*scn);

  while( dbp < dep )
  {
    const VECT_OP_TYPE* sbp = sp++;
    const VECT_OP_TYPE* sep = sbp + (srn*scn);

    for(; sbp < sep; sbp+=srn )
      *dbp++ = *sbp;
  }

  return dp;
}

VECT_OP_TYPE VECT_OP_FUNC(Seq)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE beg, VECT_OP_TYPE incr )
{
  const VECT_OP_TYPE* dep = dbp + dn;
  unsigned i = 0;
  for(; dbp<dep; ++i)
    *dbp++ = beg + (incr*i);
  return beg + (incr*i);
}


void VECT_OP_FUNC(FnThresh)( const VECT_OP_TYPE* xV, unsigned xN, unsigned wndN, VECT_OP_TYPE* yV, unsigned yStride, VECT_OP_TYPE (*fnPtr)(const VECT_OP_TYPE*, unsigned) )
{
  int i0 = cmIsOddU(wndN) ?  (wndN-1)/2 : wndN/2;
  int i1 = cmIsOddU(wndN) ?  (wndN-1)/2 : wndN/2 - 1;
  int i,j;
  
  i0 = -i0;

  if( fnPtr == NULL )
    fnPtr = &(VECT_OP_FUNC(Median));

  for(i=0; i<xN; ++i,++i0,++i1)
  {
    j = (i*yStride);
    if( i0 < 0 )
      if( i1 >= xN )
        yV[j] = (*fnPtr)(xV,xN);
      else
        yV[j] = (*fnPtr)(xV,i1+1);
    else if( i1 >= xN )
      yV[j] = (*fnPtr)(xV+i0,xN-i0);
    else
      yV[j] = (*fnPtr)(xV+i0,wndN);
  }
}


void VECT_OP_FUNC(MedianFilt)( const VECT_OP_TYPE* xV, unsigned xN, unsigned wndN, VECT_OP_TYPE* yV, unsigned yStride )
{
  int i0 = cmIsOddU(wndN) ?  (wndN-1)/2 : wndN/2;
  int i1 = cmIsOddU(wndN) ?  (wndN-1)/2 : wndN/2 - 1;
  int i,j;
  VECT_OP_TYPE tV[ wndN ];
  
  i0 = -i0;

  VECT_OP_FUNC(Fill)(tV,wndN,0);

  for(i=0; i<xN; ++i,++i0,++i1)
  {

    j = (i*yStride);

    // note that the position of the zero padding in tV[]
    // does not matter because the median calcluation does
    // not make any assumptions about the order of the argument
    // vector.

    if( i0 < 0 )
    {
      VECT_OP_FUNC(Copy)(tV,wndN+i0,xV);
      VECT_OP_FUNC(Fill)(tV+wndN+i0,labs(i0),0);
      //VECT_OP_FUNC(Print)(NULL,1,wndN,tV,-1,-1);
      
      yV[j] = VECT_OP_FUNC(Median)(tV,wndN);
      continue;
    }



    if( i1 >= xN )
    {
      VECT_OP_FUNC(Copy)(tV,wndN-(i1-xN+1),xV+i0);
      VECT_OP_FUNC(Fill)(tV+wndN-(i1-xN+1),i1-xN+1,0);
      //VECT_OP_FUNC(Print)(NULL,1,wndN,tV,-1,-1);
     
      yV[j] = VECT_OP_FUNC(Median)(tV,wndN);
      continue;
    }

    //VECT_OP_FUNC(Print)(NULL,1,wndN,xV+i0,-1,-1);
    yV[j] = VECT_OP_FUNC(Median)(xV+i0,wndN);
    
  }
}

unsigned* VECT_OP_FUNC(LevEditDistAllocMtx)(unsigned maxN)
{
  maxN += 1;

  unsigned* m = cmMemAllocZ(unsigned,maxN*maxN);
  unsigned* p = m;
  unsigned i;

  // initialize the comparison matrix with the default costs in the
  // first row and column
  // (Note that this matrix is not oriented in column major order like most 'cm' matrices.)
  for(i=0; i<maxN; ++i)
  {
    p[i]          = i;		// 0th row
    p[ i * maxN ] = i;		// 0th col
  }

  return m;
}

double VECT_OP_FUNC(LevEditDist)(unsigned mtxMaxN, unsigned* m, const VECT_OP_TYPE* s0, int n0, const VECT_OP_TYPE* s1, int n1, unsigned maxN )
{
  mtxMaxN += 1;

  assert( n0 < mtxMaxN && n1 < mtxMaxN );
  
  int			  v		= 0;
  unsigned  i;
  // Note that m[maxN,maxN] is not oriented in column major order like most 'cm' matrices.

  for(i=1; i<n0+1; ++i)
  {
    unsigned ii 	= i  * mtxMaxN;	// current row
    unsigned i_1 	= ii - mtxMaxN;	// previous row
    unsigned j;	
    for( j=1; j<n1+1; ++j)
    {
      int cost = s0[i-1] == s1[j-1] ? 0 : 1;

      //m[i][j] = min( m[i-1][j] + 1, min( m[i][j-1] + 1, m[i-1][j-1] + cost ) );
					
      m[ ii + j ] = v = cmMin( m[ i_1 + j] + 1, cmMin( m[ ii + j - 1] + 1, m[ i_1 + j - 1 ] + cost ) );
    }
  }	
  return (double) v / maxN;		
}


double VECT_OP_FUNC(LevEditDistWithCostThresh)( int mtxMaxN, unsigned* m, const VECT_OP_TYPE* s0, int n0, const VECT_OP_TYPE* s1, int n1, double maxCost, unsigned maxN ) 
{
  mtxMaxN += 1;

  int			v		= 0;

  maxCost = cmMin(1.0,cmMax(0.0,maxCost));
		
  int iMaxCost = ceil( maxCost * maxN );
			
  assert( iMaxCost > 0 && maxCost > 0 );
			
  // If the two strings are different lengths and the min possible distance is
  // greater than the threshold then return the threshold as the cost.
  // (Note: For strings of different length the min possible distance is the 
  // difference in length between the two strings).
  if( abs(n0-n1) > iMaxCost )
    return maxCost;
			
  int i;
  // for each row in the matrix ...
  for(i=1; i<n0+1; ++i)
  {
    int ii 	= i  * mtxMaxN;	// current row
    int i_1 = ii - mtxMaxN;	// previous row
				
    // Limit the row to (2*iMaxCost)+1 diagnal strip.
    // This strip is based on the idea that the best case can be precomputed for
    // all matrix elements in advance - where the best case for position i,j is:
    // abs(i-j). This can be justified based on the idea that the least possible
    // distance between two strings of length i and j is abs(i-1).  The minimum least
    // possible distance is therefore found on the matrix diagnal and grows as the
    // distance from the diagnal increases.
				
    int ji = cmMax( 1, i - iMaxCost ); 
    int jn = cmMin(iMaxCost + i, n1) + 1;
    int j;
				
    // fill in (max cost + 1) as the value in the column before the starting column
    // (it will be referred to during the first computation in this row)
    if( ji >= 2 )
      m[ ii + (ji-1) ] = iMaxCost + 1;
				
    // for each column in the diagnal stripe - beginning with the leftmost column.
    for( j=ji; j<jn; ++j)
    {
      int cost = s0[i-1] == s1[j-1] ? 0 : 1;

      m[ ii + j ] = v = cmMin( m[ i_1 + j] + 1, cmMin( m[ ii + j - 1] + 1, m[ i_1 + j - 1 ] + cost ) );
    }
				
    // fill in (max cost + 1) in the column following the last column 
    // (it will be referred to during computation of the following row)
    if( j < n1+1 )
      m[ii + j] = iMaxCost + 1;
  }	
			
  assert( v >= 0 );
			
			
  return cmMin( maxCost , (double) v / maxN);		
}

#endif
