#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmFloatTypes.h"
#include "cmMath.h"
#include <sys/types.h> // u_char

// TODO: rewrite to avoid copying
// this code comes via csound source ...
double 		cmX80ToDouble( unsigned char rate[10] )
{
  	char sign;
    short exp = 0;
    unsigned long mant1 = 0;
    unsigned long mant0 = 0;
    double val;
    unsigned char* p = (unsigned char*)rate;

    exp = *p++;
    exp <<= 8;
    exp |= *p++;
    sign = (exp & 0x8000) ? 1 : 0;
    exp &= 0x7FFF;
    
    mant1 = *p++;
    mant1 <<= 8;
    mant1 |= *p++;
    mant1 <<= 8;
    mant1 |= *p++;
    mant1 <<= 8;
    mant1 |= *p++;

    mant0 = *p++;
    mant0 <<= 8;
    mant0 |= *p++;
    mant0 <<= 8;
    mant0 |= *p++;
    mant0 <<= 8;
    mant0 |= *p++;

    /* special test for all bits zero meaning zero 
       - else pow(2,-16383) bombs */
    if (mant1 == 0 && mant0 == 0 && exp == 0 && sign == 0)
      return 0.0;
    else {
      val  = ((double)mant0) * pow(2.0,-63.0);
      val += ((double)mant1) * pow(2.0,-31.0);
      val *= pow(2.0,((double) exp) - 16383.0);
      return sign ? -val : val;
    }
}

// TODO: rewrite to avoid copying
/*
 * Convert double to IEEE 80 bit floating point
 * Should be portable to all C compilers.
 * 19aug91 aldel/dpwe  covered for MSB bug in Ultrix 'cc'
 */

void cmDoubleToX80(double val, unsigned char rate[10])
{
    char sign = 0;
    short exp = 0;
    unsigned long mant1 = 0;
    unsigned long mant0 = 0;
    unsigned char* p = (unsigned char*)rate;

    if (val < 0.0)	{  sign = 1;  val = -val; }
	
    if (val != 0.0)	/* val identically zero -> all elements zero */
      {
        exp = (short)(log(val)/log(2.0) + 16383.0);
        val *= pow(2.0, 31.0+16383.0-(double)exp);
        mant1 =((unsigned)val);
        val -= ((double)mant1);
        val *= pow(2.0, 32.0);
        mant0 =((double)val);
      }
    
    *p++ = ((sign<<7)|(exp>>8));
    *p++ = (u_char)(0xFF & exp);
    *p++ = (u_char)(0xFF & (mant1>>24));
    *p++ = (u_char)(0xFF & (mant1>>16));
    *p++ = (u_char)(0xFF & (mant1>> 8));
    *p++ = (u_char)(0xFF & (mant1));
    *p++ = (u_char)(0xFF & (mant0>>24));
    *p++ = (u_char)(0xFF & (mant0>>16));
    *p++ = (u_char)(0xFF & (mant0>> 8));
    *p++ = (u_char)(0xFF & (mant0));

}

bool		cmIsPowerOfTwo( unsigned x )
{
  return !( (x < 2) || (x & (x-1)) );
}

unsigned 	cmNextPowerOfTwo(	unsigned val )
{
  unsigned i;
	unsigned mask 	= 1;
	unsigned msb 	= 0;
	unsigned cnt	= 0;
	
	// if val is a power of two return it
	if( cmIsPowerOfTwo(val) )
		return val;

	// next pow of zero is 2
	if( val == 0 )
		return 2;
	
	// if the next power of two can't be represented in 32 bits
	if( val > 0x80000000)
  {
    assert(0);
		return 0;
	}

	// find most sig. bit that is set - the number with only the next msb set is next pow 2 
	for(i=0; i<31; i++,mask<<=1)
		if( mask & val )
		{
			msb = i;
			cnt++;
		}
		
		
	return 1 << (msb + 1);	
}

unsigned cmNearPowerOfTwo( unsigned i )
{
  unsigned vh = cmNextPowerOfTwo(i);

  if( vh == 2 )
    return vh;

  unsigned vl = vh / 2;

  if( vh - i < i - vl )
    return vh;
  return vl;
}

