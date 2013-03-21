
VECT_OP_TYPE* VECT_OP_FUNC(Col)(    VECT_OP_TYPE* m, unsigned ci, unsigned rn, unsigned cn );
VECT_OP_TYPE* VECT_OP_FUNC(Row)(    VECT_OP_TYPE* m, unsigned ri, unsigned rn, unsigned cn );
VECT_OP_TYPE* VECT_OP_FUNC(ElePtr)( VECT_OP_TYPE* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn );
VECT_OP_TYPE  VECT_OP_FUNC(Ele)(    VECT_OP_TYPE* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn );
void          VECT_OP_FUNC(Set)(    VECT_OP_TYPE* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn, VECT_OP_TYPE v );

const VECT_OP_TYPE* VECT_OP_FUNC(CCol)(    const VECT_OP_TYPE* m, unsigned ci, unsigned rn, unsigned cn );
const VECT_OP_TYPE* VECT_OP_FUNC(CRow)(    const VECT_OP_TYPE* m, unsigned ri, unsigned rn, unsigned cn );
const VECT_OP_TYPE* VECT_OP_FUNC(CElePtr)( const VECT_OP_TYPE* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn );
VECT_OP_TYPE        VECT_OP_FUNC(CEle)(    const VECT_OP_TYPE* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn );



/// Fill a vector with a value. If value is 0 then the function is accellerated via memset().
VECT_OP_TYPE* VECT_OP_FUNC(Fill)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE value  );

// Fill a vector with zeros
VECT_OP_TYPE* VECT_OP_FUNC(Zero)( VECT_OP_TYPE* dbp, unsigned dn  );

// analogous to memmove()
VECT_OP_TYPE* VECT_OP_FUNC(Move)(  VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp );

/// Fill the vector from various sources
VECT_OP_TYPE* VECT_OP_FUNC(Copy)(  VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp );
VECT_OP_TYPE* VECT_OP_FUNC(CopyN)( VECT_OP_TYPE* dbp, unsigned dn, unsigned d_stride, const VECT_OP_TYPE* sp, unsigned s_stride );
VECT_OP_TYPE* VECT_OP_FUNC(CopyU)( VECT_OP_TYPE* dbp, unsigned dn, const unsigned* sp );
VECT_OP_TYPE* VECT_OP_FUNC(CopyI)( VECT_OP_TYPE* dbp, unsigned dn, const int* sp );
VECT_OP_TYPE* VECT_OP_FUNC(CopyF)( VECT_OP_TYPE* dbp, unsigned dn, const float* sp );
VECT_OP_TYPE* VECT_OP_FUNC(CopyD)( VECT_OP_TYPE* dbp, unsigned dn, const double* sp );
VECT_OP_TYPE* VECT_OP_FUNC(CopyS)( VECT_OP_TYPE* dbp, unsigned dn, const cmSample_t* sp );
VECT_OP_TYPE* VECT_OP_FUNC(CopyR)( VECT_OP_TYPE* dbp, unsigned dn, const cmReal_t* sp );

// Shrink the elemetns of dbp[dn] by copying all elements past t+tn to t.
// This operation results in overwriting the elements in the range t[tn].
// t[tn] must be entirely inside dbp[dn].
VECT_OP_TYPE* VECT_OP_FUNC(Shrink)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* t, unsigned tn );

// Expand dbp[[dn] by shifting all elements past t to t+tn.
// This produces a set of empty elements in t[tn].
// t must be inside or at the end of dbp[dn].
// This results in a reallocation of dbp[]. Be sure to call cmMemFree(dbp)
// to release the returned pointer.
VECT_OP_TYPE* VECT_OP_FUNC(Expand)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* t, unsigned tn );                                                            

// Replace the elements t[tn] with the elements in u[un]. 
// t must be inside or at the end of dbp[dn].
// This operation may result in a reallocation of dbp[]. Be sure to call cmMemFree(dbp)
// to release the returned pointer.
// IF dbp==NULL and tn==0 then the dbp[un] is allocated and returned 
// with the contents of u[un].
VECT_OP_TYPE* VECT_OP_FUNC(Replace)(VECT_OP_TYPE* dbp, unsigned* dn, const VECT_OP_TYPE* t, unsigned tn, const VECT_OP_TYPE* u, unsigned un );

