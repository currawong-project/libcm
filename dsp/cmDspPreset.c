#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmLinkedHeap.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmFileSys.h"
#include "cmDspValue.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspPreset.h"
#include "cmLex.h"
#include "cmCsv.h"

const cmChar_t* _cmDspPresetGroupLabelStr(cmDspPresetMgr_t* p, _cmDspPresetGrp_t* gp )
{ return cmStringNullGuard(cmSymTblLabel(p->stH,gp->symId)); }

const cmChar_t* _cmDspPresetLabelStr(cmDspPresetMgr_t* p, _cmDspPresetPre_t* pp )
{ return cmStringNullGuard(cmSymTblLabel(p->stH,pp->symId)); }

const cmChar_t* _cmDspPresetInstLabelStr(cmDspPresetMgr_t* p, _cmDspPresetInst_t* ip )
{ return cmStringNullGuard(cmSymTblLabel(p->stH,ip->symId)); }

const cmChar_t* _cmDspPresetVarLabelStr(cmDspPresetMgr_t* p, _cmDspPresetVar_t* vp )
{ return cmStringNullGuard(cmSymTblLabel(p->stH,vp->symId)); }

void _cmDspPresetAlloc( cmDspPresetMgr_t* p )
{
  p->err           = NULL;
  p->lhH           = cmLHeapNullHandle;
  p->stH           = cmSymTblNullHandle;
  p->list          = NULL;
  p->gp            = NULL;
  p->dfltPathJsFn  = NULL;
  p->dfltPathCsvFn = NULL;
}

cmDspRC_t _cmDspPresetLoad( cmDspPresetMgr_t* p, cmCtx_t* ctx, cmErr_t* err, cmLHeapH_t lhH, cmSymTblH_t stH, const cmChar_t* fnPrefixStr )
{
  cmDspRC_t rc = kOkDspRC;

  p->err           = err;
  p->lhH           = lhH;
  p->stH           = stH;
  p->list          = NULL;
  p->gp            = NULL;
  p->dfltPathJsFn  = NULL;
  p->dfltPathCsvFn = NULL;
  
  const cmChar_t* path;
  const cmChar_t fnSuffixStr[] = "_preset";
  unsigned fnN = strlen(fnPrefixStr) + strlen(fnSuffixStr) + 1;
  cmChar_t fn[ fnN ];
  strcpy(fn,fnPrefixStr);
  strcat(fn,fnSuffixStr);
  assert( strlen(fn) == fnN - 1 );

  // form JSON preset file name
  if((path  = cmFsMakeFn(cmFsPrefsDir(),fn,"js",NULL)) == NULL )
    return cmErrMsg(p->err,kFileSysFailDspRC,"Default preset JSON file name formation failed.");

  p->dfltPathJsFn = cmLhAllocStr(p->lhH,path);

  cmFsFreeFn(path);

  // form CSV preset file name
  if((path  = cmFsMakeFn(cmFsPrefsDir(),fn,"csv",NULL)) == NULL )
    return cmErrMsg(p->err,kFileSysFailDspRC,"Default preset CSV file name formation failed.");

  p->dfltPathCsvFn = cmLhAllocStr(p->lhH,path);

  cmFsFreeFn(path);

  // read JSON preset file
  if( cmFsIsFile(p->dfltPathJsFn) )
    if((rc = _cmDspPresetRead(p,ctx,p->dfltPathJsFn)) != kOkDspRC )
      return rc;

  return kOkDspRC;
}

bool _cmDspPresetIsInitialized( cmDspPresetMgr_t* p )
{
  return p->err != NULL && cmLHeapIsValid(p->lhH) && cmSymTblIsValid(p->stH);
}

cmDspRC_t _cmDspPresetUnload(  cmDspPresetMgr_t* p, cmCtx_t* ctx )
{
  cmDspRC_t rc;
  if( _cmDspPresetIsInitialized(p) )
  {
    if((rc = _cmDspPresetWrite(p,ctx,p->dfltPathJsFn)) != kOkDspRC )
      cmErrMsg(p->err,rc,"DSP Preset JSON write on unload failed.");

    if((rc = _cmDspPresetWriteCsv(p,ctx,p->dfltPathCsvFn)) != kOkDspRC )
      cmErrMsg(p->err,rc,"DSP Preset CSV write on unload failed.");

  }
  _cmDspPresetAlloc(p);

  return kOkDspRC;
}

/*
{
presetGroupArray:
[
  {
    group: "myGroup"
    presetArray:
    [
      {
      preset:"myPreset"
      instArray:
      [
        {
          inst:"myInst"
          varArray:
          [
            {
               var:"myVar"
               value:<value>
            }
          ]
        }
      ]
      }
    ]
  }
]
 
}
 */

  cmDspRC_t _cmDspPresetRdErr( cmDspPresetMgr_t* p, cmJsRC_t jsRC, const cmChar_t* errLabel, const cmChar_t* msg )
{
  if( jsRC == kNodeNotFoundJsRC )
    return cmErrMsg(p->err,kJsonFailDspRC,"The JSON node '%s' could not be found while reading the preset %s.",cmStringNullGuard(errLabel),cmStringNullGuard(msg));
  
  return cmErrMsg(p->err,kJsonFailDspRC,"JSON preset read failed on '%s'.",cmStringNullGuard(msg));

}