bool     cmIsOddU(    unsigned v ) { return v % 2 == 1; }
bool     cmIsEvenU(   unsigned v ) { return !cmIsOddU(v); }
unsigned cmNextOddU(  unsigned v ) { return cmIsOddU(v)  ? v : v+1; }
unsigned cmPrevOddU(  unsigned v ) { return cmIsOddU(v)  ? v : v-1; }
unsigned cmNextEvenU( unsigned v ) { return cmIsEvenU(v) ? v : v+1; }
unsigned cmPrevEvenU( unsigned v ) { return cmIsEvenU(v) ? v : v-1; }

unsigned cmModIncr(int idx, int delta, int maxN )
{
  int sum = idx + delta;

  if( sum >= maxN )
    return sum - maxN;

  if( sum < 0 )
    return maxN + sum;

  return sum;
}


// modified bessel function of first kind, order 0
// ref: orfandis appendix B io.m
double	cmBessel0( double x )
{
	double eps = pow(10.0,-9.0);
	double n = 1.0;
	double S = 1.0;
	double D = 1.0;

	while(D > eps*S)
	{
		double T = x /(2.0*n);
		n = n+1;
		D = D * pow(T,2.0);
		S = S + D;
	}

	return S;

}

//=================================================================
// The following elliptic-related function approximations come from
// Parks & Burrus, Digital Filter Design, Appendix program 9, pp. 317-326
// which in turn draws directly on other sources

// calculate complete elliptic integral (quarter period) K
// given *complimentary* modulus kc
cmReal_t cmEllipK( cmReal_t kc )
{
  cmReal_t a = 1, b = kc, c = 1, tmp;

  while( c > cmReal_EPSILON )
  {
    c = 0.5*(a-b);
    tmp = 0.5*(a+b);
    b = sqrt(a*b);
    a = tmp;
  }

  return M_PI/(2*a);
}

// calculate elliptic modulus k
// given ratio of complete elliptic integrals r = K/K'
// (solves the "degree equation" for fixed N = K*K1'/K'K1)
cmReal_t cmEllipDeg( cmReal_t r )
{
  cmReal_t q,a,b,c,d;
  a = b = c = 1;
  d = q = exp(-M_PI*r);

  while( c > cmReal_EPSILON )
  {
    a = a + 2*c*d;
    c = c*d*d;
    b = b + c;
    d = d*q;
  }

  return 4*sqrt(q)*pow(b/a,2);
}

// calculate arc elliptic tangent u (elliptic integral of the 1st kind)
// given argument x = sc(u,k) and *complimentary* modulus kc
cmReal_t cmEllipArcSc( cmReal_t x, cmReal_t kc )
{
  cmReal_t a = 1, b = kc, y = 1/x, tmp;
  unsigned L = 0;

  while( true )
  {
    tmp = a*b;
    a += b;
    b = 2*sqrt(tmp);
    y -= tmp/y;
    if( y == 0 )
      y = sqrt(tmp) * 1E-10;
    if( fabs(a-b)/a < cmReal_EPSILON )
      break;
    L *= 2;
    if( y < 0 )
      L++;
  }

  if( y < 0 )
    L++;

  return (atan(a/y) + M_PI*L)/a;
}

// calculate Jacobi elliptic functions sn, cn, and dn
// given argument u and *complimentary* modulus kc
cmRC_t cmEllipJ( cmReal_t u, cmReal_t kc, cmReal_t* sn, cmReal_t* cn, cmReal_t* dn )
{
  assert( sn != NULL || cn != NULL || dn != NULL );

  if( u == 0 )
  {
    if( sn != NULL ) *sn = 0;
    if( cn != NULL ) *cn = 1;
    if( dn != NULL ) *dn = 1;
    return cmOkRC;
  }

  int i;
  cmReal_t a,b,c,d,e,tmp,_sn,_cn,_dn;
  cmReal_t aa[16], bb[16];

  a = 1;
  b = kc;

  for( i = 0; i < 16; i++ )
  {
    aa[i] = a;
    bb[i] = b;
    tmp = (a+b)/2;
    b = sqrt(a*b);
    a = tmp;
    if( (a-b)/a < cmReal_EPSILON )
      break;
  }

  c = a/tan(u*a);
  d = 1;

  for( ; i >= 0; i-- )
  {
    e = c*c/a;
    c = c*d;
    a = aa[i];
    d = (e + bb[i]) / (e+a);
  }

  _sn = 1/sqrt(1+c*c);
  _cn = _sn*c;
  _dn = d;

  if( sn != NULL ) *sn = _sn;
  if( cn != NULL ) *cn = _cn;
  if( dn != NULL ) *dn = _dn;

  return cmOkRC;
}

