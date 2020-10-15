//( { file_desc:"Math vector operations." kw:[vop math] }
//)
//( { label:misc desc:"Miscellaneous vector operations." kw:[vop] }

// Compute the cummulative sum of sbp[dn]. Equivalent to Matlab cumsum().
T_t* cmVOT_CumSum(T_t* dbp, unsigned dn, const T_t* sbp ); 

// Returns true if all values in each vector are equal.
bool          cmVOT_Equal(    const T_t* s0p, const T_t* s1p, unsigned sn );

// Same as Matlab linspace() v[i] = i * (limit-1)/n
T_t*  cmVOT_LinSpace( T_t* dbp, unsigned dn, T_t base, T_t limit );

//======================================================================================================================
//)


//( { label:Print desc:"Vector printing functions." kw:[vop] }
// Setting fieldWidth or decPltCnt to to negative values result in fieldWidth == 10 or decPlCnt == 4
//
void          cmVOT_Printf( cmRpt_t* rpt, unsigned rn, unsigned cn, const T_t* dbp, int fieldWidth, int decPlCnt, const char* fmt, unsigned flags );
void          cmVOT_Print(  cmRpt_t* rpt, unsigned rn, unsigned cn, const T_t* dbp );
void          cmVOT_PrintE( cmRpt_t* rpt, unsigned rn, unsigned cn, const T_t* dbp );

void          cmVOT_PrintLf( const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const T_t* dbp, unsigned fieldWidth, unsigned decPlCnt, const char* fmt );
void          cmVOT_PrintL(  const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const T_t* dbp );
void          cmVOT_PrintLE( const char* label, cmRpt_t* rpt, unsigned rn, unsigned cn, const T_t* dbp );
//======================================================================================================================
//)


//( { label:Normalization desc:"Normalization and standardization functions." kw:[vop] }

// Normalize the vector of proabilities by dividing through by the sum.
// This leaves the relative proportions of each value unchanged while producing a total probability of 1.0.
//
T_t* cmVOT_NormalizeProbabilityVV(T_t* dbp, unsigned dn, const T_t* sbp);
T_t* cmVOT_NormalizeProbability(T_t* dbp, unsigned dn);
T_t* cmVOT_NormalizeProbabilityN(T_t* dbp, unsigned dn, unsigned stride);
//
// Standardize the columns of the matrix by subtracting the mean and dividing by the standard deviation.
// uV[dcn] returns the mean of the data and is optional.
// sdV[dcn] return the standard deviation of the data and is optional.
T_t* cmVOT_StandardizeRows( T_t* dbp, unsigned drn, unsigned dcn, T_t* uV, T_t* sdV );
T_t* cmVOT_StandardizeCols( T_t* dbp, unsigned drn, unsigned dcn, T_t* uV, T_t* sdV );
//
// Normalize by dividing through by the max. value.
// dp[] ./= max(dp). Returns the index of the max value.
unsigned  cmVOT_NormToMax(    T_t* dp, unsigned dn );
//
// Normalize by dividing through by the max. absolute value.
// db[] .*= fact / abs(max(dp));
unsigned cmVOT_NormToAbsMax(   T_t* dp, unsigned dn, T_t fact );
//======================================================================================================================
//)


//( { label:"Mean and variance" desc:"Compute mean and variance." kw:[vop] }

T_t  cmVOT_Mean(     const T_t* sp, unsigned sn );
T_t  cmVOT_MeanN(    const T_t* sp, unsigned sn, unsigned stride  );
//
// Take the mean of each column/row of a matrix.
// Set 'dim' to 0 to return mean of columns else return mean of rows.
T_t*  cmVOT_MeanM(    T_t* dp, const T_t* sp, unsigned srn, unsigned scn, unsigned dim );
//
// Take the mean of the first 'cnt' element of each column/row of a matrix.
// Set 'dim' to 0 to return mean of columns else return mean of rows.
// If 'cnt' is greater than the number of elements in the column/row then 'cnt' is
// reduced to the number of elements in the column/row.
T_t*  cmVOT_MeanM2(    T_t* dp, const T_t* sp, unsigned srn, unsigned scn, unsigned dim, unsigned cnt );
//
// Find the mean of the  data points returned by srcFuncPtr(argPtr,i) and return it in dp[dim].
// 'dim' is both the size of dp[] and the length of each data point returned by srcFuncPtr().
// srcFuncPtr() will be called 'cnt' times but it may return NULL on some calls if the associated
// data point should not be included in the mean calculation.
T_t*  cmVOT_Mean2(   T_t* dp, const T_t* (*srcFuncPtr)(void* arg, unsigned idx ), unsigned dim, unsigned cnt, void* argPtr );
//
// avgPtr is optional - set to NULL to compute the average
T_t  cmVOT_Variance( const T_t* sp, unsigned sn, const T_t* avgPtr );
T_t  cmVOT_VarianceN(const T_t* sp, unsigned sn, unsigned stride, const T_t* avgPtr );
//
// Set dim=0 to return variance of columns otherwise return variance or rows.
T_t*  cmVOT_VarianceM(T_t* dp,  const T_t* sp, unsigned srn, unsigned scn, const T_t* avgPtr, unsigned dim );
//======================================================================================================================
//)


//( { label:"Covariance" desc:"Matrix covariance" kw:[vop] }

// Calculate the sample covariance matrix from a set of Gaussian distributed multidimensional data.
// sp[dn,scn] is the data set.
// dn is the dimensionality of the data.
// scn is the count of data points
// up[dn] is an optional mean vector. If up == NULL then the mean of the data is calculated internally. 
// selIdxV[scn] can be used to select a subset of datapoints to process.
// If selIdxV[] is non-NULL then only columns where selIdxV[i]==selKey will be processed.
//
// dp[dn,dn] = covar( sp[dn,scn], u[dn] )
void  cmVOT_GaussCovariance(T_t* dp, unsigned dn, const T_t* sp, unsigned scn, const T_t* up, const unsigned* selIdxV, unsigned selKey );

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
void  cmVOT_GaussCovariance2(T_t* dp, unsigned dn, const T_t* (*srcFuncPtr)(void* userPtr, unsigned idx), unsigned sn, void* userPtr, const T_t* uV, const unsigned* selIdxV, unsigned selKey );
//======================================================================================================================
//)

//( { label:"Float point normal" desc:"Evaluate the 'normalness of floating point values." kw:[vop] }

// Returns true if all values are 'normal' according the the C macro 'isnormal'.
// This function will return false if any of the values are zero.
bool          cmVOT_IsNormal( const T_t* sp, unsigned sn );

// Returns true if all values are 'normal' or zero according the the C macro 'isnormal'.
// This function accepts zeros as normal.
bool          cmVOT_IsNormalZ(const T_t* sp, unsigned sn );

// Set dp[dn] to the indexes of the non-normal numbers in sp[dn].
// Returns the count of indexes stored in dp[].
unsigned      cmVOT_FindNonNormal( unsigned* dp, unsigned dn, const T_t* sp );
unsigned      cmVOT_FindNonNormalZ( unsigned* dp, unsigned dn, const T_t* sp );
//======================================================================================================================
//)


//( { label:"Measure" desc:"Measure features of a vector." kw:[vop] }

