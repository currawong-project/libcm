#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmComplexTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmAudioFile.h"
#include "cmMidi.h"
#include "cmFile.h"
#include "cmFileSys.h"
#include "cmProcObj.h"
#include "cmProcTemplate.h"
#include "cmProc.h"
#include "cmProc2.h"
#include "cmMath.h"
#include "cmVectOps.h"
#include "cmKeyboard.h"
#include "cmGnuPlot.h"
#include "cmStack.h"
#include "cmRbm.h"

#include <time.h> // time()



void cmTestPrint( cmRpt_t* rpt, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);
  if(rpt != NULL )
    cmRptVPrintf(rpt,fmt,vl);
  else
    vprintf(fmt,vl);

  va_end(vl);
}

void cmStandardizeTest(cmCtx_t* ctx )
{
  cmRpt_t* rpt = ctx->err.rpt;
  
  unsigned rn = 3;
  unsigned cn = 2;

  double m[] = { 4, 5, 6, 7, 8, 9 };
  double uV[rn];
  double sdV[rn];

  cmVOD_PrintL("m", rpt, rn, cn, m );

  cmVOD_StandardizeRows(m, rn, cn, uV, sdV );

  cmVOD_PrintL("uV", rpt, 1, rn, uV);
  cmVOD_PrintL("sdV", rpt, 1, rn, sdV );
  cmVOD_PrintL("m",   rpt, rn, cn, m );

 
}

void cmBinMtxFileTest(cmCtx_t* ctx )
{
    const char* dataFn  = "/home/kevin/temp/cmRbmData0.mtx";
    unsigned    pointsN = 6;             // count of data points in mtx (columns) 
    unsigned    dimN    = 4;             // dim of binary data matrix (rows)
    double probV[]      = {.2,.5,.8,.6}; // probabilities of generating a 1.0

    assert(sizeof(probV)/sizeof(probV[0]) == dimN);

    // alloc the data matrix
    double* data0M = cmMemAllocZ( double, dimN*pointsN );
    unsigned i,j;

    // generate a stochastic binary data matrix according to prob's in probV[]
    for(i=0; i<pointsN; ++i)
      for(j=0; j<dimN; ++j)
        data0M[ i*dimN + j ] =  rand() < (probV[j] * RAND_MAX);

    // write the binary matrix file
    cmBinMtxFileWrite(dataFn, dimN, pointsN, NULL, data0M, NULL, ctx->err.rpt );

    // print the original data
    cmVOD_PrintL("data0M",&ctx->rpt,dimN,pointsN,data0M);

    // read the binary matrix file
    unsigned rn,cn,en;
    cmRC_t    rc0     = cmBinMtxFileSize(ctx,dataFn,&rn,&cn,&en);
    double*   data1M  = cmMemAllocZ(double,rn*cn);
    unsigned* colCntV = cmMemAllocZ(unsigned,rn);
    cmRC_t    rc1     = cmBinMtxFileRead(ctx,dataFn,rn,cn,en,data1M,colCntV);

    if( rc0 != cmOkRC || rc1 != cmOkRC )
      cmRptPrintf(&ctx->rpt,"Error reading binary matrix file:%s",cmStringNullGuard(dataFn));

    // print the binary matrix file
    cmVOU_PrintL("colCntV",&ctx->rpt,1,rn,colCntV);
    cmVOD_PrintL("data1M", &ctx->rpt,rn,cn,data1M);


    cmMemFree(colCntV);
    cmMemFree(data0M);
    cmMemFree(data1M);
}


void cmFileGetLineTest(cmCtx_t* ctx )
{
  const cmChar_t* fn = "/home/kevin/temp/labels.txt";
  cmFileH_t f = cmFileNullHandle;

  if( cmFileOpen(&f,fn,kReadFileFl,&ctx->rpt) == kOkFileRC )
  {
    unsigned bufByteCnt = 0;
    cmChar_t* buf = NULL;
    bool fl = true;
    while(fl)
    {
      switch( cmFileGetLine(f,buf,&bufByteCnt) )
      {
        case kOkFileRC:
          cmRptPrintf(&ctx->rpt,"%i %i %s\n",bufByteCnt,strlen(buf),buf);
          break;

        case kBufTooSmallFileRC:
          buf = cmMemResizeZ(cmChar_t,buf,bufByteCnt);
          break;

        default:
          fl = false;
          break;
      }
    }

    cmMemFree(buf);
    cmFileClose(&f);
  }
}

// Test the cmPvAnl API.
// See cmPvAnlProcTest.m for the equivalent octave code.
void cmPvAnlTest(cmCtx* c )
{
  const char* ifn         = "/home/kevin/temp/onset0.wav";
  const char* ofn         = "/home/kevin/temp/test0.aif";
  unsigned    afChIdx     = 0;
  unsigned    afChCnt     = 1;
  unsigned    afBegSmpIdx = 1000000;
  unsigned    afEndSmpIdx = 1010000;
  unsigned    wndSmpCnt   = 4096;
  unsigned    hopSmpCnt   = 1024;
  unsigned    procSmpCnt  = hopSmpCnt;

  cmAudioFileRd*  afRd  = cmAudioFileRdAlloc(c,NULL,procSmpCnt,ifn, afChIdx, afBegSmpIdx, afEndSmpIdx );
  assert(afRd != NULL );

  cmAudioFileWr*  afWr  = cmAudioFileWrAlloc(c,NULL,procSmpCnt,ofn, afRd->info.srate,afChCnt,afRd->info.bits );
  assert(afWr != NULL );

  cmPvAnl*        pvAnl = cmPvAnlAlloc(c,NULL,procSmpCnt,afRd->info.srate,wndSmpCnt,hopSmpCnt, kNoCalcHzPvaFl );
  assert(pvAnl != NULL);

  while( cmAudioFileRdRead(afRd) != cmEofRC )
  {
    while( cmPvAnlExec(pvAnl,afRd->outV, afRd->outN ) )
    {

      printf(" : %f %i\n",cmVOR_Sum(pvAnl->magV,pvAnl->binCnt),pvAnl->binCnt);
    }
    
    cmAudioFileWrExec(afWr,0,afRd->outV,afRd->outN);
  }

  cmPvAnlFree(&pvAnl);
  cmAudioFileWrFree(&afWr);
  cmAudioFileRdFree(&afRd);
}

void cmAudioFileProcTest(cmCtx_t* ctx)
{

  //const cmChar_t*  aifFn = "/home/kevin/media/audio/sourcetone/00-11-060-I-Shapeshifter-TranquilVapor.aiff";
  //const cmChar_t*   wavFn  = "/home/kevin/temp/mas/onset_conv/Piano 3_01.aif";
  const cmChar_t*   wavFn  = "/home/kevin/temp/mas/onsets/Piano 3_01.wav";
  //const cmChar_t*   wavFn  = "/home/kevin/temp/onsetsConv0.aif";
  //const cmChar_t*   wavFn  = "/home/kevin/media/audio/20110723-Kriesberg/Audio Files/Piano 3_01.aif";
  const cmChar_t*   fn     = wavFn;
  cmAudioFileInfo_t afInfo;
  cmRC_t            cmRC;
  cmAudioFileH_t    afH    = cmAudioFileNewOpen( fn, &afInfo, &cmRC, &ctx->rpt ); 

  if( cmRC != kOkAfRC )
    printf("Unable to open the audio file:%s\n",fn);
  else
  {
    //cmAudioFileReport( afH, &ctx->rpt, 9785046, 100);
    //cmAudioFileReport( afH, &ctx->rpt, 15134420, 100);   // onset_conv/Piano 3_01.aif
    //cmAudioFileReport( afH, &ctx->rpt, 12862654, 100);   // Audio Files/Piano 3_01.wav
    cmAudioFileReport( afH, &ctx->rpt, 96092658, 100);     // onsets/Piano 3_01.wav

    cmAudioFileDelete(&afH);
  }
}