//=================================================================
// bilinear transform
// z = (2*sr + s)/(2*sr - s)
cmRC_t cmBlt( unsigned n, cmReal_t sr, cmReal_t* rp, cmReal_t* ip )
{
  unsigned i;
  cmReal_t a = 2*sr,
           tr, ti, td;

  for( i = 0; i < n; i++ )
  {
    tr = rp[i];
    ti = ip[i];
    td = pow(a-tr, 2) + ti*ti;
    rp[i] = (a*a - tr*tr - ti*ti)/td;
    ip[i] = 2*a*ti/td;
    if( tr < -1E15 )
      rp[i] = 0;
    if( fabs(ti) > 1E15 )
      ip[i] = 0;
  }

  return cmOkRC;
}

unsigned cmHzToMidi( double hz )
{

  float midi = 12.0 * log2(hz/13.75) + 9;

  if( midi < 0 )
    midi = 0;
  if( midi > 127 )
    midi = 127;

  return (unsigned)lround(midi);
}

float    cmMidiToHz( unsigned midi )
{
  double m = midi <= 127 ? midi : 127;
  
  return (float)( 13.75 * pow(2.0,(m - 9.0)/12.0)); 
}


//=================================================================
// Floating point byte swapping

// Unions used to type-pun the swapping functions and thereby 
// avoid strict aliasing problems with -O2.  Using unions for 
// this purpose is apparently legal under C99 but not C++.

typedef union
{
  unsigned u;
  float    f;
} _cmMathU_t;

typedef union
{
  unsigned long long u;
  double    f;
} _cmMathUL_t;

unsigned           cmFfSwapFloatToUInt( float v )
{
  assert( sizeof(float) == sizeof(unsigned));
  _cmMathU_t u;
  u.f=v;
  return cmSwap32(u.u);
}

float              cmFfSwapUIntToFloat( unsigned v )
{
  assert( sizeof(float) == sizeof(unsigned));
  _cmMathU_t u;

  u.u = cmSwap32(v);
  return u.f;
}

unsigned long long cmFfSwapDoubleToULLong( double v )
{
  assert( sizeof(double) == sizeof(unsigned long long));
  _cmMathUL_t u;
  u.f = v;
  return cmSwap64(u.u);
}

double             cmFfSwapULLongToDouble( unsigned long long v )
{
  assert( sizeof(double) == sizeof(unsigned long long));
  _cmMathUL_t u;
  u.u = cmSwap64(v);
  return u.f;
}

int      cmRandInt( int min, int max )
{
  assert( min <= max );
  int offs = max - min;
  return min + cmMax(0,cmMin(offs,(int)round(offs * (double)rand() / RAND_MAX)));
}

unsigned cmRandUInt( unsigned min, unsigned max )
{
  assert( min <= max );
  unsigned offs = max - min;
  return min + cmMax(0,cmMin(offs,(unsigned)round(offs * (double)rand() / RAND_MAX)));
}

float    cmRandFloat( float min, float max )
{
  assert( min <= max );
  float offs = max - min;
  return min + cmMax(0,cmMin(offs,(float)(offs * (double)rand() / RAND_MAX)));
}

double   cmRandDouble( double min, double max )
{
  assert( min <= max );
  double offs = max - min;
  return min + cmMax(0,cmMin(offs,(offs * (double)rand() / RAND_MAX)));
}


//=================================================================
// Base on: http://stackoverflow.com/questions/3874627/floating-point-comparison-functions-for-c-sharp

bool cmIsCloseD( double x0, double x1, double eps )
{
  double d = fabs(x0-x1);
  
  if( x0 == x1 )
    return true;

  if( x0==0 || x1==0 || d<DBL_MIN )
    return d < (eps * DBL_MIN);

  return (d / cmMin( fabs(x0) + fabs(x1), DBL_MAX)) < eps;
}

bool cmIsCloseF( float  x0, float  x1, double  eps_d )
{
  float eps = (float)eps_d;
  float d = fabsf(x0-x1);
  
  if( x0 == x1 )
    return true;

  if( x0==0 || x1==0 || d<FLT_MIN )
    return d < (eps * FLT_MIN);

  return (d / cmMin( fabsf(x0) + fabsf(x1), FLT_MAX)) < eps;
}

bool cmIsCloseI( int x0, int x1, double eps )
{
  if( x0 == x1 )
    return true;
  
  return abs(x0-x1)/(abs(x0)+abs(x1)) < eps;
}


