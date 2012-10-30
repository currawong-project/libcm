#ifndef cmOp_h
#define cmOp_h

#ifdef __cplusplus
extern "C" {
#endif

  void     vs_Zero(   cmSample_t v[], unsigned vn);
  cmReal_t vs_Sine(   cmSample_t v[], unsigned vn, cmReal_t hzRad, cmReal_t initPhs );
  void     vs_Rand(   cmSample_t v[], unsigned vn, cmSample_t min, cmSample_t max );

  void     vs_Copy(   cmSample_t d[], const cmSample_t s[], unsigned n );

  cmSample_t vs_SquaredSum( const cmSample_t s[], unsigned n );

  // d[] = s[] * mult;
  void     vs_MultVVS(   cmSample_t d[], const cmSample_t s[], unsigned n, cmReal_t mult );

  //d[] += s[] * mult
  void     vs_SumMultVVS(cmSample_t d[], const cmSample_t s[], unsigned n, cmReal_t mult ); 

  // Interpolate the values of y[yn] at the points defined by x[vn] and store the result in v[vn].
  // User linear interpolation.
  //void     vs_Interp2( cmSample_t v[], unsigned vn, const cmSample_t[] x, const cmSample_t y[], unsigned yn );

#ifdef __cplusplus
}
#endif

#endif
