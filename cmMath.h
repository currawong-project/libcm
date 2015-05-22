#ifndef cmMath_h
#define cmMath_h

double   cmX80ToDouble( unsigned char s[10] );
void     cmDoubleToX80( double v, unsigned char s[10] );

bool     cmIsPowerOfTwo(   unsigned i );
unsigned cmNextPowerOfTwo( unsigned i );
unsigned cmNearPowerOfTwo( unsigned i );

bool     cmIsOddU(    unsigned v );
bool     cmIsEvenU(   unsigned v );
unsigned cmNextOddU(  unsigned v );
unsigned cmPrevOddU(  unsigned v );
unsigned cmNextEvenU( unsigned v );
unsigned cmPrevEvenU( unsigned v );


// modified bessel function of first kind, order 0
// ref: orfandis appendix B io.m
double   cmBessel0( double x );


//=================================================================
// The following elliptic-related function approximations come from
// Parks & Burrus, Digital Filter Design, Appendix program 9, pp. 317-326
// which in turn draws directly on other sources

// calculate complete elliptic integral (quarter period) K
// given *complimentary* modulus kc
cmReal_t cmEllipK( cmReal_t kc );

// calculate elliptic modulus k
// given ratio of complete elliptic integrals r = K/K'
// (solves the "degree equation" for fixed N = K*K1'/K'K1)
cmReal_t cmEllipDeg( cmReal_t r );

// calculate arc elliptic tangent u (elliptic integral of the 1st kind)
// given argument x = sc(u,k) and *complimentary* modulus kc
cmReal_t cmEllipArcSc( cmReal_t x, cmReal_t kc );

// calculate Jacobi elliptic functions sn, cn, and dn
// given argument u and *complimentary* modulus kc
cmRC_t   cmEllipJ( cmReal_t u, cmReal_t kc, cmReal_t* sn, cmReal_t* cn, cmReal_t* dn );


//=================================================================
// bilinear transform
// z = (2*sr + s)/(2*sr - s)
cmRC_t cmBlt( unsigned n, cmReal_t sr, cmReal_t* rp, cmReal_t* ip );


//=================================================================
// Pitch conversion
unsigned cmHzToMidi( double hz );
float    cmMidiToHz( unsigned midi );

//=================================================================
// Floating point byte swapping
unsigned           cmFfSwapFloatToUInt( float v );
float              cmFfSwapUIntToFloat( unsigned v );
unsigned long long cmFfSwapDoubleToULLong( double v );
double             cmFfSwapULLongToDouble( unsigned long long v );

//=================================================================
int      cmRandInt( int min, int max );
unsigned cmRandUInt( unsigned min, unsigned max );
float    cmRandFloat( float min, float max );
double   cmRandDouble( double min, double max );

//=================================================================
bool cmIsCloseD( double   x0, double   x1, double eps );
bool cmIsCloseF( float    x0, float    x1, double eps );
bool cmIsCloseI( int      x0, int      x1, double eps );
bool cmIsCloseU( unsigned x0, unsigned x1, double eps );

#endif