/// Fill the the destination vector from a source vector where the source vector contains
/// srcStride interleaved elements to be ignored. 
VECT_OP_TYPE* VECT_OP_FUNC(CopyStride)(  VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp, unsigned srcStride );


/// Assuming a row vector positive shiftCnt rotates right, negative shiftCnt rotates left.
VECT_OP_TYPE* VECT_OP_FUNC(Rotate)( VECT_OP_TYPE* dbp, unsigned dn, int shiftCnt );

/// Equivalent to Matlab circshift().
VECT_OP_TYPE* VECT_OP_FUNC(RotateM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* sbp, int rShift, int cShift  );

/// Assuming a row vector positive shiftCnt shifts right, negative shiftCnt shifts left.
VECT_OP_TYPE* VECT_OP_FUNC(Shift)( VECT_OP_TYPE* dbp, unsigned dn, int shiftCnt, VECT_OP_TYPE fill );

/// Reverse the contents of the vector.
VECT_OP_TYPE* VECT_OP_FUNC(Flip)(  VECT_OP_TYPE* dbp, unsigned dn);



VECT_OP_TYPE* VECT_OP_FUNC(SubVS)(  VECT_OP_TYPE* dp, unsigned dn, VECT_OP_TYPE v );
VECT_OP_TYPE* VECT_OP_FUNC(SubVV)(  VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* v );
VECT_OP_TYPE* VECT_OP_FUNC(SubVVS)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* v, VECT_OP_TYPE s );
VECT_OP_TYPE* VECT_OP_FUNC(SubVVNN)(VECT_OP_TYPE* dp, unsigned dn, unsigned dnn, const VECT_OP_TYPE* sp, unsigned snn );
VECT_OP_TYPE* VECT_OP_FUNC(SubVVV)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sb0p, const VECT_OP_TYPE* sb1p );
VECT_OP_TYPE* VECT_OP_FUNC(SubVSV)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE  s0, const VECT_OP_TYPE* sb1p );

VECT_OP_TYPE* VECT_OP_FUNC(AddVS)(  VECT_OP_TYPE* dp, unsigned dn, VECT_OP_TYPE v );
VECT_OP_TYPE* VECT_OP_FUNC(AddVV)(  VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* v );
VECT_OP_TYPE* VECT_OP_FUNC(AddVVS)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* v, VECT_OP_TYPE s );
VECT_OP_TYPE* VECT_OP_FUNC(AddVVNN)(VECT_OP_TYPE* dp, unsigned dn, unsigned dnn, const VECT_OP_TYPE* sp, unsigned snn );
VECT_OP_TYPE* VECT_OP_FUNC(AddVVV)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sb0p, const VECT_OP_TYPE* sb1p );

VECT_OP_TYPE* VECT_OP_FUNC(MultVVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p, const VECT_OP_TYPE* sb1p );
VECT_OP_TYPE* VECT_OP_FUNC(MultVV)(  VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp );
VECT_OP_TYPE* VECT_OP_FUNC(MultVVNN)(VECT_OP_TYPE* dp,  unsigned dn, unsigned dnn, const VECT_OP_TYPE* sp, unsigned snn );
VECT_OP_TYPE* VECT_OP_FUNC(MultVS)(  VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE s );
VECT_OP_TYPE* VECT_OP_FUNC(MultVVS)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE s );
VECT_OP_TYPE* VECT_OP_FUNC(MultVaVS)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE s );
VECT_OP_TYPE* VECT_OP_FUNC(MultSumVVS)(VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE s );