// Successive call to to ZeroCrossCount should preserve the value pointed to by delaySmpPtr.
unsigned      cmVOT_ZeroCrossCount( const T_t* sp, unsigned n, T_t* delaySmpPtr);

// Calculuate the sum of the squares of all elements in bp[bn]. 
T_t cmVOT_SquaredSum( const T_t* bp, unsigned bn );

// sn must be <= wndSmpCnt. If sn < wndSmpCnt then sp[sn] is treated as a
// a partially filled buffer padded with wndSmpCnt-sn zeros.
// rms = sqrt( sum(sp[1:sn] .* sp[1:sn]) / wndSmpCnt )
T_t  cmVOT_RMS( const T_t* sp, unsigned sn, unsigned wndSmpCnt );

// This function handles the case were sn is not an integer multiple of 
// wndSmpCnt or hopSmpCnt.  In this case the function computes zero 
// padded RMS values for windows which go past the end of sp[sn].
T_t* cmVOT_RmsV( T_t* dp, unsigned dn, const T_t* sp, unsigned sn, unsigned wndSmpCnt, unsigned hopSmpCnt );

// Return the magnitude (Euclidean Norm) of a vector.
T_t  cmVOT_EuclidNorm( const T_t* sp, unsigned sn );

T_t  cmVOT_AlphaNorm(const T_t* sp, unsigned sn, T_t alpha );

//======================================================================================================================
//)



//( { label:"Distance" desc:"Calculate various vector distances." kw:[vop] }

// Return the Itakura-Saito distance between a modelled power spectrum (up) and another power spectrum (sp).
T_t cmVOT_ItakuraDistance( const T_t* up, const T_t* sp, unsigned sn );

// Return the cosine distance between two vectors.
T_t  cmVOT_CosineDistance( const T_t* s0P, const T_t* s1p, unsigned sn );

// Return the Euclidean distance between two vectors
T_t  cmVOT_EuclidDistance( const T_t* s0p, const T_t* s1p, unsigned sn );

// Return the Manhattan distance between two vectors
T_t  cmVOT_L1Distance( const T_t* s0p, const T_t* s1p, unsigned sn );

// Return the Mahalanobis distance between a vector and the mean of the distribution.
// The mean vector could be replaced with another vector drawn from the same distribution in which
// case the returned value would reflect the distance between the two vectors.
// 'sn' is the dimensionality of the data.
// up[D] and invCovM[sn,sn] are the mean and inverse of the covariance matrix of the distribution from 
// which sp[D] is drawn. 
T_t  cmVOT_MahalanobisDistance( const T_t* sp, unsigned sn, const T_t* up, const T_t* invCovM );

// Return the KL distance between two probability distributions up[sn] and sp[sn].
// Since up[] and sp[] are probability distributions they must sum to 1.0.
T_t cmVOT_KL_Distance( const T_t* up, const T_t* sp, unsigned sn );

// Return the KL distance between a prototype vector up[sn] and another vector sp[sn].
// This function first normalizes the two vectors to sum to 1.0 before calling
// cmVOT_KL_Distance(up,sp,sn);
T_t cmVOT_KL_Distance2( const T_t* up, const T_t* sp, unsigned sn );


// Measure the Euclidean distance between a vector and all the columns in a matrix.
// If dv[scn] is no NULL then return the Euclidean distance from sv[scn] to each column of sm[srn,scn].
// The function returns the index of the closest data point (column) in sm[].
unsigned cmVOT_EuclidDistanceVM( T_t* dv, const T_t* sv, const T_t* sm, unsigned srn, unsigned scn );

// Measure the distance between each column in s0M[ rn, s0cn ] and 
// each column in s1M[rn, s1cn ]. If dM is non-NULL store the 
// result in dM[s1cn, s0cn]. The difference between s0M[:,0] and s1M[:,0] 
// is stored in dM[0,0], the diff. between s0M[:,1] and s1M[:,1] is stored 
// in dM[1,0], etc.  If mvV[s0cn] is non-NULL then  minV[i] is set with
// the distance from s0M[:,i] to the nearest column in s1M[]. If miV[s0cn]
// is non-NULL then it is set with the column index of s1M[] which is
// closest to s0M[:,i].  In other words mvV[i] gives the distance to column
// miV[i] from column s0M[:,i].
// In those cases where the distane from a prototype (centroid) to the data point
// is not the same as from the data point to the centroid then s1M[] is considered
// to hold the prototypes and s0M[] is considered to hold the data points.
// The distance function returns the distance from a prototype 'cV[dimN]' to
// an datapoint dV[dimN]. 'dimN' is the dimensionality of the data vector
// and is threfore equal to 'rn'.
void cmVOT_DistVMM( 
  T_t*       dM,    // dM[s1cn,s0cn] return distance mtx (optional) 
  T_t*       mvV,   // mvV[s0cn] distance to closest data point in s0M[]. (optional)
  unsigned*           miV,   // miV[s0cn] column index into s1M[] of closest data point to s0M[:,i]. (optional)
  unsigned            rn,    // dimensionality of the data and the row count for s0M[] and s1M[] 
  const T_t* s0M,   // s0M[rn,s0cn] contains one data point per column
  unsigned            s0cn,  // count of data points (count of columns in s0M[]
  const T_t* s1M,   // s1M[rn,s1cn] contains one prototype per column
  unsigned            s1cn,  // count of prototypes (count of columns in s1m[]
  T_t (*distFunc)( void* userPtr, const T_t* cV, const T_t* dV, unsigned dimN ), 
  void*               userPtr );

//======================================================================================================================
//)

//( { label:"Select columns" desc:"Select columns based on distance." kw:[vop] }

// Select 'selIdxN' columns from sM[srn,scn].
// dM[srn,selIdxN] receives copies of the selected columns.
// selIdxV[selIdxN] receives the column indexes of the selected columns.
// Both dM[] and selIdxV[] are optional.
// In each case the first selected point is chosen at random.
// SelectRandom() then selects the following selIdxN-1 points at random.
// SelectMaxDist() selects the next selIdxN-1 points by selecting 
// the point whose combined distance to the previously selected points
// is greatest. SelectMaxAvgDist() selectes the points whose combined
// average distance is greatest relative the the previously selected
// points.
void cmVOT_SelectRandom(     T_t* dM, unsigned* selIdxV, unsigned selIdxN, const T_t* sM, unsigned srn, unsigned scn );
void cmVOT_SelectMaxDist(    T_t* dM, unsigned* selIdxV, unsigned selIdxN, const T_t* sM, unsigned srn, unsigned scn, T_t (*distFunc)( void* userPtr, const T_t* s0V, const T_t* s1V, unsigned sn ), void* distUserPtr );
void cmVOT_SelectMaxAvgDist( T_t* dM, unsigned* selIdxV, unsigned selIdxN, const T_t* sM, unsigned srn, unsigned scn, T_t (*distFunc)( void* userPtr, const T_t* s0V, const T_t* s1V, unsigned sn ), void* distUserPtr ); 

//======================================================================================================================
//)

//( { label:"Matrix multiplication" desc:"Various matrix multiplication operations." kw:[vop] }

