#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmGnuPlot.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h> // read/write/close

enum
{
  kX_PdFl       = 0x01,  // the data set contains explicit x coordinates
  kY_PdFl       = 0x02,  // the data set contains explicit y coordinates
  kZ_PdFl       = 0x04,  // the data set contains explicit z coordinates
  kImpulse_PdFl = 0x08,   // plot using the gnuplot impulse style

  kInvalidLineId = -2,
  kSolidLineId   = -1,
  kDashedLineId  = 0
  
};

typedef struct
{
  unsigned  flags;        // see kXXX_PdFl
  char*     legendStrH;   // plot legend label for this data set
  char*     colorStrH;    // string containing a gnuplot color spec
  double*   xyzMtxPtr;    // data to be plotted contained in a column major mtx with 1,2, or 3 columns
  unsigned  rn;           //
  unsigned  cn;           //
  int       lineId;       // gnuplot line style id  
  int       pointId;      // gnuplot point style id  
} cmPlotData;


//----------------------------------------------------------------------------------------------------------
enum
{
  kTitleIdx = 0,
  kXLabelIdx,
  kYLabelIdx,
  kZLabelIdx,
  kPlotStrCnt
};

enum
{
  kXMinRangeIdx,
  kXMaxRangeIdx,
  kYMinRangeIdx,
  kYMaxRangeIdx,
  kZMinRangeIdx,
  kZMaxRangeIdx,
  kRangeCnt
};

// There is one cmPlot per sub-plot.  These records are held in cmPlotPage.plotPtrArray
typedef struct
{
  cmChar_t*       strArray[ kPlotStrCnt ]; // an array of various labels and titles 
  double          range[ kRangeCnt ];      // a set of range limits for each dimension (used to automatically fill in coord values when they are not explicitely given)
  unsigned        rowIdx;                  // the plot page row index of this sub-plot
  unsigned        colIdx;                  // the plot page col index of this sub-plot
  cmPlotData**    dataPtrArray;            // pointer to data sets containing data for this sub-plot
  unsigned        dataCnt;            
} cmPlot;

//----------------------------------------------------------------------------------------------------------

// The plotter contains a single cmPlotPage (pointed to by _cmpp).
typedef struct
{
  unsigned    rowCnt;       // number of rows of sub-plots
  unsigned    colCnt;       // number of columns of sub-plots
  cmChar_t*   titleStrH;    // page title
  int         pipeFd[2];    // communication pipe with gnuplot process
  int         pid;          // process id of the gnuplot process
  cmPlot**    plotPtrArray;  // vector of sub-plots
  unsigned    plotCnt;  //
  unsigned    curPlotIdx;   // the sub-plot currently receiving plotting commands and data
} cmPlotPage;



cmPlotPage  _cmPlotPage = { 0,0,NULL,{-1,-1},-1,NULL,0,cmInvalidIdx};
cmPlotPage* _cmpp = NULL;


void _cmPrintf( int fd, const char* fmt, ... )
{
  const int bufCnt = 255;
  char buf[bufCnt+1];
  buf[bufCnt]='\0';
  va_list vl;
  va_start(vl,fmt);
  int n = vsnprintf(buf,bufCnt,fmt,vl);
  assert( n < 255 );
  
  write(fd,buf,n);

  va_end(vl);
    
}

// unexpected event signal handler
void cmPlotSignalHandler( int sig )
{
  switch( sig )
  {
    case SIGCHLD:
      if( _cmpp != NULL )
        _cmpp->pid = -1;
      break;
    case SIGBUS:
    case SIGSEGV:
    case SIGTERM:
      cmPlotFinalize();
      break;
  }
}

unsigned _cmPlotError( cmPlotPage* p, unsigned rc, bool sysErrFl, const char* fmt, ... )
{
  va_list vl;
  va_start(vl,fmt);

  fprintf(stderr,"cmPlot Error:");

  vfprintf(stderr,fmt,vl);

  
  if( sysErrFl )
    fprintf(stderr," System Msg:%s\n",strerror(errno));
  
  va_end(vl);
  return rc;
}


//----------------------------------------------------------------------------------------------------------
void _cmPlotDataInit( cmPlotData* p )
{
  p->flags      = 0;
  p->legendStrH = NULL;
  p->colorStrH  = NULL;
  p->xyzMtxPtr  = NULL;
  p->rn         = 0;
  p->cn         = 0;
  p->lineId     = kInvalidLineId;
  p->pointId    = kInvalidPlotPtId;
}

