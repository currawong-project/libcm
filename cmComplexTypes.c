

void cmVOCR_MultVVV( cmComplexR_t* y, const cmComplex_t* x0, const cmComplexR_t* x1, unsigned n )
{
  unsigned i;
  for(i=0; i<n; ++i)
  {
    cmReal_t ab = x0[i].r * x1[i].r;
    cmReal_t bd = x0[i].i * x1[i].i;
    cmReal_t bc = x0[i].i * x1[i].r;
    cmReal_t ad = x0[i].r * x1[i].i;
    y[i].r = ab - bd;
    y[i].i = bc + ad;    
  }

}

void cmVOCR_Abs(     cmReal_t* y, const cmComplexR_t* x, unsigned n )
{
  for(i=0; i<n; ++i)
    y[i] = cmAbsR(x[i]);
}

void cmVOCR_MultR_VV(  cmComplexR_t* y, const cmReal_t* x, unsigned n )
{
  unsigned i;
  for(i=0; i<n; ++i)
  {
    y[i].r *= x[i];
    y[i].i *= x[i];
  }
}

void cmVOCR_DivR_VV(   cmComplexR_t* y, const cmReal_t* x, unsigned n )
{
  unsigned i;
  for(i=0; i<n; ++i)
  {
    y[i].r /= x[i];
    y[i].i /= x[i];
  }  
}