// Return the sum of the products (dot product)
T_t cmVOT_MultSumVV(  const T_t* s0p, const T_t* s1p, unsigned sn  );
T_t cmVOT_MultSumVS(  const T_t* s0p, unsigned sn, T_t s );

// Number of elements in the dest vector is expected to be the same 
// as the number of source matrix rows.
// mcn gives the number of columns in the source matrix which is 
//  expected to match the number of elements in the source vector.
// dbp[dn,1] = mp[dn,mcn] * vp[mcn,1] 
T_t* cmVOT_MultVMV( T_t* dbp, unsigned dn, const T_t* mp, unsigned mcn, const T_t* vp ); 

// Multiply a row vector with a matrix to produce a row vector. 
// dbp[1,dn] = v[1,vn] * m[vn,dn]
T_t* cmVOT_MultVVM( T_t* dbp, unsigned dn, const T_t* vp, unsigned vn, const T_t* mp ); 

// Same as MultVMtV() except M is transposed as part of the multiply.  
// mrn gives the number of rows in m[] and number of elements in vp[]
// dpb[dn] = mp[mrn,dn] * vp[mrn]
T_t* cmVOT_MultVMtV( T_t* dbp, unsigned dn, const T_t* mp, unsigned mrn, const T_t* vp ); 

// Same as MultVMV() but where the matrix is diagonal.
T_t* cmVOT_MultDiagVMV( T_t* dbp, unsigned dn, const T_t* mp, unsigned mcn, const T_t* vp ); 

// Generalized matrix multiply.
// If transposition is selected for M0 or M1 then the given dimension represent the size of the matrix 'after' the transposion. 
// d[drn,dcn] = alpha * op(m0[drn,m0cn_m1rn]) * op(m1[m0cn_m1rn,dcn]) + beta * d[drn,dcn]
/// See enum { kTranpsoseM0Fl=0x01, kTransposeM1Fl=0x02 } in cmVectOps for flags.
T_t* cmVOT_MultMMM1(T_t* dbp, unsigned drn, unsigned dcn, T_t alpha, const T_t* m0, const T_t* m1, unsigned m0cn_m1rn, T_t beta, unsigned flags );

// Same a cmVOT_MultMMM1 except allows the operation on a sub-matrix by providing the physical (memory) row count rather than the logical (matrix) row count.
T_t* cmVOT_MultMMM2(T_t* dbp, unsigned drn, unsigned dcn, T_t alpha, const T_t* m0, const T_t* m1, unsigned m0cn_m1rn, T_t beta, unsigned flags, unsigned dprn, unsigned m0prn, unsigned m1prn );

// d[drn,dcn] = m0[drn,m0cn] * m1[m1rn,dcn] 
T_t* cmVOT_MultMMM( T_t* dbp, unsigned drn, unsigned dcn, const T_t* m0, const T_t* m1, unsigned m0cn_m1rn );

// same as MultMMM() except second source matrix is transposed prior to the multiply
T_t* cmVOT_MultMMMt(T_t* dbp, unsigned drn, unsigned dcn, const T_t* m0, const T_t* m1, unsigned m0cn_m1rn );

//======================================================================================================================
//)

//( { label:"Linear algebra" desc:"Miscellaneous linear algebra operations. Determinant, Inversion, Cholesky decompostion. Linear system solver." kw:[vop] }

// Initialize dbp[dn,dn] as a square symetric positive definite matrix using values
// from a random uniform distribution. This is useful for initializing random 
// covariance matrices as used by multivariate Gaussian distributions
// If t is non-NULL it must point to a block of scratch memory of t[dn,dn].
// If t is NULL then scratch memory is internally allocated and deallocated.
T_t* cmVOT_RandSymPosDef( T_t* dbp, unsigned dn, T_t* t );


// Compute the determinant of any square matrix.
T_t  cmVOT_DetM(    const T_t* sp, unsigned srn );

// Compute the determinant of a diagonal matrix.
T_t  cmVOT_DetDiagM(    const T_t* sp, unsigned srn);

// Compute the log determinant of any square matrix.
T_t  cmVOT_LogDetM(    const T_t* sp, unsigned srn );

// Compute the log determinant of a diagonal matrix.
T_t  cmVOT_LogDetDiagM(    const T_t* sp, unsigned srn);


// Compute the inverse of a square matrix.  Returns NULL if the matrix is not invertable.
// 'drn' is the dimensionality of the data.
T_t* cmVOT_InvM(    T_t* dp, unsigned drn );

// Compute the inverse of a diagonal matrix. Returns NULL if the matrix is not invertable.
T_t* cmVOT_InvDiagM(    T_t* dp, unsigned drn );

// Solve a linear system of the form AX=B where A[an,an] is square. 
// Since A is square B must have 'an' rows.
// Result is returned in B.
// Returns a pointer to B on success or NULL on fail.
// NOTE: Both A and B are overwritten by this operation.
T_t* cmVOT_SolveLS( T_t* A, unsigned an, T_t* B, unsigned bcn ); 

// Perform a Cholesky decomposition of the square symetric matrix U[un,un].
// The factorization has the form: A=U'TU.
// If the factorization is successful A is set to U and a pointer to A is returned.
// Note that the lower triangle of A is not overwritten. See CholZ().
// If the factorization fails NULL is returned.
T_t* cmVOT_Chol(T_t* A, unsigned an );

// Same as Chol() but sets the lower triangle of U to zero. 
// This is equivalent ot the Matlab version.
T_t* cmVOT_CholZ(T_t* U, unsigned un );

// Calculate the best fit line: b0 + b1*x_i through the points x_i,y_i.
// Set x to NULL if it uses sequential integers [0,1,2,3...]
void cmVOT_Lsq1(const T_t* x, const T_t* y, unsigned n, T_t* b0, T_t* b1 );


//======================================================================================================================
//)

//( { label:"Stretch/Shrink" desc:"Stretch or shrink a vector by resampling." kw:[vop] }

// Return the average value of the contents of sbp[] between two fractional indexes
T_t  cmVOT_FracAvg( double bi, double ei, const T_t* sbp, unsigned sn );

// Shrinking function - Decrease the size of sbp[] by averaging blocks of values into single values in dbp[]
T_t* cmVOT_DownSampleAvg( T_t* dbp, unsigned dn, const T_t* sbp, unsigned sn );

// Stretching function - linear interpolate between points in sbp[] to fill dbp[] ... where dn > sn
T_t* cmVOT_UpSampleInterp( T_t* dbp, unsigned dn, const T_t* sbp, unsigned sn );

// Stretch or shrink the sbp[] to fit into dbp[]
T_t* cmVOT_FitToSize( T_t* dbp, unsigned dn, const T_t* sbp, unsigned sn );

// Stretch or shrink sV[] to fit into dV[] using a simple linear mapping.
// When stretching (sn<dn) each source element is repeated dn/sn times
// and the last fraction position is interpolated.  When shrinking 
// (sn>dn) each dest value is formed by the average of sequential segments
// of sn/dn source elements. Fractional values are used at the beginning
// and end of each segment.
T_t* cmVOT_LinearMap(T_t* dV, unsigned dn, T_t* sV, unsigned sn );

//======================================================================================================================
//)

//( { label:"Random number generation" desc:"Generate random numbers." kw:[vop] }