void _cmPlotDataCons( cmPlotData* p, unsigned flags, const char* legendStr, const char* colorStr, double* mtxPtr, unsigned rn, unsigned cn, unsigned styleFlags )
{
  p->flags      = flags + (cmIsFlag(styleFlags,kImpulsePlotFl) ? kImpulse_PdFl : 0);
  p->legendStrH = cmMemAllocStr(legendStr);
  p->colorStrH  = cmMemAllocStr(colorStr); 
  p->xyzMtxPtr  = mtxPtr;
  p->rn         = rn;
  p->cn         = cn;
  p->lineId     = ((styleFlags & kPlotLineMask) >> kPlotLineShift) - 2; // convert from the interface style flags to gnuplot id's 
  p->pointId    =   styleFlags & kPlotPtMask;
}

void _cmPlotDataFree( cmPlotData* p )
{
  cmMemPtrFree(&p->legendStrH);
  cmMemPtrFree(&p->colorStrH);
  //cmDM_Free(&p->xyzMtxPtr);
  cmMemPtrFree(&p->xyzMtxPtr);
}

/*
bool _cmPlotDataFreeFE( unsigned i, cmPlotData* p, void *vp )
{
  _cmPlotDataFree(p);
  return true;
}
*/


//----------------------------------------------------------------------------------------------------------
void _cmPlotInit( cmPlot* p )
{
  unsigned i;
  for(i=0; i<kPlotStrCnt; ++i)
    p->strArray[i] = NULL;

  for(i=0; i<kRangeCnt; ++i)
    p->range[i] = 0;

  p->rowIdx          = cmInvalidIdx;
  p->colIdx          = cmInvalidIdx;
  p->dataPtrArray    = NULL;
  p->dataCnt    = 0;
}

void _cmPlotCons( cmPlot* p, unsigned ri, unsigned ci )
{
  p->rowIdx          = ri;
  p->colIdx          = ci;
  assert( p->dataPtrArray == NULL );
  //p->dataPtrArray    = cmPlotDataVect_AllocEmpty();
} 

void _cmPlotInsertData( cmPlot* p, cmPlotData* rp)
{
  cmPlotData* nrp = cmMemAlloc(cmPlotData,1);
  *nrp = *rp;

  p->dataPtrArray = cmMemResizeP( cmPlotData*, p->dataPtrArray, p->dataCnt + 1 );
  p->dataPtrArray[ p->dataCnt ] = nrp;
  ++p->dataCnt;
}

void _cmPlotClearData( cmPlot* p )
{
  unsigned i;

  // release the strings 
  for(i=0; i<kPlotStrCnt; ++i)
    cmMemPtrFree(&p->strArray[i]);

  // release the plot data
  for(i=0; i<p->dataCnt; ++i)
  {
    _cmPlotDataFree( p->dataPtrArray[i] );
    cmMemPtrFree( &p->dataPtrArray[i] );
  }

  // set the data cnt to 0
  p->dataCnt = 0;
  
}


void _cmPlotFree( cmPlot* p )
{
  _cmPlotClearData(p);
  cmMemPtrFree(&p->dataPtrArray);
}


//----------------------------------------------------------------------------------------------------------
void _cmPlotPageInit( cmPlotPage* rp )
{
  rp->rowCnt      = 0;
  rp->colCnt      = 0;
  rp->titleStrH   = NULL;
  rp->pipeFd[0]   = -1;
  rp->pipeFd[1]   = -1;
  rp->pid         = -1;
  rp->plotPtrArray = NULL;
  rp->curPlotIdx  = cmInvalidIdx;
}

cmRC_t _cmPlotPageCons( cmPlotPage* rp, int pid, int inFd, int outFd, const char* terminalStr  )
{
  cmRC_t rc = kOkPlRC;

  rp->pid         = pid;
  rp->pipeFd[0]   = inFd;
  rp->pipeFd[1]   = outFd;
  rp->plotPtrArray = NULL; //cmPlotVect_AllocEmpty();

  if(terminalStr != NULL )
    _cmPrintf( outFd, "set terminal %s\n",terminalStr );

  return rc;
}

void _cmPlotPageSetup( cmPlotPage* rp, const char* title, unsigned rowCnt, unsigned colCnt )
{
  unsigned i,ri, ci;

  rp->titleStrH  = cmMemResizeStr(rp->titleStrH,title); // acStringAssign(&rp->titleStrH,title);
  rp->rowCnt     = rowCnt;
  rp->colCnt     = colCnt;
  rp->curPlotIdx = rowCnt*colCnt > 0 ? 0 : cmInvalidIdx;


  // free any resources held by each plot and empty the plot array
  for(i=0; i<rp->plotCnt; ++i)
  {
    _cmPlotFree(rp->plotPtrArray[i]);
    cmMemPtrFree( &rp->plotPtrArray[i] );
  }
  rp->plotCnt = 0;


  // insert rowCnt*colCnt blank plot records


  // allocate the plotVect[]
  rp->plotPtrArray = cmMemResizeZ( cmPlot*, rp->plotPtrArray, rowCnt*colCnt );
  rp->plotCnt = rowCnt * colCnt;


  // initialize each cmPlot record
  for(ri=0,i=0; ri<rowCnt; ++ri)
    for(ci=0; ci<colCnt; ++ci,++i)
    {
      rp->plotPtrArray[i] = cmMemAllocZ(cmPlot,1);

      _cmPlotInit( rp->plotPtrArray[i]);
      _cmPlotCons( rp->plotPtrArray[i], ri, ci);
    }
}

