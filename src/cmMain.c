//( { file_desc:"Template 'main.c' for 'libcm' based program"  kw:[demo]}

#include "cmGlobal.h"
#include "cmRpt.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"

void print(void* cmRptUserPtr, const char* text)
{ printf(text); }


int main(int argc, char* argv[] )
{

  // initialize the heap check library
  bool       memDebugFl          = true;
  unsigned   memPadByteCnt       = memDebugFl ? 8 : 0;
  unsigned   memAlignByteCnt     = 16;
  unsigned   memFlags            = memDebugFl ? kTrackMmFl | kDeferFreeMmFl | kFillUninitMmFl : 0;

  cmRpt_t    rpt;
  cmRptSetup(&rpt,print,print,NULL);

  //cmMdTest(&rpt);
  //return 0;

  cmMdInitialize( memPadByteCnt, memAlignByteCnt, memFlags, &rpt );
  
  cmLHeapTest();

  cmMdReport();
  cmMdFinalize();
  return 0;
}

//)