VECT_OP_TYPE* VECT_OP_FUNC(DivVVS)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p, VECT_OP_TYPE sb1 );
VECT_OP_TYPE* VECT_OP_FUNC(DivVVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p, const VECT_OP_TYPE* sb1p );
VECT_OP_TYPE* VECT_OP_FUNC(DivVV)(  VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p );
VECT_OP_TYPE* VECT_OP_FUNC(DivVVNN)(VECT_OP_TYPE* dp,  unsigned dn, unsigned dnn, const VECT_OP_TYPE* sp, unsigned snn );
VECT_OP_TYPE* VECT_OP_FUNC(DivVS)(  VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE s );
VECT_OP_TYPE* VECT_OP_FUNC(DivVSV)( VECT_OP_TYPE* dp,  unsigned dn, const VECT_OP_TYPE  s0, const VECT_OP_TYPE* sb1p );

// Set dest to 0 if denominator is 0.
VECT_OP_TYPE* VECT_OP_FUNC(DivVVVZ)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p, const VECT_OP_TYPE* sb1p );
VECT_OP_TYPE* VECT_OP_FUNC(DivVVZ)(  VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sb0p );

// Divide columns of dp[:,i] by each value in the source vector sp[i]. 
VECT_OP_TYPE*  VECT_OP_FUNC(DivMS)(    VECT_OP_TYPE* dp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* sp ); 


VECT_OP_TYPE  VECT_OP_FUNC(Sum)(      const VECT_OP_TYPE* sp, unsigned sn );
VECT_OP_TYPE  VECT_OP_FUNC(SumN)(     const VECT_OP_TYPE* sp, unsigned sn, unsigned stride );

// Sum the columns of sp[srn,scn] into dp[scn]. 
// dp[] is zeroed prior to computing the sum.
VECT_OP_TYPE*  VECT_OP_FUNC(SumM)(     const VECT_OP_TYPE* sp, unsigned srn, unsigned scn, VECT_OP_TYPE* dp );

// Sum the rows of sp[srn,scn] into dp[srn]
// dp[] is zeroed prior to computing the sum.
VECT_OP_TYPE*  VECT_OP_FUNC(SumMN)(    const VECT_OP_TYPE* sp, unsigned srn, unsigned scn, VECT_OP_TYPE* dp );

VECT_OP_TYPE*   VECT_OP_FUNC(Abs)(   VECT_OP_TYPE* dbp, unsigned dn );


VECT_OP_TYPE  VECT_OP_FUNC(Median)(   const VECT_OP_TYPE* sp, unsigned sn );
unsigned      VECT_OP_FUNC(MinIndex)( const VECT_OP_TYPE* sp, unsigned sn, unsigned stride );
unsigned      VECT_OP_FUNC(MaxIndex)( const VECT_OP_TYPE* sp, unsigned sn, unsigned stride );
VECT_OP_TYPE  VECT_OP_FUNC(Min)(      const VECT_OP_TYPE* sp, unsigned sn, unsigned stride );
VECT_OP_TYPE  VECT_OP_FUNC(Max)(      const VECT_OP_TYPE* sp, unsigned sn, unsigned stride );

VECT_OP_TYPE*  VECT_OP_FUNC(MinVV)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sp );
VECT_OP_TYPE*  VECT_OP_FUNC(MaxVV)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sp );


/// Return index of max/min value into dp[scn] of each column of sp[srn,scn]
unsigned*  VECT_OP_FUNC(MinIndexM)( unsigned* dp, const VECT_OP_TYPE* sp, unsigned srn, unsigned scn );
unsigned*  VECT_OP_FUNC(MaxIndexM)( unsigned* dp, const VECT_OP_TYPE* sp, unsigned srn, unsigned scn );

/// Return true if s0p[sn] is equal to s1p[sn]
bool       VECT_OP_FUNC(IsEqual)( const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn );

/// Return true if all elements of s0p[sn] are within 'pct' percent of s1p[sn].
bool VECT_OP_FUNC(IsClose)( const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn, double pct );

/// Return the most frequently occuring element in sp.
VECT_OP_TYPE  VECT_OP_FUNC(Mode)(     const VECT_OP_TYPE* sp, unsigned sn );

/// Replace all values <= lteKeyVal with replaceVal.  sp==dp is legal.
VECT_OP_TYPE* VECT_OP_FUNC(ReplaceLte)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sp, VECT_OP_TYPE lteKeyVal, VECT_OP_TYPE replaceVal );

