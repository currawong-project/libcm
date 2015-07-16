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

/// Increment or decrement 'idx' by 'delta' always wrapping the result into the range
/// 0 to (maxN-1).
/// 'idx': initial value 
/// 'delta':  incremental amount
/// 'maxN' - 1 : maximum return value.
unsigned cmModIncr(int idx, int delta, int maxN );

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

//=================================================================
// Run a length 'lfsrN' linear feedback shift register (LFSR) for 'yN' iterations to
// produce a length 'yN' bit string in yV[yN].
// 'lfsrN' count of bits in the shift register range: 2<= lfsrN <= 32.
// 'tapMask' is a bit mask which gives the tap indexes positions for the LFSR. 
// The least significant bit corresponds to the maximum delay tap position.  
// The min tap position is therefore denoted by the tap mask bit location 1 << (lfsrN-1).
// A minimum of two taps must exist.
// 'seed' sets the initial delay state.
// 'yV[yN]' is the the output vector
// 'yN' is count of elements in yV.
// The function resturn kOkAtRC on success or kInvalidArgsRCRC if any arguments are invalid.
// /sa cmLFSR_Test.
void   cmLFSR( unsigned lfsrN, unsigned tapMask, unsigned seed, unsigned* yV, unsigned yN );

// Example and test code for cmLFSR() 
bool cmLFSR_Test();


// Generate a set of 'goldN' Gold codes using the Maximum Length Sequences (MLS) generated
// by a length 'lfsrN' linear feedback shift register.
// 'err' is an error object to be set if the the function fails.
// 'lfsrN' is the length of the Linear Feedback Shift Registers (LFSR) used to generate the MLS.
// 'poly_coeff0' tap mask for the first LFSR.
// 'coeff1' tap mask the the second LFSR.
// 'goldN' is the count of Gold codes to generate. 
// 'yM[mlsN', goldN] is a column major output matrix where each column contains a Gold code.
// 'mlsN' is the length of the maximum length sequence for each Gold code which can be
// calculated as mlsN = (1 << a->lfsrN) - 1.
// Note that values of 'lfsrN' and the 'poly_coeffx' must be carefully selected such that
// they will produce a MLS.  For example to generate a MLS with length 31 set 'lfsrN' to 5 and
// then select poly_coeff from two different elements of the set {0x12 0x14 0x17 0x1B 0x1D 0x1E}.
// See http://www.ece.cmu.edu/~koopman/lfsr/index.html for a complete set of MSL polynomial
// coefficients for given LFSR lengths.
// Returns false if insufficient balanced pairs exist.
bool   cmGenGoldCodes( unsigned lfsrN, unsigned poly_coeff0, unsigned poly_coeff1, unsigned goldN, int* yM, unsigned mlsN  );

#endif
