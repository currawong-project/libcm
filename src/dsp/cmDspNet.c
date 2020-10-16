//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#include "cmPrefix.h"
#include "cmGlobal.h"
#include "cmFloatTypes.h"
#include "cmRpt.h"
#include "cmErr.h"
#include "cmCtx.h"
#include "cmMem.h"
#include "cmMallocDebug.h"
#include "cmLinkedHeap.h"
#include "cmFileSys.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmPrefs.h"
#include "cmDspValue.h"
#include "cmMsgProtocol.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmSerialPort.h"
#include "cmTime.h"
#include "cmAudioSys.h"
#include "cmProcObj.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspStore.h"
#include "cmDspSys.h"
#include "cmDspPreset.h"
#include "cmDspNet.h"

///============================================================================================
///============================================================================================
/*
cmDspSysAlloc()
  - create the p->netNodeArray[]
  - create the p->thH network thread but leave it paused
  
cmDspSysLoad()
  - _cmDspSysNetPreload() - set srcConnList to NULL

  - load the progam - this result in calls to _cmDspSysNetCreateSrcConn()
    which creates the p->srcConnList.

  - _cmDspSysNetSync() - just before exiting the load process go into sync mode:
      - initialize the p->netNodeArray[].
      - set the srcId of each record in the p->srcConnList.
      - start the sync thread
          while(1)
          {
            1) send connection requests to dst machines

            2) if( connFl = all src conn's have dst id's )
               send 'done' to all remote nodes
            
            3) nodeFl = if have recv'd 'done' from all remote nodes

            4) if connFl && nodeFl
                 p->syncState = kSyncSuccessDspId;
                 break;
         }

Enable Audio:
  - Call reset on all instances

cmDspSysUnload()
  - p->syncState = kSyncPreDspId
  

cmDspSysFree()
   - delete p->netNodeArray[]
   - delete p->thH network thread
//--------------------------------------------------------





 */
///============================================================================================


// implemented in cmDspSys.c
cmDspInst_t* _cmDspSysInstSymIdToPtr( cmDsp_t* p, unsigned instSymId );
cmDsp_t* _cmDspHandleToPtr( cmDspSysH_t h );

cmDspRC_t _cmDspSysNetSend( cmDsp_t* p, unsigned remoteNetNodeId, unsigned subSelId, unsigned srcId, unsigned dstId, const cmChar_t* errMsg )
{
  cmDspRC_t rc = kOkDspRC;
  cmDspNetMsg_t m;
  memset(&m,0,sizeof(m));

  // we should never be sending to ourselves
  assert( remoteNetNodeId != cmUdpNetLocalNodeId(p->netH));

  // form the error msg
  m.asSubIdx = cmDspSys_AsSubIdx_Zero;
  m.selId    = kNetSyncSelAsId;
  m.subSelId = subSelId;
  m.srcId    = srcId;
  m.dstId    = dstId;

  if( cmUdpNetSendById(p->netH, remoteNetNodeId, &m, sizeof(m) ) == kOkUnRC )
  {
    //cmSleepUs(p->sendWaitMs*1000);
  }
  else
  {
    rc = kNetFailDspRC;

    if( errMsg != NULL )
      cmErrMsg(&p->err,rc,errMsg);    
  }

  return rc;
}

// set echoFl if the remote node should respond with it's own 'hello' msg
cmDspRC_t _cmDspSysNetSendHello( cmDsp_t* p, unsigned remoteNetNodeId, bool echoFl )
{ 
  return _cmDspSysNetSend(p, remoteNetNodeId, kNetHelloSelAsId, echoFl ? 1 : 0, cmInvalidId, "A network send failed while sending 'hello'." );
}