// Generate a vector of uniformly distributed random numbers in the range minVal to maxVal.
T_t* cmVOT_Random( T_t* dbp, unsigned dn, T_t minVal, T_t maxVal );

// Generate dn random numbers integers between 0 and wn-1 based on a the relative 
// weights in wp[wn].  Note thtat the weights do not have to sum to 1.0.
unsigned*  cmVOT_WeightedRandInt( unsigned* dbp, unsigned dn, const T_t* wp, unsigned wn );

// Generate a vector of normally distributed univariate random numbers 
T_t* cmVOT_RandomGauss(  T_t* dbp, unsigned dn, T_t mean, T_t var );

// Generate a vector of normally distributed univariate random numbers where each value has been drawn from a 
// seperately parameterized Gaussian distribution. meanV[] and varV[] must both contain dn velues.
T_t* cmVOT_RandomGaussV( T_t* dbp, unsigned dn, const T_t* meanV, const T_t* varV );

// Generate a matrix of multi-dimensional random values. Each column represents a single vector value and each row contains a dimension.
// meanV[] and varV[] must both contain drn elements where each meanV[i],varV[i] pair parameterize one dimensions Gaussian distribution.
T_t* cmVOT_RandomGaussM( T_t* dbp, unsigned drn, unsigned dcn, const T_t* meanV, const T_t* varV );
T_t* cmVOT_RandomGaussDiagM( T_t* dbp, unsigned drn, unsigned dcn, const T_t* meanV, const T_t* diagCovarM );

// Generate a matrix of multivariate random values drawn from a normal distribution.
// The dimensionality of the values are 'drn'.
// The count of returned values is 'dcn'.
// meanV[drn] and covarM[drn,drn] parameterize the normal distribution.
// The covariance matrix must be symetric and positive definite. 
// t[(drn*drn) ] points to scratch memory or is set to NULL if the function should
//   allocate the memory internally.
// Based on octave function mvrnd.m.
T_t* cmVOT_RandomGaussNonDiagM( T_t* dbp, unsigned drn, unsigned dcn, const T_t* meanV, const T_t* covarM, T_t* t );

// Same as RandomGaussNonDiagM() except requires the upper trianglular 
// Cholesky factor of the covar matrix in 'uM'. 
T_t* cmVOT_RandomGaussNonDiagM2( T_t* dbp, unsigned drn, unsigned dcn, const T_t* meanV, const T_t* uM );


// Generate a matrix of N*K multi-dimensional data points.
// Where D is the dimensionality of the data. (D == drn).
// K is the number of multi-dimensional PDF's (clusters).
// N is the number of data points to generate per cluster. 
// dbp[ D, N*K ] contains the returned data point. 
// The first N columns is associated with the cluster 0, 
// the next N columns is associated with cluster 1, ...
// meanM[ D, K ] and varM[D,K] parameterize the generating PDF.s for each cluster
T_t* cmVOT_RandomGaussMM( T_t* dbp, unsigned drn, unsigned dcn, const T_t* meanM, const T_t* varM, unsigned K );


// Evaluate the univariate normal distribution defined by 'mean' and 'stdDev'.
T_t* cmVOT_GaussPDF( T_t* dbp, unsigned dn, const T_t* sbp, T_t mean, T_t stdDev );

// Evaluate a multivariate normal distribution defined by meanV[D] and covarM[D,D]
// at the data points held in the columns of xM[D,N]. Return the evaluation
// results in the vector yV[N].  D is the dimensionality of the data. N is the number of
// data points to evaluate and values to return in yV[N]. 
// Set diagFl to true if covarM is diagonal.
// The function fails and returns false if the covariance matrix is singular.
bool cmVOT_MultVarGaussPDF( T_t* yV, const T_t* xM, const T_t* meanV, const T_t* covarM, unsigned D, unsigned N, bool diagFl );

// Same as multVarGaussPDF[] except takes the inverse covar mtx invCovarM[D,D] 
// and log determinant of covar mtx. 
// Always returns yV[].
T_t* cmVOT_MultVarGaussPDF2( T_t* yV, const T_t* xM, const T_t* meanV, const T_t* invCovarM, T_t logDet, unsigned D, unsigned N, bool diagFl );

// Same as multVarGaussPDF[] except uses a function to obtain the data vectors.
// srcFunc() can filter the data points by returning NULL if the data vector at frmIdx should
// not be evaluated against the PDF. In this case yV[frmIdx] will be set to 0.
T_t* cmVOT_MultVarGaussPDF3( 
  T_t*       yV, 
  const T_t* (*srcFunc)(void* funcDataPtr, unsigned frmIdx ),
  void*               funcDataPtr,
  const T_t* meanV, 
  const T_t* invCovarM, 
  T_t        logDet, 
  unsigned            D, 
  unsigned            N, 
  bool                diagFl );


//======================================================================================================================
//)


//( { label:"Signal generators" desc:"Generate periodic signals." kw:[vop] }

// The following functions all return the phase of the next value. 
unsigned      cmVOT_SynthSine(      T_t* dbp, unsigned dn, unsigned phase, double srate, double hz );
unsigned      cmVOT_SynthCosine(    T_t* dbp, unsigned dn, unsigned phase, double srate, double hz );
unsigned      cmVOT_SynthSquare(    T_t* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt );
unsigned      cmVOT_SynthTriangle(  T_t* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt );
unsigned      cmVOT_SynthSawtooth(  T_t* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt );
unsigned      cmVOT_SynthPulseCos(  T_t* dbp, unsigned dn, unsigned phase, double srate, double hz, unsigned otCnt );
unsigned      cmVOT_SynthImpulse(   T_t* dbp, unsigned dn, unsigned phase, double srate, double hz );
unsigned      cmVOT_SynthPhasor(    T_t* dbp, unsigned dn, unsigned phase, double srate, double hz ); 


// Return value should be passed back via delaySmp on the next call.
T_t  cmVOT_SynthPinkNoise( T_t* dbp, unsigned dn, T_t delaySmp );

//======================================================================================================================
//)

//( { label:"Exponential conversion" desc:"pow() and log() functions." kw:[vop] }

// Raise dbp[] to the power 'expon'
T_t* cmVOT_PowVS(  T_t* dbp, unsigned dn, T_t expon );
T_t* cmVOT_PowVVS( T_t* dbp, unsigned dn, const T_t* sp, T_t expon );

// Take the natural log of all values in sbp[dn]. It is allowable for sbp point to the same array as dbp=.
T_t* cmVOT_LogV( T_t* dbp, unsigned dn, const T_t* sbp );

//======================================================================================================================
//)

//( { label:"dB Conversions" desc:"Convert vectors between dB,linear and power representations." kw:[vop] }

// Convert a magnitude (amplitude) spectrum to/from decibels. 
// It is allowable for dbp==sbp.
T_t* cmVOT_AmplToDbVV( T_t* dbp, unsigned dn, const T_t* sbp, T_t minDb );
T_t* cmVOT_DbToAmplVV( T_t* dbp, unsigned dn, const T_t* sbp);

T_t* cmVOT_PowToDbVV( T_t* dbp, unsigned dn, const T_t* sbp, T_t minDb );
T_t* cmVOT_DbToPowVV( T_t* dbp, unsigned dn, const T_t* sbp);