cmDspRC_t _cmDspPresetRead(  cmDspPresetMgr_t* p, cmCtx_t* ctx, const cmChar_t* fn )
{
  cmDspRC_t     rc  = kOkDspRC;
  cmJsonH_t     jsH = cmJsonNullHandle;
  cmJsonNode_t* pga, *pa, *ia, *va;
  unsigned      gi,pi,ii,vi;
  cmJsRC_t      jsRC;
  const cmChar_t* errLabelPtr = NULL;

  if( cmJsonInitializeFromFile(&jsH,fn,ctx) != kOkJsRC )
    return cmErrMsg(p->err,kJsonFailDspRC,"The JSON preset file '%s' could not be opened.",cmStringNullGuard(fn));

  if((pga = cmJsonFindValue(jsH,"presetGroupArray",NULL,kArrayTId)) == NULL )
  {
    rc = cmErrMsg(p->err,kJsonFailDspRC,"JSON preset read failed. The 'presetGroupArray' could not be found.");
    goto errLabel;
  }

  // for each group
  for(gi=0; gi<cmJsonChildCount(pga); ++gi)
  {
    cmChar_t* groupLabel = NULL;

    // read the group header
    if(( jsRC = cmJsonMemberValues(cmJsonArrayElementC(pga,gi), &errLabelPtr, 
          "group",       kStringTId,&groupLabel,
          "presetArray", kArrayTId, &pa,
          NULL )) != kOkJsRC )
    {
      rc = _cmDspPresetRdErr(p,jsRC,errLabelPtr,"group object");
      goto errLabel;
    }

    // for each preset in this group
    for(pi=0; pi<cmJsonChildCount(pa); ++pi)
    {
      cmChar_t* presetLabel = NULL;

      // read the preset header
      if(( jsRC = cmJsonMemberValues(cmJsonArrayElementC(pa,pi), &errLabelPtr, 
            "preset",    kStringTId, &presetLabel,
            "instArray", kArrayTId,  &ia,
            NULL )) != kOkJsRC )
      {
        rc = _cmDspPresetRdErr(p,jsRC,errLabelPtr,"preset object");
        goto errLabel;
      }

      // create the preset record
      if((rc = _cmDspPresetCreatePreset(p,groupLabel,presetLabel)) != kOkDspRC )
        goto errLabel;

      // for each instance in this preset
      for(ii=0; ii<cmJsonChildCount(ia); ++ii)
      {
        cmChar_t* instLabel = NULL;
        
        // read the instance header
        if(( jsRC = cmJsonMemberValues(cmJsonArrayElementC(ia,ii), &errLabelPtr, 
              "inst",     kStringTId,&instLabel,
              "varArray", kArrayTId, &va,
              NULL )) != kOkJsRC )
        {
          rc = _cmDspPresetRdErr(p,jsRC,errLabelPtr,"instance object");
          goto errLabel;
        }

        // create the preset instance record
        if(( rc = _cmDspPresetCreateInstance(p, cmSymTblRegisterSymbol(p->stH,instLabel) )) != kOkDspRC )
          goto errLabel;

        // for each var
        for(vi=0; vi<cmJsonChildCount(va); ++vi)
        {
          const cmChar_t* varLabel = NULL;
          const cmJsonNode_t* vnp;
          cmDspValue_t value;

          // get the var obj 
          const cmJsonNode_t* obp = cmJsonArrayElementC(va,vi);
          assert( obp->typeId == kObjectTId );

          // get the var label
          if( cmJsonStringMember(obp,"var",&varLabel) != kOkJsRC || varLabel==NULL )
            rc = cmErrMsg(p->err,kJsonFailDspRC,"A preset var label could not be read in group:%s preset:%s inst:%s var index:%i.",_cmDspPresetGroupLabelStr(p,p->gp),_cmDspPresetLabelStr(p,p->gp->pp),_cmDspPresetInstLabelStr(p,p->gp->pp->ip),vi);
         
          // fine the value node
          if(( vnp = cmJsonFindValue(jsH,"value",obp,kInvalidTId)) == NULL )
            rc = cmErrMsg(p->err,kJsonFailDspRC,"A preset value label could not be read in group:%s preset:%s inst:%s var index:%i.",_cmDspPresetGroupLabelStr(p,p->gp),_cmDspPresetLabelStr(p,p->gp->pp),_cmDspPresetInstLabelStr(p,p->gp->pp->ip),vi);

          switch( vnp->typeId )
          {
            case kTrueTId:
              cmDsvSetBool(&value,true);
              break;

            case kFalseTId:
              cmDsvSetBool(&value,false);
              break;

            case kRealTId:
              cmDsvSetDouble(&value,vnp->u.realVal);
              break;

            case kIntTId:
              cmDsvSetInt(&value,vnp->u.intVal);
              break;
              
            case kStringTId:
              cmDsvSetStrz(&value,cmLhAllocStr(p->lhH,cmStringNullGuard(vnp->u.stringVal)));
              break;
              
            default:
              {
                rc = cmErrMsg(p->err,kJsonFailDspRC,"An invalid JSON type (%i) was encountered while reading preset group:%s preset:%s inst:%s var index:%i.",_cmDspPresetGroupLabelStr(p,p->gp),_cmDspPresetLabelStr(p,p->gp->pp),_cmDspPresetInstLabelStr(p,p->gp->pp->ip),vi);
                goto errLabel;
              }
          }

          // create the var preset recd
          if((rc = _cmDspPresetCreateVar(p,cmSymTblRegisterSymbol(p->stH,varLabel),&value)) != kOkDspRC )
            goto errLabel;          
        }

      }

    }

  }

 errLabel:
  if( cmJsonFinalize(&jsH) != kOkJsRC )
    rc = cmErrMsg(p->err,kJsonFailDspRC,"The JSON preset tree finalization failed.");
  

  return rc;
}


 // return ptr to array node
  cmJsonNode_t*  _cmDspPresetWriteArrObj( 
    cmDspPresetMgr_t* p,
    cmJsonH_t       jsH,
    cmJsonNode_t*   parentPtr,
    const cmChar_t* label,
    const cmChar_t* labelValue,
    const cmChar_t* arrLabel )