// This code test the af
void cmAudioFileReadWriteTest(cmCtx_t* ctx )
{
  const cmChar_t* inDir  = "/home/kevin/src/cm/src/data/audio_file_format_test";
  const cmChar_t* outDir = "/home/kevin/temp/af1/out";
  unsigned        dirCnt = 0;
  unsigned        i      = 0;

  if( cmPlotInitialize(NULL) != cmOkRC )
    return;

  if( cmFsIsDir(outDir) == false )
    cmFsMkDir(outDir);

  // get the files in the input directory
  cmFileSysDirEntry_t* dep = cmFsDirEntries( inDir, kFileFsFl | kFullPathFsFl, &dirCnt );
  
  for(i=0; dep != NULL && i<dirCnt; ++i)
  {
    cmAudioFileH_t    afH;
    cmAudioFileInfo_t afInfo;
    cmRC_t            cmRC;

    // open the ith file in the input directory
    if( cmAudioFileIsValid( afH = cmAudioFileNewOpen( dep[i].name, &afInfo, &cmRC, &ctx->rpt)) == false)
    {
      cmRptPrintf(&ctx->rpt,"Audio file open error occurred on %s\n",dep[i].name);
      continue;
    }
    
    cmFileSysPathPart_t* pp;                                       
    unsigned             smpCnt = afInfo.frameCnt * afInfo.chCnt;   // count of samples to read and write
    cmSample_t*          buf    = cmMemAlloc( cmSample_t, smpCnt ); // allocate the sample buffer
    cmSample_t*          bp[ afInfo.chCnt];
    unsigned             actualFrmCnt;
    unsigned             chIdx  = 0;
    unsigned             j;

    // initialize the audio channel buffer
    for(j=0; j<afInfo.chCnt; ++j)
      bp[j] = buf + (j*afInfo.frameCnt);

    // parse the input file name
    if((pp = cmFsPathParts(dep[i].name)) == NULL )
      cmRptPrintf(&ctx->rpt,"Unable to locate the file parts for '%s'.",dep[i].name);
    else
    {
      // use the input file name to form the output file name
      const cmChar_t* outFn    = cmFsMakeFn(outDir,pp->fnStr,"bin",NULL);
      const cmChar_t* audOutFn = cmFsMakeFn(outDir,pp->fnStr,"aif",NULL);
      cmAudioFileH_t  aofH     = cmAudioFileNewCreate(audOutFn, afInfo.srate, afInfo.bits, afInfo.chCnt, &cmRC, &ctx->rpt);

      // read the entire audio file into the sample buffer
      if( cmAudioFileReadSample(afH,afInfo.frameCnt,chIdx,afInfo.chCnt,bp, &actualFrmCnt ) != kOkAfRC )
        cmRptPrintf(&ctx->rpt,"Audio file read error occurred on %s\n",dep[i].name);

      // write the audio file out as a binary matrix file
      if( cmBinMtxFileWrite(outFn, actualFrmCnt, afInfo.chCnt, buf, NULL, NULL, &ctx->rpt ) != cmOkRC )
        cmRptPrintf(&ctx->rpt,"Binary matrix write failed on '%s'\n", outFn );

      // write the audio output file
      if( cmAudioFileIsValid(aofH) )
        if( cmAudioFileWriteSample(aofH,afInfo.frameCnt,afInfo.chCnt, bp ) != cmOkRC)
          cmRptPrintf(&ctx->rpt,"Audio output file write failed on '%s'.", audOutFn);

      // plot the audio file signals
      cmPlotSetup(pp->fnStr,1,1);
      for(j=0; j<afInfo.chCnt; ++j)
        cmPlotLineS(NULL,NULL,bp[j],NULL,afInfo.frameCnt,NULL,kSolidPlotLineId);
      cmPlotDraw();

      //cmKeyPress(NULL);

      // close the output audio file
      if( cmAudioFileIsValid(aofH) )
        cmAudioFileDelete(&aofH);

      cmFsFreeFn(audOutFn);  // release the output audio file name
      cmFsFreeFn(outFn);     // release the output file name
      cmFsFreePathParts(pp); // release the parse recd
    }

    cmAudioFileDelete(&afH); // release the audio file
    cmMemPtrFree(&buf);      // release the audio buffer
  }
  
  cmFsDirFreeEntries(dep);

  cmPlotFinalize();
}

void cmZeroCrossTest( cmRpt_t* rpt )
{
  double srate = 32;
  unsigned vn = srate * 2;
  cmSample_t v6[ vn ];
  unsigned impPhs = 0;
  double hz = 2;
  cmSample_t d = 0;

  impPhs = cmVOS_SynthSine(  v6, vn, impPhs,  srate, hz );
  cmVOS_MultVS( v6, vn, -1 );

  unsigned zc = cmVOS_ZeroCrossCount( v6, vn, &d);  
  cmTestPrint(rpt,"zero cross: %i \n",zc);

  cmVOS_Print( rpt, 1, vn, v6 ); 

}

void cmMelTest(cmRpt_t* rpt)
{
  double   srate   = 44100;
  unsigned binCnt  = 513;
  unsigned bandCnt = 36;

  cmSample_t t[ binCnt * bandCnt ];
 
  cmCtx*       c = cmCtxAlloc(NULL,rpt,cmLHeapNullHandle,cmSymTblNullHandle);
  cmMatrixBuf* m = cmMatrixBufAlloc( c, NULL, binCnt, bandCnt );
 
  cmVOS_MelMask( m->bufPtr, bandCnt, binCnt, srate, kShiftMelFl );
  cmVOS_Transpose( t, m->bufPtr, bandCnt, binCnt );

  cmPlotSetup("Test Proc Impl",1,1);

  unsigned i;
  double sum = 0;
  for(i=0; i<bandCnt; ++i )
  {
    sum += cmVOS_Sum( t + (i*binCnt), binCnt );

    cmPlotLineS( NULL,  NULL, t + (i*binCnt), NULL, 35, NULL, kSolidPlotLineId  );    
  }

  printf("sum:%f\n",sum);

  cmPlotDraw();
  cmKeyPress(NULL);    

  cmCtxFree(&c);  
  cmMatrixBufFree( &m);
}

 void cmDctTest()
 {
   unsigned coeffCnt = 20;
   unsigned filtCnt  = 36;
   cmSample_t m[ coeffCnt * filtCnt ];
   cmSample_t t[ coeffCnt * filtCnt ];

   
   cmVOS_DctMatrix( m, coeffCnt, filtCnt );
   cmVOS_Transpose( t, m, coeffCnt, filtCnt );

   cmPlotSetup("Test",1,1);

  unsigned i;
  for(i=0; i<coeffCnt; ++i )
  {
    cmPlotLineS( NULL,  NULL,  t+(i*filtCnt), NULL, filtCnt, NULL, kSolidPlotLineId  );    
  }

  cmPlotDraw();
  cmKeyPress(NULL);    
   
 }