/// Return the index of 'key' in sp[sn] or cmInvalidIdx if 'key' does not exist.
unsigned VECT_OP_FUNC(Find)( const VECT_OP_TYPE* sp, unsigned sn, VECT_OP_TYPE key );

/// Count the number of times 'key' occurs in sp[sn].
unsigned VECT_OP_FUNC(Count)(const VECT_OP_TYPE* sp, unsigned sn, VECT_OP_TYPE key );

/// Set only the diagonal of a square mtx to sbp.
VECT_OP_TYPE* VECT_OP_FUNC(Diag)( VECT_OP_TYPE* dbp, unsigned n, const VECT_OP_TYPE* sbp );

/// Set the diagonal of a square mtx to db to sbp and set all other values to zero.
VECT_OP_TYPE* VECT_OP_FUNC(DiagZ)( VECT_OP_TYPE* dbp, unsigned n, const VECT_OP_TYPE* sbp ); 

/// Create an identity matrix (only sets 1's not zeros).
VECT_OP_TYPE* VECT_OP_FUNC(Identity)(  VECT_OP_TYPE* dbp, unsigned rn, unsigned cn );

/// Zero the matrix and then fill it as an identity matrix.
VECT_OP_TYPE* VECT_OP_FUNC(IdentityZ)( VECT_OP_TYPE* dbp, unsigned rn, unsigned cn );

/// Transpose the matrix sbp[srn,scn] into dbp[scn,srn]
VECT_OP_TYPE* VECT_OP_FUNC(Transpose)(  VECT_OP_TYPE* dbp, const VECT_OP_TYPE* sbp, unsigned srn, unsigned scn );

/// Fill dbp[] with a sequence of values. Returns next value. 
VECT_OP_TYPE  VECT_OP_FUNC(Seq)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE beg, VECT_OP_TYPE incr );


/// Apply a median or other filter of order wndN to xV[xN] and store the result in yV[xN].
/// When the window goes off either side of the vector the window is shortened.
/// This algorithm produces the same result as the fn_thresh function in MATLAB fv codebase.
void VECT_OP_FUNC(FnThresh)( const VECT_OP_TYPE* xV, unsigned xN, unsigned wndN, VECT_OP_TYPE* yV, unsigned yStride, VECT_OP_TYPE (*fnPtr)(const VECT_OP_TYPE*, unsigned) );


/// Apply a median filter of order wndN  to xV[xN] and store the result in yV[xN].
/// When the window goes off either side of the vector the missing elements are considered
/// to be 0.
/// This algorithm produces the same result as the MATLAB medfilt1() function.
void VECT_OP_FUNC(MedianFilt)( const VECT_OP_TYPE* xV, unsigned xN, unsigned wndN, VECT_OP_TYPE* yV, unsigned yStride );



/// Allocate and initialize a matrix for use by LevEditDist().
/// This matrix can be released with a call to cmMemFree().
unsigned* VECT_OP_FUNC(LevEditDistAllocMtx)(unsigned mtxMaxN);

/// Return the Levenshtein edit distance between two vectors.
/// m must point to a matrix pre-allocated by VECT_OP_FUNC(InitiLevEditDistMtx)(maxN).
double  VECT_OP_FUNC(LevEditDist)(unsigned mtxMaxN, unsigned* m, const VECT_OP_TYPE* s0, int n0, const VECT_OP_TYPE* s1, int n1, unsigned maxN );

/// Return the Levenshtein edit distance between two vectors.
/// Edit distance with a max cost threshold. This version of the algorithm
/// will run faster than LevEditDist() because it will stop execution as soon
/// as the distance exceeds 'maxCost'.
/// 'maxCost' must be between 0.0 and 1.0 or it is forced into this range.
/// The maximum distance returned will be 'maxCost'.
/// m must point to a matrix pre-allocated by VECT_OP_FUNC(InitiLevEditDistMtx)(maxN).
double VECT_OP_FUNC(LevEditDistWithCostThresh)( int mtxMaxN, unsigned* m, const VECT_OP_TYPE* s0, int n0, const VECT_OP_TYPE* s1, int n1, double maxCost, unsigned maxN );