cmDspRC_t _cmDspSysNetSendSyncError( cmDsp_t* p, cmDspRC_t errRc )
{
  cmDspRC_t     rc = kOkDspRC;
  unsigned      i;

  
  // for each non-local node
  for(i=0; i<p->netNodeCnt; ++i)
    if( p->netNodeArray[i].localFl == false)
    {  
      unsigned  remoteNetNodeId = p->netNodeArray[i].id;
      cmDspRC_t rc0;

      // send the error code in the dstId
      if((rc0 =  _cmDspSysNetSend(p, remoteNetNodeId, kNetErrSelAsId, cmUdpNetLocalNodeId(p->netH), errRc, "A network send failed while signaling an error." )) != kOkDspRC )
        rc = rc0;
    }

  if( cmThreadPause(p->thH,kPauseThFl) != kOkThRC )
    rc = cmErrMsg(&p->err,kThreadFailDspRC,"An attempt to pause the sync. thread failed after signaling an error.");

  if( p->netVerbosity > 0 )
    cmRptPrintf(p->err.rpt,"Sync:Done - Fail\n");

  return rc;
}

bool _cmDspSysNetIsNodeAwake( cmDsp_t* p, unsigned netNodeId )
{
  unsigned i;
  for(i=0; i<p->netNodeCnt; ++i)
    if( p->netNodeArray[i].id == netNodeId )
      return p->netNodeArray[i].helloFl;

  assert(0); // unknown net node id
  return false;
}

bool _cmDspSysNet_AreAllDstIdsResolved( cmDsp_t* p )
{
  // check that we have received all dst id req's from each remote node
  unsigned i;
  for(i=0; i<p->netNodeCnt; ++i)
    if( p->netNodeArray[i].localFl == false && p->netNodeArray[i].reqDoneFl==false)
      return false;

  _cmDspSrcConn_t* rp  = p->srcConnList;

  // for each src connection which does not yet have a destination id.
  for(; rp != NULL; rp = rp->link)
    if( rp->dstId == cmInvalidId )
      return false;

  return true;
}

// send connection requests to the specified remote node
cmDspRC_t _cmDspSysNetSendConnRequests( cmDsp_t* p, unsigned dstNetNodeId )
{
  cmDspRC_t        rc  = kOkDspRC;
  _cmDspSrcConn_t* rp  = p->srcConnList;

  // for each src connection which does not yet have a destination id.
  for(; rp != NULL; rp = rp->link)
  {
    
    // if this src conn has not been assigned a dstId and it's source node is awake
    if( rp->dstId == cmInvalidId && rp->dstNetNodeId==dstNetNodeId && _cmDspSysNetIsNodeAwake(p,rp->dstNetNodeId) )
    {
      // calc the msg size
      unsigned sn0     = strlen(rp->dstInstLabel) + 1;
      unsigned sn1     = strlen(rp->dstVarLabel) + 1;
      unsigned byteCnt = sizeof(cmDspNetMsg_t) + sn0 + sn1;

      // create msg buffer
      char buf[ byteCnt ];
      memset(buf,0,byteCnt);

      // fill in the msg
      cmDspNetMsg_t* cp = (cmDspNetMsg_t*)buf;
      cp->asSubIdx = cmDspSys_AsSubIdx_Zero;
      cp->selId    = kNetSyncSelAsId;
      cp->subSelId = kNetDstIdReqSelAsId;
      cp->srcId    = rp->srcId;
      cp->dstId    = cmInvalidId;
    
      char* dp = buf + sizeof(*cp);

      memcpy(dp,rp->dstInstLabel,sn0);
      dp += sn0;

      memcpy(dp,rp->dstVarLabel,sn1);
      dp += sn1;

      assert(dp == buf + byteCnt );

      // send the msg
      if( cmUdpNetSendById(p->netH, rp->dstNetNodeId, buf, byteCnt ) != kOkUnRC )
      {
        rc = cmErrMsg(&p->err,kNetFailDspRC,"A network send failed while registering remote nodes.");
        goto errLabel;
      }

      if( p->netVerbosity > 1 )
        cmRptPrintf(p->err.rpt,"Sync: send req to %i\n",rp->dstNetNodeId);

      //cmSleepUs(p->sendWaitMs*1000); // wait  between transmissions
    }

  }

 errLabel:
  if( rc != kOkDspRC )
    _cmDspSysNetSendSyncError(p,rc);

  return rc;
}

// Return true when the 'doneFl' on all nodes has been set.
// The doneFl is cleared on the beginning of the sync. process.
// The doneFl is set as each remote node signals that it has 
// all of the dstId's that it needs by sending kNetDoneSelAsId 
// messages.
bool  _cmDspSysNetCheckNetNodeStatus( cmDsp_t* p )
{
  unsigned i;
  for(i=0; i<p->netNodeCnt; ++i)
    if( p->netNodeArray[i].doneFl == false )
      return false;

  return true;
}