cmRC_t  _cmPlotPageFree( cmPlotPage* rp )
{
  unsigned i;
  cmRC_t rc = kOkPlRC;

  cmMemPtrFree(&rp->titleStrH);
  //acStringDelete( &rp->titleStrH );

  // if the plot process was successfully started  - stop it here
  if( rp->pid > 0)
  {
    int rc;
    kill(rp->pid,SIGKILL);
    wait(&rc);
    rp->pid = -1;
  }



  // close the pipe input to the plot process
  for(i=0; i<2; ++i)
  {
    if( rp->pipeFd[i] != -1 )
      if( close(rp->pipeFd[i]) == -1 )
        rc = _cmPlotError(rp,kPipeCloseFailedPlRC,true,"Pipe %i close() failed.",i);

    rp->pipeFd[i] = -1;
  }

  // deallocate the plot array
  if( rp->plotPtrArray != NULL )
  {
    for(i=0; i<rp->plotCnt; ++i)
    {
      _cmPlotFree(rp->plotPtrArray[i] );
      cmMemPtrFree(&rp->plotPtrArray[i]);
    }

    cmMemPtrFree(&rp->plotPtrArray);
    rp->plotCnt = 0;
  }
 
  return rc;
}
//----------------------------------------------------------------------------------------------------------


cmRC_t cmPlotInitialize( const char* terminalStr )
{
  cmRC_t rc = kOkPlRC;

  // if this is the first call to this function
  if( _cmpp == NULL )
  {
    struct sigaction sa;

    _cmpp = &_cmPlotPage;

    _cmPlotPageInit(_cmpp);
 
    sa.sa_handler = cmPlotSignalHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGBUS, &sa, NULL) == -1)
    {
      rc = _cmPlotError(_cmpp,kSignalFailedPlRC,true,"sigaction(SIGBUS) failed.");
      goto errLabel;
    }

    sa.sa_handler = cmPlotSignalHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
    {
      rc = _cmPlotError(_cmpp,kSignalFailedPlRC,true,"sigaction(SIGSEGV) failed.");
      goto errLabel;
    }

    sa.sa_handler = cmPlotSignalHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
      rc =  _cmPlotError(_cmpp,kSignalFailedPlRC,true,"sigaction(SIGTERM) failed.");
      goto errLabel;
    }

    sa.sa_handler = cmPlotSignalHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
      rc = _cmPlotError(_cmpp,kSignalFailedPlRC,true,"sigaction(SIGCHLD) failed.");
      goto errLabel;
    }

  }
  else // if this is the second or greater call to this function
  {
    if((rc = cmPlotFinalize()) != kOkPlRC )
      return rc;
  }

  int pipeFD[2];

  // create the pipe
  if( pipe(pipeFD) == -1 )
  {
    rc = _cmPlotError(_cmpp,kPipeFailedPlRC,true,"pipe() failed.");
    goto errLabel;
  }

  int pid;

  // create the child proces
  switch( pid = fork() )
  {
    case -1:
      printf("Error\n");
      rc = _cmPlotError(_cmpp,kForkFailedPlRC,true,"fork() failed.");
      goto errLabel;
      break;

    case 0:

        close(fileno(stdin));      // close stdin
        dup(pipeFD[0]);            // replace stdin with the pipe input


        execlp("gnuplot","gnuplot",NULL); // start gnuplot

        // under normal conditions execlp() should never return 
        rc = _cmPlotError(_cmpp,kExecFailedPlRC,true,"exec() failed.");
        
        goto errLabel;
        break;

    default:
      // normal return for parent process
      rc = _cmPlotPageCons(_cmpp,pid,pipeFD[0],pipeFD[1], terminalStr );
      break;
  }

  return rc;

 errLabel:

  cmPlotFinalize();
  return rc;
}