T_t* cmVOT_LinearToDb(    T_t* dbp, unsigned dn, const T_t* sp, T_t mult );
T_t* cmVOT_dBToLinear(    T_t* dbp, unsigned dn, const T_t* sp, T_t mult );
T_t* cmVOT_AmplitudeToDb( T_t* dbp, unsigned dn, const T_t* sp );
T_t* cmVOT_PowerToDb(     T_t* dbp, unsigned dn, const T_t* sp );
T_t* cmVOT_dBToAmplitude( T_t* dbp, unsigned dn, const T_t* sp );
T_t* cmVOT_dBToPower(     T_t* dbp, unsigned dn, const T_t* sp );
//======================================================================================================================
//)

//( { label:"DSP Windows" desc:"DSP windowing functions." kw:[vop] }

T_t  cmVOT_KaiserBetaFromSidelobeReject(  double sidelobeRejectDb );
T_t  cmVOT_KaiserFreqResolutionFactor( double sidelobeRejectDb );
T_t* cmVOT_Kaiser(  T_t* dbp, unsigned dn, double beta );
T_t*	cmVOT_Gaussian(T_t* dbp, unsigned dn, double mean, double variance );
T_t* cmVOT_Hamming( T_t* dbp, unsigned dn );
T_t* cmVOT_Hann(    T_t* dbp, unsigned dn );
T_t* cmVOT_Triangle(T_t* dbp, unsigned dn );

// The MATLAB equivalent Hamming and Hann windows.
//T_t* cmVOT_HammingMatlab(T_t* dbp, unsigned dn );
T_t* cmVOT_HannMatlab(   T_t* dbp, unsigned dn );

// Simulates the MATLAB GaussWin function. Set arg to 2.5 to simulate the default arg
// as used by MATLAB.
T_t*	cmVOT_GaussWin( T_t* dbp, unsigned dn, double arg );
//======================================================================================================================
//)

//( { label:"DSP Filters" desc:"DSP filtering functions." kw:[vop] }

// Direct form II algorithm based on the MATLAB implmentation
// http://www.mathworks.com/access/helpdesk/help/techdoc/ref/filter.html#f83-1015962
// The only difference between this function and the equivalent MATLAB filter() function
// is that the first feedforward coeff is given as a seperate value. The first  b coefficient
//  in this function is therefore the same as the second coefficient in the MATLAB function.
//  and the first a[] coefficient (which is generally set to 1.0) is skipped.
//  Example:
//  Matlab:         b=[.5 .4 .3]  a=[1 .2 .1]
//  Equiv:  b0 = .5 b=[   .4 .3]  a=[  .2 .1]; 
//
// y[yn] - output vector 
// x[xn] - input vector. xn must be <= yn. if xn < yn then the end of y[] is set to zero.
// b0 - signal scale. This can also be seen as b[0] (which is not included in b[])
// b[dn] - feedforward coeff's b[1..dn-1]
// a[dn] - feedback coeff's    a[1..dn-1]
// d[dn+1] - delay registers - note that this array must be one element longer than the coeff arrays.
// 
T_t* cmVOT_Filter( T_t* y, unsigned yn, const T_t* x, unsigned xn, cmReal_t b0, const cmReal_t* b, const cmReal_t* a,  cmReal_t* d, unsigned dn );

struct cmFilter_str;
//typedef cmRC_t (*cmVOT_FiltExecFunc_t)( struct acFilter_str* f,  const T_t* x, unsigned xn, T_t* y, unsigned yn );
T_t* cmVOT_FilterFilter(struct cmFilter_str* f, cmRC_t (*func)( struct cmFilter_str* f,  const T_t* x, unsigned xn, T_t* y, unsigned yn ), const cmReal_t bb[], unsigned bn, const cmReal_t aa[], unsigned an, const T_t* x, unsigned xn, T_t* y, unsigned yn );

// Compute the coefficients of a low/high pass FIR filter
// wndV[dn] gives the window function used to truncate the ideal low-pass impulse response.
// Set wndV to NULL to use a unity window.
// See enum { kHighPass_LPSincFl=0x01, kNormalize_LPSincFl=0x02 } in cmVectOps.h
T_t* cmVOT_LP_Sinc(T_t* dp, unsigned dn, const T_t* wndV, double srate, double fcHz, unsigned flags );



//======================================================================================================================
//)

//( { label:"Spectral Masking" desc:"A collection of spectral masking functions." kw:[vop] }

// Compute a set of filterCnt mel filter masks for wieghting magnitude spectra consisting of binCnt bins.
// The spectrum is divided into bandCnt equal bands in the mel domain
// Each row of the matrix contains the mask for a single filter band consisting of binCnt elements.  
// See enum{ kShiftMelFl=0x01, kNormalizeMelFl=0x02 } in cmVectOps.h
// Set kShiftMelFl to shift the mel bands onto the nearest FFT bin.
// Set kNormalizeMelFl to normalize the combined filters for unity gain.
T_t* cmVOT_MelMask( T_t* maskMtx, unsigned bandCnt, unsigned binCnt, double srate, unsigned flags );

// Fill binIdxV[bandCnt] and cntV[bandCnt] with a bin to band map.
// binIdx[] contains the first (minimum) bin index for a given band.
// cntV[]   contains the count of bins for each band.
// bandCnt is the number of bark bands to return 
// The function returns the actual number of bands mapped which will always be <= 23.
unsigned cmVOT_BarkMap(unsigned* binIdxV, unsigned* cntV, unsigned bandCnt, unsigned binCnt, double srate );

// Calc a set of triangle fitler masks into each row of maskMtx.
// maskMtx[ bandCnt, binCnt ] - result matrix
// binHz - freq resolution of the output filters.
// stSpread - Semi-tone spread above and below each center frequency (stSpread*2) is the total bandwidth. 
//            (Only used if lowHzV or uprHzV are NULL)
// lowHz[ bandCnt ] - set of upper frequency limits for each band.
// ctrHz[ bandCnt ] set to the center value in Hz for each band
// uprHz[ bandCnt ] - set of lower frequency limits for each band.
// Note if lowHz[] and uprHz[] are set to NULL then stSpread is used to set the bandwidth of each band.
T_t* cmVOT_TriangleMask(T_t* maskMtx, unsigned bandCnt, unsigned binCnt, const T_t* ctrHzV, T_t binHz, T_t stSpread, const T_t* lowHzV, const T_t* uprHzV );

// Calculate a set of Bark band triangle filters into maskMtx.
// Each row of maskMtx contains the filter for one band.
// maskMtx[ bandCnt, binCnt ]
// bandCnt - the number of triangle bankds. If bandCnt is > 24 it will be reduced to 24.
// binCnt - the number of bins in the filters.
// binHz - the width of each bin in Hz.
T_t* cmVOT_BarkMask(T_t* maskMtx, unsigned bandCnt, unsigned binCnt, double binHz );

// Terhardt 1979 (Calculating virtual pitch, Hearing Research #1, pp 155-182)
// See enum { kNoTtmFlags=0, kModifiedTtmFl=0x01 } in cmVectOps.h
T_t* cmVOT_TerhardtThresholdMask(T_t* maskV, unsigned binCnt,  double srate, unsigned flags);