// Send kNetDoneSelAsId msgs to all remote nodes to indicate that
// this node has all of it's dstId's.
cmDspRC_t _cmDspSysNetSendSyncDone( cmDsp_t* p )
{
  cmDspRC_t     rc = kOkDspRC;
  unsigned i;

  // broadcast the sync 'done' msg to each non-local node
  if( p->netDoneSentFl )
    return rc;

  for(i=0; i<p->netNodeCnt; ++i)
    if( p->netNodeArray[i].localFl == false )
    {
      if((rc = _cmDspSysNetSend(p, p->netNodeArray[i].id, kNetDoneSelAsId, cmInvalidId, cmInvalidId, "A network send failed while signaling sync. completion." )) != kOkDspRC )
        goto errLabel;

    }


  // create the src connection map
  _cmDspSrcConn_t* sp = p->srcConnList;
  if( sp != NULL && p->srcConnMapCnt == 0 )
  {
    if( p->netVerbosity > 0 )
      cmRptPrintf(p->err.rpt,"Sync:Creating src map\n");

    // get the count of src nodes
    unsigned n;
    for(n=0; sp != NULL; sp = sp->link )
      ++n;

    // allocate the srcConnMap
    p->srcConnMap = cmLhResizeNZ( p->ctx.lhH, _cmDspSrcConn_t*, p->srcConnMap, n );
    p->srcConnMapCnt = n;

    // load the srcConnMap
    for(sp = p->srcConnList; sp != NULL; sp = sp->link )
    {
      assert( sp->srcId < n );
      p->srcConnMap[ sp->srcId ] = sp;

    }
  }

  // create the dst connection map
  _cmDspDstConn_t* dp = p->dstConnList;
  if( dp != NULL && p->dstConnMapCnt == 0 )
  {
    if( p->netVerbosity > 0 )
      cmRptPrintf(p->err.rpt,"Sync:Creating dst map\n");

    unsigned n;
    // get the count of dst nodes
    for(n=0; dp != NULL; dp = dp->link )
      ++n;

    // allocate the dstConnMap
    p->dstConnMap = cmLhResizeNZ( p->ctx.lhH, _cmDspDstConn_t*, p->dstConnMap, n );
    p->dstConnMapCnt = n;
  
    // load the dstConnMap
    for(dp = p->dstConnList; dp != NULL; dp = dp->link )
    {
      assert( dp->dstId < n );
      p->dstConnMap[ dp->dstId ] = dp;
    }

  }
  
  p->netDoneSentFl = true;

  if( p->netVerbosity > 0 )
    cmRptPrintf(p->err.rpt,"Sync: Done Sent\n",i);


 errLabel:
  if( rc != kOkDspRC )
    _cmDspSysNetSendSyncError(p,rc);

  return rc;
}

// Sync thread callback function
bool _cmDspSysNetSyncThreadCb( void* param )
{
  cmDsp_t* p = (cmDsp_t*)param;
  bool connFl = true; // 
  bool nodeFl = true;
  
  // receive a group of waiting messages from remote nodes
  // WARNING: calling cmUdpNetReceive() here means that the audio engine cannot be
  // enabled - otherwise this thread and the audio system thread will simultaneously
  // attempt to read the UDP port. This will result in unsafe thread conflicts.
  if( cmUdpNetReceive(p->netH, NULL ) != kOkUnRC )
  {
    cmErrMsg(&p->err,kNetFailDspRC,"UDP Net receive failed during sync. mode.");
    _cmDspSysNetSendSyncError(p,kNetFailDspRC);
  }

  // check if all the src connections have been assigned dst id's
  connFl = _cmDspSysNet_AreAllDstIdsResolved(p);

  // if all the src connections have dst id's then send a 'done' signal
  // to all other nodes so that they know this node is ready to leave
  // sync mode
  if( connFl )
  {
    if( _cmDspSysNetSendSyncDone(p) != kOkDspRC )
      goto errLabel;
  }

  // prevent the thread from burning too much time
  cmSleepUs(p->sendWaitMs*1000);
  
  // check if all nodes have completed transmission to this node
  nodeFl = _cmDspSysNetCheckNetNodeStatus(p);

  // if the connections have all been setup and all the net nodes have
  // received a 'done' signal 
  if( connFl && nodeFl )
  {
    // mark the sync as complete
    p->syncState = kSyncSuccessDspId;

    // the sync. is done - pause this thread
    if( cmThreadPause( p->thH, kPauseThFl ) != kOkThRC )
      cmErrMsg(&p->err,kThreadFailDspRC,"The attempt to pause the sync. thread upon completion failed.");

    if( p->netVerbosity > 0 )
      cmRptPrintf(p->err.rpt,"Sync Done!\n");
  }

 errLabel:
  return true;
}



