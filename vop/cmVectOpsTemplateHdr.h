// \file cmVectOpsTemplateHdr.h
/// Vector operations interface.

/// Setting fieldWidth or decPltCnt to to negative values result in fieldWidth == 10 or decPlCnt == 4
void          VECT_OP_FUNC(Printf)( cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* dbp, unsigned fieldWidth, unsigned decPlCnt, const char* fmt, unsigned flags );
void          VECT_OP_FUNC(Print)(  cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* dbp );
void          VECT_OP_FUNC(PrintE)( cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* dbp );

void          VECT_OP_FUNC(PrintLf)( const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* dbp, unsigned fieldWidth, unsigned decPlCnt, const char* fmt );
void          VECT_OP_FUNC(PrintL)(  const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* dbp );
void          VECT_OP_FUNC(PrintLE)( const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const VECT_OP_TYPE* dbp );



/// Normalize the vector of proabilities by dividing through by the sum.
/// This leaves the relative proportions of each value unchanged while producing a total probability of 1.0.
VECT_OP_TYPE* VECT_OP_FUNC(NormalizeProbabilityVV)(VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp);
VECT_OP_TYPE* VECT_OP_FUNC(NormalizeProbability)(VECT_OP_TYPE* dbp, unsigned dn);
VECT_OP_TYPE* VECT_OP_FUNC(NormalizeProbabilityN)(VECT_OP_TYPE* dbp, unsigned dn, unsigned stride);

/// Standardize the columns of the matrix by subtracting the mean and dividing by the standard deviation.
/// uV[dcn] returns the mean of the data and is optional.
/// sdV[dcn] return the standard deviation of the data and is optional.
VECT_OP_TYPE* VECT_OP_FUNC(StandardizeRows)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, VECT_OP_TYPE* uV, VECT_OP_TYPE* sdV );
VECT_OP_TYPE* VECT_OP_FUNC(StandardizeCols)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, VECT_OP_TYPE* uV, VECT_OP_TYPE* sdV );

/// dbp[] = sbp<0 .* sbp 
/// Overlapping the source and dest is allowable as long as dbp <= sbp. 
VECT_OP_TYPE* VECT_OP_FUNC(HalfWaveRectify)(VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp );

/// Compute the cummulative sum of sbp[dn]. Equivalent to Matlab cumsum().
VECT_OP_TYPE* VECT_OP_FUNC(CumSum)(VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp ); 

VECT_OP_TYPE  VECT_OP_FUNC(Mean)(     const VECT_OP_TYPE* sp, unsigned sn );
VECT_OP_TYPE  VECT_OP_FUNC(MeanN)(    const VECT_OP_TYPE* sp, unsigned sn, unsigned stride  );

// Take the mean of each column/row of a matrix.
// Set 'dim' to 0 to return mean of columns else return mean of rows.
VECT_OP_TYPE*  VECT_OP_FUNC(MeanM)(    VECT_OP_TYPE* dp, const VECT_OP_TYPE* sp, unsigned srn, unsigned scn, unsigned dim );

// Find the mean of the  data points returned by srcFuncPtr(argPtr,i) and return it in dp[dim].
// 'dim' is both the size of dp[] and the length of each data point returned by srcFuncPtr().
// srcFuncPtr() will be called 'cnt' times but it may return NULL on some calls if the associated
// data point should not be included in the mean calculation.
VECT_OP_TYPE*  VECT_OP_FUNC(Mean2)(   VECT_OP_TYPE* dp, const VECT_OP_TYPE* (*srcFuncPtr)(void* arg, unsigned idx ), unsigned dim, unsigned cnt, void* argPtr );


// avgPtr is optional - set to NULL to compute the average
VECT_OP_TYPE  VECT_OP_FUNC(Variance)( const VECT_OP_TYPE* sp, unsigned sn, const VECT_OP_TYPE* avgPtr );
VECT_OP_TYPE  VECT_OP_FUNC(VarianceN)(const VECT_OP_TYPE* sp, unsigned sn, unsigned stride, const VECT_OP_TYPE* avgPtr );

// Set dim=0 to return variance of columns otherwise return variance or rows.
VECT_OP_TYPE*  VECT_OP_FUNC(VarianceM)(VECT_OP_TYPE* dp,  const VECT_OP_TYPE* sp, unsigned srn, unsigned scn, const VECT_OP_TYPE* avgPtr, unsigned dim );

// dp[] ./= max(dp). Returns the index of the max value.
unsigned  VECT_OP_FUNC(NormToMax)(    VECT_OP_TYPE* dp, unsigned dn );


VECT_OP_TYPE  VECT_OP_FUNC(AlphaNorm)(const VECT_OP_TYPE* sp, unsigned sn, VECT_OP_TYPE alpha );


// Calculate the sample covariance matrix from a set of Gaussian distributed multidimensional data.
// sp[dn,scn] is the data set.
// dn is the dimensionality of the data.
// scn is the count of data points
// up[dn] is an optional mean vector. If up == NULL then the mean of the data is calculated internally. 
// selIdxV[scn] can be used to select a subset of datapoints to process.
// If selIdxV[] is non-NULL then only columns where selIdxV[i]==selKey will be processed.
//
// dp[dn,dn] = covar( sp[dn,scn], u[dn] )
void  VECT_OP_FUNC(GaussCovariance)(VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sp, unsigned scn, const VECT_OP_TYPE* up, const unsigned* selIdxV, unsigned selKey );