//Schroeder et al., 1979, JASA, Optimizing digital speech coders by exploiting masking properties of the human ear
T_t* cmVOT_ShroederSpreadingFunc(T_t* m, unsigned bandCnt, double srate);

//======================================================================================================================
//)

//( { label:"Machine learning" desc:"K-means clustering and Viterbi algorithms." kw:[vop] }

// Assign each data point to one of k clusters using an expectation-maximization algorithm.
// k gives the number of clusters to identify
// Each column of sp[ srn, scn ] contains a multidimensional data point. 
// srn therefore defines the dimensionality of the data.
// Each column of centroidV[ srn, k ] is set to the centroid of each of k clusters.
// classIdxV[ scn ] assigns the index (0 to k-1) of a cluster to each soure data point
// The function returns the number of iterations required for the EM process to converge.
// selIdxV[ scn ] is optional and contains a list of id's assoc'd with each column of sM.
// selKey is a integer value.
// If selIdxV is non-NULL then only columns of sM[] where selIdxV[] == selKey will be clustered.
// All columns of sM[] where the associated column in selIdxV[] do not match will be ignored.
// Set 'initFromCentroidFl' to true if the initial centroids should be taken from centroidM[].
// otherwise the initial centroids are selected from 'k' random data points in sp[].
// The distance function distFunc(cV,dV,dN) is called to determine the distance from a 
// centroid the centroid 'cV[dN]' to a data point 'dV[dN]'. 'dN' is the dimensionality of the
// feature vector and is therefore equal to 'srn'.
unsigned cmVOT_Kmeans( 
  unsigned*           classIdxV, 
  T_t*       centroidM, 
  unsigned            k, 
  const T_t* sp, 
  unsigned            srn, 
  unsigned            scn, 
  const unsigned*     selIdxV, 
  unsigned            selKey, 
  bool                initFromCentroidFl,  
  T_t (*distFunc)( void* userPtr, const T_t* cV, const T_t* dV, unsigned dN ), 
  void*               userDistPtr ); 

// 'srcFunc() should return NULL if the data point located at 'frmIdx' should not be included in the clustering.
// Clustering is considered to be complete after 'maxIterCnt' iterations or when
// 'deltaStopCnt' or fewer data points change class on a single iteration
unsigned cmVOT_Kmeans2( 
  unsigned*           classIdxV,         // classIdxV[scn] - data point class assignments
  T_t*       centroidM,         // centroidM[srn,K] - cluster centroids
  unsigned            K,                 // count of clusters
  const T_t* (*srcFunc)(void* userPtr, unsigned frmIdx ),
  unsigned            srn,               // dimensionality of each data point
  unsigned            scn,               // count of data points
  void*               userSrcPtr,        // callback data for srcFunc
  T_t (*distFunc)( void* userPtr, const T_t* cV, const T_t* dV, unsigned dN ), 
  void*              userDistPtr,        // arg. to distFunc()
  int                iterCnt,            // max. number of iterations (-1 to ignore)
  int                deltaStopCnt);      // if less than deltaStopCnt data points change classes on a given iteration then convergence occurs.

// Determine the most likely state sequece stateV[timeN] given a 
// transition matrix a[stateN,stateN], 
// observation probability matrix b[stateN,timeN] and 
// initial state probability vector phi[stateN].  
// a[i,j] is the probability of transitioning from state i to state j.
// b[i,t] is the probability of state i emitting the obj t.
void cmVOT_DiscreteViterbi(unsigned* stateV, unsigned timeN, unsigned stateN, const T_t* phi, const T_t* a, const T_t* b );


//======================================================================================================================
//)

//( { label:"Graphics" desc:"Graphics related algorithms." kw:[vop] }

// Generate the set of coordinates which describe a circle with a center at x,y.
// dbp[dn,2] must contain 2*dn elements.  The first column holds the x coord and and the second holds the y coord.
T_t* cmVOT_CircleCoords( T_t* dbp, unsigned dn, T_t x, T_t y, T_t varX, T_t varY );

// Clip the line defined by x0,y0 to x1,y1 into the rect defined by xMin,yMin xMax,yMax.
bool cmVOT_ClipLine( T_t* x0, T_t* y0, T_t* x1, T_t* y1, T_t xMin, T_t yMin, T_t xMax, T_t yMax );

// Return true if the line defined by x0,y0 to x1,y1 intersects with
// the rectangle formed by xMin,yMin - xMax,yMax
bool cmVOT_IsLineInRect( T_t x0, T_t y0, T_t x1, T_t y1, T_t xMin, T_t yMin, T_t xMax, T_t yMax );


// Return the perpendicular distance from the line formed by x0,y0 and x1,y1
// and the point px,py
T_t cmVOT_PtToLineDistance( T_t x0, T_t y0, T_t x1, T_t y1, T_t px, T_t py);                              

//======================================================================================================================
//)

//( { label:"Miscellaneous DSP" desc:"Common DSP algorithms." kw:[vop] }

// Compute the complex transient detection function from successive spectral frames.
// The spectral magntidue mag0V precedes mag1V and the phase (radians) spectrum phs0V precedes the phs1V which precedes phs2V.  
// binCnt gives the length of each of the spectral vectors.
T_t  cmVOT_ComplexDetect(const T_t* mag0V, const T_t* mag1V, const T_t* phs0V, const T_t* phs1V, const T_t* phs2V, unsigned binCnt );

// Compute a set of DCT-II coefficients. Result dp[ coeffCnt, filtCnt ]
T_t* cmVOT_DctMatrix( T_t* dp, unsigned coeffCnt, unsigned filtCnt );


// Set the indexes of local peaks greater than threshold in dbp[].
// Returns the number of peaks in dbp[]
// The maximum number of peaks from n source values is max(0,floor((n-1)/2)).
// Note that peaks will never be found at index 0 or index sn-1.
unsigned cmVOT_PeakIndexes( unsigned* dbp, unsigned dn, const T_t* sbp, unsigned sn, T_t threshold );

// Return the index of the bin containing v otherwise return kInvalidIdx if v is below sbp[0] or above sbp[ n-1 ]
// The bin limits are contained in sbp[].
// The value in spb[] are therefore expected to be in increasing order.
// The value returned will be in the range 0:sn-1.
unsigned cmVOT_BinIndex( const T_t* sbp, unsigned sn, T_t v );


// Given the points x0[xy0N],y0[xy0N] fill y1[i] with the interpolated value of y0[] at
// x1[i].  Note that x0[] and x1[] must be increasing monotonic.
// This function is similar to the octave interp1() function.
void cmVOT_Interp1(T_t* y1, const T_t* x1, unsigned xy1N, const T_t* x0, const T_t* y0, unsigned xy0N );
 
//======================================================================================================================
//)


//( { label:"Matrix ops" desc:"Common 2D matrix operations and accessors." kw:[vop] }

// 2D matrix accessors
T_t* cmVOT_Col(    T_t* m, unsigned ci, unsigned rn, unsigned cn );
T_t* cmVOT_Row(    T_t* m, unsigned ri, unsigned rn, unsigned cn );
T_t* cmVOT_ElePtr( T_t* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn );
T_t  cmVOT_Ele(    T_t* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn );
void          cmVOT_Set(    T_t* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn, T_t v );