// During DSP network allocation and connection this function is called
// to register remote instance/var targets.
_cmDspSrcConn_t*  _cmDspSysNetCreateSrcConn( cmDsp_t* p, unsigned dstNetNodeId, const cmChar_t* dstInstLabel, const cmChar_t* dstVarLabel )
{    
  if( dstNetNodeId == cmUdpNetLocalNodeId(p->netH) )
  {
    cmErrMsg(&p->err,kNetFailDspRC,"Cannot connect a network node (node:%s inst:%s label:%s)to itself.",cmStringNullGuard(cmUdpNetNodeIdToLabel(p->netH,dstNetNodeId)),cmStringNullGuard(dstInstLabel),cmStringNullGuard(dstVarLabel));
    return NULL;
  }

  // register the remote node 
  _cmDspSrcConn_t* rp = cmLhAllocZ( p->ctx.lhH, _cmDspSrcConn_t, 1 );
  rp->dstNetNodeId   = dstNetNodeId;
  rp->dstInstLabel   = cmLhAllocStr( p->ctx.lhH, dstInstLabel );
  rp->dstVarLabel    = cmLhAllocStr( p->ctx.lhH, dstVarLabel );
  rp->link           = p->srcConnList;
  p->srcConnList     = rp;

  if( p->netVerbosity > 1 )
    cmRptPrintf(p->err.rpt,"Sync: create src for %i\n", dstNetNodeId );

  return rp;
}

// This function is called in response to receiving a connection request on the
// dst machine. It sends a dstId to match the srcId provided by the src machine.
cmDspRC_t _cmDspSysNetSendDstConnId( cmDsp_t* p, unsigned srcNetNodeId, unsigned srcId, unsigned dstId )
{
  cmDspRC_t     rc = kOkDspRC;
  if((rc = _cmDspSysNetSend(p, srcNetNodeId, kNetDstIdSelAsId, srcId, dstId, "A network send failed while sending a dst. id." )) == kOkDspRC )
  {
    if( p->netVerbosity > 1 )
      cmRptPrintf(p->err.rpt,"Sync:Sent dst id to %i\n",srcNetNodeId);
  }

  return rc;
}

// Handle 'hello' messages
cmDspRC_t  _cmDspSysNetReceiveHello( cmDsp_t* p, unsigned remoteNetNodeId, bool echoFl )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i;

  for(i=0; i<p->netNodeCnt; ++i)
    if( p->netNodeArray[i].id == remoteNetNodeId )
    {
      p->netNodeArray[i].helloFl = true;

      // if echo was requested then respond ...
      if( echoFl )
      {
        // ... but the remote node should not respond - because we already know about it
        _cmDspSysNetSendHello(p,remoteNetNodeId,false);
      }

      if( p->netVerbosity > 0 )
        cmRptPrintf(p->err.rpt,"Sync:hello from %i\n",remoteNetNodeId);

      // send connection requests to the remote net node
      if((rc = _cmDspSysNetSendConnRequests(p, remoteNetNodeId )) == kOkDspRC )
      {
        // send 'req done' msg
        rc = _cmDspSysNetSend(p, remoteNetNodeId, kNetDstIdReqDoneAsId, 0, cmInvalidId, "A network send failed while sending 'dst req done'." );
      }

      return rc;
    }

  return cmErrMsg(&p->err,kNetFailDspRC,"Received a 'hello' message from an unknown remote network node (id=%i).",remoteNetNodeId);  
}