bool cmIsCloseU( unsigned x0, unsigned x1, double eps )
{
  if( x0 == x1 )
    return true;
  if( x0 > x1 )
    return (x0-x1)/(x0+x1) < eps;
  else
    return (x1-x0)/(x0+x1) < eps;
}

//=================================================================

// cmLFSR() implementation based on note at bottom of:
// http://www.ece.cmu.edu/~koopman/lfsr/index.html
void cmLFSR( unsigned lfsrN, unsigned tapMask, unsigned seed, unsigned* yV, unsigned yN )
{
  assert( 0 < lfsrN && lfsrN < 32 );
  
  unsigned i;
  for(i=0; i<yN; ++i)
  {
    if( (yV[i] = seed & 1)==1 )
      seed = (seed >> 1) ^ tapMask;
    else
      seed = (seed >> 1);

  }
}

bool cmMLS_IsBalanced( const unsigned* xV, int xN)
{
  int      a = 0;
  unsigned i;

  for(i=0; i<xN; ++i)
    if( xV[i] == 1 )
      ++a;

  return abs(a - (xN-a)) == 1;
}

unsigned _cmGenGoldCopy( int* y, unsigned yi, unsigned yN, unsigned* x, unsigned xN)
{
  unsigned i;
  for(i=0; i<xN; ++i,++yi)
    y[yi] = x[i]==1 ? -1 : 1;

  assert(yi <= yN);
  return yi;
}

bool cmGenGoldCodes( unsigned lfsrN, unsigned poly_coeff0, unsigned poly_coeff1, unsigned goldN, int* yM, unsigned mlsN  )
{
  bool      retFl = true;
  unsigned  yi    = 0;
  unsigned  yN    = goldN * mlsN;
  unsigned* mls0V = cmMemAllocZ(unsigned,mlsN);
  unsigned* mls1V = cmMemAllocZ(unsigned,mlsN);
  unsigned* xorV  = cmMemAllocZ(unsigned,mlsN);
  
  unsigned  i,j;
  
  cmLFSR(lfsrN, poly_coeff0, 1 << (lfsrN-1), mls0V, mlsN);

  cmLFSR(lfsrN, poly_coeff1, 1 << (lfsrN-1), mls1V, mlsN);

  if( cmMLS_IsBalanced(mls0V,mlsN) )
    yi = _cmGenGoldCopy(yM, yi, yN, mls0V, mlsN);

  if( yi<yN && cmMLS_IsBalanced(mls1V,mlsN) )
    yi = _cmGenGoldCopy(yM, yi, yN, mls1V, mlsN);

  
  for(i=0;  yi < yN && i<mlsN-1; ++i )
  {
    for(j=0; j<mlsN; ++j)
      xorV[j] = (mls0V[j] + mls1V[ (i+j) % mlsN ]) % 2;
    
    if( cmMLS_IsBalanced(xorV,mlsN) )
      yi = _cmGenGoldCopy(yM,yi,yN,xorV,mlsN);
  }

  if(yi < yN )
  {    
    //rc = cmErrMsg(err,kOpFailAtRC,"Gold code generation failed.  Insuffient balanced pairs.");
    retFl = false;
  }
  
  cmMemFree(mls0V);
  cmMemFree(mls1V);
  cmMemFree(xorV);

  return retFl;

}

bool  cmLFSR_Test()
{
  // lfsrN          = 5;   % 5    6    7;
  // poly_coeff0    = 0x12;  % 0x12 0x21 0x41;
  // poly_coeff1    = 0x1e;  % 0x1e 0x36 0x72;

  unsigned lfsrN = 7;
  unsigned pc0   = 0x41;
  unsigned pc1   = 0x72;
  unsigned mlsN    = (1 << lfsrN)-1;

  unsigned yN = mlsN*2;
  unsigned yV[ yN ];
  unsigned i;

  cmLFSR( lfsrN, pc0, 1 << (lfsrN-1), yV, yN );

  for(i=0; i<mlsN; ++i)
    if( yV[i] != yV[i+mlsN] )
      return false;

  //atVOU_PrintL(NULL,"0x12",yV,mlsN,2);

  cmLFSR( lfsrN, pc1, 1 << (lfsrN-1), yV, yN );

  //atVOU_PrintL(NULL,"0x17",yV,mlsN,2);

  for(i=0; i<mlsN; ++i)
    if( yV[i] != yV[i+mlsN] )
      return false;

  return true;
}