const T_t* cmVOT_CCol(    const T_t* m, unsigned ci, unsigned rn, unsigned cn );
const T_t* cmVOT_CRow(    const T_t* m, unsigned ri, unsigned rn, unsigned cn );
const T_t* cmVOT_CElePtr( const T_t* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn );
T_t        cmVOT_CEle(    const T_t* m, unsigned ri, unsigned ci, unsigned rn, unsigned cn );


// Set only the diagonal of a square mtx to sbp.
T_t* cmVOT_Diag( T_t* dbp, unsigned n, const T_t* sbp );

// Set the diagonal of a square mtx to db to sbp and set all other values to zero.
T_t* cmVOT_DiagZ( T_t* dbp, unsigned n, const T_t* sbp ); 

// Create an identity matrix (only sets 1's not zeros).
T_t* cmVOT_Identity(  T_t* dbp, unsigned rn, unsigned cn );

// Zero the matrix and then fill it as an identity matrix.
T_t* cmVOT_IdentityZ( T_t* dbp, unsigned rn, unsigned cn );

// Transpose the matrix sbp[srn,scn] into dbp[scn,srn]
T_t* cmVOT_Transpose(  T_t* dbp, const T_t* sbp, unsigned srn, unsigned scn );

//======================================================================================================================
//)


//( { label:"Fill,move,copy" desc:"Basic data movement and initialization." kw:[vop] }

// Fill a vector with a value. If value is 0 then the function is accellerated via memset().
T_t* cmVOT_Fill( T_t* dbp, unsigned dn, T_t value  );

// Fill a vector with zeros
T_t* cmVOT_Zero( T_t* dbp, unsigned dn  );

// Analogous to memmove()
T_t* cmVOT_Move(  T_t* dbp, unsigned dn, const T_t* sp );

// Fill the vector from various sources
T_t* cmVOT_Copy(  T_t* dbp, unsigned dn, const T_t* sp );
T_t* cmVOT_CopyN( T_t* dbp, unsigned dn, unsigned d_stride, const T_t* sp, unsigned s_stride );
T_t* cmVOT_CopyU( T_t* dbp, unsigned dn, const unsigned* sp );
T_t* cmVOT_CopyI( T_t* dbp, unsigned dn, const int* sp );
T_t* cmVOT_CopyF( T_t* dbp, unsigned dn, const float* sp );
T_t* cmVOT_CopyD( T_t* dbp, unsigned dn, const double* sp );
T_t* cmVOT_CopyS( T_t* dbp, unsigned dn, const cmSample_t* sp );
T_t* cmVOT_CopyR( T_t* dbp, unsigned dn, const cmReal_t* sp );

// Fill the the destination vector from a source vector where the source vector contains
// srcStride interleaved elements to be ignored. 
T_t* cmVOT_CopyStride(  T_t* dbp, unsigned dn, const T_t* sp, unsigned srcStride );


//======================================================================================================================
//)

//( { label:"Shrink/Expand/Replace" desc:"Change the size of a vector." kw:[vop] }


// Shrink the elemetns of dbp[dn] by copying all elements past t+tn to t.
// This operation results in overwriting the elements in the range t[tn].
// t[tn] must be entirely inside dbp[dn].
T_t* cmVOT_Shrink( T_t* dbp, unsigned dn, const T_t* t, unsigned tn );

// Expand dbp[[dn] by shifting all elements past t to t+tn.
// This produces a set of empty elements in t[tn].
// t must be inside or at the end of dbp[dn].
// This results in a reallocation of dbp[]. Be sure to call cmMemFree(dbp)
// to release the returned pointer.
T_t* cmVOT_Expand( T_t* dbp, unsigned dn, const T_t* t, unsigned tn );                                                            

// Replace the elements t[tn] with the elements in u[un]. 
// t must be inside or at the end of dbp[dn].
// This operation may result in a reallocation of dbp[]. Be sure to call cmMemFree(dbp)
// to release the returned pointer.
// IF dbp==NULL and tn==0 then the dbp[un] is allocated and returned 
// with the contents of u[un].
T_t* cmVOT_Replace(T_t* dbp, unsigned* dn, const T_t* t, unsigned tn, const T_t* u, unsigned un );

//======================================================================================================================
//)



//( { label:"Rotate/Shift/Flip/Sequence" desc:"Modify/generate the vector sequence." kw:[vop] }

// Assuming a row vector positive shiftCnt rotates right, negative shiftCnt rotates left.
T_t* cmVOT_Rotate( T_t* dbp, unsigned dn, int shiftCnt );

// Equivalent to Matlab circshift().
T_t* cmVOT_RotateM( T_t* dbp, unsigned drn, unsigned dcn, const T_t* sbp, int rShift, int cShift  );

// Assuming a row vector positive shiftCnt shifts right, negative shiftCnt shifts left.
T_t* cmVOT_Shift( T_t* dbp, unsigned dn, int shiftCnt, T_t fill );

// Reverse the contents of the vector.
T_t* cmVOT_Flip(  T_t* dbp, unsigned dn);

// Fill dbp[] with a sequence of values. Returns next value. 
T_t  cmVOT_Seq( T_t* dbp, unsigned dn, T_t beg, T_t incr );




//======================================================================================================================
//)

//( { label:"Arithmetic" desc:"Add,Sub,Mult,Divde" kw:[vop] }

T_t* cmVOT_SubVS(  T_t* dp, unsigned dn, T_t v );
T_t* cmVOT_SubVV(  T_t* dp, unsigned dn, const T_t* v );
T_t* cmVOT_SubVVS( T_t* dp, unsigned dn, const T_t* v, T_t s );
T_t* cmVOT_SubVVNN(T_t* dp, unsigned dn, unsigned dnn, const T_t* sp, unsigned snn );
T_t* cmVOT_SubVVV( T_t* dp, unsigned dn, const T_t* sb0p, const T_t* sb1p );
T_t* cmVOT_SubVSV( T_t* dp, unsigned dn, const T_t  s0, const T_t* sb1p );

T_t* cmVOT_AddVS(  T_t* dp, unsigned dn, T_t v );
T_t* cmVOT_AddVV(  T_t* dp, unsigned dn, const T_t* v );
T_t* cmVOT_AddVVS( T_t* dp, unsigned dn, const T_t* v, T_t s );
T_t* cmVOT_AddVVNN(T_t* dp, unsigned dn, unsigned dnn, const T_t* sp, unsigned snn );
T_t* cmVOT_AddVVV( T_t* dp, unsigned dn, const T_t* sb0p, const T_t* sb1p );

T_t* cmVOT_MultVVV( T_t* dbp, unsigned dn, const T_t* sb0p, const T_t* sb1p );
T_t* cmVOT_MultVV(  T_t* dbp, unsigned dn, const T_t* sbp );
T_t* cmVOT_MultVVNN(T_t* dp,  unsigned dn, unsigned dnn, const T_t* sp, unsigned snn );
T_t* cmVOT_MultVS(  T_t* dbp, unsigned dn, T_t s );
T_t* cmVOT_MultVVS( T_t* dbp, unsigned dn, const T_t* sbp, T_t s );
T_t* cmVOT_MultVaVS( T_t* dbp, unsigned dn, const T_t* sbp, T_t s );
T_t* cmVOT_MultSumVVS(T_t* dbp, unsigned dn, const T_t* sbp, T_t s );