void cmMtxMultTest(cmRpt_t* rpt)
{
  unsigned mrn = 3;
  unsigned mcn = 2;
  unsigned vn  = mcn;
  unsigned on  = mrn;

  double m[] = { 1, 2, 3, 4, 5, 6 };
  double v[] = { 1, 2 };
  double o[ on ];


  cmVOD_MultVMV( o, mrn, m, mcn, v );

  cmVOD_PrintL( "o:\n", rpt, 1, on, o );

  cmVOD_MultVVM( o, on, v, vn, m );

  cmVOD_PrintL( "o:\n", rpt, 1, on, o );


  cmReal_t A[] = { 8, 3, 4, 1, 5, 9, 6, 7, 2 }; // magic(3)
  cmReal_t B[] = { 1, 2, 3, 4, 5, 6};

  unsigned arn = 3;
  unsigned cmn = 3;
  unsigned brn = cmn;
  unsigned bcn = 2;

  cmReal_t D[arn*bcn];
  cmVOR_MultMMM( D, arn, bcn, A, B, brn );

  cmVOR_PrintL("D:\n",rpt,arn,bcn,D);

  // D = B*At
  cmVOR_MultMMMt( D, bcn, arn, B, A, cmn );
  cmVOR_PrintL("Dt:\n",rpt,bcn,arn,D);

  // D += B*At
  cmVOR_MultMMM1( D, bcn, arn, 1.0, B, A, cmn, 1.0, kTransposeM1Fl );
  cmVOR_PrintL("Dt:\n",rpt,bcn,arn,D);

  // D = B*At - with explicit physical row counts for each matrix
  cmVOR_MultMMM2( D, bcn, arn, 1.0, B, A, cmn, 0.0, kTransposeM1Fl, bcn, bcn, cmn );
  cmVOR_PrintL("Dt:\n",rpt,bcn,arn,D);

  // D = 3*B*At - with explicit physical row counts for each matrix
  cmVOR_MultMMM2( D, bcn, arn, 3.0, B, A, cmn, 0.0, kTransposeM1Fl, bcn, bcn, cmn );
  cmVOR_PrintL("Dt:\n",rpt,bcn,arn,D);


/// dpb[dn] = mp[mrn,dn] * vp[mrn]
  double v1[] = {1, 2, 3}; 
  cmVOD_MultVMtV( o, mcn, m, mrn, v1 );

  cmVOD_PrintL( "o:\n", rpt, 1, on, o );
  

  cmTestPrint(rpt,"multsum: %f\n", cmVOR_MultSumVV( A, B, 6 ));

}