cmRC_t cmPlotFinalize()
{
  cmRC_t rc = kOkPlRC, rc0;
  struct sigaction sa;

  if( _cmpp == NULL )
    return kOkPlRC;

  // install some unexpected event signal handlers to clean up if the application 
  // process crashes prior to calling cmPlotFinalize(). This will prevent unconnected gnuplot
  // processes from being left in the process list.

  sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);

  if( sigaction( SIGCHLD,&sa,NULL) == -1 )
    rc =_cmPlotError(_cmpp,kSignalFailedPlRC,true,"sigaction(SIGCHLD) restore failed.");

  if( sigaction( SIGTERM,&sa,NULL) == -1 )
    rc =_cmPlotError(_cmpp,kSignalFailedPlRC,true,"sigaction(SIGTERM) restore failed.");

  if( sigaction( SIGSEGV,&sa,NULL) == -1 )
    rc =_cmPlotError(_cmpp,kSignalFailedPlRC,true,"sigaction(SIGSEGV) restore failed.");

  if( sigaction( SIGBUS,&sa,NULL) == -1 )
    rc =_cmPlotError(_cmpp,kSignalFailedPlRC,true,"sigaction(SIGBUS) restore failed.");


  // restore the child termination signal handler
  signal(SIGCHLD,SIG_DFL);

  rc0 = _cmPlotPageFree(_cmpp);

  return rc==kOkPlRC ? rc0 : rc;
}



cmRC_t cmPlotInitialize2( const char* terminalStr, const char* title, unsigned rowCnt, unsigned colCnt )
{
  cmRC_t rc;
  if((rc = cmPlotInitialize(terminalStr)) != cmOkRC )
    return rc;
  return cmPlotSetup(title,rowCnt,colCnt);
}


cmRC_t cmPlotSetup( const char* title, unsigned rowCnt, unsigned colCnt )
{   
  _cmPlotPageSetup( _cmpp,title,rowCnt,colCnt); 
  return kOkPlRC; 
}


// called to locate a cmPlot given plot row/col indexes 
unsigned _cmRowColToPlotIndex( cmPlotPage* p, unsigned ri, unsigned ci )
{
  unsigned i;

  for(i=0; i<_cmpp->plotCnt; ++i)
    if( _cmpp->plotPtrArray[i]->rowIdx==ri && _cmpp->plotPtrArray[i]->colIdx==ci )
      return i;



  _cmPlotError(_cmpp,kPlotNotFoundPlRC,false,"No plot exists at row:%i and col:%i\n",ri,ci);

  return cmInvalidIdx;
}

cmRC_t cmPlotSelectSubPlot( unsigned ri, unsigned ci )
{
  unsigned i;

  if((i= _cmRowColToPlotIndex( _cmpp, ri, ci ) ) != cmInvalidIdx )
    _cmpp->curPlotIdx = i;

  return kOkPlRC;
}

cmPlot* _cmPlotGetCurPlotPtr()
{
  if( _cmpp->curPlotIdx == cmInvalidIdx )
  {
     _cmPlotError(_cmpp,kNoCurPlotPlRC,false,"No plot exists for the current page.");
     assert(0);
     return NULL;
  };

  assert( _cmpp->curPlotIdx < _cmpp->plotCnt );
  cmPlot* p = _cmpp->plotPtrArray[_cmpp->curPlotIdx];

  assert( p != NULL );

  return p;
}

cmRC_t cmPlotSetLabels( const char* titleStr, const char* xLabelStr, const char* yLabelStr, const char* zLabelStr )
{
  cmPlot* p = _cmPlotGetCurPlotPtr();

  p->strArray[kTitleIdx]  = cmMemAllocStr( titleStr );  // acStringAssign( &p->strArray[ kTitleIdx  ], titleStr  );
  p->strArray[kXLabelIdx] = cmMemAllocStr( xLabelStr ); // acStringAssign( &p->strArray[ kXLabelIdx ], xLabelStr );
  p->strArray[kYLabelIdx] = cmMemAllocStr( yLabelStr ); // acStringAssign( &p->strArray[ kYLabelIdx ], yLabelStr );
  p->strArray[kZLabelIdx] = cmMemAllocStr( zLabelStr ); //acStringAssign( &p->strArray[ kZLabelIdx ], zLabelStr );

  return kOkPlRC;
}


cmRC_t cmPlotSetRange( double xMin, double xMax, double yMin, double yMax, double zMin, double zMax )
{
  cmPlot* p = _cmPlotGetCurPlotPtr();

  if( xMin != 0 || xMax != 0 )
  {
    p->range[ kXMinRangeIdx ] = xMin;
    p->range[ kXMaxRangeIdx ] = xMax;
  }

  if( yMin != 0 || yMax != 0 )
  {
    p->range[ kYMinRangeIdx ] = yMin;
    p->range[ kYMaxRangeIdx ] = yMax;
  }

  if( zMin != 0 || zMax != 0 )
  {
    p->range[ kZMinRangeIdx ] = zMin;
    p->range[ kZMaxRangeIdx ] = zMax;
  }

  return kOkPlRC;
  
}