// Calculate the sample covariance matrix.
// dp[ dn*dn ] - output matrix
// dn - dimensionality of the data
// srcFuncPtr - User defined function which is called to return a pointer to a data vector at index 'idx'.
//          The returned data vector must contain 'dn' elements.  The function should return NULL
//          if the data point associated with 'idx' should not be included in the covariance calculation.
// sn - count of data vectors 
// userPtr - User arg. passed to srcFuncPtr.    
// uV[ dn ] - mean of the data set (optional)
// Note that this function computes the covariance matrix in 2 serial passes (1 if the mean vector is given)
// through the 'sn' data points.  
// The result of this function are identical to the octave cov() function.
void  VECT_OP_FUNC(GaussCovariance2)(VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* (*srcFuncPtr)(void* userPtr, unsigned idx), unsigned sn, void* userPtr, const VECT_OP_TYPE* uV, const unsigned* selIdxV, unsigned selKey );

bool          VECT_OP_FUNC(Equal)(    const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn );

// Returns true if all values are 'normal' according the the C macro 'isnormal'.
// This function will return false if any of the values are zero.
bool          VECT_OP_FUNC(IsNormal)( const VECT_OP_TYPE* sp, unsigned sn );

// Returns true if all values are 'normal' or zero according the the C macro 'isnormal'.
// This function accepts zeros as normal.
bool          VECT_OP_FUNC(IsNormalZ)(const VECT_OP_TYPE* sp, unsigned sn );

// Set dp[dn] to the indexes of the non-normal numbers in sp[dn].
// Returns the count of indexes stored in dp[].
unsigned      VECT_OP_FUNC(FindNonNormal)( unsigned* dp, unsigned dn, const VECT_OP_TYPE* sp );
unsigned      VECT_OP_FUNC(FindNonNormalZ)( unsigned* dp, unsigned dn, const VECT_OP_TYPE* sp );


/// Successive call to to ZeroCrossCount should preserve the value pointed to by delaySmpPtr.
unsigned      VECT_OP_FUNC(ZeroCrossCount)( const VECT_OP_TYPE* sp, unsigned n, VECT_OP_TYPE* delaySmpPtr);

/// sn must be <= wndSmpCnt. If sn < wndSmpCnt then sp[sn] is treated as a
/// a partially filled buffer padded with wndSmpCnt-sn zeros.
/// rms = sqrt( sum(sp[1:sn] .* sp[1:sn]) / wndSmpCnt )
VECT_OP_TYPE  VECT_OP_FUNC(RMS)( const VECT_OP_TYPE* sp, unsigned sn, unsigned wndSmpCnt );

/// This function handles the case were sn is not an integer multiple of 
/// wndSmpCnt or hopSmpCnt.  In this case the function computes zero 
/// padded RMS values for windows which go past the end of sp[sn].
VECT_OP_TYPE* VECT_OP_FUNC(RmsV)( VECT_OP_TYPE* dp, unsigned dn, const VECT_OP_TYPE* sp, unsigned sn, unsigned wndSmpCnt, unsigned hopSmpCnt );


/// Return the magnitude (Euclidean Norm) of a vector.
VECT_OP_TYPE  VECT_OP_FUNC(EuclidNorm)( const VECT_OP_TYPE* sp, unsigned sn );

// Return the Itakura-Saito distance between a modelled power spectrum (up) and another power spectrum (sp).
VECT_OP_TYPE VECT_OP_FUNC(ItakuraDistance)( const VECT_OP_TYPE* up, const VECT_OP_TYPE* sp, unsigned sn );

/// Return the cosine distance between two vectors.
VECT_OP_TYPE  VECT_OP_FUNC(CosineDistance)( const VECT_OP_TYPE* s0P, const VECT_OP_TYPE* s1p, unsigned sn );

/// Return the Euclidean distance between two vectors
VECT_OP_TYPE  VECT_OP_FUNC(EuclidDistance)( const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn );

/// Return the Manhattan distance between two vectors
VECT_OP_TYPE  VECT_OP_FUNC(L1Distance)( const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn );

/// Return the Mahalanobis distance between a vector and the mean of the distribution.
/// The mean vector could be replaced with another vector drawn from the same distribution in which
/// case the returned value would reflect the distance between the two vectors.
/// 'sn' is the dimensionality of the data.
/// up[D] and invCovM[sn,sn] are the mean and inverse of the covariance matrix of the distribution from 
/// which sp[D] is drawn. 
VECT_OP_TYPE  VECT_OP_FUNC(MahalanobisDistance)( const VECT_OP_TYPE* sp, unsigned sn, const VECT_OP_TYPE* up, const VECT_OP_TYPE* invCovM );

/// Return the KL distance between two probability distributions up[sn] and sp[sn].
/// Since up[] and sp[] are probability distributions they must sum to 1.0.
VECT_OP_TYPE VECT_OP_FUNC(KL_Distance)( const VECT_OP_TYPE* up, const VECT_OP_TYPE* sp, unsigned sn );

/// Return the KL distance between a prototype vector up[sn] and another vector sp[sn].
/// This function first normalizes the two vectors to sum to 1.0 before calling
// VECT_OP_FUNC(KL_Distance)(up,sp,sn);
VECT_OP_TYPE VECT_OP_FUNC(KL_Distance2)( const VECT_OP_TYPE* up, const VECT_OP_TYPE* sp, unsigned sn );


/// Measure the Euclidean distance between a vector and all the columns in a matrix.
/// If dv[scn] is no NULL then return the Euclidean distance from sv[scn] to each column of sm[srn,scn].
/// The function returns the index of the closest data point (column) in sm[].
unsigned VECT_OP_FUNC(EuclidDistanceVM)( VECT_OP_TYPE* dv, const VECT_OP_TYPE* sv, const VECT_OP_TYPE* sm, unsigned srn, unsigned scn );