cmDspRC_t _cmDspSysNetReceiveReqDone( cmDsp_t* p, unsigned remoteNetNodeId )
{
  unsigned i;

  for(i=0; i<p->netNodeCnt; ++i)
    if( p->netNodeArray[i].id == remoteNetNodeId )
    {
      p->netNodeArray[i].reqDoneFl = true;

      if( p->netVerbosity > 0 )
        cmRptPrintf(p->err.rpt,"Sync: Req Done from %i\n",remoteNetNodeId);

      return kOkDspRC;
    }  

  return cmErrMsg(&p->err,kNetFailDspRC,"Received a 'req done' message from an unknown remote network node (id=%i).",remoteNetNodeId);  

}

// given a srcNetNodeId/srcId return the associated dst connection
_cmDspDstConn_t* _cmDspSysNetSrcIdToDstConn( cmDsp_t* p, unsigned srcNetNodeId, unsigned srcId )
{
  _cmDspDstConn_t* rp = p->dstConnList;
  for(; rp != NULL; rp = rp->link )
    if( rp->srcNetNodeId == srcNetNodeId && rp->srcId == srcId )
      return rp;

  return NULL;
}


// Handle kNetSyncSelAsId messages.
// Called on the dst machine to receive a src connection request
cmDspRC_t _cmDspSysNetReceiveSrcConnRequest( cmDsp_t* p, const cmDspNetMsg_t* msg, unsigned srcNetNodeId )
{
  cmDspRC_t rc = kOkDspRC;  
  _cmDspDstConn_t* rp;

  // if a dstId has not already been assigned to this src
  if((rp = _cmDspSysNetSrcIdToDstConn(p, srcNetNodeId, msg->srcId )) == NULL )
  {
    if( p->netVerbosity > 1 )
      cmRptPrintf(p->err.rpt,"Sync:Rcvd src conn request from %i\n",srcNetNodeId);

    cmDspInst_t*      instPtr   = NULL;
    const cmDspVar_t* varPtr;
    const cmChar_t*   instLabel = (const cmChar_t*)(msg+1);
    const cmChar_t*   varLabel  = instLabel + strlen(instLabel) + 1;

    // get the symbols assoc'd with the dst inst/var
    unsigned instSymId = cmSymTblId(p->ctx.stH,instLabel);
    unsigned varSymId  = cmSymTblId(p->ctx.stH,varLabel);
    assert( instSymId != cmInvalidId );

    // find the dst inst
    if((instPtr = _cmDspSysInstSymIdToPtr(p,instSymId)) == NULL )
    {
      rc = cmErrMsg(&p->err,kInstNotFoundDspRC,"The instance associated with '%s' was not found.",cmStringNullGuard(instLabel));
      goto errLabel;
    }

    // find dst var
    if((varPtr = cmDspVarSymbolToPtr(&p->ctx, instPtr, varSymId, kInDsvFl )) == NULL )
    {
      rc = cmErrMsg(&p->err,kVarNotFoundDspRC,"The variable '%s' on the instance '%s' was not found.",cmStringNullGuard(varLabel),cmStringNullGuard(instLabel));
      goto errLabel;
    }

    // register the new dst connection 
    rp                  = cmLhAllocZ( p->ctx.lhH, _cmDspDstConn_t, 1 );
    rp->srcNetNodeId    = srcNetNodeId;
    rp->srcId           = msg->srcId;
    rp->dstId           = p->nextDstId++;
    rp->dstInst         = instPtr;
    rp->dstVarId        = varPtr->constId;
    rp->link            = p->dstConnList;
    p->dstConnList      = rp;

  }
 
  assert( rp != NULL );

  // send the dstId associated with this connection back to the source
  if((rc = _cmDspSysNetSendDstConnId(p,srcNetNodeId,msg->srcId,rp->dstId)) != kOkDspRC )
    goto errLabel;
 

 errLabel:
  if( rc != kOkDspRC )
    _cmDspSysNetSendSyncError(p,rc);

  return rc;
}

