
typedef struct cmDotPort_str
{
  cmChar_t*             label;
  struct cmDotPort_str* link;
} cmDotPort_t;

typedef struct cmDotProc_str
{
  cmChar_t*             class;
  cmChar_t*             inst;
  
  cmDotPort_t*          list;
  struct cmDotProc_str* link;
} cmDotProc_t;

typedef struct cmDot_str
{
  cmDotProc_t* list;
} cmDot_t;


void _cmDotNewProc( cmDot_t* p, const cmChar_t* class, const cmChar_t* inst )
{
  
}

void _cmDotNewPort( cmDot_t* p, const cmChar_t* srcStr, const cmChar_t* srcPortStr, const cmChar_t* dstStr, const cmChar_t* dstPortStr )
{
}


cmDotRC_t cmDspPgmJsonToDot( cmCtx_t* ctx, const cmChar_t* inFn, const cmChar_t* outFn )
{
  cmDotRC_t rc  = kOkDotRC;
  cmJsonH_t jsH = cmJsonNullHandle;
  cmErr_t   err;
  cmJsonNode_t* arr;
    cmJsonNode_t* rp;
    const char* errLbl = NULL;
  
  cmErrSetup(&err,&ctx->rpt,"cmDspPgmJsonToDot");
  
  if( cmJsonInitializeFromFile( &jsH, inFn, ctx ) != kOkJsRC )
    return cmErrMsg(&err,kJsonFailDotRC,"The program description file '%s' could not be opened.",cmStringNullGuard(inFn));

  if((arr = cmJsonFindValue(jsH, "inst_array", NULL, kArrayTId )) == NULL )
  {
  }

  unsigned n = cmJsonChildCount(arr);
  
  for(i=0; i<n; ++i)
  {
    
    if((rp = cmJsonArrayElement(arr,i)) == NULL )
    {
    }

    cmChar_t* classStr = NULL;
    cmChar_t* instStr = NULL;
    if( cmJsonMemberValues(rp, &errLbl,
        "class", kStringTId, &classStr,
        "label", kStringTId, &instStr
        NULL ) != kOkJsRC )
    {
    }

    _cmDotNewProc( p, classStr, instStr );
    
  }

  if((arr = cmJsonFindValue(jsH, "conn_array", NULL, kArrayTId)) == NULL )
  {
  }

  unsigned n = cmJsonChildCount(arr);

  for(i=0; i<n; ++i)
  {
    if((rp = cmJsonArrayElement(arr,i)) == NULL )
    {
    }

    cmChar_t* srcStr     = NULL;
    cmChar_t* srcPortStr = NULL;
    cmChar_t* dstStr     = NULL;
    cmChar_t* dstPortStr = NULL;

    if( cmJsonMemberValues(rp, &errLbl,
        "sid",  kStringTId, &srcStr,
        "svar", kStringTId, &srcPortStr,
        "did",  kStringTId, &dstStr,
        "dvar", kStringTId, &dstPortStr,
        NULL) != kOkJsRC )
    {
    }

    _cmDotNewPort( p, srcStr, srcPortStr, dstStr, dstPortStr );
    
    
  }
  

  
 errLabel:
  cmJsonFinalize(&jsH);
  
  return rc;
}