{
  cmJsonNode_t* obp = NULL;
  cmJsonNode_t* anp = NULL;

  if( (obp = cmJsonCreateObject(jsH,parentPtr)) == NULL )
  {
    cmErrMsg(p->err,kJsonFailDspRC,"JSON object created failed during preset write of %s:%s.",cmStringNullGuard(label),cmStringNullGuard(labelValue));
     goto errLabel;
  }

  if( cmJsonInsertPairString(jsH,obp,label,labelValue) != kOkJsRC )
  {
     cmErrMsg(p->err,kJsonFailDspRC,"JSON pair w/ string create failed during preset write of %s:%s.",cmStringNullGuard(label),cmStringNullGuard(labelValue));
     goto errLabel;
  }

  if( (anp = cmJsonInsertPairArray(jsH,obp,arrLabel)) == NULL )
  {
    cmErrMsg(p->err,kJsonFailDspRC,"JSON pair w/ array create failed during preset write of %s:%s.",cmStringNullGuard(label),cmStringNullGuard(labelValue));
    goto errLabel;
  }

 errLabel:

  return anp;
}

cmDspRC_t _cmDspPresetWrite( cmDspPresetMgr_t* p, cmCtx_t* ctx, const cmChar_t* fn )
{
  cmDspRC_t           rc  = kOkDspRC;
  cmJsonH_t           jsH = cmJsonNullHandle;
  cmJsonNode_t*       pga, *pa, *ia, *va; 
  _cmDspPresetGrp_t*  gp;
  _cmDspPresetPre_t*  pp;
  _cmDspPresetInst_t* ip;
  _cmDspPresetVar_t*  vp;

  
  if( cmJsonInitialize(&jsH,ctx) != kOkJsRC )
    return cmErrMsg(p->err,kJsonFailDspRC,"JSON tree initialization failed during preset writing.");
  
  // create the root object in the blank tree
  if( cmJsonCreateObject(jsH,NULL) == NULL )
  {
   rc = cmErrMsg(p->err,kJsonFailDspRC,"JSON preset write failed while creating the root object.");
    goto errLabel;
  }
  
  // create the root presetGroupArray
  if((pga = cmJsonInsertPairArray(jsH, cmJsonRoot(jsH), "presetGroupArray" )) == NULL )
  {
    rc = cmErrMsg(p->err,kJsonFailDspRC,"JSON preset write failed on 'presetGroupArray'.");
    goto errLabel;
  }

  rc = kJsonFailDspRC;

  // for each group
  for(gp=p->list; gp!=NULL; gp=gp->link)
  {
    // create the group object and presetArray
    if((pa = _cmDspPresetWriteArrObj(p,jsH,pga,"group",_cmDspPresetGroupLabelStr(p,gp),"presetArray")) == NULL )
      goto errLabel;

    // for each preset
    for(pp=gp->list; pp!=NULL; pp=pp->link)
    {
      // create the preset object and instArray
      if((ia = _cmDspPresetWriteArrObj(p,jsH,pa,"preset",_cmDspPresetLabelStr(p,pp),"instArray")) == NULL )
        goto errLabel;
      
      // for each inst
      for(ip=pp->list; ip!=NULL; ip=ip->link)
      {
        // create the inst object and varArray
        if((va = _cmDspPresetWriteArrObj(p,jsH,ia,"inst",_cmDspPresetInstLabelStr(p,ip),"varArray")) == NULL )
          goto errLabel;

        // for each var
        for(vp=ip->list; vp!=NULL; vp=vp->link)
        {
          // create the var object
          cmJsonNode_t* obp;
          if((obp = cmJsonCreateObject(jsH,va)) == NULL )
          {
            cmErrMsg(p->err,kJsonFailDspRC,"JSON preset write failed during var object create.");
            goto errLabel;
          }

          // insert the var label 
          if( cmJsonInsertPairString(jsH,obp,"var",_cmDspPresetVarLabelStr(p,vp)) != kOkJsRC )
          {
            cmErrMsg(p->err,kJsonFailDspRC,"JSON preset write failed during var label create.");
            goto errLabel;
          }

          assert( cmDsvIsMtx(&vp->value) == false && cmDsvIsJson(&vp->value) == false );

          // determine the var value type - and write the var value
          unsigned tid =  cmDsvBasicType(&vp->value);
          
          switch(tid)
          {
            case kBoolDsvFl:
              if( cmJsonInsertPairBool(jsH,obp,"value",cmDsvGetBool(&vp->value)) != kOkJsRC )
              {
                cmErrMsg(p->err,kJsonFailDspRC,"JSON preset write failed on 'bool' value.");
                goto errLabel;
              }
              break;

            case kStrzDsvFl:
              if( cmJsonInsertPairString(jsH,obp,"value",cmDsvGetStrcz(&vp->value)) != kOkJsRC )
              {
                cmErrMsg(p->err,kJsonFailDspRC,"JSON preset write failed on 'string' value.");
                goto errLabel;
              }
              break;

            case kFloatDsvFl:
            case kDoubleDsvFl:
            case kRealDsvFl:
            case kSampleDsvFl:
              if( cmJsonInsertPairReal(jsH,obp,"value",cmDsvGetDouble(&vp->value)) != kOkJsRC )
              {
                cmErrMsg(p->err,kJsonFailDspRC,"JSON preset write failed on 'double' value.");
                goto errLabel;
              }

              break;

            default:
              {
                if( cmDsvCanConvertFlags(kIntDsvFl,tid) )
                {
                  if( cmJsonInsertPairInt(jsH,obp,"value",cmDsvGetInt(&vp->value)) != kOkJsRC )
                  {
                    cmErrMsg(p->err,kJsonFailDspRC,"JSON preset write failed on 'int' value.");
                    goto errLabel;
                  }
                }
                else
                {
                  rc = cmErrMsg(p->err,kJsonFailDspRC,"Unable to convert DSV type 0x%x to JSON type.",tid);
                  goto errLabel;
                }
              }
              break;
          } // switch

        }
      }
    }
  }

  // write the JSON tree
  if( cmJsonWrite(jsH, cmJsonRoot(jsH), fn ) != kOkJsRC )
  {
    rc = cmErrMsg(p->err,kJsonFailDspRC,"JSON preset write failed.");
    goto errLabel;
  }

  rc = kOkDspRC;

 errLabel:
  if( cmJsonFinalize(&jsH) != kOkJsRC )
    rc = cmErrMsg(p->err,kJsonFailDspRC,"JSON tree finalization failed during preset writing.");

  return rc;
}