cmDspRC_t _cmDspSysNetReceiveDstId(cmDsp_t* p, const cmDspNetMsg_t* msg, unsigned remoteNetNodeId)
{
  cmDspRC_t rc;

  _cmDspSrcConn_t* sp = p->srcConnList;
  for(; sp != NULL; sp = sp->link )
    if( msg->srcId == sp->srcId )
    {
      sp->dstId = msg->dstId;
      return kOkDspRC;
    }

  rc = cmErrMsg(&p->err,kNetNodeNotFoundDspRC,"The src id %i associated with the dst id %i could not be found.",msg->srcId,msg->dstId);
  _cmDspSysNetSendSyncError(p,rc);

  return rc;
}

// Handle kNetDoneSelAsId messages.
cmDspRC_t _cmDspSysNetReceiveRemoteSyncDone( cmDsp_t* p, const cmDspNetMsg_t* msg, unsigned remoteNetNodeId )
{
  cmDspRC_t rc = kOkDspRC;

  unsigned i;
  for(i=0; i<p->netNodeCnt; ++i)
    if( p->netNodeArray[i].id == remoteNetNodeId )
    {
      p->netNodeArray[i].doneFl = true;
      if( p->netVerbosity > 0 )
        cmRptPrintf(p->err.rpt,"Sync: Rcvd done from %i\n",remoteNetNodeId);
      break;
    }

  return rc;
}

// Handle kNetErrSelAsId messages
cmDspRC_t _cmDspSysNetReceiveRemoteSyncError( cmDsp_t* p, const cmDspNetMsg_t* msg, unsigned remoteNetNodeId )
{
  cmDspRC_t rc = kOkDspRC;

  if( p->netVerbosity > 0 )      
    cmRptPrintf(p->err.rpt,"Sync: Rcvd error from %i\n",remoteNetNodeId);

  if( cmThreadPause(p->thH,kPauseThFl) != kOkThRC )
    rc = cmErrMsg(&p->err,kThreadFailDspRC,"The attempt to pause the thread due to a remote error failed.");

  return rc;
}

// Verifify that a net node id is valid.
cmDspRC_t _cmDspSysNetValidateNetNodeId( cmDsp_t* p, unsigned netNodeId )
{
  cmDspRC_t rc = kOkDspRC;

  unsigned i;
  for(i=0; i<p->netNodeCnt; ++i)
    if( p->netNodeArray[i].id == netNodeId )
      break;

  if( i == p->netNodeCnt )
    rc = cmErrMsg(&p->err,kNetNodeNotFoundDspRC,"A message arrived from a unknown net node (id=%i).",netNodeId);

  return rc;
}

///============================================================================================
// main hooks to the DSP system 

// called by cmDspSysInitialize()
 cmDspRC_t  _cmDspSysNetAlloc( cmDsp_t* p )
{
  if((p->netNodeCnt = cmUdpNetNodeCount(p->netH)) != 0)
    p->netNodeArray = cmMemAllocZ(_cmDspNetNode_t, p->netNodeCnt );

  if( cmThreadCreate(&p->thH,_cmDspSysNetSyncThreadCb,p, p->err.rpt) != kOkThRC )
    return cmErrMsg(&p->err,kThreadFailDspRC,"The network syncronization thread create failed.");

  p->sendWaitMs = 10;
  p->syncState = kSyncPreDspId;
  p->netVerbosity = 1;
  return kOkDspRC;
}

// called by cmDspSysFinalize()
cmDspRC_t  _cmDspSysNetFree( cmDsp_t* p )
{
  cmMemFree(p->netNodeArray);

  if( cmThreadDestroy(&p->thH) != kOkThRC )
    cmErrMsg(&p->err,kThreadFailDspRC,"The network syncrhonization thread destroy failed.");

  return kOkDspRC;
}

// called by cmDspSysLoad()
cmDspRC_t  _cmDspSysNetPreLoad( cmDsp_t* p )
{
  p->srcConnList = NULL;
  return kOkDspRC;
}

// called by cmDspSysUnload() 
cmDspRC_t  _cmDspSysNetUnload( cmDsp_t* p )
{
  if( cmUdpNetEnableListen(p->netH, false ) != kOkUnRC )
    return cmErrMsg(&p->err,kNetFailDspRC,"The network failed to exit 'listening' mode.");

  p->syncState     = kSyncPreDspId;
  p->nextDstId     = 0;
  p->dstConnList   = NULL;
  p->srcConnMap    = NULL;
  p->srcConnMapCnt = 0;
  p->dstConnMap    = NULL;
  p->dstConnMapCnt = 0;
  p->netDoneSentFl = false;
  return kOkDspRC;
}