/// Measure the distance between each column in s0M[ rn, s0cn ] and 
/// each column in s1M[rn, s1cn ]. If dM is non-NULL store the 
/// result in dM[s1cn, s0cn]. The difference between s0M[:,0] and s1M[:,0] 
/// is stored in dM[0,0], the diff. between s0M[:,1] and s1M[:,1] is stored 
/// in dM[1,0], etc.  If mvV[s0cn] is non-NULL then  minV[i] is set with
/// the distance from s0M[:,i] to the nearest column in s1M[]. If miV[s0cn]
/// is non-NULL then it is set with the column index of s1M[] which is
/// closest to s0M[:,i].  In other words mvV[i] gives the distance to column
/// miV[i] from column s0M[:,i].
/// In those cases where the distane from a prototype (centroid) to the data point
/// is not the same as from the data point to the centroid then s1M[] is considered
/// to hold the prototypes and s0M[] is considered to hold the data points.
/// The distance function returns the distance from a prototype 'cV[dimN]' to
/// an datapoint dV[dimN]. 'dimN' is the dimensionality of the data vector
/// and is threfore equal to 'rn'.
void VECT_OP_FUNC(DistVMM)( 
  VECT_OP_TYPE*       dM,    // dM[s1cn,s0cn] return distance mtx (optional) 
  VECT_OP_TYPE*       mvV,   // mvV[s0cn] distance to closest data point in s0M[]. (optional)
  unsigned*           miV,   // miV[s0cn] column index into s1M[] of closest data point to s0M[:,i]. (optional)
  unsigned            rn,    // dimensionality of the data and the row count for s0M[] and s1M[] 
  const VECT_OP_TYPE* s0M,   // s0M[rn,s0cn] contains one data point per column
  unsigned            s0cn,  // count of data points (count of columns in s0M[]
  const VECT_OP_TYPE* s1M,   // s1M[rn,s1cn] contains one prototype per column
  unsigned            s1cn,  // count of prototypes (count of columns in s1m[]
  VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* cV, const VECT_OP_TYPE* dV, unsigned dimN ), 
  void*               userPtr );

/// Select 'selIdxN' columns from sM[srn,scn].
/// dM[srn,selIdxN] receives copies of the selected columns.
/// selIdxV[selIdxN] receives the column indexes of the selected columns.
/// Both dM[] and selIdxV[] are optional.
/// In each case the first selected point is chosen at random.
/// SelectRandom() then selects the following selIdxN-1 points at random.
/// SelectMaxDist() selects the next selIdxN-1 points by selecting 
/// the point whose combined distance to the previously selected points
/// is greatest. SelectMaxAvgDist() selectes the points whose combined
/// average distance is greatest relative the the previously selected
/// points.
void VECT_OP_FUNC(SelectRandom)(     VECT_OP_TYPE* dM, unsigned* selIdxV, unsigned selIdxN, const VECT_OP_TYPE* sM, unsigned srn, unsigned scn );
void VECT_OP_FUNC(SelectMaxDist)(    VECT_OP_TYPE* dM, unsigned* selIdxV, unsigned selIdxN, const VECT_OP_TYPE* sM, unsigned srn, unsigned scn, VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* s0V, const VECT_OP_TYPE* s1V, unsigned sn ), void* distUserPtr );
void VECT_OP_FUNC(SelectMaxAvgDist)( VECT_OP_TYPE* dM, unsigned* selIdxV, unsigned selIdxN, const VECT_OP_TYPE* sM, unsigned srn, unsigned scn, VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* s0V, const VECT_OP_TYPE* s1V, unsigned sn ), void* distUserPtr ); 

/// Return the sum of the products (dot product)
VECT_OP_TYPE VECT_OP_FUNC(MultSumVV)(  const VECT_OP_TYPE* s0p, const VECT_OP_TYPE* s1p, unsigned sn  );
VECT_OP_TYPE VECT_OP_FUNC(MultSumVS)(  const VECT_OP_TYPE* s0p, unsigned sn, VECT_OP_TYPE s );

/// Number of elements in the dest vector is expected to be the same 
/// as the number of source matrix rows.
/// mcn gives the number of columns in the source matrix which is 
//  expected to match the number of elements in the source vector.
/// dbp[dn,1] = mp[dn,mcn] * vp[mcn,1] 
VECT_OP_TYPE* VECT_OP_FUNC(MultVMV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* mp, unsigned mcn, const VECT_OP_TYPE* vp ); 

/// Multiply a row vector with a matrix to produce a row vector. 
/// dbp[1,dn] = v[1,vn] * m[vn,dn]
VECT_OP_TYPE* VECT_OP_FUNC(MultVVM)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* vp, unsigned vn, const VECT_OP_TYPE* mp ); 

/// Same as MultVMtV() except M is transposed as part of the multiply.  
/// mrn gives the number of rows in m[] and number of elements in vp[]
/// dpb[dn] = mp[mrn,dn] * vp[mrn]
VECT_OP_TYPE* VECT_OP_FUNC(MultVMtV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* mp, unsigned mrn, const VECT_OP_TYPE* vp ); 

/// Same as MultVMV() but where the matrix is diagonal.
VECT_OP_TYPE* VECT_OP_FUNC(MultDiagVMV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* mp, unsigned mcn, const VECT_OP_TYPE* vp ); 