cmDspRC_t _cmDspPresetWriteCsv( cmDspPresetMgr_t* p, cmCtx_t* ctx, const cmChar_t* fn )
{
  cmDspRC_t           rc  = kOkDspRC;
  cmCsvH_t            csvH   = cmCsvNullHandle;
  _cmDspPresetGrp_t*  gp;
  _cmDspPresetPre_t*  pp;
  _cmDspPresetInst_t* ip;
  _cmDspPresetVar_t*  vp;

  if( cmCsvInitialize(&csvH,ctx) != kOkCsvRC )
    return cmErrMsg(p->err,kCsvFailDspRC,"CSV initialization failed during preset writing.");
  
  // for each group
  for(gp=p->list; gp!=NULL; gp=gp->link)
  {
    

    // for each preset
    for(pp=gp->list; pp!=NULL; pp=pp->link)
    {
      
      
      // for each inst
      for(ip=pp->list; ip!=NULL; ip=ip->link)
      {
        // for each var
        for(vp=ip->list; vp!=NULL; vp=vp->link)
        {
          

          assert( cmDsvIsMtx(&vp->value) == false && cmDsvIsJson(&vp->value) == false );

          cmCsvCell_t* cellPtr = NULL;
          unsigned     lexTId  = 0;

          if( cmCsvAppendRow(csvH, &cellPtr, cmCsvInsertSymText(csvH,_cmDspPresetGroupLabelStr(p,gp)), kStrCsvTFl, lexTId ) != kOkCsvRC )
          {
            rc = cmErrMsg(p->err,kCsvFailDspRC,"CSV create failed during 'group' name insertion.");
            goto errLabel;
          }

          if( cmCsvInsertColAfter(csvH, cellPtr, &cellPtr, cmCsvInsertSymText(csvH,_cmDspPresetLabelStr(p,pp)), kStrCsvTFl, lexTId ) != kOkCsvRC )
          {
            rc = cmErrMsg(p->err,kCsvFailDspRC,"CSV create failed during 'Preset' name insertion.");
            goto errLabel;
          }

          if( cmCsvInsertColAfter(csvH, cellPtr, &cellPtr, cmCsvInsertSymText(csvH,_cmDspPresetInstLabelStr(p,ip)), kStrCsvTFl, lexTId ) != kOkCsvRC )
          {
            rc = cmErrMsg(p->err,kCsvFailDspRC,"CSV create failed during 'inst' name insertion.");
            goto errLabel;
          }

          if( cmCsvInsertColAfter(csvH, cellPtr, &cellPtr, cmCsvInsertSymText(csvH,_cmDspPresetVarLabelStr(p,vp)), kStrCsvTFl, lexTId ) != kOkCsvRC )
          {
            rc = cmErrMsg(p->err,kCsvFailDspRC,"CSV create failed during 'inst' name insertion.");
            goto errLabel;
          }

          // determine the var value type - and write the var value
          unsigned tid =  cmDsvBasicType(&vp->value);
          
          switch(tid)
          {
            case kBoolDsvFl:
              if( cmCsvInsertIntColAfter( csvH, cellPtr, &cellPtr, cmDsvGetInt(&vp->value), lexTId ) != kOkCsvRC )
              {
                cmErrMsg(p->err,kCsvFailDspRC,"CSV preset write failed on 'bool' value.");
                goto errLabel;
              }
              break;

            case kStrzDsvFl:
              if( cmCsvInsertQTextColAfter( csvH, cellPtr, &cellPtr, cmDsvGetStrcz(&vp->value), lexTId ) != kOkCsvRC )
              {
                cmErrMsg(p->err,kCsvFailDspRC,"CSV preset write failed on 'string' value.");
                goto errLabel;
              }
              break;

            case kFloatDsvFl:
            case kDoubleDsvFl:
            case kRealDsvFl:
            case kSampleDsvFl:
              if( cmCsvInsertDoubleColAfter( csvH, cellPtr, &cellPtr, cmDsvGetDouble(&vp->value), lexTId ) != kOkCsvRC )
              {
                cmErrMsg(p->err,kCsvFailDspRC,"CSV preset write failed on 'double' value.");
                goto errLabel;
              }

              break;

            default:
              {
                if( cmDsvCanConvertFlags(kIntDsvFl,tid) )
                {
                  if( cmCsvInsertIntColAfter( csvH, cellPtr, &cellPtr, cmDsvGetInt(&vp->value), lexTId ) != kOkCsvRC )
                  {
                    cmErrMsg(p->err,kCsvFailDspRC,"CSV preset write failed on 'int' value.");
                    goto errLabel;
                  }
                }
                else
                {
                  rc = cmErrMsg(p->err,kCsvFailDspRC,"Unable to convert DSV type 0x%x to CSV type.",tid);
                  goto errLabel;
                }
              }
              break;
          } // switch


        }
      }
    }
  }

  // write the JSON tree
  if( cmCsvWrite(csvH, fn ) != kOkCsvRC )
  {
    rc = cmErrMsg(p->err,kCsvFailDspRC,"CSV preset write failed.");
    goto errLabel;
  }

  rc = kOkDspRC;

 errLabel:
  if( cmCsvFinalize(&csvH) != kOkJsRC )
    rc = cmErrMsg(p->err,kCsvFailDspRC,"CSV finalization failed during preset writing.");

  return rc;
}