// Call this function to enter 'sync' mode.
cmDspRC_t _cmDspSysNetSync( cmDsp_t* p )
{
  cmDspRC_t       rc          = kOkDspRC;
  unsigned        localNodeId = cmUdpNetLocalNodeId(p->netH);
  unsigned        i;
  
  // if there is no network then there is nothing to do
  if( p->netNodeCnt == 0 )
  {
    p->syncState = kSyncSuccessDspId;
    return kOkDspRC;
  }

  p->syncState = kSyncPendingDspId;

  // be sure the sync thread is paused before continuing
  if( cmThreadPause(p->thH, kPauseThFl | kWaitThFl ) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kThreadFailDspRC,"The network sync thread could not be paused prior to initialization.");
    goto errLabel;
  }

  // initialize the netNodeArry[]
  for(i=0; i<p->netNodeCnt; ++i)
  {
    p->netNodeArray[i].id        = cmUdpNetNodeId(p->netH,i);
    p->netNodeArray[i].localFl   = p->netNodeArray[i].id == localNodeId;
    p->netNodeArray[i].doneFl    = p->netNodeArray[i].localFl;
    p->netNodeArray[i].helloFl   = false;
    p->netNodeArray[i].reqDoneFl = false;
    p->netNodeArray[i].doneFl    = false;

    assert( p->netNodeArray[i].id != cmInvalidId );
  }

  // initialize the src connection array - this array was created when the 
  // DSP instance network was formed
  _cmDspSrcConn_t* rp = p->srcConnList;
  for(i=0; rp != NULL; rp = rp->link,++i)
  {
    rp->srcId = i;
    rp->dstId = cmInvalidId;
  }

  // clear the dst conn records
  _cmDspDstConn_t* dp = p->dstConnList;
  while( dp!=NULL)
  {
    _cmDspDstConn_t* np = dp->link;
    cmLhFree(p->ctx.lhH,dp);
    dp = np;
  }

  // clear the connection maps
  p->srcConnMapCnt = 0;
  p->dstConnMapCnt = 0;
  p->netDoneSentFl = false;

  // enter listening mode
  if( cmUdpNetEnableListen(p->netH, true ) != kOkUnRC )
  {
    rc = cmErrMsg(&p->err,kNetFailDspRC,"The network failed to go into 'listening' mode.");
    goto errLabel;
  }

  // start the sync process
  if( cmThreadPause(p->thH,0) != kOkThRC )
  {
    rc = cmErrMsg(&p->err,kThreadFailDspRC,"The network sync thread could not be un-paused prior to begin synchronization.");
    goto errLabel;
  }

  // broadcast 'hello' to all remote listeners and request that they respond with their own 'hello'.
  for(i=0; i<p->netNodeCnt; ++i)
    if( p->netNodeArray[i].localFl == false )
      if((rc = _cmDspSysNetSendHello(p,p->netNodeArray[i].id,true)) != kOkDspRC )
        goto errLabel;

  if( p->netVerbosity > 0 )
    cmRptPrintf(p->err.rpt,"Sync:Entering sync loop\n");

 errLabel:
  if( rc != kOkDspRC )
    p->syncState = kSyncFailDspId;

  return rc;
}

cmDspRC_t _cmDspSysNetRecvEvent( cmDsp_t* p,  const void* msgPtr, unsigned msgByteCnt )
{
  cmDspNetMsg_t* m    = (cmDspNetMsg_t*)msgPtr;
  cmRC_t         rc   = cmOkRC;
  //bool           jsFl = false;

  // if the value associated with this msg is a mtx then set
  // its mtx data area pointer to just after the msg header.
  if( cmDsvIsJson(&m->value) )
  {
    //rc = cmDsvDeserializeJson(&m->value,p->jsH);
    assert(0);
    //jsFl = true;
  }
  else
    rc = cmDsvDeserializeInPlace(&m->value,msgByteCnt-sizeof(cmDspNetMsg_t));

  if( rc != kOkDsvRC )
    rc = cmErrMsg(&p->err,kInvalidStateDspRC,"Deserialize failed on network event message.");
  else
  {
    assert( m->dstId < p->dstConnMapCnt );

    // form the event
    cmDspEvt_t e;
    e.flags      = 0;
    e.srcInstPtr = NULL;
    e.srcVarId   = cmInvalidId;
    e.valuePtr   = &m->value;
    e.dstVarId   = p->dstConnMap[ m->dstId ]->dstVarId;
    e.dstDataPtr = NULL;
    
    cmDspInst_t* instPtr = p->dstConnMap[ m->dstId ]->dstInst;

    assert(instPtr != NULL );

    // send the event
    if( instPtr->recvFunc != NULL )
      rc = instPtr->recvFunc(&p->ctx,instPtr,&e);
  }


  //if( jsFl )
  //  cmJsonClearTree(p->jsH);

  
  return rc;
}