void cmShiftRotateTest(cmRpt_t* rpt)
{
  unsigned vn = 5;
  double v[] = { 0, 1, 2, 3, 4 };


  cmVOD_Print( rpt, 1, vn, v );
  
  cmVOD_Rotate( v, vn, 2 );

  cmVOD_Print( rpt, 1, vn, v );

  cmVOD_Rotate( v, vn, -3 );

  cmVOD_Print( rpt, 1, vn, v );

  cmVOD_Shift( v, vn, 2, 9 );

  cmVOD_Print( rpt, 1, vn, v );

  cmVOD_Shift( v, vn, -2, 9 );

  cmVOD_Print( rpt, 1, vn, v );

  unsigned pn = 6;
  double p[] = { 0, 1, 0, 1, 0, 1 };
  unsigned pi[5];

  unsigned pm = cmVOD_PeakIndexes( pi, 5, p, pn, 1.1 );
  
  printf("%i : ",pm);
  cmVOU_Print( rpt, 1, pm, pi );


  cmReal_t m0[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  cmReal_t m1[9];

  cmVOD_PrintL("m0:\n",rpt,3,3,m0);
  cmVOR_RotateM( m1,  3,3, m0, 3, -2 );

  cmVOD_PrintL("m1:\n",rpt,3,3,m1);


}

void cmRandomTest(cmRpt_t* rpt )
{
  cmPlotSetup("Random",1,1);
  unsigned i;
  unsigned yn = 1000;
  unsigned y[yn]; 
  cmReal_t w[] = { .2, .3, .5};
  
  // seed the random number generator with a clock with seconds resolution
  srand((unsigned)time(NULL));

  unsigned wn = sizeof(w)/sizeof(w[0]);

  unsigned h[wn];

  cmVOR_WeightedRandInt(y,yn,w,wn);

  cmVOU_Hist(h,wn,y,yn);

  cmVOU_Print(rpt,1,wn,h);

  for(i=0; i<wn; ++i)
    printf("%f ",(float)h[i]/yn);
  printf("\n");

}


void cmTestGaussWin(cmRpt_t* rpt)
{
  unsigned vn = 10;
  double v[ vn ];

  unsigned n = 4;
  cmVOD_GaussWin(v,n,5.0/3.0);
  cmVOD_Print(rpt,1,n,v);



}

// meanV[K] and varV[K] identify the center and variance of K clusters
// kV[xcn] identifes the cluster associated with each data point
// Each column of xM[] contains a single 2D data point (xrn is therefore always 2)
void cmPlotGauss2D( cmRpt_t* rpt, const cmReal_t* meanV, const cmReal_t* varV, const cmReal_t* xM, unsigned xD, unsigned xN, const char* colorStr )
{
  cmReal_t  txM[ xD*xN ];
  cmVOR_Transpose(txM,xM,xD,xN);

  cmPlotLineR( NULL, txM, txM + xN, NULL, xN, colorStr, kCirclePlotPtId );

  unsigned cn = 100;
  cmReal_t cV[ cn*2 ];
  cmVOR_CircleCoords( cV, cn, meanV[0], meanV[1], varV[0], varV[1] );
  cmPlotLineR( NULL, cV, cV+cn, NULL, cn, colorStr, kSolidPlotLineId );

  cmPlotLineR( NULL, meanV, meanV+1, NULL, 1, colorStr, kAsteriskPlotPtId );

  printf("%s\n",colorStr);
  cmVOR_Print(rpt,xN,xD,txM);
}

cmReal_t _cmProcTestKmeansDist( void* userPtr, const cmReal_t* v0, const cmReal_t* v1, unsigned vn )
{ return cmVOR_EuclidDistance(v0,v1,vn); }

void cmGaussTest(cmRpt_t* rpt )
{
  cmReal_t minV = -1;
  cmReal_t incr = .1;
  unsigned xn = ((fabs(minV)-minV)/incr)+2;
  cmReal_t x0[xn],y0[xn];
  cmVOR_Seq(x0,xn,minV,incr);

  //cmVOR_Print(rpt,1,xn,x0);

  cmVOR_GaussPDF( y0, xn, x0, 0, 1 );

  cmPlotSetup("Gauss",1,1);
  //cmPlotLineR( NULL,  NULL, y0, NULL, xn, NULL, kSolidPlotLineId  );    

  unsigned x1N   = 10;
  unsigned x1D   = 2; 
  cmReal_t m1V[] = {  0, 0 };
  cmReal_t v1V[] = { .5,.5 };
  cmReal_t x1[ x1D * x1N ];

  cmVOR_RandomGaussM( x1, x1D, x1N, m1V, v1V );
  //cmPlotGauss2D(rpt,m1V,v1V,x1,x1D,x1N,"magenta");


  unsigned x2N   = 5;                           // data points per cluster
  unsigned x2D   = 2;                           // data dimensions   
  unsigned x2K   = 3;                           // count of clusters
  cmReal_t m2M[] = {  0, 0,  1,  1,  2,  2 };   // cluster means
  cmReal_t v2M[] = { .5,.5, .5, .5, .5, .5 };   // cluster variances
  cmReal_t x2[ x2D * x2N * x2K ];               // data matrix
  char     x2c[][6] = { "red","green","blue" };
  cmVOR_RandomGaussMM( x2, x2D, x2N*x2K, m2M, v2M, x2K );

  cmVOR_Print(rpt,x2D,x2N*x2K,x2);

  unsigned k = 0;
  //for(k=0; k<x2K; ++k)
  //  cmPlotGauss2D( rpt, m2M + (k*x2D), v2M + (k*x2D), x2 + (k*x2D*x2N), x2D, x2N, x2c[k] );


  unsigned classIdxV[ x2N*x2K ];  // kmeans generated point assignment vector
  cmReal_t centroidM[ x2D*x2K ];  // kmeans generated centroids

  // classify each point using kmeans
  unsigned iterCnt =  cmVOR_Kmeans(classIdxV, centroidM, x2K, x2, x2D, x2N*x2K, NULL, 0, false, _cmProcTestKmeansDist, NULL );

  cmTestPrint(rpt,"kmeans iterations:%i\n",iterCnt);

  // plot kmeans clusters
  for(k=0; k<x2K; ++k)
  {
    cmReal_t pM[ x2D * x2N ];
    cmReal_t dV[ x2D ];
    cmReal_t vV[ x2D ];

    unsigned i,j=0;
    
    cmVOR_Fill(vV,x2D,0);

    // for each data point in cluster k
    for(i=0; i<x2N*x2K; ++i)
      if( classIdxV[i] == k )
      {
        // store the data point for later plotting
        cmVOR_Copy( pM+(j++*x2D), x2D,                x2 + (i*x2D) );

        // calculate the variance 
        cmVOR_SubVVV(  dV, x2D, centroidM + (k*x2D),  x2 + (i*x2D ));
        cmVOR_MultVV(  dV, x2D, dV);
        cmVOR_AddVV(   vV, x2D, dV );
      }

    // normalize the variance
    if( j - 1 > 0 )
      cmVOR_DivVS( vV, x2D, j-1 );

    // plot data points in cluster k
    cmPlotGauss2D( rpt, centroidM + (k*x2D), vV, pM, x2D, j, x2c[k] );
  }

 
  cmPlotDraw();
  cmKeyPress(NULL);    
  
}


void cmFPExceptTest()
{


  double n = DBL_EPSILON;
  double d = DBL_MAX;
  //double r0 = sqrt(-1.0);
  double r0 = n/d;
  printf("%e\n",r0);
 
}


void cmLaTest(cmRpt_t* rpt)
{
  unsigned   a0n  = 3;
  unsigned   a1n  = 4;
  cmReal_t   A0[] = { 1,4,7,2,5,8,3,6,9 };
  cmReal_t   A1[] = { 1,0,0,0, 0,-2,0,0, 0,0,3,0, 0,0,0,4 };
  cmReal_t   t[ a1n*a1n ];

  cmReal_t det = cmVOR_DetM(A0,a0n);
  printf("det:%e\n",det);

  cmVOR_InvM(A0,a0n);
  cmVOR_PrintE(rpt,a0n,a0n,A0);

  det = cmVOR_DetDiagM(A1,a1n);
  printf("det:%e\n",det);

  cmVOR_InvDiagM(A1,a1n);
  cmVOR_PrintE(rpt,a1n,a1n,A1);

  cmReal_t A[] = { 8, 3, 4, 1, 5, 9, 6, 7, 2 }; // magic(3)
  cmReal_t B[] = { 1, 2, 3, 4, 5, 6};

  //cmVOR_SolveLS(A,3,B,2);

  cmVOR_Print(rpt,3,2,B);

  cmReal_t D[2*3];
  cmVOR_MultMMM( D, 2, 3, B, A, 3 );

  cmVOR_Print(rpt,2,3,D);

  cmVOR_RandSymPosDef( A1, a1n, t );

  cmVOR_PrintL("A1:\n",rpt,a1n,a1n,A1);

  cmVOR_Chol(A1,a1n);

  cmVOR_PrintL("A1:\n",rpt,a1n,a1n,A1);

  cmVOR_RandSymPosDef( A1, a1n, t );

  cmVOR_PrintL("A1:\n",rpt,a1n,a1n,A1);

  cmVOR_CholZ(A1,a1n);

  cmVOR_PrintL("A1:\n",rpt,a1n,a1n,A1);

}

typedef struct
{
  const cmReal_t* xM; // matrix base
  unsigned        D; // row cnt
  unsigned        N; // col cnt
} cmTestReadFuncData_t;

const cmReal_t* cmTestReadFunc( void* userPtr, unsigned colIdx )
{
  cmTestReadFuncData_t* p = (cmTestReadFuncData_t*)userPtr;
  assert( userPtr != NULL && colIdx < p->N );
  return p->xM + (colIdx*p->D);
}

void cmMvnProbTest(cmRpt_t* rpt)
{
  unsigned D = 2;
  unsigned N = 3;
  cmReal_t uV[] = { 0, 0 };
  cmReal_t sM[] = { 1, 0, 0, 1 };
  cmReal_t xM[] = { 1, 2, 3, 4, 5, 6 };
  cmReal_t yV[ N ];
  cmTestReadFuncData_t r;

  cmVOR_MultVarGaussPDF( yV, xM, uV, sM, D, N, false );

  //cmVOR_PrintE(rpt,1,N,yV);


  cmVOR_RandSymPosDef(sM, D, NULL );

  //cmVOR_PrintL("sM:\n",rpt,D,D,sM);

  cmVOR_RandomGaussNonDiagM( xM, D, N, uV, sM, NULL );

  //cmVOR_PrintL("yM:\n",rpt,D,N,xM);


  cmReal_t S[] ={ 0.67462, 0.49828, 0.49828, 0.36804 };
  cmVOR_MultVS(S,N*N,10);
  cmReal_t mu[]   = {0, 0};
  cmReal_t logDet = cmVOR_LogDetM(S,D);
  cmReal_t x[]    = {-.1, -.1, 0, 0, .1, .1};
  bool     diagFl = false;

  cmVOR_InvM(S,D);

  r.xM = x;
  r.D  = D;
  r.N  = N;
  cmVOR_MultVarGaussPDF3( yV, cmTestReadFunc, &r, mu, S, logDet, D, N, diagFl );

  cmVOR_PrintL("pr:\n",rpt,1,N,yV);


  cmVOR_MultVarGaussPDF2( yV, x, mu, S, logDet, D, N, diagFl );

  cmVOR_PrintL("pr:\n",rpt,1,N,yV);

}

void cmCovarTest(cmRpt_t* rpt)
{
  unsigned D = 2;
  unsigned N = 1000;

  cmReal_t xM[D*N];
  
  cmReal_t uV[D];
  cmReal_t sM[D*D];
  cmReal_t scM[D*D];

  srand((unsigned)time(NULL));

  if(1)
  {
    cmVOR_RandSymPosDef(sM,D,NULL);
    cmVOR_Random(uV,D,0,1);
  }

  if(0)
  {
    sM[0] = .1;
    sM[1] = 0;
    sM[2] = 0;
    sM[3] = .1;

  }

  if(0)
  {
    uV[0] = 0;
    uV[1] = 0;
    sM[0] = .48533;
    sM[1] = .27140;
    sM[2] = .27140;
    sM[3] = .15191;
  }

  cmVOR_RandomGaussNonDiagM(xM,D,N,uV,sM,NULL); 
 
  cmVOR_GaussCovariance(scM,D,xM,N,NULL,NULL,0);

  //cmVOR_PrintL("xM:\n",    rpt, D,N,xM);
  cmVOR_PrintL("uV: ", rpt, 1, D, uV);
  cmVOR_PrintL("sM:\n",rpt, D, D, sM);
  cmVOR_PrintL("covar:\n",rpt, D,D,scM);

}

const cmReal_t* cmCovarSrcFunc( void* p, unsigned idx )
{ return ((const cmReal_t*)p) + 3*idx; }

void cmCovarTest2(cmRpt_t* rpt )
{
  const int D    = 3;
  const int N    = 10;
  
  // each data point is in a column of xM[]
  cmReal_t  xM[] = {0.18621, 0.39466, 0.29122, 0.49663, 0.58397, 0.98434, 0.26542, 0.88850, 0.10009, 0.18815, 0.42153, 0.30218, 0.56357, 0.55696, 0.50647, 0.64502, 0.78920, 0.70395, 0.88892, 0.26669, 0.27277, 0.74299, 0.32620, 0.89648, 0.99930, 0.78351, 0.35355, 0.86343, 0.87964, 0.21095};
  
  cmReal_t sM[ D * D ];

  cmVOR_GaussCovariance2(sM,D,cmCovarSrcFunc,N,xM,NULL,NULL,0);

  cmVOR_PrintL("1 covar: ",rpt, D,D,sM);

  cmVOR_GaussCovariance(sM,D,xM,N,NULL,NULL,0);
 
  cmVOR_PrintL("2 covar: ",rpt, D,D,sM);
 
  /*
    m = [
   0.18621   0.39466   0.29122;
   0.49663   0.58397   0.98434;
   0.26542   0.88850   0.10009;
   0.18815   0.42153   0.30218;
   0.56357   0.55696   0.50647;
   0.64502   0.78920   0.70395;
   0.88892   0.26669   0.27277;
   0.74299   0.32620   0.89648;
   0.99930   0.78351   0.35355;
   0.86343   0.87964   0.21095;
   ]

  octave> cov(m)
  ans =
  0.0885575   0.0092695   0.0123219
  0.0092695   0.0546553  -0.0168115
  0.0123219  -0.0168115   0.0909342
  */
}

void cmMahalanobisTest(cmRpt_t* rpt )
{
  const int D    = 3;
  const int N    = 10;
  // each data point is in a column of xM[]
  cmReal_t  xM[] = {0.18621, 0.39466, 0.29122, 0.49663, 0.58397, 0.98434, 0.26542, 0.88850, 0.10009, 0.18815, 0.42153, 0.30218, 0.56357, 0.55696, 0.50647, 0.64502, 0.78920, 0.70395, 0.88892, 0.26669, 0.27277, 0.74299, 0.32620, 0.89648, 0.99930, 0.78351, 0.35355, 0.86343, 0.87964, 0.21095};
  cmReal_t  uV[D];
  //cmReal_t  xV[] = {0.633826, 0.349463, 0.053582 };
  cmReal_t  xV[] = {0.72477,   0.98973,   0.11622};
  cmReal_t  sM[ D * D ];

  // find the mean of the data set (mean across the columns)
  cmVOR_Mean2( uV, cmCovarSrcFunc, D, N, xM );

  cmVOR_PrintL("mean: ",rpt, 1, D, uV );

  cmVOR_GaussCovariance(sM,D,xM,N,uV,NULL,0);

  cmVOR_PrintL("covar: ",rpt, D,D,sM);

  cmReal_t* r = cmVOR_InvM(sM,D);

  cmVOR_PrintL("inv covar: ",rpt, D,D,sM);
  
  cmReal_t d = cmVOR_MahalanobisDistance( xV, D, uV, sM );

  cmRptPrintf(rpt,"Mahalanobis dist:%f %p\n",d,r);
  /*
    octave>m =
    0.18621   0.39466   0.29122
    0.49663   0.58397   0.98434
    0.26542   0.88850   0.10009
    0.18815   0.42153   0.30218
    0.56357   0.55696   0.50647
    0.64502   0.78920   0.70395
    0.88892   0.26669   0.27277
    0.74299   0.32620   0.89648
    0.99930   0.78351   0.35355
    0.86343   0.87964   0.21095

    octave> u = mean(m)
    u = 0.58396   0.58908   0.46220

    octave> y   
    y = 0.633826   0.349463   0.053582

    octave> sqrt((y-u)*inv(cov(m))*(y-u)')
    ans =  2.0322

  */
}

void cmRandIntSeqTest(cmRpt_t* rpt)
{
  unsigned vn = 10;
  unsigned v[vn];
  unsigned i = 0;
  for(i=0; i<10; ++i)
  {
    cmVOU_RandomSeq(v,vn);
    cmVOU_PrintL("v: ", rpt, 1, vn, v );
  }
  
}

//------------------------------------------------------------------------------------------------------------
void cmSynthTest()
{
  unsigned vn   = 128;
  unsigned blkN = 2;

  cmSample_t v0[ vn*blkN ];
  cmSample_t v1[ vn*blkN ];
  cmSample_t v2[ vn*blkN ];
  cmSample_t v3[ vn*blkN ];
  cmSample_t v4[ vn*blkN ];
  cmSample_t v5[ vn*blkN ];
  cmSample_t v6[ vn*blkN ];
  cmSample_t v7[ vn*blkN ];

  double   srate  = vn;
  double   hz     = 1;
  unsigned sinPhs = 0,cosPhs = 0, sqrPhs=0, sawPhs=0, triPhs=0, pulPhs=0, impPhs=0, phsPhs=0;
  unsigned otCnt  = 7;
  unsigned i;

  for(i=0; i<blkN; ++i)
  {
    sinPhs = cmVOS_SynthSine(     v0+(i*vn), ((i+1)*vn), sinPhs,  srate, hz );
    cosPhs = cmVOS_SynthCosine(   v1+(i*vn), ((i+1)*vn), cosPhs,  srate, hz );
    sqrPhs = cmVOS_SynthSquare(   v2+(i*vn), ((i+1)*vn), sqrPhs,  srate, hz, otCnt );
    sawPhs = cmVOS_SynthSawtooth( v3+(i*vn), ((i+1)*vn), sawPhs,  srate, hz, otCnt );
    triPhs = cmVOS_SynthTriangle( v4+(i*vn), ((i+1)*vn), triPhs,  srate, hz, otCnt );
    pulPhs = cmVOS_SynthPulseCos( v5+(i*vn), ((i+1)*vn), pulPhs,  srate, hz, otCnt );
    impPhs = cmVOS_SynthImpulse(  v6+(i*vn), ((i+1)*vn), impPhs,  srate, hz );
    phsPhs = cmVOS_SynthPhasor(   v7+(i*vn), ((i+1)*vn), phsPhs,  srate, hz );
  }

  cmPlotSetup("Test Proc Impl",2,1);


  cmPlotLineS( "cos",  NULL, v1, NULL, vn*blkN, NULL, kSolidPlotLineId  );
  cmPlotLineS( "sqr",  NULL, v2, NULL, vn*blkN, NULL, kSolidPlotLineId  );
  cmPlotLineS( "imp",  NULL, v6, NULL, vn*blkN, NULL, kSolidPlotLineId  );
  cmPlotLineS( "tri",  NULL, v4, NULL, vn*blkN, NULL, kSolidPlotLineId  );

  cmPlotSelectSubPlot( 1, 0 );
  cmPlotLineS( "sin",  NULL, v0, NULL, vn*blkN, NULL, kSolidPlotLineId  );
  cmPlotLineS( "saw",  NULL, v3, NULL, vn*blkN, NULL, kSolidPlotLineId  );
  cmPlotLineS( "pul",  NULL, v5, NULL, vn*blkN, NULL, kSolidPlotLineId  );
  cmPlotLineS( "phs",  NULL, v7, NULL, vn*blkN, NULL, kSolidPlotLineId  );


  cmPlotDraw();
  //cmPlotPrint(false);
}


void cmMeanVarTest(cmRpt_t* rpt )
{
  enum { cnt = 7, dim=2 };
  cmReal_t v[cnt*dim] = {0, 1,  3, 4,  6, 7,  9, 10,  12, 13,  15, 16,  18, 19 };

  cmReal_t mean;
  cmTestPrint(rpt, "sum:%f\n",  cmVOR_SumN(v,cnt, dim));
  cmTestPrint(rpt, "mean:%f\n", mean = cmVOR_MeanN(v,cnt,dim));
  cmTestPrint(rpt, "var:%f\n",  cmVOR_VarianceN(v,cnt,dim,NULL));
  cmTestPrint(rpt, "var:%f\n",  cmVOR_VarianceN(v,cnt,dim,&mean));

  unsigned rn = 3;
  unsigned cn = 5;
  cmReal_t mM[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14};
  cmReal_t aV[cn];
  cmReal_t vV[cn];

  cmVOR_MeanM(aV,mM,rn,cn,0);
  cmVOR_PrintL("mean cols: ", rpt,1, cn, aV );

  cmVOR_VarianceM(vV,mM,rn,cn,aV,0);
  cmVOR_PrintL("var cols: ", rpt,1, cn, vV );

  cmVOR_MeanM(aV,mM,rn,cn,1);
  cmVOR_PrintL("mean rows: ", rpt,1, rn, aV );

  cmVOR_VarianceM(vV,mM,rn,cn,aV,1);
  cmVOR_PrintL("var rows: ", rpt,1, rn, vV );

  
  cmVOR_VarianceM(vV,mM,rn,cn,NULL,0);
  cmVOR_PrintL("var cols: ", rpt,1, cn, vV );
  cmVOR_VarianceM(vV,mM,rn,cn,NULL,1);
  cmVOR_PrintL("var rows: ", rpt,1, rn, vV );


  cmReal_t mV[] = { 1,2,2,2 };

  cmTestPrint(rpt,"Mode:%f\n",cmVOR_Mode(mM,rn*cn));
  cmTestPrint(rpt,"Mode:%f\n",cmVOR_Mode(mV,5));

  
}

void cmMedianFilterTest( cmRpt_t* rpt )
{
  enum { xn=5 };

  unsigned wn;

  cmReal_t x[xn] = { 0, 1, 2, 3, 4 };
  cmReal_t y[xn];

  for( wn=1; wn<=7;  ++wn)
  {
    cmVOR_MedianFilt( x, xn, wn, y, 1 );
    cmTestPrint(NULL,"%i : ",wn);
    cmVOR_Print(rpt,1,xn,y);
  }
}



void cmConstQTest1( cmConstQ* p, cmRpt_t* rpt )
{
  const char*  fn  = "/home/kevin/src/ac/m2i.txt";
  cmCtx*       c   = cmCtxAlloc(NULL,rpt, cmLHeapNullHandle,cmSymTblNullHandle);
  cmMatrixBuf* m0p = cmMatrixBufAllocFile( c, NULL, fn );

  printf("target mtx:%i x %i\n", m0p->rn,     m0p->cn);
  printf("sparse mtx:%i x %i\n", p->wndSmpCnt, p->constQBinCnt);

  unsigned ri,ci;

  for(ri=0; ri<m0p->rn; ++ri)
  {
    double sum0  = 0;
    double sum1  = 0;
    double dsum  = 0;
    double md    = 0;
    unsigned mi  = -1;

    for(ci=0; ci<m0p->cn; ++ci)
    {
      double v0 = cmMatrixBufColPtr( m0p, ci)[ri];
      double v1 = cimag( p->skM[ (ci*p->wndSmpCnt)+ri ] );
      double d  = fabs(v1-v0);

      sum0  += v0;
      sum1  += v1;
      dsum  += d;

      if( d > md )
      {
        mi = ci;
        md = d;
      }

    }

    printf("%3i (%4i % 9e) % 9e%% s0:% 9e s1:% 9e\n",ri,mi,md,dsum/sum0,sum0,sum1);

  }

  cmMatrixBufFree(&m0p);
  cmCtxFree(&c);

}

void cmSRCTest( cmRpt_t* rpt )
{
  const char*  ifn          = "/home/kevin/src/ac/test0.aif"; 
  //ifn          = "/home/kevin/src/st/fv/audio/00-11-060-I-Shapeshifter-TranquilVapor.aiff";
  const char*  ofn          = "/home/kevin/src/ac/temp1.aif";
  unsigned     upFact       = 1;
  unsigned     dnFact       = 8;
  unsigned     procSmpCnt   = 64;
  unsigned     iChIdx       = 0;
  unsigned     dnProcSmpCnt = procSmpCnt / dnFact; 
  unsigned     oChCnt        = 1;
  unsigned     oBitsPerSmp   = 16;
  unsigned     oChIdx        = 0;

  cmCtx*         c   = cmCtxAlloc(NULL,rpt,cmLHeapNullHandle,cmSymTblNullHandle);
  cmAudioFileRd* arp = cmAudioFileRdAlloc( c, NULL, procSmpCnt, ifn, iChIdx, 0, cmInvalidIdx );

  if( arp != NULL )
  {
    cmSRC*         srp = cmSRCAlloc( c, NULL, arp->info.srate,procSmpCnt, upFact, dnFact );
    cmSRC*         urp = cmSRCAlloc( c, NULL, arp->info.srate/dnFact,dnProcSmpCnt, dnFact, upFact );
    cmAudioFileWr* awp = cmAudioFileWrAlloc( c, NULL, procSmpCnt, ofn, arp->info.srate, oChCnt, oBitsPerSmp);
    
    //cmSRCShow(c,srp);
    

    printf("frame cnt:%i\n",arp->info.frameCnt);

    unsigned i = 0, j=0, cnt=10000;
    while( cmAudioFileRdRead( arp ) == cmOkRC )
    {      
      if( arp->eofFl || arp->outN != procSmpCnt )
        break;

      cmSRCExec(srp,arp->outV, arp->outN);
      cmSRCExec(urp,srp->outV, srp->outN);
      cmAudioFileWrExec( awp, oChIdx, urp->outV, urp->outN );
      

      i+=arp->outN;
      if( i > procSmpCnt * cnt )
      {
        j += i;
        i = 0;
        printf("%i ",j); 
        fflush(stdout);
      }
      
    }

    printf("%i done\n",j + i);
    fflush(stdout);

    cmAudioFileWrFree(&awp);
    cmSRCFree(&urp);
    cmSRCFree(&srp);
    
  }

  cmAudioFileRdFree(&arp);
  cmCtxFree(&c);
}


void cmBeatHistTest( cmRpt_t* rpt )
{
  unsigned i; 
  cmCtx c;

  cmCtxInit(&c, rpt,cmLHeapNullHandle,cmSymTblNullHandle);

  cmPlotSetup("Beat Histogram Test",1,1);

  enum { wndN = 13 };

  unsigned frmCnt = 512;                 // df[] element count
  double   srate  = 1/.0116;             // df[] sample rate
  double   bpm    = 120;                 // beats per minute
  //unsigned spb    = floor(60*srate/bpm); // samples per beat

  cmSample_t wndV[ wndN   ];
  cmSample_t df[   frmCnt ];
  cmSample_t is[   frmCnt ];

  // create a df signal by convolving a impulse train with a hanning window
  cmVOS_HannMatlab(wndV,wndN);
  cmVOS_SynthImpulse( is, frmCnt, 0, srate, bpm/60 ); 
  cmConvolveSignal( &c, wndV, wndN, is, frmCnt, df, frmCnt );

  //cmPlotLineS( "df",  NULL, df, NULL, frmCnt, NULL, kSolidPlotLineId  );

  //cmVOS_Print(rpt, 1, 25, df );

  cmBeatHist* p = cmBeatHistAlloc(&c,NULL,frmCnt);

  //cmVOR_Print(rpt, p->frmCnt, p->maxLagCnt, p->m );

  for(i=0; i<frmCnt; ++i)
    cmBeatHistExec(p,df[i]);

  cmBeatHistCalc(p);

  cmPlotDraw();

  cmBeatHistFree(&p);
   
}


void cmOlaProcTest( cmRpt_t* rpt )
{
  cmCtx c;
  cmOla ola;
  unsigned wndSmpCnt  = 32;
  unsigned hopSmpCnt  = 8;
  unsigned procSmpCnt = 4;
  unsigned wndCnt     = 8;
  unsigned i = 0;

  cmSample_t wndV[] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1 };
  
  assert( sizeof(wndV)/sizeof(wndV[0]) == wndSmpCnt );

  cmCtxAlloc(&c, rpt,cmLHeapNullHandle,cmSymTblNullHandle);
  cmOlaAlloc(&c,&ola,wndSmpCnt,hopSmpCnt, procSmpCnt, kUnityWndId );

  for(i=0; i<wndCnt; ++i)
  {
    cmOlaExecS(&ola,wndV,wndSmpCnt);

    int j;
    for(j=0; j<hopSmpCnt/procSmpCnt; ++j)
    {
      cmVOS_Print( rpt, 1, ola.procSmpCnt, ola.outPtr );
      cmOlaExecOut(&ola);
    }
  }

  cmObjFreeStatic( cmOlaFree, cmOla, ola );
  cmObjFreeStatic( cmCtxFree, cmCtx, c );
}