_cmDspPresetVar_t*  _cmDspPresetFindVar( _cmDspPresetInst_t* ip, unsigned varSymId )
{
  _cmDspPresetVar_t* vp = ip->list;
  for(; vp!=NULL; vp=vp->link)
    if( vp->symId == varSymId )
      return vp;
  return NULL;
}

_cmDspPresetInst_t* _cmDspPresetFindInst( _cmDspPresetPre_t* pp, unsigned instSymId )
{
  _cmDspPresetInst_t* ip = pp->list;
  for(; ip!=NULL; ip=ip->link)
    if( ip->symId == instSymId )
      return ip;
  return NULL;
}

_cmDspPresetPre_t*  _cmDspPresetFindPreset( _cmDspPresetGrp_t* gp, unsigned preSymId )
{
  _cmDspPresetPre_t* pp = gp->list;
  for(; pp!=NULL; pp=pp->link)
    if( pp->symId == preSymId )
      return pp;
  return NULL;
} 

_cmDspPresetGrp_t*  _cmDspPresetFindGroup( cmDspPresetMgr_t* p, unsigned grpSymId )
{
  _cmDspPresetGrp_t* gp = p->list;
  for(; gp != NULL; gp = gp->link)
    if( gp->symId == grpSymId )
      return gp;
  return NULL;
}