void cmPlot2DLine( const float* x, const float* y, unsigned n, const char* color, int lineType, int lineWidth, int pointType )
{
  //char cmd[]  = "reset\nset size 1,1\nset origin 0,0\nset multiplot layout 1,1\nplot '-' binary array=16  format='%float' origin=(16,0) dx=10 using 1 with lines\n";
  char cmd[]  = "reset\nset size 1,1\nset origin 0,0\nset multiplot layout 1,1\nplot '-' binary record=16  format='%float' using 1:2 with lines\n";
  char cmd2[] = "unset multiplot\n";
  
  unsigned i;

  int rc = write(_cmpp->pipeFd[1],cmd,strlen(cmd));

  //int rc = fprintf(fp,"%s",cmd);

  for( i=0; i<n; ++i)
  {
    write(_cmpp->pipeFd[1],x+i,sizeof(float));  
    write(_cmpp->pipeFd[1],y+i,sizeof(float));  
  }



  write(_cmpp->pipeFd[1],cmd2,strlen(cmd2));

  printf("%i %s",rc,cmd);
}

void cmPlot2DLine1( const float* x, const float* y, unsigned n, const char* color, int lineType, int lineWidth, int pointType )
{
  //char cmd[] =  "reset\nset size 1,1\nset origin 0,0\nset multiplot layout 1,1\nplot '/home/kevin/src/gnuplot/data1.bin' binary record=16x2\n";
  char cmd[]  = "reset\nset size 1,1\nset origin 0,0\nset multiplot layout 1,1\nplot '-' binary record=16  format='%float' using 1:2 with lines\n";
  char cmd2[] = "unset multiplot\n";
  
  unsigned i;

  int rc = write(_cmpp->pipeFd[1],cmd,strlen(cmd));

  //int rc = fprintf(fp,"%s",cmd);

  for( i=0; i<n; ++i)
  {
    write(_cmpp->pipeFd[1],x+i,sizeof(float));  
    write(_cmpp->pipeFd[1],y+i,sizeof(float));  
  }



  write(_cmpp->pipeFd[1],cmd2,strlen(cmd2));

  printf("%i %s",rc,cmd);
}




/// Clear the current current subplot 
cmRC_t cmPlotClear()
{
  cmPlot*  p  = _cmPlotGetCurPlotPtr();
  _cmPlotClearData(p);
  return kOkPlRC;
}


void _cmPlotInsertDataRecd( cmPlot* p, unsigned flags, const char* legendStr, const char* colorStr, unsigned styleFlags, double* mtxPtr, unsigned rn, unsigned cn )
{  
  cmPlotData r;

  _cmPlotDataInit(&r);
  _cmPlotDataCons(&r,flags,legendStr,colorStr,mtxPtr,rn,cn,styleFlags);
  _cmPlotInsertData(p,&r);

}

cmRC_t cmPlotLineF( const char* legendStr, const float* x, const float* y, const float* z, unsigned n, const char* colorStr, unsigned styleFlags )
{
  cmPlot*      p        = _cmPlotGetCurPlotPtr();
  unsigned     flags    = 0;
  unsigned     rn       = 0;
  unsigned     ri       = 0;
  const float* array[3] = {x,y,z};
  unsigned     i;

  // determine which  data vectors were provided
  for(i=0; i<3; ++i)
    if( array[i] != NULL )
    {
      ++rn;
      switch(i)
      {
        case 0: flags = cmSetFlag(flags,kX_PdFl); break;
        case 1: flags = cmSetFlag(flags,kY_PdFl); break;
        case 2: flags = cmSetFlag(flags,kZ_PdFl); break;
        default:
          {assert(0);}
      }
  }

  // create the matrix to hold the data
  double* mtxPtr = cmMemAlloc(double,rn*n);

  // copy the data into the matrix
  for(i=0; i<3; ++i)
    if( array[i] != NULL )
    {
      unsigned ci;
      for(ci=0; ci<n; ++ci)
        mtxPtr[ ci*rn + ri ] = array[i][ci];
      
      ++ri;
    }  


  // store the a record to represent this line
  _cmPlotInsertDataRecd(p, flags, legendStr, colorStr, styleFlags, mtxPtr, rn, n ); 

  return kOkPlRC;  
}