void  cmTestBarkFiltMask(cmRpt_t* rpt)
{
  //                  0  1   2   3   4   5   6   7    8   9    10   11   12   13   14   15   16   17   18   19   20   21   22    23   
  //cmReal_t bl[]= {  0,100,200,300,400,510,630,770, 920,1080,1270,1480,1720,2000,2320,2700,3150,3700,4400,5300,6400,7700, 9500,12000};
  //cmReal_t bc[]= { 50,150,250,350,450,570,700,840,1000,1170,1370,1600,1850,2150,2500,2900,3400,4000,4800,5800,7000,8500,10500,13500};
  //cmReal_t bh[]= {100,200,300,400,510,630,770,920,1080,1270,1480,1720,2000,2320,2700,3150,3700,4400,5300,6400,7700,9500,12000,15500};

  //            -1   0  1   2   3   4   5   6   7    8   9    10   11   12   13   14   15   16   17   18   19   20   21   22    23    (23+1)
  cmReal_t b[]= {0, 50,150,250,350,450,570,700,840,1000,1170,1370,1600,1850,2150,2500,2900,3400,4000,4800,5800,7000,8500,10500,13500, 15500 };

  unsigned bandCnt = 24;
  unsigned binCnt  = 64;
  unsigned binHz   = 100.0;
  cmReal_t maskMtx[ bandCnt * binCnt ];
  cmReal_t stSpread = 0;


  cmVOR_TriangleMask(maskMtx, bandCnt, binCnt, b+1, binHz, stSpread, b+0, b+2 );

  cmVOR_Print( rpt, bandCnt, binCnt, maskMtx);
  
}