/// Generalized matrix multiply.
/// If transposition is selected for M0 or M1 then the given dimension represent the size of the matrix 'after' the transposion. 
/// d[drn,dcn] = alpha * op(m0[drn,m0cn_m1rn]) * op(m1[m0cn_m1rn,dcn]) + beta * d[drn,dcn]
//// See enum { kTranpsoseM0Fl=0x01, kTransposeM1Fl=0x02 } in cmVectOps for flags.
VECT_OP_TYPE* VECT_OP_FUNC(MultMMM1)(VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, VECT_OP_TYPE alpha, const VECT_OP_TYPE* m0, const VECT_OP_TYPE* m1, unsigned m0cn_m1rn, VECT_OP_TYPE beta, unsigned flags );

/// Same a VECT_OP_FUNC(MultMMM1) except allows the operation on a sub-matrix by providing the physical (memory) row count rather than the logical (matrix) row count.
VECT_OP_TYPE* VECT_OP_FUNC(MultMMM2)(VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, VECT_OP_TYPE alpha, const VECT_OP_TYPE* m0, const VECT_OP_TYPE* m1, unsigned m0cn_m1rn, VECT_OP_TYPE beta, unsigned flags, unsigned dprn, unsigned m0prn, unsigned m1prn );

/// d[drn,dcn] = m0[drn,m0cn] * m1[m1rn,dcn] 
VECT_OP_TYPE* VECT_OP_FUNC(MultMMM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* m0, const VECT_OP_TYPE* m1, unsigned m0cn_m1rn );

/// same as MultMMM() except second source matrix is transposed prior to the multiply
VECT_OP_TYPE* VECT_OP_FUNC(MultMMMt)(VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* m0, const VECT_OP_TYPE* m1, unsigned m0cn_m1rn );
                                   

// Raise dbp[] to the power 'expon'
VECT_OP_TYPE* VECT_OP_FUNC(PowVS)(  VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE expon );
VECT_OP_TYPE* VECT_OP_FUNC(PowVVS)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp, VECT_OP_TYPE expon );

// Take the natural log of all values in sbp[dn]. It is allowable for sbp point to the same array as dbp=.
VECT_OP_TYPE* VECT_OP_FUNC(LogV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp );

// Convert a magnitude (amplitude) spectrum to/from decibels. 
// It is allowable for dbp==sbp.
VECT_OP_TYPE* VECT_OP_FUNC(AmplToDbVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE minDb );
VECT_OP_TYPE* VECT_OP_FUNC(DbToAmplVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp);

VECT_OP_TYPE* VECT_OP_FUNC(PowToDbVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE minDb );
VECT_OP_TYPE* VECT_OP_FUNC(DbToPowVV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp);

/// Initialize dbp[dn,dn] as a square symetric positive definite matrix using values
/// from a random uniform distribution. This is useful for initializing random 
/// covariance matrices as used by multivariate Gaussian distributions
/// If t is non-NULL it must point to a block of scratch memory of t[dn,dn].
/// If t is NULL then scratch memory is internally allocated and deallocated.
VECT_OP_TYPE* VECT_OP_FUNC(RandSymPosDef)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE* t );



/// Compute the determinant of any square matrix.
VECT_OP_TYPE  VECT_OP_FUNC(DetM)(    const VECT_OP_TYPE* sp, unsigned srn );

/// Compute the determinant of a diagonal matrix.
VECT_OP_TYPE  VECT_OP_FUNC(DetDiagM)(    const VECT_OP_TYPE* sp, unsigned srn);

/// Compute the log determinant of any square matrix.
VECT_OP_TYPE  VECT_OP_FUNC(LogDetM)(    const VECT_OP_TYPE* sp, unsigned srn );

/// Compute the log determinant of a diagonal matrix.
VECT_OP_TYPE  VECT_OP_FUNC(LogDetDiagM)(    const VECT_OP_TYPE* sp, unsigned srn);


/// Compute the inverse of a square matrix.  Returns NULL if the matrix is not invertable.
/// 'drn' is the dimensionality of the data.
VECT_OP_TYPE* VECT_OP_FUNC(InvM)(    VECT_OP_TYPE* dp, unsigned drn );

/// Compute the inverse of a diagonal matrix. Returns NULL if the matrix is not invertable.
VECT_OP_TYPE* VECT_OP_FUNC(InvDiagM)(    VECT_OP_TYPE* dp, unsigned drn );

/// Solve a linear system of the form AX=B where A[an,an] is square. 
/// Since A is square B must have 'an' rows.
/// Result is returned in B.
/// Returns a pointer to B on success or NULL on fail.
/// NOTE: Both A and B are overwritten by this operation.
VECT_OP_TYPE* VECT_OP_FUNC(SolveLS)( VECT_OP_TYPE* A, unsigned an, VECT_OP_TYPE* B, unsigned bcn ); 

/// Perform a Cholesky decomposition of the square symetric matrix U[un,un].
/// The factorization has the form: A=U'TU.
/// If the factorization is successful A is set to U and a pointer to A is returned.
/// Note that the lower triangle of A is not overwritten. See CholZ().
/// If the factorization fails NULL is returned.
VECT_OP_TYPE* VECT_OP_FUNC(Chol)(VECT_OP_TYPE* A, unsigned an );

/// Same as Chol() but sets the lower triangle of U to zero. 
/// This is equivalent ot the Matlab version.
VECT_OP_TYPE* VECT_OP_FUNC(CholZ)(VECT_OP_TYPE* U, unsigned un );


/// Return the average value of the contents of sbp[] between two fractional indexes
VECT_OP_TYPE  VECT_OP_FUNC(FracAvg)( double bi, double ei, const VECT_OP_TYPE* sbp, unsigned sn );

/// Shrinking function - Decrease the size of sbp[] by averaging blocks of values into single values in dbp[]
VECT_OP_TYPE* VECT_OP_FUNC(DownSampleAvg)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, unsigned sn );