cmRC_t cmPlotLineD( const char* legendStr, const double* x, const double* y, const double* z, unsigned n, const char* colorStr, unsigned styleFlags )
{
  cmPlot*      p        = _cmPlotGetCurPlotPtr();
  unsigned     flags    = 0;
  unsigned     rn       = 0;
  unsigned     ri       = 0;
  const double* array[3] = { x,y,z};
  unsigned     i;

  // determine wihc  data vectors were provided
  for(i=0; i<3; ++i)
    if( array[i] != NULL )
    {
      ++rn;
      switch(i)
      {
        case 0: flags = cmSetFlag(flags,kX_PdFl); break;
        case 1: flags = cmSetFlag(flags,kY_PdFl); break;
        case 2: flags = cmSetFlag(flags,kZ_PdFl); break;
        default:
          {assert(0);}
      }
  }

  // create the matrix to hold the data
  double* mtxPtr = cmMemAlloc(double,rn*n);

  // copy the data into the matrix
  for(i=0; i<3; ++i)
    if( array[i] != NULL )
    {
      unsigned ci;
      for(ci=0; ci<n; ++ci)
        mtxPtr[ ci*rn + ri ] = array[i][ci];
      
      ++ri;
    }  

  // store the a record to represent this line
  _cmPlotInsertDataRecd(p, flags, legendStr, colorStr, styleFlags, mtxPtr, rn, n ); 

  return kOkPlRC;
}

cmRC_t cmPlotLineMD( const double* x, const double* y, const double* z, unsigned rn, unsigned cn, unsigned styleFlags )
{
  cmRC_t rc;
  unsigned i;
  for(i=0; i<cn; ++i)
    if((rc = cmPlotLineD( NULL, x==NULL ? NULL : x+(i*rn), y==NULL ? NULL : y+(i*rn), z==NULL ? NULL : z+(i*rn), rn, NULL, styleFlags )) != cmOkRC )
      return rc;

  return cmOkRC;
}


inline const double* _cmPrintData( int fd, unsigned i, const double* p, double minV, double fact )
{
  if( p != NULL )
    _cmPrintf(fd,"%f ",*p++);
  else
    if( fact != 0 )
    {
      double v = minV + (fact * i );
      _cmPrintf(fd,"%f ",v);
    }  

  return p;
}