void cmTestFftTemplate(cmRpt_t* rpt)
{
  double   srate = 16;
  double   hz    = 1;
  unsigned n     = 16;
  unsigned otCnt = 3;

  cmCtx*   c     = cmCtxAlloc( NULL, rpt,cmLHeapNullHandle,cmSymTblNullHandle);

  // take the fft of a vector of cmSample_t values
  cmFftSR*   p0;
  cmIFftRS*  p2;
  cmSample_t vs[n];  
  cmVOS_SynthSquare( vs, n, 0, srate, hz, otCnt );
  cmVOS_PrintL( "\nsig:\n",rpt, 1, n, vs);
  p0  = cmFftAllocSR(c,NULL, vs, n, kToPolarFftFl );
  p2  = cmIFftAllocRS(c,NULL,p0->binCnt);
  cmFftExecSR( p0, NULL, 0 );  
  cmVOR_DivVS( p0->magV, p0->binCnt, p0->wndSmpCnt );
  cmVOR_PrintL( "\nmag:\n",rpt, 1, p0->binCnt, p0->magV );
  cmIFftExecPolarRS( p2, p0->magV, p0->phsV );
  cmVOS_PrintL( "\nifft:\n",rpt,1, p2->binCnt, p2->outV );
  cmIFftFreeRS(&p2);
  cmFftFreeSR(&p0);

  // take the fft of a vector of cmReal_t values
  cmFftRR*   p1;
  cmIFftRR*  p3;
  cmReal_t   vr[n];
  cmVOR_SynthSquare( vr, n, 0, srate, hz, otCnt );
  cmVOR_PrintL("\nsig\n", rpt, 1, n, vr);
  p1  = cmFftAllocRR(c,NULL, vr, n, kToPolarFftFl );
  p3  = cmIFftAllocRR(c,NULL, p1->binCnt);
  cmFftExecRR( p1, NULL, 0 );  

  cmVOR_DivVS( p1->magV, p1->binCnt, p1->wndSmpCnt );
  cmVOR_PrintL( "\nmag:\n",rpt, 1, p1->binCnt, p1->magV );
  cmIFftExecPolarRR( p3, p1->magV, p1->phsV );
  cmVOR_PrintL( "\nifft:\n",rpt,1, p3->binCnt, p3->outV );
  cmIFftFreeRR(&p3);
  cmFftFreeRR(&p1);

  cmCtxFree(&c);
} 