T_t* cmVOT_DivVVS( T_t* dbp, unsigned dn, const T_t* sb0p, T_t sb1 );
T_t* cmVOT_DivVVV( T_t* dbp, unsigned dn, const T_t* sb0p, const T_t* sb1p );
T_t* cmVOT_DivVV(  T_t* dbp, unsigned dn, const T_t* sb0p );
T_t* cmVOT_DivVVNN(T_t* dp,  unsigned dn, unsigned dnn, const T_t* sp, unsigned snn );
T_t* cmVOT_DivVS(  T_t* dbp, unsigned dn, T_t s );
T_t* cmVOT_DivVSV( T_t* dp,  unsigned dn, const T_t  s0, const T_t* sb1p );

// Set dest to 0 if denominator is 0.
T_t* cmVOT_DivVVVZ( T_t* dbp, unsigned dn, const T_t* sb0p, const T_t* sb1p );
T_t* cmVOT_DivVVZ(  T_t* dbp, unsigned dn, const T_t* sb0p );

// Divide columns of dp[:,i] by each value in the source vector sp[i]. 
T_t*  cmVOT_DivMS(    T_t* dp, unsigned drn, unsigned dcn, const T_t* sp ); 

//======================================================================================================================
//)

//( { label:"Sum vectors" desc:"Operations which take sum vector elements." kw:[vop] }

T_t  cmVOT_Sum(      const T_t* sp, unsigned sn );
T_t  cmVOT_SumN(     const T_t* sp, unsigned sn, unsigned stride );

// Sum the columns of sp[srn,scn] into dp[scn]. 
// dp[] is zeroed prior to computing the sum.
T_t*  cmVOT_SumM(     const T_t* sp, unsigned srn, unsigned scn, T_t* dp );

// Sum the rows of sp[srn,scn] into dp[srn]
// dp[] is zeroed prior to computing the sum.
T_t*  cmVOT_SumMN(    const T_t* sp, unsigned srn, unsigned scn, T_t* dp );

//======================================================================================================================
//)


//( { label:"Min/max/median/mode" desc:"Simple descriptive statistics." kw:[vop] }

T_t  cmVOT_Median(   const T_t* sp, unsigned sn );
unsigned      cmVOT_MinIndex( const T_t* sp, unsigned sn, unsigned stride );
unsigned      cmVOT_MaxIndex( const T_t* sp, unsigned sn, unsigned stride );
T_t  cmVOT_Min(      const T_t* sp, unsigned sn, unsigned stride );
T_t  cmVOT_Max(      const T_t* sp, unsigned sn, unsigned stride );

T_t*  cmVOT_MinVV( T_t* dp, unsigned dn, const T_t* sp );
T_t*  cmVOT_MaxVV( T_t* dp, unsigned dn, const T_t* sp );


// Return index of max/min value into dp[scn] of each column of sp[srn,scn]
unsigned*  cmVOT_MinIndexM( unsigned* dp, const T_t* sp, unsigned srn, unsigned scn );
unsigned*  cmVOT_MaxIndexM( unsigned* dp, const T_t* sp, unsigned srn, unsigned scn );

// Return the most frequently occuring element in sp.
T_t  cmVOT_Mode(     const T_t* sp, unsigned sn );

//======================================================================================================================
//)

//( { label:"Compare/Find" desc:"Compare, find, replace and count elements in a vector." kw:[vop] }

// Return true if s0p[sn] is equal to s1p[sn]
bool       cmVOT_IsEqual( const T_t* s0p, const T_t* s1p, unsigned sn );

// Return true if all elements of s0p[sn] are within 'eps' of s1p[sn].
// This function is based on cmMath.h:cmIsCloseX()
bool cmVOT_IsClose( const T_t* s0p, const T_t* s1p, unsigned sn, double eps );

// Replace all values <= lteKeyVal with replaceVal.  sp==dp is legal.
T_t* cmVOT_ReplaceLte( T_t* dp, unsigned dn, const T_t* sp, T_t lteKeyVal, T_t replaceVal );

// Return the index of 'key' in sp[sn] or cmInvalidIdx if 'key' does not exist.
unsigned cmVOT_Find( const T_t* sp, unsigned sn, T_t key );

// Count the number of times 'key' occurs in sp[sn].
unsigned cmVOT_Count(const T_t* sp, unsigned sn, T_t key );

//======================================================================================================================
//)



//( { label:"Absolute value" desc:"Absolute value and signal rectification." kw:[vop] }

T_t*   cmVOT_Abs(   T_t* dbp, unsigned dn );

// Half wave rectify the source vector.
// dbp[] = sbp<0 .* sbp 
// Overlapping the source and dest is allowable as long as dbp <= sbp. 
T_t* cmVOT_HalfWaveRectify(T_t* dbp, unsigned dn, const T_t* sp );

//======================================================================================================================
//)


//( { label:"Filter" desc:"Apply filtering to a vector taking into account vector begin/end conditions." kw:[vop] }

// Apply a median or other filter of order wndN to xV[xN] and store the result in yV[xN].
// When the window goes off either side of the vector the window is shortened.
// This algorithm produces the same result as the fn_thresh function in MATLAB fv codebase.
void cmVOT_FnThresh( const T_t* xV, unsigned xN, unsigned wndN, T_t* yV, unsigned yStride, T_t (*fnPtr)(const T_t*, unsigned) );


// Apply a median filter of order wndN  to xV[xN] and store the result in yV[xN].
// When the window goes off either side of the vector the missing elements are considered
// to be 0.
// This algorithm produces the same result as the MATLAB medfilt1() function.
void cmVOT_MedianFilt( const T_t* xV, unsigned xN, unsigned wndN, T_t* yV, unsigned yStride );
//======================================================================================================================
//)


//( { label:"Edit distance" desc:"Calculate the Levenshtein edit distance between vectors." kw:[vop] }

// Allocate and initialize a matrix for use by LevEditDist().
// This matrix can be released with a call to cmMemFree().
unsigned* cmVOT_LevEditDistAllocMtx(unsigned mtxMaxN);

// Return the Levenshtein edit distance between two vectors.
// m must point to a matrix pre-allocated by cmVOT_InitiLevEditDistMtx(maxN).
double  cmVOT_LevEditDist(unsigned mtxMaxN, unsigned* m, const T_t* s0, int n0, const T_t* s1, int n1, unsigned maxN );

// Return the Levenshtein edit distance between two vectors.
// Edit distance with a max cost threshold. This version of the algorithm
// will run faster than LevEditDist() because it will stop execution as soon
// as the distance exceeds 'maxCost'.
// 'maxCost' must be between 0.0 and 1.0 or it is forced into this range.
// The maximum distance returned will be 'maxCost'.
// m must point to a matrix pre-allocated by cmVOT_InitiLevEditDistMtx(maxN).
double cmVOT_LevEditDistWithCostThresh( int mtxMaxN, unsigned* m, const T_t* s0, int n0, const T_t* s1, int n1, double maxCost, unsigned maxN );

//======================================================================================================================
//)