unsigned        _cmDspPresetGroupCount( cmDspPresetMgr_t* p )
{
  unsigned cnt = 0;
  _cmDspPresetGrp_t* gp = p->list;
  for(; gp!=NULL; gp=gp->link)
    ++cnt;
  return cnt;
}

_cmDspPresetGrp_t* _cmDspPresetGroupFromIndex( cmDspPresetMgr_t* p, unsigned idx )
{
  unsigned i = 0;
  _cmDspPresetGrp_t* gp = p->list;
  for(; gp!=NULL; gp=gp->link,++idx)
    if( i == idx )
      break;

  return gp;
}

unsigned        _cmDspPresetGroupSymId( cmDspPresetMgr_t* p, unsigned groupIdx )
{
  _cmDspPresetGrp_t* gp;
  if((gp = _cmDspPresetGroupFromIndex(p,groupIdx)) == NULL )
    return cmInvalidIdx;
  return gp->symId;
}

const cmChar_t* _cmDspPresetGroupLabel( cmDspPresetMgr_t* p, unsigned groupIdx )
{
  unsigned symId;
  if((symId = _cmDspPresetGroupSymId(p,groupIdx)) == cmInvalidId )
    return NULL;

  return cmSymTblLabel(p->stH,symId);
}

cmJsonNode_t*  _cmDspPresetCreateJsonListHdr( cmDspPresetMgr_t* p, cmJsonH_t jsH, const cmChar_t* label )
{
  cmJsonNode_t* anp, *tnp;
  cmDspRC_t rc = kJsonFailDspRC;

  // create the container array
  if((anp = cmJsonInsertPairArray(jsH, cmJsonRoot(jsH), label )) == NULL)
    goto errLabel;
  
  // create the title array
  if((tnp = cmJsonCreateArray(jsH, anp )) == NULL )
    goto errLabel;

  if( cmJsonCreateString(jsH,tnp, "label" ) != kOkJsRC )
    goto errLabel;

  if( cmJsonCreateString(jsH,tnp, "sym" ) != kOkJsRC )
    goto errLabel;

  rc = kOkDspRC;
 errLabel:
  return rc == kOkDspRC ? anp : NULL;
}

cmDspRC_t _cmDspPresetGroupJsonList(  cmDspPresetMgr_t* p, cmJsonH_t* jsHPtr )
{
  cmJsonNode_t* anp;
  cmDspRC_t rc = kJsonFailDspRC;
  unsigned i,n;

  // create the container array and title elements in the first row
  if((anp = _cmDspPresetCreateJsonListHdr(p,*jsHPtr,"groupArray")) == NULL )
    goto errLabel;

  // get the count of groups
  n = _cmDspPresetGroupCount(p);

  // for each group
  for(i=0; i<n; ++i)
  {
    // create the row array
    cmJsonNode_t* tnp;
    if((tnp = cmJsonCreateArray(*jsHPtr,anp)) == NULL )
      goto errLabel;

    // insert the group label
    if( cmJsonCreateString(*jsHPtr, tnp, cmStringNullGuard( _cmDspPresetGroupLabel(p,i))) != kOkJsRC )
      goto errLabel;

    // insert the group symbol id
    if( cmJsonCreateInt(*jsHPtr, tnp, _cmDspPresetGroupSymId(p,i)) != kOkJsRC )
      goto errLabel;
  }
  
  rc = kOkDspRC;

 errLabel:
  if( rc != kOkDspRC )
    rc = cmErrMsg(p->err,rc,"Preset group array JSON object create failed.");

  return rc;

}
  
unsigned        _cmDspPresetPresetCount( cmDspPresetMgr_t* p, unsigned groupIdx )
{
  unsigned cnt = 0;
  _cmDspPresetGrp_t* gp;
  _cmDspPresetPre_t* pp;

  if((gp = _cmDspPresetGroupFromIndex(p,groupIdx)) == NULL )
    return 0;

  pp = gp->list;
  for(; pp!=NULL; pp=pp->link)
    ++cnt;

  return cnt;
}

_cmDspPresetPre_t* _cmDspPresetPreFromIndex( cmDspPresetMgr_t* p, unsigned gi, unsigned pi )
{
  _cmDspPresetGrp_t* gp;
  _cmDspPresetPre_t* pp;

  if((gp = _cmDspPresetGroupFromIndex(p,gi)) == NULL )
    return NULL;

  unsigned i = 0;
  pp = gp->list;
  for(; pp!=NULL; pp=pp->link,++i)
    if( i == pi )
      break;

  return pp;
}