cmReal_t cmSelDistFunc( void* userPtr, const cmReal_t* v0, const cmReal_t* v1, unsigned vn )
{ return cmVOR_EuclidDistance(v0,v1,vn); }

void cmSelectCols(cmRpt_t* rpt)
{
  unsigned  mcn  = 10;
  unsigned  mrn  = 3;
  unsigned  selN = 3;
  cmReal_t  m0[ mrn * mcn ];
  cmReal_t  m1[ mrn * selN ];
  
  unsigned  selIdxV[ selN ];
  
  cmVOR_Random(m0,mrn*mcn,0.0,1.0);
  
  cmVOR_PrintL("\norg\n",rpt, mrn, mcn, m0 );
  
  cmVOR_SelectRandom(m1,selIdxV,selN,m0,mrn,mcn);

  cmVOR_PrintL("\nrandom\n",rpt, mrn, selN, m1 );

  cmVOU_PrintL("\nindexes\n",rpt, 1, selN, selIdxV );

  cmVOR_SelectMaxDist(m1, selIdxV, selN, m0, mrn, mcn, cmSelDistFunc, NULL ); 

  cmVOR_PrintL("\nselect max dist\n",rpt, mrn, selN, m1 );

  cmVOU_PrintL("\nindexes\n",rpt, 1, selN, selIdxV );

  cmVOR_SelectMaxAvgDist(m1, selIdxV, selN, m0, mrn, mcn, cmSelDistFunc, NULL ); 

  cmVOR_PrintL("\nselect max avg dist\n",rpt, mrn, selN, m1 );

  cmVOU_PrintL("\nindexes\n",rpt, 1, selN, selIdxV );
    
}