/// Stretching function - linear interpolate between points in sbp[] to fill dbp[] ... where dn > sn
VECT_OP_TYPE* VECT_OP_FUNC(UpSampleInterp)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, unsigned sn );

/// Stretch or shrink the sbp[] to fit into dbp[]
VECT_OP_TYPE* VECT_OP_FUNC(FitToSize)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, unsigned sn );

/// Stretch or shrink sV[] to fit into dV[] using a simple linear mapping.
/// When stretching (sn<dn) each source element is repeated dn/sn times
/// and the last fraction position is interpolated.  When shrinking 
/// (sn>dn) each dest value is formed by the average of sequential segments
/// of sn/dn source elements. Fractional values are used at the beginning
/// and end of each segment.
VECT_OP_TYPE* VECT_OP_FUNC(LinearMap)(VECT_OP_TYPE* dV, unsigned dn, VECT_OP_TYPE* sV, unsigned sn );

/// Generate a vector of uniformly distributed random numbers in the range minVal to maxVal.
VECT_OP_TYPE* VECT_OP_FUNC(Random)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE minVal, VECT_OP_TYPE maxVal );

/// Generate dn random numbers integers between 0 and wn-1 based on a the relative 
/// weights in wp[wn].  Note thtat the weights do not have to sum to 1.0.
unsigned*  VECT_OP_FUNC(WeightedRandInt)( unsigned* dbp, unsigned dn, const VECT_OP_TYPE* wp, unsigned wn );

/// Generate a vector of normally distributed univariate random numbers 
VECT_OP_TYPE* VECT_OP_FUNC(RandomGauss)(  VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE mean, VECT_OP_TYPE var );

/// Generate a vector of normally distributed univariate random numbers where each value has been drawn from a 
/// seperately parameterized Gaussian distribution. meanV[] and varV[] must both contain dn velues.
VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussV)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* varV );

/// Generate a matrix of multi-dimensional random values. Each column represents a single vector value and each row contains a dimension.
/// meanV[] and varV[] must both contain drn elements where each meanV[i],varV[i] pair parameterize one dimensions Gaussian distribution.
VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* varV );
VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussDiagM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* diagCovarM );

/// Generate a matrix of multivariate random values drawn from a normal distribution.
/// The dimensionality of the values are 'drn'.
/// The count of returned values is 'dcn'.
/// meanV[drn] and covarM[drn,drn] parameterize the normal distribution.
/// The covariance matrix must be symetric and positive definite. 
/// t[(drn*drn) ] points to scratch memory or is set to NULL if the function should
///   allocate the memory internally.
/// Based on octave function mvrnd.m.
VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussNonDiagM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* covarM, VECT_OP_TYPE* t );

/// Same as RandomGaussNonDiagM() except requires the upper trianglular 
/// Cholesky factor of the covar matrix in 'uM'. 
VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussNonDiagM2)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* uM );


/// Generate a matrix of N*K multi-dimensional data points.
/// Where D is the dimensionality of the data. (D == drn).
/// K is the number of multi-dimensional PDF's (clusters).
/// N is the number of data points to generate per cluster. 
/// dbp[ D, N*K ] contains the returned data point. 
/// The first N columns is associated with the cluster 0, 
/// the next N columns is associated with cluster 1, ...
/// meanM[ D, K ] and varM[D,K] parameterize the generating PDF.s for each cluster
VECT_OP_TYPE* VECT_OP_FUNC(RandomGaussMM)( VECT_OP_TYPE* dbp, unsigned drn, unsigned dcn, const VECT_OP_TYPE* meanM, const VECT_OP_TYPE* varM, unsigned K );

/// Generate the set of coordinates which describe a circle with a center at x,y.
/// dbp[dn,2] must contain 2*dn elements.  The first column holds the x coord and and the second holds the y coord.
VECT_OP_TYPE* VECT_OP_FUNC(CircleCoords)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE x, VECT_OP_TYPE y, VECT_OP_TYPE varX, VECT_OP_TYPE varY );

/// The following functions all return the phase of the next value. 
unsigned      VECT_OP_FUNC(SynthSine)(      VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz );
unsigned      VECT_OP_FUNC(SynthCosine)(    VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz );
unsigned      VECT_OP_FUNC(SynthSquare)(    VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt );
unsigned      VECT_OP_FUNC(SynthTriangle)(  VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt );
unsigned      VECT_OP_FUNC(SynthSawtooth)(  VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt );
unsigned      VECT_OP_FUNC(SynthPulseCos)(  VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt );
unsigned      VECT_OP_FUNC(SynthImpulse)(   VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz );
unsigned      VECT_OP_FUNC(SynthPhasor)(    VECT_OP_TYPE* dbp, unsigned dn, unsigned phase, double srate, double hz ); 


/// Return value should be passed back via delaySmp on the next call.
VECT_OP_TYPE  VECT_OP_FUNC(SynthPinkNoise)( VECT_OP_TYPE* dbp, unsigned dn, VECT_OP_TYPE delaySmp );

VECT_OP_TYPE* VECT_OP_FUNC(LinearToDb)(    VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp, VECT_OP_TYPE mult );
VECT_OP_TYPE* VECT_OP_FUNC(dBToLinear)(    VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp, VECT_OP_TYPE mult );
VECT_OP_TYPE* VECT_OP_FUNC(AmplitudeToDb)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp );
VECT_OP_TYPE* VECT_OP_FUNC(PowerToDb)(     VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp );
VECT_OP_TYPE* VECT_OP_FUNC(dBToAmplitude)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp );
VECT_OP_TYPE* VECT_OP_FUNC(dBToPower)(     VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sp );