unsigned        _cmDspPresetPresetSymId( cmDspPresetMgr_t* p, unsigned groupIdx, unsigned presetIdx )
{
  _cmDspPresetPre_t* pp;
  if((pp = _cmDspPresetPreFromIndex(p,groupIdx,presetIdx)) == NULL )
    return cmInvalidId;
  return pp->symId;
}

const cmChar_t* _cmDspPresetPresetLabel( cmDspPresetMgr_t* p, unsigned groupIdx, unsigned presetIdx )
{
  unsigned symId;
  if((symId = _cmDspPresetPresetSymId(p,groupIdx,presetIdx)) == cmInvalidId )
    return NULL;

  return cmSymTblLabel(p->stH, symId);
}

cmDspRC_t _cmDspPresetPresetJsonList(  cmDspPresetMgr_t* p, cmJsonH_t* jsHPtr, unsigned groupSymId )
{
  cmJsonNode_t* anp;
  cmDspRC_t rc = kJsonFailDspRC;
  _cmDspPresetGrp_t* gp;
  _cmDspPresetPre_t* pp;

  // find the group containing the preset list
  if((gp = _cmDspPresetFindGroup(p, groupSymId )) == NULL )
    return cmErrMsg(p->err,kPresetGrpNotFoundDspRC,"The preset JSON list could not be created because the group '%s', was not found.",cmStringNullGuard(cmSymTblLabel(p->stH,groupSymId)));
  
  // create the JSON container array and title element in the first row
  if((anp = _cmDspPresetCreateJsonListHdr(p,*jsHPtr,"presetArray")) == NULL )
    goto errLabel;

  // for each preset in this group
  for(pp=gp->list; pp!=NULL; pp=pp->link)
  {
    cmJsonNode_t* tnp;
    // create the row array
    if((tnp = cmJsonCreateArray(*jsHPtr,anp)) == NULL )
      goto errLabel;

    // write the preset label
    if( cmJsonCreateString(*jsHPtr, tnp, cmStringNullGuard( _cmDspPresetLabelStr(p,pp))) != kOkJsRC )
      goto errLabel;

    // write the preset symbol id
    if( cmJsonCreateInt(*jsHPtr, tnp, pp->symId ) != kOkJsRC )
      goto errLabel;
  }
  
  rc = kOkDspRC;

 errLabel:
  if( rc != kOkDspRC )
    rc = cmErrMsg(p->err,rc,"Preset array JSON object create failed.");

  return rc;

}




cmDspRC_t _cmDspPresetCreatePreset(    cmDspPresetMgr_t* p, const cmChar_t* groupLabel, const cmChar_t* presetLabel )
{
  cmDspRC_t          rc         = kOkDspRC;
  unsigned           groupSymId = cmSymTblRegisterSymbol(p->stH,groupLabel);
  unsigned           preSymId   = cmSymTblRegisterSymbol(p->stH,presetLabel);
  _cmDspPresetGrp_t* gp         = NULL;
  _cmDspPresetPre_t* pp         = NULL;

  // if the group does not already exist ...
  if((gp = _cmDspPresetFindGroup(p,groupSymId)) != NULL )
    p->gp = gp;
  else
  { // ... then create it
    gp = cmLhAllocZ(p->lhH,_cmDspPresetGrp_t,1);
    gp->symId = groupSymId;
    gp->link  = p->list;
    p->list   = gp;
    p->gp     = gp;
  }

  // if the preset does not already exist ...
  if((pp = _cmDspPresetFindPreset(gp,preSymId)) != NULL )
    cmErrMsg(p->err,kOkDspRC,"The preset label %s is already in use in the group:'%s'.",cmStringNullGuard(presetLabel),cmStringNullGuard(groupLabel));
  else
  {
    // ... then create it
    pp = cmLhAllocZ(p->lhH,_cmDspPresetPre_t,1);
    pp->symId = preSymId;
    pp->link  = gp->list;
    gp->list  = pp;
    gp->pp    = pp;
  }
  
  return rc;
}

cmDspRC_t _cmDspPresetCreateInstance(  cmDspPresetMgr_t* p, unsigned instSymId )
{
  // a current group and preset must exist
  assert( p->gp != NULL && p->gp->pp != NULL);

  cmDspRC_t           rc = kOkDspRC;
  _cmDspPresetInst_t* ip;
  _cmDspPresetPre_t*  pp = p->gp->pp;

  // an instance with the same name should not already exist in this preset
  if((ip = _cmDspPresetFindInst(pp,instSymId)) != NULL )
    return cmErrMsg(p->err,kDuplPresetInstDspRC,"A duplicate preset instance named '%s' was encounted in group:'%s' preset:'%s'.",cmStringNullGuard(cmSymTblLabel(p->stH,instSymId)),_cmDspPresetGroupLabelStr(p,p->gp),_cmDspPresetLabelStr(p,pp));

  ip = cmLhAllocZ(p->lhH,_cmDspPresetInst_t,1);
  ip->symId = instSymId;
  ip->link  = pp->list;
  pp->list  = ip;
  pp->ip    = ip;  
  
  return rc;
}