void cmNMF_Test( cmRpt_t* rpt )
{
  //const char* plotDev = "wxwidgets";
  int      r          = 2;
  int      n          = 100;
  int      m          = 1000;
  int      vn         = 25;
  unsigned maxIterCnt = 2000;
  unsigned convergeCnt    = 40;

  cmReal_t* V  = cmMemAllocZ( cmReal_t, n*m );
  cmReal_t* W  = cmMemAllocZ( cmReal_t, n*r );
  cmReal_t* H  = cmMemAllocZ( cmReal_t, r*m );
  cmReal_t* d  = cmMemAllocZ( cmReal_t, vn  );
  cmReal_t* t  = cmMemAllocZ( cmReal_t, vn  );
  unsigned  i;  

  cmCtx*    ctx = cmCtxAlloc(NULL,rpt,cmLHeapNullHandle,cmSymTblNullHandle);
  cmNmf_t*  nmf = cmNmfAlloc(ctx, NULL, n, m, r, maxIterCnt, convergeCnt ); 
  cmVOR_Hann(d,vn);

  // prevent zeros in V[]
  cmVOR_Random(V,n*m,0.0,0.1);


  for(i=0; i<m; ++i)
  {
    cmVOR_MultVVS( t, vn, d, (cmReal_t)i/m );
    cmVOR_AddVV(   V + (i*n), vn, t );
  }
  
  for(i=0; i<m; ++i)
  {
    cmVOR_MultVVS( t, vn, d, 1.0 - ((cmReal_t)i/m) );
    cmVOR_AddVV(   V + (i*n) + n - (vn+1), vn, t );    
  }
 
  cmNmfExec( nmf, V, m );

  //cmPlviewPlotImage(plotDev, vRptFunc, NULL, V, n, m, 0, m, 0, n );
  //cmPlviewPlotColY(plotDev, vRptFunc, NULL, nmf->W, n, r, 0, n, 0.0, 1.0 );
  //cmPlviewPlotRowY(plotDev, vRptFunc, NULL, nmf->H, r, m, 0, m, 0.0, 1.0 );

  cmNmfFree(&nmf);
  cmCtxFree(&ctx);
  cmMemPtrFree(&V);
  cmMemPtrFree(&W);
  cmMemPtrFree(&H);
  cmMemPtrFree(&d);
  cmMemPtrFree(&t);
}

void cmLinearMapTest( cmRpt_t* rpt )
{
  int sN = 4;
  int dN = 15;

  cmReal_t sV[sN];
  cmVOR_Seq( sV, sN, 0, 1 );

  cmReal_t dV[dN];

  cmVOR_LinearMap(dV, dN, sV, sN );

  cmVOR_Print(rpt, 1, dN, dV );



  int s2N = 15;
  int d2N = 4;

  cmReal_t s2V[sN];
  cmReal_t d2V[dN];
  cmVOR_Seq( s2V, s2N, 0, 1 );
  
  cmVOR_LinearMap(d2V, d2N, s2V, s2N );

  cmVOR_Print(rpt, 1, d2N, d2V );
}

void cmMemErrCallback( void* userDataPtr, const char* fmt, va_list vl )
{ vprintf(fmt,vl);}

// Test a cmProc
void cmProcTestProc(cmCtx_t* ctx )
{
  unsigned baseSymId = 1000;
  unsigned dfltBlockByteCnt = 1024;

  cmLHeapH_t  lhH = cmLHeapCreate(dfltBlockByteCnt,ctx);
  cmSymTblH_t stH = cmSymTblCreate(cmSymTblNullHandle,baseSymId,ctx);
  cmCtx*      c   = cmCtxAlloc(NULL,&ctx->rpt,lhH,stH);

  assert( cmLHeapIsValid(lhH));
  assert( cmSymTblIsValid(stH));

  //cmShiftBufTest(c);
  cmPvAnlTest(c);

  cmLHeapDestroy(&lhH);
  cmSymTblDestroy(&stH);
  cmCtxFree(&c);

}

void cmPuTest(cmCtx_t* ctx);
void  cmAudLabelFileTest(cmCtx_t* ctx);

void cmProcTestNoInit(cmCtx_t* ctx)
{
  //cmSelectCols(rpt);
  //cmNMF_Test(rpt);
  //cmGmmTest( rpt );
  //cmMvnProbTest(rpt);
  //cmShiftBufTest(rpt,NULL);
  //cmLinearMapTest(&ctx->rpt);
  //cmCovarTest2(&ctx->rpt);
  //cmMahalanobisTest(&ctx->rpt);
  //cmProcTestProc(ctx);              // test a cmProcObj
  //cmAudioFileProcTest(ctx);
  //cmAudioFileReadWriteTest(ctx);

  //cmPuTest(ctx);
  //cmFileGetLineTest(ctx);
  //cmAudLabelFileTest(ctx);

  //cmStackTest(ctx);
  //cmBinMtxFileTest(ctx );
  //cmMtxMultTest(ctx->err.rpt);
  //cmStandardizeTest(ctx);
  cmRbmBinaryTest(ctx);
}

void cmProcTestGnuPlot( cmCtx_t* ctx )
{
  cmPlotInitialize(NULL);

  cmProcTestNoInit(ctx);
  
  cmTestPrint(&ctx->rpt,"%s\n","press any key");
  cmKeyPress(NULL);
  cmPlotFinalize();

}

void cmProcTest(cmCtx_t* ctx)
{
  cmKbRecd kb;

  cmMdInitialize( ctx->guardByteCnt, ctx->alignByteCnt, ctx->mmFlags, &ctx->rpt );

  cmPlotInitialize(NULL);

  // cmAudioFileTest();
  
  // this should be added to cmCtxInit()
  //cmSetupFloatPointExceptHandler(NULL);

  //cmDelayTest();

  //cmMelTest();
  //cmDctTest();
  //cmMtxMultTest(rpt);

  //cmSonesTest();

  //cmProcImplTestSynth();
  //cmZeroCrossTest();
  //cmFIRTest();
  //cmFftTest();
  //cmFftTestComplex();
  //cmAudioFileWrTest();
  //cmSRCTest();

  //cmProcImplTest1();
  //cmFuncFilterTest();
  //cmRandomTest(rpt);
  //cmTestGaussWin();
  //cmTestMtxBuf();

  //cmDhmmTest();
  //cmGaussTest(rpt);
  //cmLaTest(rpt);
  //cmFPExceptTest();
  //cmMvnProbTest(rpt);
  //cmShiftRotateTest(rpt);

  //cmMeanVarTest(rpt);

  //cmStatsProcTest(rpt);

  //cmMedianFilterTest(rpt);

  //cmFilterTest(rpt);
  //cmBeatHistTest(rpt);

  //cmIFftTest(rpt);
  //cmConvolveTest(rpt);
  //cmBeatHistTest(rpt);

  //cmWndFuncTest(rpt);a

  //cmGmmTest( rpt );
  //cmChmmTest( rpt );

  //cmCovarTest(rpt);
  //cmRandIntSeqTest(rpt);

  //cmChordTest(rpt);
  //cmConstQTest1()
  //void cmSRCTest( rpt );

  //cmFuncFilterTest();

  //cmOlaProcTest(rpt);

  //cmTestBarkFiltMask(rpt);

  //cmTestFftTemplate(rpt);

  cmProcTestNoInit(ctx);


  cmTestPrint(&ctx->rpt,"%s\n","press any key");
  cmKeyPress(&kb);
  cmPlotFinalize();

  cmMdReport( kIgnoreNormalMmFl );
  cmMdFinalize();

}