VECT_OP_TYPE  VECT_OP_FUNC(KaiserBetaFromSidelobeReject)(  double sidelobeRejectDb );
VECT_OP_TYPE  VECT_OP_FUNC(KaiserFreqResolutionFactor)( double sidelobeRejectDb );
VECT_OP_TYPE* VECT_OP_FUNC(Kaiser)(  VECT_OP_TYPE* dbp, unsigned dn, double beta );
VECT_OP_TYPE*	VECT_OP_FUNC(Gaussian)(VECT_OP_TYPE* dbp, unsigned dn, double mean, double variance );
VECT_OP_TYPE* VECT_OP_FUNC(Hamming)( VECT_OP_TYPE* dbp, unsigned dn );
VECT_OP_TYPE* VECT_OP_FUNC(Hann)(    VECT_OP_TYPE* dbp, unsigned dn );
VECT_OP_TYPE* VECT_OP_FUNC(Triangle)(VECT_OP_TYPE* dbp, unsigned dn );

/// The MATLAB equivalent Hamming and Hann windows.
//VECT_OP_TYPE* VECT_OP_FUNC(HammingMatlab)(VECT_OP_TYPE* dbp, unsigned dn );
VECT_OP_TYPE* VECT_OP_FUNC(HannMatlab)(   VECT_OP_TYPE* dbp, unsigned dn );

/// Simulates the MATLAB GaussWin function. Set arg to 2.5 to simulate the default arg
/// as used by MATLAB.
VECT_OP_TYPE*	VECT_OP_FUNC(GaussWin)( VECT_OP_TYPE* dbp, unsigned dn, double arg );


/// Direct form II algorithm based on the MATLAB implmentation
/// http://www.mathworks.com/access/helpdesk/help/techdoc/ref/filter.html#f83-1015962
/// The only difference between this function and the equivalent MATLAB filter() function
/// is that the first feedforward coeff is given as a seperate value. The first  b coefficient
///  in this function is therefore the same as the second coefficient in the MATLAB function.
///  and the first a[] coefficient (which is generally set to 1.0) is skipped.
///  Example:
///  Matlab:         b=[.5 .4 .3]  a=[1 .2 .1]
///  Equiv:  b0 = .5 b=[   .4 .3]  a=[  .2 .1]; 
///
/// y[yn] - output vector 
/// x[xn] - input vector. xn must be <= yn. if xn < yn then the end of y[] is set to zero.
/// b0 - signal scale. This can also be seen as b[0] (which is not included in b[])
/// b[dn] - feedforward coeff's b[1..dn-1]
/// a[dn] - feedback coeff's    a[1..dn-1]
/// d[dn+1] - delay registers - note that this array must be one element longer than the coeff arrays.
/// 
VECT_OP_TYPE* VECT_OP_FUNC(Filter)( VECT_OP_TYPE* y, unsigned yn, const VECT_OP_TYPE* x, unsigned xn, cmReal_t b0, const cmReal_t* b, const cmReal_t* a,  cmReal_t* d, unsigned dn );

struct cmFilter_str;
//typedef cmRC_t (*VECT_OP_FUNC(FiltExecFunc_t))( struct acFilter_str* f,  const VECT_OP_TYPE* x, unsigned xn, VECT_OP_TYPE* y, unsigned yn );
VECT_OP_TYPE* VECT_OP_FUNC(FilterFilter)(struct cmFilter_str* f, cmRC_t (*func)( struct cmFilter_str* f,  const VECT_OP_TYPE* x, unsigned xn, VECT_OP_TYPE* y, unsigned yn ), const cmReal_t bb[], unsigned bn, const cmReal_t aa[], unsigned an, const VECT_OP_TYPE* x, unsigned xn, VECT_OP_TYPE* y, unsigned yn );

/// Compute the coefficients of a low/high pass FIR filter
/// See enum { kHighPass_LPSincFl=0x01, kNormalize_LPSincFl=0x02 } in acVectOps.h
VECT_OP_TYPE* VECT_OP_FUNC(LP_Sinc)(VECT_OP_TYPE* dp, unsigned dn, double srate, double fcHz, unsigned flags );

/// Compute the complex transient detection function from successive spectral frames.
/// The spectral magntidue mag0V precedes mag1V and the phase (radians) spectrum phs0V precedes the phs1V which precedes phs2V.  
/// binCnt gives the length of each of the spectral vectors.
VECT_OP_TYPE  VECT_OP_FUNC(ComplexDetect)(const VECT_OP_TYPE* mag0V, const VECT_OP_TYPE* mag1V, const VECT_OP_TYPE* phs0V, const VECT_OP_TYPE* phs1V, const VECT_OP_TYPE* phs2V, unsigned binCnt );


/// Compute a set of filterCnt mel filter masks for wieghting magnitude spectra consisting of binCnt bins.
/// The spectrum is divided into bandCnt equal bands in the mel domain
/// Each row of the matrix contains the mask for a single filter band consisting of binCnt elements.  
/// See enum{ kShiftMelFl=0x01, kNormalizeMelFl=0x02 } in cmVectOps.h
/// Set kShiftMelFl to shift the mel bands onto the nearest FFT bin.
/// Set kNormalizeMelFl to normalize the combined filters for unity gain.
VECT_OP_TYPE* VECT_OP_FUNC(MelMask)( VECT_OP_TYPE* maskMtx, unsigned bandCnt, unsigned binCnt, double srate, unsigned flags );



