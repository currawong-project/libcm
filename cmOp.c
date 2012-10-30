#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmOp.h"

void     vs_Zero( cmSample_t v[], unsigned vn)
{ memset(v,0,sizeof(v[0])*vn); }


cmReal_t vs_Sine( cmSample_t v[], unsigned vn, cmReal_t hzRad, cmReal_t initPhs )
{
  const cmSample_t* ep  = v + vn;
  double            phs = initPhs;

  while(v<ep)
  {
    *v++ = (cmSample_t)sin( phs );
    phs += hzRad;
  }

  return (cmReal_t)phs;
}

void vs_Rand( cmSample_t v[], unsigned vn, cmSample_t min, cmSample_t max )
{
  const cmSample_t* ep = v + vn;
  while(v<ep)
    *v++ = ((cmSample_t)rand()/RAND_MAX) * (max-min) + min;

}

void     vs_MultVVS(   cmSample_t d[], const cmSample_t s[], unsigned n, cmReal_t mult )
{
  const cmSample_t* ep = d + n;
  while(d<ep)
    *d++ = *s++ * mult;
}

void     vs_SumMultVVS(   cmSample_t d[], const cmSample_t s[], unsigned n, cmReal_t mult )
{
  const cmSample_t* ep = d + n;
  while(d<ep)
    *d++ += *s++ * mult;
}


void     vs_Copy(   cmSample_t d[], const cmSample_t s[], unsigned n )
{
  memcpy(d,s,n*sizeof(d[0]));
}

cmSample_t     vs_SquaredSum(   const cmSample_t s[], unsigned n )
{
  cmSample_t        sum = 0;
  const cmSample_t* ep  = s + n;

  for(;s<ep;++s)
    sum += *s * *s;
  return sum;
}


/*
unsigned vs_Interp2( cmSample_t v[], unsigned vn, const cmSample_t[] xx, const cmSample_t y[], unsigned yn )
{

  unsigned i = 0;
  for(; i<vn; ++i)
  {
    double   x  = fmod(*xx++,yn);
    unsigned x0 = floor(x);
    unsigned x1 = x0 + 1;
    double   d  = x - x0;

    if( x1>=yn || x0>=yn)
      break;

    *v++ = y[x0] + (y[x1] - y[x0]) * d;
  }

  return i;
}
*/