cmRC_t _cmPlotDraw(int fd, bool printDataFl )
{
  unsigned ri,ci,di;
  _cmPrintf(fd,"reset\n");
  _cmPrintf(fd,"set size 1,1\n");
  _cmPrintf(fd,"set origin 0,0\n");
  _cmPrintf(fd,"set multiplot layout %i,%i\n",_cmpp->rowCnt,_cmpp->colCnt);

  for(ri=0; ri<_cmpp->rowCnt; ++ri)
    for(ci=0; ci<_cmpp->colCnt; ++ci)
    {

      // get the plot at ri,ci
      unsigned plotIdx = _cmRowColToPlotIndex(_cmpp,ri,ci);
      assert( plotIdx != cmInvalidIdx );
      cmPlot* p = _cmpp->plotPtrArray[plotIdx];

      // get the count of data sets assigned to this plot
      unsigned dataCnt = p->dataCnt;


      if( dataCnt > 0 )
      {
        bool printPlotKeywordFl = false;

        // note which ranges are valid
        bool isXRangeFl = p->range[ kXMinRangeIdx ] != 0 || p->range[ kXMaxRangeIdx != 0 ];
        bool isYRangeFl = p->range[ kYMinRangeIdx ] != 0 || p->range[ kYMaxRangeIdx != 0 ];
        bool isZRangeFl = p->range[ kZMinRangeIdx ] != 0 || p->range[ kZMaxRangeIdx != 0 ];

        // set the plot title
         if( p->strArray[kTitleIdx] != NULL )
          _cmPrintf(fd,"set title '%s'\n",(p->strArray[kTitleIdx]));
         else
         {
           // if this is a one plot page use the page title as the plot title 
           if( _cmpp->titleStrH != NULL && _cmpp->rowCnt==1 && _cmpp->colCnt == 1 )
             _cmPrintf(fd,"set title '%s'\n", _cmpp->titleStrH );
         }

         // set the plot x label
        if( p->strArray[kXLabelIdx] != NULL )
          _cmPrintf(fd,"set xlabel '%s'\n",(p->strArray[kXLabelIdx]));

        // set the plot y label
        if(  p->strArray[kYLabelIdx] != NULL )
          _cmPrintf(fd,"set ylabel '%s'\n",(p->strArray[kYLabelIdx]));


        for(di=0; di<dataCnt; ++di)
        {
          cmPlotData* dp = p->dataPtrArray[di];

          unsigned    eleCnt    = dp->cn; //acDM_Cols(dp->xyzMtxPtr);
          unsigned    dimCnt    = dp->rn; //acDM_Rows(dp->xyzMtxPtr);

          if( eleCnt == 0 || dimCnt==0 )
            continue;          


          // must defer printing the 'plot' command until we are sure there is a non-empty matrix to print
          if( printPlotKeywordFl == false )
          {
            _cmPrintf(fd,"plot ");
            printPlotKeywordFl = true; 
          }

          bool useXRangeFl    = (cmIsFlag(dp->flags,kX_PdFl)==false) &&  isXRangeFl;
          bool useYRangeFl    = (cmIsFlag(dp->flags,kY_PdFl)==false) &&  isYRangeFl;
          bool useZRangeFl    = (cmIsFlag(dp->flags,kZ_PdFl)==false) &&  isZRangeFl; 
          bool useRangeFl     = useXRangeFl | useYRangeFl | useZRangeFl;

          _cmPrintf(fd," '-' binary %s=%i format='%%double' ", (dimCnt==1) && useRangeFl ? "array":"record",eleCnt);

          if( (dimCnt == 1) && (useXRangeFl || useYRangeFl)  )
          {
            _cmPrintf(fd," origin=(%f,%f) ", useXRangeFl ? p->range[ kXMinRangeIdx ] : 0, useYRangeFl ? p->range[ kYMinRangeIdx ] : 0);
            
            if( useXRangeFl )
              _cmPrintf(fd, " dx=%f ", (p->range[ kXMaxRangeIdx ] - p->range[ kXMinRangeIdx ]) / eleCnt );

            if( useYRangeFl )
              _cmPrintf(fd, " dy=%f ", (p->range[ kYMaxRangeIdx ] - p->range[ kYMinRangeIdx ]) / eleCnt );

            _cmPrintf(fd," using %i ", 1 );            
          }  
          else
            _cmPrintf(fd," using %i:%i ", dimCnt==1 ? 0 : 1, dimCnt==1 ? 1 : 2 );

          if( dp->legendStrH != NULL )
            _cmPrintf(fd," title '%s' ", dp->legendStrH);
          else
            _cmPrintf(fd, " notitle ");



          bool impulseFl = cmIsFlag(dp->flags,kImpulse_PdFl );
          
          if( impulseFl || (dp->lineId != kInvalidLineId) || (dp->pointId != kInvalidPlotPtId) )
          {

            _cmPrintf(fd," with ");

            if( impulseFl )
              _cmPrintf(fd,"impulses");
            else
            {
              if( dp->lineId != kInvalidLineId )
                _cmPrintf(fd,"lines");

              if( dp->pointId != kInvalidPlotPtId )
                _cmPrintf(fd,"points pt %i ", dp->pointId);
            }

            if( dp->colorStrH != NULL )
              _cmPrintf(fd," lt rgb '%s' ",  dp->colorStrH );
              
          }
           
          if( di+1 < dataCnt )
            _cmPrintf(fd,",");




          else
          {
            _cmPrintf(fd,"\n");

            // for each data set contained in this plot
            for(di=0; di<dataCnt; ++di)
            {
              cmPlotData*    dp  = p->dataPtrArray[di];
              //acDM*          mp  = dp->xyzMtxPtr;
              unsigned       eleCnt   = dp->cn; //acDM_Cols(mp);
              const double*  ddp      = dp->xyzMtxPtr; //acDM_ConstPtr(mp);

              // if we are printing the output to the console instead of sending it too gnuplot
              if( fd == fileno(stdout) )
              {
                if( printDataFl )
                {
                  unsigned       i   = 0;
                  for(i=0; i<eleCnt; ++i )
                    ddp=_cmPrintData( fd, i,ddp, 0, 0 );
                }
              }
              else  
              {
                // Note: each row contains a of the matrix contains the data for a given dimension
                // (e.g. x coords are in row 0, y coords are in row 1, etc). If the matrix contains
                // multiple rows then writing the matrix memory buffer out linearly will result
                // in interleaving the coordinates as: x0 y0 x1 y1 x2 y2 .... 
                write(fd,ddp,dp->rn*eleCnt*sizeof(double));
              }
              
            }

          }

        } // if dataCnt > 0
      } // for ci  
    } // for ri

      /*
  _cmPrintf(fd,"\n");

  for(ri=0; ri<_cmpp->rowCnt; ++ri)
    for(ci=0; ci<_cmpp->colCnt; ++ci)
    {
      // get the plot at ri,ci
      cmPlot* p = cmPlotVect_Ptr(_cmpp->plotPtrArray,_acRowColToPlotIndex(_cmpp,ri,ci));

      // get the count of data sets assigned to this plot
      unsigned dataCnt = cmPlotDataVect_Count(p->dataPtrArray);

      // for each data set contained in this plot
      for(di=0; di<dataCnt; ++di)
      {
        cmPlotData*    dp  = cmPlotDataVect_Ptr(p->dataPtrArray,di);
        acDM*          mp  = dp->xyzMtxPtr;
        unsigned       eleCnt   = acDM_Cols(mp);
        const double*  ddp = acDM_ConstPtr(mp);

        // if we are printing the output to the console instead of sending it too gnuplot
        if( fd == fileno(stdout) )
        {
          if( printDataFl )
          {
            unsigned       i   = 0;
            for(i=0; i<eleCnt; ++i )
              ddp=_acPrintData( fd, i,ddp, 0, 0 );
          }
        }
        else  
        {
          // Note: each row contains a of the matrix contains the data for a given dimension
          // (e.g. x coords are in row 0, y coords are in row 1, etc). If the matrix contains
          // multiple rows then writing the matrix memory buffer out linearly will result
          // in interleaving the coordinates as: x0 y0 x1 y1 x2 y2 .... 
          write(fd,ddp,acDM_Rows(mp)*eleCnt*sizeof(double));
        }
      }
    }
      */
  _cmPrintf(fd,"\nunset multiplot\n");
  return kOkPlRC;
}