/// Fill binIdxV[bandCnt] and cntV[bandCnt] with a bin to band map.
/// binIdx[] contains the first (minimum) bin index for a given band.
/// cntV[]   contains the count of bins for each band.
/// bandCnt is the number of bark bands to return 
/// The function returns the actual number of bands mapped which will always be <= 23.
unsigned VECT_OP_FUNC(BarkMap)(unsigned* binIdxV, unsigned* cntV, unsigned bandCnt, unsigned binCnt, double srate );

/// Calc a set of triangle fitler masks into each row of maskMtx.
/// maskMtx[ bandCnt, binCnt ] - result matrix
/// binHz - freq resolution of the output filters.
/// stSpread - Semi-tone spread above and below each center frequency (stSpread*2) is the total bandwidth. 
///            (Only used if 
/// lowHz[ bandCnt ] - set of upper frequency limits for each band.
/// ctrHz[ bandCnt ] set to the center value in Hz for each band
/// uprHz[ bandCnt ] - set of lower frequency limits for each band.
/// Note if lowHz[] and uprHz[] are set to NULL then stSpread is used to set the bandwidth of each band.
VECT_OP_TYPE* VECT_OP_FUNC(TriangleMask)(VECT_OP_TYPE* maskMtx, unsigned bandCnt, unsigned binCnt, const VECT_OP_TYPE* ctrHzV, VECT_OP_TYPE binHz, VECT_OP_TYPE stSpread, const VECT_OP_TYPE* lowHzV, const VECT_OP_TYPE* uprHzV );

/// Calculate a set of Bark band triangle filters into maskMtx.
/// Each row of maskMtx contains the filter for one band.
/// maskMtx[ bandCnt, binCnt ]
/// bandCnt - the number of triangle bankds. If bandCnt is > 24 it will be reduced to 24.
/// binCnt - the number of bins in the filters.
/// binHz - the width of each bin in Hz.
VECT_OP_TYPE* VECT_OP_FUNC(BarkMask)(VECT_OP_TYPE* maskMtx, unsigned bandCnt, unsigned binCnt, double binHz );

// Terhardt 1979 (Calculating virtual pitch, Hearing Research #1, pp 155-182)
// See enum { kNoTtmFlags=0, kModifiedTtmFl=0x01 } in cmVectOps.h
VECT_OP_TYPE* VECT_OP_FUNC(TerhardtThresholdMask)(VECT_OP_TYPE* maskV, unsigned binCnt,  double srate, unsigned flags);

//Schroeder et al., 1979, JASA, Optimizing digital speech coders by exploiting masking properties of the human ear
VECT_OP_TYPE* VECT_OP_FUNC(ShroederSpreadingFunc)(VECT_OP_TYPE* m, unsigned bandCnt, double srate);

/// Compute a set of DCT-II coefficients. Result dp[ coeffCnt, filtCnt ]
VECT_OP_TYPE* VECT_OP_FUNC(DctMatrix)( VECT_OP_TYPE* dp, unsigned coeffCnt, unsigned filtCnt );


/// Set the indexes of local peaks greater than threshold in dbp[].
/// Returns the number of peaks in dbp[]
/// The maximum number of peaks from n source values is max(0,floor((n-1)/2)).
/// Note that peaks will never be found at index 0 or index sn-1.
unsigned VECT_OP_FUNC(PeakIndexes)( unsigned* dbp, unsigned dn, const VECT_OP_TYPE* sbp, unsigned sn, VECT_OP_TYPE threshold );

/// Return the index of the bin containing v or acInvalidIdx if v is below sbp[0] or above sbp[ n-1 ]
/// The bin limits are contained in sbp[].
/// The value in spb[] are therefore expected to be in increasing order.
/// The value returned will be in the range 0:sn-1.
unsigned VECT_OP_FUNC(BinIndex)( const VECT_OP_TYPE* sbp, unsigned sn, VECT_OP_TYPE v );


/// Assign each data point to one of k clusters using an expectation-maximization algorithm.
/// k gives the number of clusters to identify
/// Each column of sp[ srn, scn ] contains a multidimensional data point. 
/// srn therefore defines the dimensionality of the data.
/// Each column of centroidV[ srn, k ] is set to the centroid of each of k clusters.
/// classIdxV[ scn ] assigns the index (0 to k-1) of a cluster to each soure data point
/// The function returns the number of iterations required for the EM process to converge.
/// selIdxV[ scn ] is optional and contains a list of id's assoc'd with each column of sM.
/// selKey is a integer value.
/// If selIdxV is non-NULL then only columns of sM[] where selIdxV[] == selKey will be clustered.
/// All columns of sM[] where the associated column in selIdxV[] do not match will be ignored.
/// Set 'initFromCentroidFl' to true if the initial centroids should be taken from centroidM[].
/// otherwise the initial centroids are selected from 'k' random data points in sp[].
/// The distance function distFunc(cV,dV,dN) is called to determine the distance from a 
/// centroid the centroid 'cV[dN]' to a data point 'dV[dN]'. 'dN' is the dimensionality of the
/// feature vector and is therefore equal to 'srn'.
unsigned VECT_OP_FUNC(Kmeans)( 
  unsigned*           classIdxV, 
  VECT_OP_TYPE*       centroidM, 
  unsigned            k, 
  const VECT_OP_TYPE* sp, 
  unsigned            srn, 
  unsigned            scn, 
  const unsigned*     selIdxV, 
  unsigned            selKey, 
  bool                initFromCentroidFl,  
  VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* cV, const VECT_OP_TYPE* dV, unsigned dN ), 
  void*               userDistPtr ); 