cmDspRC_t _cmDspPresetCreateVar(       cmDspPresetMgr_t* p, unsigned varSymId, const cmDspValue_t* valPtr )
{
  assert( p->gp != NULL && p->gp->pp != NULL && p->gp->pp->ip != NULL );

  cmDspRC_t           rc = kOkDspRC;
  _cmDspPresetInst_t* ip = p->gp->pp->ip;
  _cmDspPresetVar_t*  vp = NULL;
  
  if((vp = _cmDspPresetFindVar(ip,varSymId)) != NULL)
    return cmErrMsg(p->err,kDuplPresetVarDspRC,"A duplicate preset var named '%s' was encounted in group:'%s' preset:'%s' inst:'%s'.",cmStringNullGuard(cmSymTblLabel(p->stH,varSymId)),_cmDspPresetGroupLabelStr(p,p->gp),_cmDspPresetLabelStr(p,p->gp->pp),_cmDspPresetInstLabelStr(p,ip));

  vp        = cmLhAllocZ(p->lhH,_cmDspPresetVar_t,1);
  vp->symId = varSymId;
  vp->link  = ip->list;
  ip->list  = vp;
  
  // we aren't handling matrices yet
  assert( cmDsvIsMtx(valPtr) == false && cmDsvIsJson(valPtr)==false );
  
  if( cmDsvIsStrz(valPtr) )
  {
    cmChar_t* str = cmLhAllocStr(p->lhH,cmDsvStrz(valPtr));
    cmDsvSetStrz(&vp->value,str);
  }  
  else
  {
    cmDsvCopy(&vp->value,valPtr);
  }

  return rc;
}



cmDspRC_t _cmDspPresetRecallPreset(    cmDspPresetMgr_t* p, const cmChar_t* groupLabel, const cmChar_t* presetLabel )
{
  cmDspRC_t          rc         = kOkDspRC;
  unsigned           groupSymId = cmSymTblRegisterSymbol(p->stH,groupLabel);
  unsigned           preSymId   = cmSymTblRegisterSymbol(p->stH,presetLabel);
  _cmDspPresetGrp_t* gp         = NULL;
  _cmDspPresetPre_t* pp         = NULL;

  p->gp = NULL;

  if((gp = _cmDspPresetFindGroup(p,groupSymId)) == NULL )
    return cmErrMsg(p->err,kPresetGrpNotFoundDspRC,"The preset group '%s' was not found.",cmStringNullGuard(groupLabel));

  if((pp = _cmDspPresetFindPreset(gp,preSymId)) == NULL )
    return cmErrMsg(p->err,kPresetPreNotFoundDspRC,"The preset '%s' in group '%s' was not found.",cmStringNullGuard(presetLabel),cmStringNullGuard(presetLabel));

  p->gp = gp;
  p->gp->pp = pp;

  return rc;
}

cmDspRC_t _cmDspPresetRecallInstance(  cmDspPresetMgr_t* p, unsigned instSymId )
{
    // a current group and preset must exist
  assert( p->gp != NULL && p->gp->pp != NULL);

  cmDspRC_t           rc = kOkDspRC;
  _cmDspPresetInst_t* ip;
  _cmDspPresetPre_t*  pp = p->gp->pp;

  // an instance with the same name should not already exist in this preset
  if((ip = _cmDspPresetFindInst(pp,instSymId)) == NULL )
    return cmErrMsg(p->err,kPresetInstNotFoundDspRC,"A preset instance named '%s' was not found in group:'%s' preset:'%s'.",cmStringNullGuard(cmSymTblLabel(p->stH,instSymId)),_cmDspPresetGroupLabelStr(p,p->gp),_cmDspPresetLabelStr(p,pp));

  p->gp->pp->ip = ip;

  return rc;
}

cmDspRC_t _cmDspPresetRecallVar(       cmDspPresetMgr_t* p, unsigned varSymId, cmDspValue_t* valPtr )
{
  assert( p->gp != NULL && p->gp->pp != NULL && p->gp->pp->ip != NULL );

  cmDspRC_t           rc = kOkDspRC;
  _cmDspPresetInst_t* ip = p->gp->pp->ip;
  _cmDspPresetVar_t*  vp = NULL;
  
  if((vp = _cmDspPresetFindVar(ip,varSymId)) == NULL)
    return cmErrMsg(p->err,kPresetVarNotFoundDspRC,"A preset var named '%s' was not found in the group:'%s' preset:'%s' inst:'%s'.",cmStringNullGuard(cmSymTblLabel(p->stH,varSymId)),_cmDspPresetGroupLabelStr(p,p->gp),_cmDspPresetLabelStr(p,p->gp->pp),_cmDspPresetInstLabelStr(p,ip));

  cmDsvCopy(valPtr,&vp->value);

  return rc;
}