cmRC_t cmPlotDraw()
{
  return  _cmPlotDraw(_cmpp->pipeFd[1],false);

}

cmRC_t cmPlotPrint( bool printDataFl )
{
  return _cmPlotDraw(fileno(stdout),printDataFl);
}

cmRC_t cmPlotDrawAndPrint( bool printDataFl )
{
  cmPlotDraw();
  return _cmPlotDraw(fileno(stdout),printDataFl);
}


// ncl - min column value
// nch - max column value
// nrl - min row    value
// nrh - max row    value

int fwrite_matrix(FILE* fout, float**m, int nrl, int nrh, int ncl, int nch, float* row_title, float* column_title)

{
  int    j;
  float  length;
  int    col_length;
  int    status;
  

  // calc the number of columns
  length = (float)(col_length = nch-ncl+1);

  printf("cols:%i %f\n",col_length,length);

  // write the number of columns 
  if((status = fwrite((char*)&length,sizeof(float),1,fout))!=1)
  {
    fprintf(stderr,"fwrite 1 returned %d\n",status);
    return(0);
  }

  // write the column titles 
  fwrite((char*)column_title,sizeof(float),col_length,fout);


  // write the row_title followed by the data on each line
  for(j=nrl; j<=nrh; j++)
  {

    fwrite( (char*)&row_title[j], sizeof(float), 1,         fout);
    fwrite( (char*)(m[j]+ncl),    sizeof(float), col_length,fout);
    printf("%i %li\n",j,ftell(fout));
  }


  return(1);
}



// Generate a 'matrix-binary' data file for use with: plot "data0.bin" binary
void cmPlotWriteBinaryMatrix()
{
  const char fn[] = "/home/kevin/src/gnuplot/data0.bin";
  
  unsigned rn    = 2;             // row cnt
  unsigned cn    = 16;            // col cnt
  float    srate = 16;
  float    d[rn*cn];
  //float*   m[rn];
  float    c[cn];
  //float    r[rn];
  unsigned i;
  
  for(i=0; i<cn; ++i)
  {
    d[i]      = (float)sin(2*M_PI*i/srate);
    d[cn + i] = (float)cos(2*M_PI*i/srate);
    c[i]      = i;
  } 

  //m[0] = d;
  //r[0] = 0;

  FILE* fp = fopen(fn,"wb");
  float fcn = cn;

  fwrite((char*)&fcn,sizeof(fcn),1,fp);  // write the count of columns
  fwrite((char*)c,sizeof(float),cn,fp);  // write the column labels

  for(i=0; i<rn; ++i)
  {
    fwrite((char*)&i,sizeof(float),1,fp); // write the row label
    fwrite((char*)(d +(i*cn)),sizeof(float),cn,fp); // write a data row
  }


  fclose(fp);  
}


// Generate a 'matrix-binary' data file for use with: plot "data1.bin" binary record=16x2 using 1:2
void cmPlotWriteBinaryGeneralExample()
{
  const char fn[] = "/home/kevin/src/gnuplot/data1.bin";
  
  unsigned rn  = 16;           // row cnt
  unsigned cn  = 2;            // col cnt
  float    srate     = 16;
  float    d[rn*cn];
  unsigned i;
  
  for(i=0; i<rn; ++i)
  {
    d[(i*cn)+0] = (float)cos(2*M_PI*i/srate);
    d[(i*cn)+1] = (float)sin(2*M_PI*i/srate);
   
  } 

  FILE* fp = fopen(fn,"wb");
  fwrite((char*)d,sizeof(float),rn*cn,fp);
  fclose(fp);
  
}