/// 'srcFunc() should return NULL if the data point located at 'frmIdx' should not be included in the clustering.
/// Clustering is considered to be complete after 'maxIterCnt' iterations or when
/// 'deltaStopCnt' or fewer data points change class on a single iteration
unsigned VECT_OP_FUNC(Kmeans2)( 
  unsigned*           classIdxV,         // classIdxV[scn] - data point class assignments
  VECT_OP_TYPE*       centroidM,         // centroidM[srn,K] - cluster centroids
  unsigned            K,                 // count of clusters
  const VECT_OP_TYPE* (*srcFunc)(void* userPtr, unsigned frmIdx ),
  unsigned            srn,               // dimensionality of each data point
  unsigned            scn,               // count of data points
  void*               userSrcPtr,        // callback data for srcFunc
  VECT_OP_TYPE (*distFunc)( void* userPtr, const VECT_OP_TYPE* cV, const VECT_OP_TYPE* dV, unsigned dN ), 
  void*              userDistPtr,        // arg. to distFunc()
  int                iterCnt,            // max. number of iterations (-1 to ignore)
  int                deltaStopCnt);      // if less than deltaStopCnt data points change classes on a given iteration then convergence occurs.

/// Evaluate the univariate normal distribution defined by 'mean' and 'stdDev'.
VECT_OP_TYPE* VECT_OP_FUNC(GaussPDF)( VECT_OP_TYPE* dbp, unsigned dn, const VECT_OP_TYPE* sbp, VECT_OP_TYPE mean, VECT_OP_TYPE stdDev );

/// Evaluate a multivariate normal distribution defined by meanV[D] and covarM[D,D]
/// at the data points held in the columns of xM[D,N]. Return the evaluation
/// results in the vector yV[N].  D is the dimensionality of the data. N is the number of
/// data points to evaluate and values to return in yV[N]. 
/// Set diagFl to true if covarM is diagonal.
/// The function fails and returns false if the covariance matrix is singular.
bool VECT_OP_FUNC(MultVarGaussPDF)( VECT_OP_TYPE* yV, const VECT_OP_TYPE* xM, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* covarM, unsigned D, unsigned N, bool diagFl );

/// Same as multVarGaussPDF[] except takes the inverse covar mtx invCovarM[D,D] 
/// and log determinant of covar mtx. 
/// Always returns yV[].
VECT_OP_TYPE* VECT_OP_FUNC(MultVarGaussPDF2)( VECT_OP_TYPE* yV, const VECT_OP_TYPE* xM, const VECT_OP_TYPE* meanV, const VECT_OP_TYPE* invCovarM, VECT_OP_TYPE logDet, unsigned D, unsigned N, bool diagFl );

/// Same as multVarGaussPDF[] except uses a function to obtain the data vectors.
/// srcFunc() can filter the data points by returning NULL if the data vector at frmIdx should
/// not be evaluated against the PDF. In this case yV[frmIdx] will be set to 0.
VECT_OP_TYPE* VECT_OP_FUNC(MultVarGaussPDF3)( 
  VECT_OP_TYPE*       yV, 
  const VECT_OP_TYPE* (*srcFunc)(void* funcDataPtr, unsigned frmIdx ),
  void*               funcDataPtr,
  const VECT_OP_TYPE* meanV, 
  const VECT_OP_TYPE* invCovarM, 
  VECT_OP_TYPE        logDet, 
  unsigned            D, 
  unsigned            N, 
  bool                diagFl );

/// Determine the most likely state sequece stateV[timeN] given a 
/// transition matrix a[stateN,stateN], 
/// observation probability matrix b[stateN,timeN] and 
/// initial state probability vector phi[stateN].  
/// a[i,j] is the probability of transitioning from state i to state j.
/// b[i,t] is the probability of state i emitting the obj t.
void VECT_OP_FUNC(DiscreteViterbi)(unsigned* stateV, unsigned timeN, unsigned stateN, const VECT_OP_TYPE* phi, const VECT_OP_TYPE* a, const VECT_OP_TYPE* b );


/// Clip the line defined by x0,y0 to x1,y1 into the rect defined by xMin,yMin xMax,yMax.
bool VECT_OP_FUNC(ClipLine)( VECT_OP_TYPE* x0, VECT_OP_TYPE* y0, VECT_OP_TYPE* x1, VECT_OP_TYPE* y1, VECT_OP_TYPE xMin, VECT_OP_TYPE yMin, VECT_OP_TYPE xMax, VECT_OP_TYPE yMax );

/// Return true if the line defined by x0,y0 to x1,y1 intersects with
/// the rectangle formed by xMin,yMin - xMax,yMax
bool VECT_OP_FUNC(IsLineInRect)( VECT_OP_TYPE x0, VECT_OP_TYPE y0, VECT_OP_TYPE x1, VECT_OP_TYPE y1, VECT_OP_TYPE xMin, VECT_OP_TYPE yMin, VECT_OP_TYPE xMax, VECT_OP_TYPE yMax );


/// Return the perpendicular distance from the line formed by x0,y0 and x1,y1
/// and the point px,py
VECT_OP_TYPE VECT_OP_FUNC(PtToLineDistance)( VECT_OP_TYPE x0, VECT_OP_TYPE y0, VECT_OP_TYPE x1, VECT_OP_TYPE y1, VECT_OP_TYPE px, VECT_OP_TYPE py);                              

/// Calculate the best fit line: b0 + b1*x_i through the points x_i,y_i.
/// Set x to NULL if it uses sequential integers [0,1,2,3...]
void VECT_OP_FUNC(Lsq1)(const VECT_OP_TYPE* x, const VECT_OP_TYPE* y, unsigned n, VECT_OP_TYPE* b0, VECT_OP_TYPE* b1 );