// Called from cmAudDsp.c:_cmAdUdpNetCallback() to to send an incoming msg to the DSP system.
cmDspRC_t _cmDspSysNetRecv( cmDsp_t* p, const cmDspNetMsg_t* msg, unsigned msgByteCnt, unsigned remoteNetNodeId )
{
  assert( msg->selId == kNetSyncSelAsId );

  cmDspRC_t rc = kOkDspRC;
  switch( msg->subSelId )
  {
    case kNetHelloSelAsId:
      rc = _cmDspSysNetReceiveHello(p,remoteNetNodeId,msg->srcId!=0);
      break;

    case kNetDstIdReqSelAsId:
      rc = _cmDspSysNetReceiveSrcConnRequest(p,msg,remoteNetNodeId);
      break;

    case kNetDstIdReqDoneAsId:
      rc = _cmDspSysNetReceiveReqDone(p,remoteNetNodeId);
      break;

    case kNetDstIdSelAsId:
      rc = _cmDspSysNetReceiveDstId(p,msg,remoteNetNodeId);
      break;

    case kNetDoneSelAsId:
      rc = _cmDspSysNetReceiveRemoteSyncDone(p,msg,remoteNetNodeId);
      break;

    case kNetErrSelAsId:
      rc =  _cmDspSysNetReceiveRemoteSyncError(p,msg,remoteNetNodeId);
      break;

    case kNetEvtSelAsId:
      rc = _cmDspSysNetRecvEvent(p, msg, msgByteCnt );
      break;

    default:
      cmErrMsg(&p->err,kNetFailDspRC,"Unexpected message type 'selId=%i' received by the network sync. message dispatcher.",msg->selId);
      break;
  }
  return rc;
}


cmDspRC_t _cmDspSysNetSendEvent( cmDspSysH_t h, unsigned dstNetNodeId, unsigned dstId, const cmDspEvt_t* evt )
{
  cmDsp_t* p           = _cmDspHandleToPtr(h);
  unsigned bufByteCnt  = sizeof(cmDspNetMsg_t);
  unsigned dataByteCnt = 0;

  if( evt->valuePtr != NULL )
   dataByteCnt = cmDsvSerialDataByteCount(evt->valuePtr);

  bufByteCnt += dataByteCnt;

  char          buf[ bufByteCnt ];
  cmDspNetMsg_t* hdr = (cmDspNetMsg_t*)buf;


  hdr->asSubIdx   = cmDspSys_AsSubIdx_Zero;
  hdr->selId      = kNetSyncSelAsId;
  hdr->subSelId   = kNetEvtSelAsId;
  hdr->srcId      = cmInvalidId;
  hdr->dstId      = dstId;


  if( evt->valuePtr == NULL )
    cmDsvSetNull(&hdr->value);
  else
  {
    // this function relies on the 'ne->value' field being the last field in the 'hdr'.
    if( cmDsvSerialize( evt->valuePtr, &hdr->value, sizeof(cmDspValue_t) + dataByteCnt) != kOkDsvRC )
      return cmErrMsg(&p->err,kSerializeFailMsgRC,"An attempt to serialize a network event msg failed.");
  }
  
  if( cmUdpNetSendById(p->netH, dstNetNodeId, &buf, bufByteCnt ) != kOkUnRC )
    return cmErrMsg(&p->err,kNetFailDspRC,"A network send failed while sending a DSP event message.");

  return kOkDspRC;
}

