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
#include "cmMath.h"
#include "cmFile.h"
#include "cmFileSys.h"
#include "cmSymTbl.h"
#include "cmJson.h"
#include "cmText.h"
#include "cmPrefs.h"
#include "cmProcObj.h"
#include "cmDspValue.h"
#include "cmDspCtx.h"
#include "cmDspClass.h"
#include "cmDspFx.h"
#include "cmDspKr.h"
#include "cmMsgProtocol.h"
#include "cmThread.h"
#include "cmUdpPort.h"
#include "cmUdpNet.h"
#include "cmTime.h"
#include "cmAudioSys.h"
#include "cmDspSys.h"

#include "cmDspPreset.h"  // required for cmDspNetSend
#include "cmDspNet.h"

#include "cmAudioFile.h"
#include "cmThread.h"  // used for threaded loading in wave table file mode


#include "cmProcObj.h"
#include "cmProcTemplateMain.h"
#include "cmProc.h"
#include "cmMidi.h"
#include "cmProc2.h"
#include "cmVectOpsTemplateMain.h"
#include "cmMidiPort.h"

#include "sa/cmSaProc.h"

/*
About variables:
1) Variables represent data fields within a DSP object.

2) Variables may also act as input (kInDsvFl) and/or output (kOutDsvFl) ports.
    A. Audio Ports
       - Audio output ports (kAudioBufDsvFl) publish a buffer of samples to subscribing 
         audio input ports on other instances. (See cmDspSysConnectAudio(). )

       - Output audio ports are instantiated with physical buffers to hold samples of
         audio. Input audio ports contain internal pointers with point to the
         audio buffers of connected output ports.

       - Set cmDspVarArg_t.cn to the number of channels of audio the buffer will contain.

    B. Non-Audio Ports
       - Non-audio input ports may register to be called when output port values change.
         via cmDspSysInstallCb(). 

       - Type checking is done to guarantee that the output port data type matches one of the
         input port types or can be converted to one of the input port types.
         [TODO: Check how this is handled in cmDspSetEvent().  Let's say that an input port
         takes either a number or string. This conversion is not possible within the
         cmDspValue() framework - and yet it should work - or give an error ]


3) Creating variable instances.
   A. All variables must be given default values in the instance constructor 
      _cmDspXXXAlloc(). This can be done automatically be giving default values 
      in the var args section of the cmDspSysAllocInst() call or by explicitely 
      setting default values via cmDspSetDefaultXXX() in _cmDspXXXAlloc().
   
   B. The _cmDspXXXReset() function automatically resets all the instance variables to their
      default value.  By default, variables do not transmit their values when they are 
      reset to their default value - unless the variable is marked with the kSendDfltDsvFl.
      (See cmDspClass.c:cmDspApplyDefault()). The 'out' port on cmDspScalar_t and cmDspButton_t
      are examples of instances that transmit their default value on reset.

      The order that instances are reset is determined by the order that they were created.

      A subtle case can arise when relying on default values to set the initial value of
      another object.  Given two object instances A and B. Where The output of A has the
      the kSendDfltDsvFl set and is connected to an input port on B.  If instance A was
      created before instance B then instance A will reset before instance B.  During reset
      the output port of A will transmit it's default value to B - as expected. However
      when reset is called on instance B it will overwrite this value with it's own 
      default value. 

      In order for this scenario to work - that is the output of instance A sets the initial
      value of instance B - instance B must be created first.

      Since creation order also determines execution order this solution may not always 
      be possible. It may be that we would simultaneously like instance A to exeute first 
      and also recieve its initial value from instance B.  Given the above example however 
      these two goals are mutually exclusive.
      
      [*] This could be solved by explicitely allowing a variable to be reset via
      a callback.  In this case if a variable was set via a callback during reset
      then it would not overwrite that value with its own internal value. This
      functionality would need to be set on a per instance bases - which might
      not work well with the current cmDspSysAllocInst() var args scheme.

      When a single output port is connected to multiple input ports the input ports
      are called in the order that the connections were made.

      To overcome this problem the reset cycle has been broken into two parts.
      In the first part the resetFunc is called on each instance in creation order.
      (as described above).  Following this pass a second pass is made where the
      sysRecvFunc on any instance tagged with the '_reset' attribute is called.
      This allows an event callback chain to be executed prior to the first 
      set of execFunc calls.  Note that the event callback chain(s) are programmed
      by cmDspInstallCb() connections which are setup on a per application basis.
      This allows the initial state of the network to be set outside of the
      creation order.  

      The 'button' instance implements a sysRecvFunc which will send the buttons 
      value and symbol when called with the '_reset' attribute symbol. If a 
      'reset' button is created and assigned the '_reset' attribute symbol 
      (via cmDspSysAssignAttrSymbol()) then the output of the button can be used 
      to drive a networks initial state in an order determined by the application
      programmer.
      

      [TODO: Add a check that all instance variables have default values at the end of
       the network reset. This should be easy because the instance variable flag
       kDfltSetDsvFl is set in cmDspValueSet() when the default value is set.]

  C. The default value of variables may be set via the var args portion of cmDspSysAllocInst().

     There is currently a weakness during variable instance creation function
     cmDspInstAlloc() uses the cmDspVarArg_t.flags to determine the type of the var arg
     argument.  cmDspVarArg_t.flags can therefore only be set to one DSV type - and the
     actual argument in the cmDspSysAllocInst() call must match that type.  This is very
      easy to mess up - for example by setting the flag to kIntDsvFl and then putting
     0.0 as the var arg value. This limitation also prevents ports which are also 
     required arguments to cmDspSysAllocInst() (i.e. kReqArgDsvFl) from supporting 
     multiple types.

     There are two possible ways to fix this:
     1) Include the type flag in the var args list.
     2) Specify a seperate var args flag in cmDspVarArg_t.
     Option 1 seems better because:
       a. [**] It would allow setting other per instance flags in the cmDspSysAllocInst() call [*].  
       b. It would support setting a var arg termination flag rather than relying on the current
          explicit argument count.
       c. This is also the method used in cmJsonMemberValues() and it has worked well there.
     Either of these options requires a substantial change to existing code.
     This change should therefore be made sooner rather than later.

     As the system currently works it is possible, and it often happens, that
     an instance's recv function will be called before it is reset. It seems like
     this is a bad thing.  Maybe the process of resetting and transmitting dflt
     values should be broken into seperate passes. This idea could be extended
     to type checking as follows:
         a. a reset-0 pass is made to set the internal state of each instance to
            known values.

         b. a type check pass is made where each output port
            sends a type msg to all connected input ports to provide the types that
            it will send.

         c. a reset-1 pass is made allowing all of the type information to be 
            used to determine the initial state.

         d. an initial default value transmission pass is made where some instances
            (like scalars) may transmit initial values 

         e. a reset-2 pass is made allowing the initial values to be acted on.

         f. runtime occurs

  D. Questions:
     0) What are the ways that an instance variable can get set?
        a. cmDspValueSet()    - This is the canonical value setting function.
           All other value setting function call this function.  
           The cmDspSetXXX() are type safe wrappers to this function.

        b. cmDspApplyDefault() - assign the default value to the actual variable
           via a call to cmDspValueSet().  This function is automatically called
           by cmDspApplyAllDefaults() which is usually in the instance reset function.

        c. cmDspSetEvent() - assign the value embedded in an event message
           to a variable. This is a wrapper function for cmDspValueSet() which 
           checks to see if the value need to be echoed to the UI.  See 
           more about UI echoing in the next section.

     1) Where do events arriving at an instance receive function originate from?
           There are two sources of events:
              1. The output ports of other instances.
              2. The UI (client application).
                  
            Events arriving from the UI can be distinguished from events arriving
            from other instances because they have the evt.kUiDspFl flag set.

     2) How does UI updating and echoing actually work? 
         a. Overview:
            On creation (in cmDsUi.c) variables whose value must be reflected to the UI
            are marked with the kUiDsvFl - these variables are called 'UI variables'.  
            When a UI variable receive a new value 
            the new value must be reflected in the associated UI GUI control.

            When and how this is accomplished depends on the source of the new
            variable value.  There are two sources of events arriving at 
            an instance's receive function: the output port of another instance 
            or the UI.  

            Events which originate from the UI are marked with a evt.kUiDspFl.  
            (cmDspSys.c:_cmDspSysHandleUiMsg())

            When a UI variable receives a value from the output port of another
            instance (i.e. kUiDspFl not set) then it must always send that value to 
            the UI.  In other words if the evt.kUiDspFl is NOT set then the
            value must be sent to the UI.

            When a UI variable receives a value from the UI (i.e. the evt.kUiDspFl is set)
            it may or may not need to reflect the value.  If the UI already
            reflects the value then the value does not need to be sent back
            otherwise it does.

            The UI control determines whether it wants to receive the value it 
            is sending back by setting the kDuplexDuiFl flag in it's msg to the engine.
            Upon receipt of this msg, in cmDspSys.c:_cmDspSysHandleUiMsg(),
            the system converts the msg to an event.  If the kDuplexDuiFl
            is set then the kUiEchoDspFl is set in the event.

            When a UI variable receives a value from the UI and the evt.kUiEchoDspFl
            is set then the value must be reflected, otherwise it must not.

            The rules for updating UI variables (var. w/ kUiDsvFl set) can 
            be summarized as follows:

            kUiDspFl   kUiEchoDspFl    Send to UI  Notes
            --------   ------------    ----------  -------------------------------------------------------------
                0           0            Yes       The value originated from another port and therefore must be reflected.
                0           1           <invalid>  If kUiEchoDspFl is set then so must kUiDspFl.
                1           0             No       
                1           1            Yes       The value originated from the UI and ehco was requested.

            This logic is automatically handled together by 
            cmDspSetEvent() and cmDspValueSet().
            

         b. Variable values are sent and received to and from the UI using
            kValueDuiId messages.
            
         c. It is possible to prevent values generated in the engine from being 
            reflected to the UI by setting the kNoUpdateUiDspFl in the call to
            cmDspValueSet(). 
         
         d. Instance variables are marked as UI variables by the cmDspUIXXXCreate() 
            functions. Note that instances that have assoicated UI controls generally
            have multiple UI variables.  (e.g. min,max,step,label,value).  

         e. Messages arriving from the UI are handled by cmDspSys.c:_cmDspSysHandleUiMsg()
            where they are converted to cmDspEvt_t's.  All events generated from
            msgs arriving from the UI are marked with the kUiDspFl.  If the msg kDuplexDuiFl 
            is set then the event flag kUiEchoDspFl is set - to indicate that the instance
            should send the value back to the UI.
            
            This leads to the following potential situation: The msg with  kUiEchoDspFl 
            set arrives at its target instance - which in turn calls cmDspSetEvent() to update
            the target variable value. Because kUiEchoDspFl is set the value is automatically
            reflected to the UI as expected.  However as part of the call to 
            cmDspValueSet() within cmDspSetEvent() the event is also sent to any connected
            instances - a problem would occur if kUiEchoDspFl remained set when it
            was sent to the connected instances - because they might then also try to update
            their own UI inappropriately.  In fact this is not a problem because
            _cmDspSendEvt() zeros the the evt.flags value prior to resending the event.
                

     3) Proposed Data Typing Framework:
        a. Instance variables are assigned 3 data types:
          1. cmDspSysAllocInst() var args type.  This is the type that the var args argument
           to the instance constructor must be.  The value provided by this method 
           becomes the default value for the variable.  This type must be unique - multiple
           type flags cannot be used for this value.

          2. Strict data type. This is the type used to define the variable value. 
           Any values which will be used to set the value of this variable must be able
           to be converted to this type.  

           If the variable is used as on input then the system will warn if an output port
           is assigned which cannot be converted to this type.  If the variable is used as
           an output then this is the type the variable will publish as the output type.
         
           All values arriving at the functions 'recv' function are guaranteed to be
           able to be converted to this type.

          3. Alternate data types.  Multiple types may be given to this type.
           If no strict data type is given then this will be the set of types accepted 
           for input and reported for output.  

           All value messages for the instance which do not fit the strict data type,
           but do fit the alternate data type, will be sent to the  'altRecvFunc'
           function.  The data type of events arriving at this function may therefore
           need to be decoded in order to use the assoicated values.
           
        b. Connections from an output with a strict data type can be type checked in
           advance - since they are guaranteed to emit a specific data type.

           Connections from outputs without strict data types can only be type checked
           at runtime - since it is possible for the type to change once the execution
           starts.
           
        c. Other notes about data types:
         It seems like cmDsvValues() should in general only allow one type flag (along with
         the kMtxDsvFl) to be set at a time if the flag state is legal. Is this always the
         case?  Can a macro be included to routinely check this?  Are we careful to
         not confuse the actual and possible type flags in DSP instance variables?
         This is important when checking the types of variables arriving at in input port.
         Include a macro to test the legality of the actual value type leaving output ports 
         and entering input ports.

         Note that the cmXXXDsvFl flags have the problem that it is not possible to 
         specify some multiple types. For example it is not possible to specify both 
         scalar and matrix types simultaneously.  Once the matrix flag is set it must
         be assumed that all specific data types then are matrices.  
         THIS IS A FUNDAMENTAL PROBLEM THAT MUST BE ADDRESSED.

         Note that scalar numeric values can easily be cheaply converted to other numeric
         types. It could be expensive however if vectors required conversion.  
         Vectors are currently being passed as pointers.  No conversion is occurring.

         d. Another proposal:
         Following connection time there is a type determination pass.  The network
         is traversed in execution order.  Each instance computes and emits the single 
         type assigned to each of its output ports. Once emited these types will not
         change during runtime.

         Note that this does not preclude an input receiving multiple types. 
         If a variable arrives at an input port which does not match the type of
         the variable associated with that type then it is sent to the NoTypeRecv()
         instance function.
         
         

     2. Do values only get sent on change?  If not - why not?
        No - based on cmDspClass.ccmDspValueSet() values are transmitted
        whenever they are set - there is not check for a change of value.


     3. Write a function to support generating multiple enumerated
        ports - as is required by AMix or ASplit. 
        (This is now done: See cmDspClass.h:cmDspArgSetup() )
        Update the existing code to use this scheme. (This still needs to be done.)
        Similar functions need to be written for connecting groups of ports
        in cmDspPgm.c. (This still needs to be done.)


     5.  It does not appear that the kInDsvFl and kOutDsvFl are actually used during 
         the connection process.  This means that a variable marked as an output
         could be used as an input and v.v.. Likewise variable marked as neither input
         nor output could be accessed. What are the input and output flags actually
         used for?  
         
         In fact kInDsvFl and kOutDsvFl are not used at all. (3/18/12).

         It might be nice to allow everything to be an output - but to 
         force inputs to be explicitely named.
         

     6.  Is there any implication for marking a variable as both an input and an output?
     7.  The audio buffers allocated in cmDspInstAlloc() may not  be memory aligned.
     8.  Is there a way to add a generic enable/disable function to all instances?
     9.  Should all output audio buffers be zeroed by default during _cmDspXXXReset()?
     10. Is the kConstDsvFl used? respected? necessary?
          yes - it is necessary because some variables cannot be changed after the constructor
          is completed.  For example any instance that take an argument giving the 
          number of ports as a variable.  The port count argument cannot change because it
          might invalidate connections which had been already made to the existing ports.
          TODO: find all variables which cannot be changed after the constructor and mark 
          them as const and prevent them from being the target of connections or events.

     11. Is it OK to not assign a variable as either an input or an output.  (this would
         allow it to be set from cmDspSysAllocInst() but then only changed internally).
     
     12. Write some template DSP instances that provide commented examples of the
         common scenarios which an actual instance might encounter.

     13. All errors in instances should use cmDspClassErr() or cmDspInstErr() not cmErrMsg().
         Update existing code.

     14. The way that the master controls are created is wrong.  The master controls should
         be created during the cmDspSysLoad() process rather than being created in kcApp.cpp.
         This would make them essentially identical to other controls - and would allow the
         master controls to be manipulated easily from inside a DSP instance.

     15. The code for creating and decoding messages seems to be distributed everywhere.
         All of this functionality should be moved to cmMsgProtocol.c.  See the code
         for encoding/decoding messages in cmAudioSys.c as an example.
     
     16. The default behavior of buttons should be to to NOT send out their default values or 
         symbols on instance reset. Determining whether an output value is sent on instance
         reset (as they all are currently ??? or are only UI sent out on reset???) 
         could be another argument flag setting [**].

     17. cmDspInstAlloc() should include another version called cmDspInstAllocV() 
         which takes the cmDspArg_t fields as var args.  This would allow
         array variables which currently use cmDspArgSetupN() to be given
         in one call - which would be less error prone than using cmDspArgSetupN().

     18. Add helper functions to create common dsp instances like:
         scalar,button,check,file,audio in,audio out.  These functions should
         support default values through literals or through resource paths.

     19. Design a sub-net function for making sub-nets of instance nets
         that can then be treated like instances themselves. 
         For example make a network of audio sources:
         audio file, signal generator, audio input, with gain and frequency
         controls.

     20. Network construction (cmDspPgm.c) is divided into two parts.
         First the instances are allocated and then they are connected.
         There should always be a test for a failure between construction
         phase and the connection phase and then again after the connection phase.

     21. The existing instances are not using cmReal_t and cmSample_t as they should.

     22. It is possible for cmDspInstAlloc() to fail in an instance constructor and 
         yet we are not testing for it in many instances.
         When a failure occurs after cmDspInstAlloc() how is the instance deleted
         prior to returning? ... is it necessary to delete it prior to returning?

     23. For instances which act as files and which take a file name as at an input
         port - the correct way to implement the object is to open/reopen the file
         both on reset and when a new file name is received.
         The reset open covers the case where the default filename is used.
         The receieve open covers the case where a filename is received via the input port.

     24. After each call to an instance member function (reset,recv,exec,etc.) the
         interal error object should be checked.  This way an invalid state can
         be signaled inside one of the functions without having to worry about
         propagating the error to the return value.  THis means that as long as
         a member function can report and safely complete it doesn't have to do
         much error handling internally.

     25. As it is currently implemented all audio system sub-system share a 
         single UDP network managers.  This is NOT thread-safe.  If more than
         one audio sub-system is actually used the program will crash.
         This can be solved by giving each sub-system it's own UDP network 
         manager, where each sub-system is given it's own port number.

*/

//==========================================================================================================================================
enum
{
  kLblPrId,
  kMsPrId,
  kInPrId

};

cmDspClass_t _cmPrinterDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned limitCycles;
} cmDspPrinter_t;

cmDspInst_t*  _cmDspPrinterAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "lbl",  kLblPrId,  0, 0,   kInDsvFl  | kStrzDsvFl | kOptArgDsvFl, "Label" },
    { "ms",   kMsPrId,   0, 0,   kInDsvFl  | kUIntDsvFl | kOptArgDsvFl, "Period"},
    { "in",   kInPrId,   0, 0,   kInDsvFl  | kTypeDsvMask,              "Input port"   },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspPrinter_t* p = cmDspInstAlloc(cmDspPrinter_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspSetDefaultUInt(ctx,&p->inst,kMsPrId, 0, 0 );

  return &p->inst;
}

cmDspRC_t _cmDspPrinterReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspPrinter_t* p = (cmDspPrinter_t*)inst;
  p->limitCycles = ctx->cycleCnt;
  return kOkDspRC;
}


cmDspRC_t _cmDspPrinterRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspPrinter_t* p = (cmDspPrinter_t*)inst;
  
  if( evt->dstVarId == kInPrId && ctx->cycleCnt >= p->limitCycles )
  {
    p->limitCycles =  ctx->cycleCnt + (unsigned)(cmDspUInt(inst,kMsPrId) * cmDspSampleRate(ctx) / (1000.0 * cmDspSamplesPerCycle(ctx)) );

    const cmChar_t* lbl = cmDspStrcz(inst,kLblPrId);

    if( cmDsvIsSymbol(evt->valuePtr) )
      cmRptPrintf(ctx->rpt,"%s'%s'",lbl==NULL?"":lbl,cmStringNullGuard(cmSymTblLabel(ctx->stH,cmDsvSymbol(evt->valuePtr))));
    else
      cmDsvPrint(evt->valuePtr,lbl,ctx->rpt);

    cmRptPrint(ctx->rpt,"\n");
  }

  return kOkDspRC;
}

struct cmDspClass_str* cmPrinterClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmPrinterDC,ctx,"Printer",
    NULL,
    _cmDspPrinterAlloc,
    NULL,
    _cmDspPrinterReset,
    NULL,
    _cmDspPrinterRecv,
    NULL,
    NULL,
    "Print the value of any event arriving at 'in'.");

  return &_cmPrinterDC;
}

//==========================================================================================================================================
enum
{
  kMinCntId,
  kMaxCntId,
  kIncCntId,
  kWrapCntId,
  kResetCntId,
  kOutCntId,
  kCycCntId,
  kNxtCntId,
};

cmDspClass_t _cmCounterDC;

typedef struct
{
  cmDspInst_t inst;
  double      val;
  bool        disableFl;
} cmDspCounter_t;


cmDspRC_t  _cmDspCounterValidate( cmDspInst_t* inst, double min, double max, double inc )
{
  if( max < min )
    return cmErrMsg(&inst->classPtr->err,kInvalidArgDspRC,"The counter maximum (%f) value must be greater than the counter minimum (%f) value.",max,min);

  if( max - min < inc )
    return cmErrMsg(&inst->classPtr->err,kInvalidArgDspRC,"The counter increment value (%f) must be less than or equal to the maximum - minimum difference (%f).",inc,max-min);

  return kOkDspRC;
}

cmDspInst_t*  _cmDspCounterAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "min",    kMinCntId,  0, 0, kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl, "minimum" },
    { "max",    kMaxCntId,  0, 0, kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl, "maximum" },
    { "inc",    kIncCntId,  0, 0, kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl, "increment"},
    { "wrap",   kWrapCntId, 0, 0, kInDsvFl  | kBoolDsvFl   | kOptArgDsvFl, "wrap"},
    { "reset",  kResetCntId,0, 0, kInDsvFl  | kTypeDsvMask,                "reset"},
    { "out",    kOutCntId,  0, 0, kOutDsvFl | kDoubleDsvFl,                "out"},
    { "cycles", kCycCntId,  0, 0, kOutDsvFl | kDoubleDsvFl,                "cycles"},
    { "next",   kNxtCntId,  0, 0, kInDsvFl  | kTypeDsvMask,                "next"},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspCounter_t* p = cmDspInstAlloc(cmDspCounter_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  
  double min = cmDspDefaultDouble( &p->inst, kMinCntId );
  double max = cmDspDefaultDouble( &p->inst, kMaxCntId );
  double inc = cmDspDefaultDouble( &p->inst, kIncCntId );

  if( _cmDspCounterValidate(&p->inst, min, max, inc ) != kOkDspRC )
    return NULL;
  
  cmDspSetDefaultBool(   ctx, &p->inst, kWrapCntId,    false, true);
  cmDspSetDefaultDouble( ctx, &p->inst, kOutCntId, 0.0, min );
  cmDspSetDefaultDouble( ctx, &p->inst, kCycCntId, 0.0, 0.0 );
  return &p->inst;
}

cmDspRC_t _cmDspCounterReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspCounter_t* p = (cmDspCounter_t*)inst;
  cmDspApplyAllDefaults(ctx,inst);
  p->val       = cmDspDouble(inst,kMinCntId);
  p->disableFl = false;  // the default values must be ok or the constructor fails
  return kOkDspRC;
}

void _cmDspCounterIncr( cmDspCtx_t* ctx, cmDspInst_t* inst  )
{
  cmDspCounter_t* p       = (cmDspCounter_t*)inst;
  double          min     = cmDspDouble(inst,kMinCntId);
  double          max     = cmDspDouble(inst,kMaxCntId);
  double          inc     = cmDspDouble(inst,kIncCntId);
  bool            wrapFl  = cmDspBool(inst,kWrapCntId);
  bool            limitFl = min <= max;
  
  // If min > max then no upper/lower limit is set on the value.
  // In this case the ouput will continue to increment and 
  // no 'cycle' output will be generated.

  // If wrapFl is not set then the 'cycle' output will fire
  // exactly once when the counter crosses its limit.

  // if the new value is in range then send it
  if( min <= p->val && p->val < max )
    cmDspSetDouble( ctx, inst, kOutCntId, p->val ); 


  // the current value is out of range and wrap flag is not set
  if( limitFl && (p->val < min || p->val >= max) && (wrapFl==false))
    return;
  
  // do the increment
  p->val += inc;

  // if the new value is out of range 
  if( limitFl && (p->val < min || p->val >= max) )
  {
    // if wrapping is allowed
    if( wrapFl )
    {
      if( p->val >= max )
        p->val = min + (p->val - max);   // wrap to begin
      else
        if( p->val < min ) 
          p->val = max - (min - p->val); // wrap to end
    }

    // increment the cycle counter
    cmDspSetDouble( ctx, inst, kCycCntId, cmDspDouble( inst, kCycCntId ) + 1 );
  }
  
}

cmDspRC_t _cmDspCounterRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspCounter_t* p   = (cmDspCounter_t*)inst;
  cmDspRC_t rc = kOkDspRC;

  switch( evt->dstVarId )
  {
    case kWrapCntId:
      cmDspSetEvent(ctx,inst, evt );
      break;

    case kMinCntId:
    case kMaxCntId:
    case kIncCntId:
      {
        cmDspSetEvent( ctx, inst, evt );
        double min     = cmDspDouble( inst, kMinCntId );
        double max     = cmDspDouble( inst, kMaxCntId );
        double inc     = cmDspDouble( inst, kIncCntId );
        p->disableFl   = (rc = _cmDspCounterValidate(inst, min, max, inc)) != kOkDspRC;
          
      }
      break;
      
    case kNxtCntId:
      if( !p->disableFl )
        _cmDspCounterIncr(ctx,inst);
      break;

    case kResetCntId:  // any msg on the 'reset' port causes the min value to be sent on the following 'next'
      p->val = cmDspDouble(inst,kMinCntId);
      break;

    default:
      { assert(0); }
  }

  return rc;
}



struct cmDspClass_str* cmCounterClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmCounterDC,ctx,"Counter",
    NULL,
    _cmDspCounterAlloc,
    NULL,
    _cmDspCounterReset,
    NULL,
    _cmDspCounterRecv,
    NULL,
    NULL,
    "Counter object. Set min => max to have no limit on the value." );

  return &_cmCounterDC;
}



//==========================================================================================================================================
enum
{
  kMaxPhId,
  kMultPhId,
  kPhsPhId,
  kOutPhId
};

cmDspClass_t _cmPhasorDC;

typedef struct
{
  cmDspInst_t inst;
} cmDspPhasor_t;


cmDspInst_t*  _cmDspPhasorAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  unsigned chs = 1;

  cmDspVarArg_t args[] =
  {
    { "max",  kMaxPhId,  0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "Maximum accumulator value" },
    { "mult", kMultPhId, 0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "Increment multiplier"  },
    { "phs",  kPhsPhId,  0, 0,   kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "Next phase value" },
    { "out",  kOutPhId,  0, chs, kOutDsvFl | kAudioBufDsvFl,               "Audio output." },
    { NULL, 0, 0, 0, 0 }
  };

  // allocate the instance
  cmDspPhasor_t* p = cmDspInstAlloc(cmDspPhasor_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  // assign default values to any of the the optional arg's which may not have been set from vl.
  cmDspSetDefaultSample(ctx, &p->inst, kMaxPhId,  0.0, cmSample_MAX);
  cmDspSetDefaultSample(ctx, &p->inst, kMultPhId, 0.0, 1.0);
  cmDspSetDefaultDouble(ctx, &p->inst, kPhsPhId,  0.0, 0.0);

  return &p->inst;
}

cmDspRC_t _cmDspPhasorReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  cmDspZeroAudioBuf( ctx, inst, kOutPhId );
  return kOkDspRC;
}

cmDspRC_t _cmDspPhasorExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmSample_t*       bp   = cmDspAudioBuf(ctx,inst,kOutPhId,0);
  const cmSample_t* ep   = bp + cmDspAudioBufSmpCount(ctx,inst,kOutPhId,0);
  cmSample_t        mult = cmDspSample(inst,kMultPhId);
  cmSample_t        max  = cmDspSample(inst,kMaxPhId);
  double            phs  = cmDspDouble(inst,kPhsPhId);
  cmSample_t        inc  = mult;

  for(; bp<ep; ++bp)
  {
    while( phs >= max )
      phs -= max;

    *bp = (cmSample_t)phs;

    phs += inc;

  }

  cmDspSetSample(ctx,inst,kPhsPhId,phs);
  
  return kOkDspRC;
}

cmDspRC_t _cmDspPhasorRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  switch( evt->dstVarId )
  {
    case kMultPhId:
    case kMaxPhId:
    case kPhsPhId:
      cmDspSetEvent(ctx, inst, evt );;
      break;

    default:
      { assert(0); }
  }

  return kOkDspRC;
}

struct cmDspClass_str* cmPhasorClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmPhasorDC,ctx,"Phasor",
    NULL,
    _cmDspPhasorAlloc,
    NULL,
    _cmDspPhasorReset,
    _cmDspPhasorExec,
    _cmDspPhasorRecv,
    NULL,
    NULL,
    "Ramp wave signal generator.");

  return &_cmPhasorDC;
}

//==========================================================================================================================================
enum
{
  kHzSgId,
  kShapeSgId,
  kGainSgId,
  kOtCntSgId,
  kOutSgId
};

enum
{
  kWhiteSgId, // 0
  kPinkSgId,  // 1
  kSineSgId,  // 2
  kCosSgId,   // 3
  kSawSgId,   // 4
  kSqrSgId,   // 5
  kTriSgId,   // 6
  kPulseSgId, // 7 
  kPhasorSgId // 8
};

cmDspClass_t _cmSigGenDC;

typedef struct
{
  cmDspInst_t inst;
  cmReal_t    phs;
  cmSample_t  reg;
} cmDspSigGen_t;

cmDspInst_t*  _cmDspSigGenAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "hz",     kHzSgId,     0, 0, kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Signal frequency in Hertz."   },
    { "shape",  kShapeSgId,  0, 0, kInDsvFl  | kUIntDsvFl   | kOptArgDsvFl,   "Wave shape 0=sine 1=cosine 2=white 3=pink"   },
    { "gain",   kGainSgId,   0, 0, kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "Output gain."},
    { "ot",     kOtCntSgId,  0, 0, kInDsvFl  | kUIntDsvFl   | kOptArgDsvFl,   "Overtone count."},
    { "out",    kOutSgId,    0, 1, kOutDsvFl | kAudioBufDsvFl, "Audio output." },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspSigGen_t* p = cmDspInstAlloc(cmDspSigGen_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspSetDefaultDouble(ctx, &p->inst, kHzSgId,    0.0, 1000);
  cmDspSetDefaultUInt(  ctx, &p->inst, kShapeSgId,   0, 0);
  cmDspSetDefaultUInt(  ctx, &p->inst, kOtCntSgId,   0, 0);
  cmDspSetDefaultDouble(ctx, &p->inst, kGainSgId,  0.0, 0.9 );

  return &p->inst;
}


cmDspRC_t _cmDspSigGenReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspSigGen_t* p = (cmDspSigGen_t*)inst;

  p->phs = 0;
  cmDspApplyAllDefaults(ctx,inst);
  cmDspZeroAudioBuf( ctx, inst, kOutSgId );
  p->reg = 0;
  return kOkDspRC;
}

cmDspRC_t _cmDspSigGenExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  
  cmDspSigGen_t*    p     = (cmDspSigGen_t*)inst;
  unsigned          chIdx = 0;
  cmSample_t*       bp    = cmDspAudioBuf(ctx,inst,kOutSgId,chIdx);
  unsigned          n     = cmDspAudioBufSmpCount(ctx,inst,kOutSgId,chIdx);
  const cmSample_t* ep    = bp + n;
  double            hz    = cmDspDouble(inst,kHzSgId);
  double            sr    = cmDspSampleRate(ctx);
  double            fact  = 2.0 * M_PI * hz / sr;
  int               shape = cmDspUInt(inst,kShapeSgId);
  double            offs  = shape == kCosSgId ? (-M_PI / 2.0) : 0;
  double            gain  = cmDspDouble(inst,kGainSgId);
  unsigned          otCnt = cmDspUInt(inst,kOtCntSgId);

  switch( shape )
  {
    case kWhiteSgId:
      while( bp < ep )
        *bp++ = gain * 2.0 * ((cmSample_t)rand() / RAND_MAX - 0.5);
      break;

    case kPinkSgId:
      while( bp < ep )
      {
        cmSample_t s = gain * 2.0 * ((cmSample_t)rand() / RAND_MAX - 0.5);        
        *bp++ = (s + p->reg)/2;
        p->reg = s;       
      }
      break;

    case kSineSgId:
    case kCosSgId:
      while( bp<ep )
      {
        *bp++ = (cmSample_t)(gain * sin( fact * p->phs + offs ));
        
        p->phs += 1.0;

      }
      break;


    case kSawSgId:
      p->phs = cmVOS_SynthSawtooth(bp,n,(unsigned)p->phs,sr,hz,otCnt);
      cmVOS_MultVS(bp,n,gain);
      break;

    case kSqrSgId:
      p->phs = cmVOS_SynthSquare(    bp,n,(unsigned)p->phs,sr,hz,otCnt );
      cmVOS_MultVS(bp,n,gain);
      break;

    case kTriSgId:
      p->phs = cmVOS_SynthTriangle(  bp,n,(unsigned)p->phs,sr,hz,otCnt );
      cmVOS_MultVS(bp,n,gain);
      break;

    case kPulseSgId:
      p->phs = cmVOS_SynthPulseCos(  bp,n,(unsigned)p->phs,sr,hz,otCnt );
      cmVOS_MultVS(bp,n,gain);
      break;

    case kPhasorSgId:
      p->phs = cmVOS_SynthPhasor(    bp,n,(unsigned)p->phs,sr,hz ); 
      cmVOS_MultVS(bp,n,gain);
      break;

    default:
      { assert(0); }
  }
  return kOkDspRC;
}


cmDspRC_t _cmDspSigGenRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc;
  if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
  {
    switch( evt->dstVarId )
    {
      case kShapeSgId:
        //printf("%s %i\n",cmDspInstLabel(ctx,inst),cmDspUInt(inst,kShapeSgId));
        break;
    }
  }
  return rc;
}



struct cmDspClass_str* cmSigGenClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmSigGenDC,ctx,"SigGen",
    NULL,
    _cmDspSigGenAlloc,
    NULL,
    _cmDspSigGenReset,
    _cmDspSigGenExec,
    _cmDspSigGenRecv,
    NULL,
    NULL,
    "Variable frequency and waveshape signal generator." );

  return &_cmSigGenDC;
}

//==========================================================================================================================================
enum
{
  kDeviceMiId,
  kPortMiId,
  kSmpIdxMiId,
  kStatusMiId,
  kD0MiId,
  kD1MiId,
  kSecMiId,
  kNSecMiId
};


cmDspClass_t _cmMidiInDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned midiSymId;
  unsigned prevSmpIdx;
  cmTimeSpec_t prevTimeStamp;
} cmDspMidiIn_t;

cmDspInst_t*  _cmDspMidiInAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "device", kDeviceMiId, 0,  0,  kOutDsvFl | kUIntDsvFl, "MIDI device" },
    { "port",   kPortMiId,   0,  0,  kOutDsvFl | kUIntDsvFl, "MIDI device port"},
    { "smpidx", kSmpIdxMiId, 0,  0,  kOutDsvFl | kUIntDsvFl, "Message time tag as sample index."},
    { "status", kStatusMiId, 0,  0,  kOutDsvFl | kUIntDsvFl, "MIDI status" },
    { "d0",     kD0MiId,     0,  0,  kOutDsvFl | kUIntDsvFl, "MIDI channel message d0" },
    { "d1",     kD1MiId,     0,  0,  kOutDsvFl | kUIntDsvFl, "MIDI channel message d1" },
    { "sec",    kSecMiId,    0,  0,  kOutDsvFl | kUIntDsvFl, "Time stamp integer seconds."},
    { "nsec",   kNSecMiId,   0,  0,  kOutDsvFl | kUIntDsvFl, "Time stamp fractional second (nanoseconds)."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspMidiIn_t* p = cmDspInstAlloc(cmDspMidiIn_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

 p->midiSymId =  cmDspSysAssignInstAttrSymbolStr( ctx->dspH, &p->inst, "_midi" );

  return &p->inst;
}

cmDspRC_t _cmDspMidiInReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  cmDspMidiIn_t* p  = (cmDspMidiIn_t*)inst;
  cmDspApplyAllDefaults(ctx,inst);
  p->prevSmpIdx = 0;
  return rc;
} 

cmDspRC_t  _cmDspMidiInRecvFunc(   cmDspCtx_t* ctx, struct cmDspInst_str* inst,  unsigned attrSymId, const cmDspValue_t* value )
{
  cmDspMidiIn_t* p = (cmDspMidiIn_t*)inst;

  if( attrSymId == p->midiSymId )
  {
    cmMidiPacket_t* pkt = (cmMidiPacket_t*)(value->u.m.u.vp);
    unsigned i;

    cmDspSetUInt(ctx, inst, kDeviceMiId, pkt->devIdx);
    cmDspSetUInt(ctx, inst, kPortMiId,   pkt->portIdx); 

    for(i=0; i<pkt->msgCnt; ++i)
    {
      cmMidiMsg* m = pkt->msgArray + i;
      unsigned   deltaSmpCnt = 0;
      if( p->prevTimeStamp.tv_sec!=0 && p->prevTimeStamp.tv_nsec!=0 )
        deltaSmpCnt = floor(cmTimeElapsedMicros(&p->prevTimeStamp,&m->timeStamp) * cmDspSampleRate(ctx) / 1000000.0);

      if( p->prevSmpIdx == 0 )
        p->prevSmpIdx = ctx->cycleCnt * cmDspSamplesPerCycle(ctx);
      else
        p->prevSmpIdx += deltaSmpCnt;

      cmDspSetUInt(ctx, inst, kSmpIdxMiId, p->prevSmpIdx );
      cmDspSetUInt(ctx, inst, kSecMiId,    m->timeStamp.tv_sec);
      cmDspSetUInt(ctx, inst, kNSecMiId,   m->timeStamp.tv_nsec);
      cmDspSetUInt(ctx, inst, kD1MiId,     m->d1 );
      cmDspSetUInt(ctx, inst, kD0MiId,     m->d0 );
      cmDspSetUInt(ctx, inst, kStatusMiId, m->status );

      p->prevTimeStamp = m->timeStamp;
    }
  }

  return kOkDspRC;
}

struct cmDspClass_str* cmMidiInClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmMidiInDC,ctx,"MidiIn",
    NULL,
    _cmDspMidiInAlloc,
    NULL,
    _cmDspMidiInReset,
    NULL,
    NULL,
    NULL,
    _cmDspMidiInRecvFunc,
    "Midi input port");

  return &_cmMidiInDC;
}

//==========================================================================================================================================
enum
{
  kDeviceMoId,
  kPortMoId,
  kStatusMoId,
  kD0MoId,
  kD1MoId,
  kResetMoId
};

cmDspClass_t _cmMidiOutDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned    devIdx;
  unsigned    portIdx;
  bool        enableFl;
} cmDspMidiOut_t;

cmDspRC_t _cmDspMidiOutSetDevice( cmDspCtx_t* ctx, cmDspMidiOut_t* p, const cmChar_t* deviceStr )
{
  cmDspRC_t rc = kOkDspRC;

  if( deviceStr != NULL )
    if((p->devIdx = cmMpDeviceNameToIndex(deviceStr)) == cmInvalidIdx )
      rc = cmDspInstErr(ctx,&p->inst,kInvalidArgDspRC,"The MIDI device '%s' could not be found.",cmStringNullGuard(deviceStr));
  return rc;
}

cmDspRC_t _cmDspMidiOutSetPort( cmDspCtx_t* ctx, cmDspMidiOut_t* p, const cmChar_t* portStr )
{
  cmDspRC_t rc = kOkDspRC;

  if( portStr == NULL )
    return rc;

  if( p->devIdx == cmInvalidIdx )
    rc = cmDspInstErr(ctx,&p->inst,kInvalidArgDspRC,"The MIDI port cannot be set until the MIDI device is set.");
  else
  {
    if((p->portIdx = cmMpDevicePortNameToIndex(p->devIdx,kOutMpFl,portStr)) == cmInvalidIdx )
      rc = cmDspInstErr(ctx,&p->inst,kInvalidArgDspRC,"The MIDI port '%s' could not be found on device '%s'.",cmStringNullGuard(portStr),cmStringNullGuard(cmMpDeviceName(p->devIdx)));
  }

  return rc;
}

cmDspInst_t*  _cmDspMidiOutAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "device", kDeviceMoId, 0,  0,  kInDsvFl | kStrzDsvFl | kReqArgDsvFl, "MIDI device name"},
    { "port",   kPortMoId,   0,  0,  kInDsvFl | kStrzDsvFl | kReqArgDsvFl, "MIDI port name"},
    { "status", kStatusMoId, 0,  0,  kInDsvFl | kUIntDsvFl, "MIDI status" },
    { "d0",     kD0MoId,     0,  0,  kInDsvFl | kUIntDsvFl, "MIDI channel message d0" },
    { "d1",     kD1MoId,     0,  0,  kInDsvFl | kUIntDsvFl, "MIDI channel message d1" },
    { "reset",  kResetMoId,  0,  0,  kInDsvFl | kTypeDsvMask,"All notes off" },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspMidiOut_t* p = cmDspInstAlloc(cmDspMidiOut_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  p->devIdx  = cmInvalidIdx;
  p->portIdx = cmInvalidIdx;

	cmDspSetDefaultUInt(ctx,&p->inst, kStatusMoId, 0, 0 );
  cmDspSetDefaultUInt(ctx,&p->inst, kD0MoId,     0, 0 );
  cmDspSetDefaultUInt(ctx,&p->inst, kD1MoId,     0, 0 );
  
  return &p->inst;
}

cmDspRC_t _cmDspMidiOutReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  cmDspMidiOut_t* p = (cmDspMidiOut_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  p->enableFl = false;

  if(_cmDspMidiOutSetDevice(ctx,p,cmDspStrcz(inst,kDeviceMoId)) == kOkDspRC )
    p->enableFl = _cmDspMidiOutSetPort(  ctx,p,cmDspStrcz(inst,kPortMoId)) == kOkDspRC;

  return rc;
} 

cmDspRC_t _cmDspMidiOutRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspMidiOut_t* p = (cmDspMidiOut_t*)inst;

  switch( evt->dstVarId )
  {
    case kDeviceMoId:      
      if(_cmDspMidiOutSetDevice(ctx, p, cmDsvStrcz(evt->valuePtr) ) != kOkDspRC )
        p->enableFl = false;
      break;

    case kPortMoId:
      if( _cmDspMidiOutSetPort(ctx, p, cmDsvStrcz(evt->valuePtr) ) != kOkDspRC )
        p->enableFl = false;
      break;

    case kStatusMoId:
      if( p->devIdx != cmInvalidIdx && p->portIdx != cmInvalidIdx )
      {
        unsigned status = cmDsvGetUInt(evt->valuePtr);
        unsigned d0     = cmDspUInt(inst,kD0MoId);
        unsigned d1     = cmDspUInt(inst,kD1MoId);
        if( p->enableFl )
          if( cmMpDeviceSend( p->devIdx, p->portIdx, status, d0, d1 ) != kOkMpRC )
            cmDspInstErr(ctx,inst,kInvalidArgDspRC,"MIDI send failed.");
      }
      break;

    case kResetMoId:
      {
        unsigned i;
        
        if( p->enableFl )
          for(i=0; i<kMidiChCnt; ++i)
          {          
            cmMpDeviceSend(p->devIdx,p->portIdx,kCtlMdId+i,121,0); // reset all controllers
            cmMpDeviceSend(p->devIdx,p->portIdx,kCtlMdId+i,123,0); // turn all notes off
            cmSleepMs(15);
          }
      }
      break;

    default:
      cmDspSetEvent(ctx,inst,evt);
      break;
  }


  return kOkDspRC;
}

struct cmDspClass_str* cmMidiOutClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmMidiOutDC,ctx,"MidiOut",
    NULL,
    _cmDspMidiOutAlloc,
    NULL,
    _cmDspMidiOutReset,
    NULL,
    _cmDspMidiOutRecv,
    NULL,
    NULL,
    "Midi input port");

  return &_cmMidiOutDC;
}

//==========================================================================================================================================
enum
{
  kChAiId,
  kGainAiId,
  kOutAiId
};

cmDspClass_t _cmAudioInDC;

typedef struct
{
  cmDspInst_t inst;
  bool        errFl; // used to control error reporting
} cmDspAudioIn_t;

cmDspInst_t*  _cmDspAudioInAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "ch",   kChAiId,   0,      0,  kInDsvFl  | kUIntDsvFl   | kReqArgDsvFl, "Audio input channel index"},
    { "gain", kGainAiId, 0,      0,  kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl, "Input gain multiplier" },
    { "out",  kOutAiId,  0,      1,  kOutDsvFl | kAudioBufDsvFl,              "Audio output" },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspAudioIn_t* p = cmDspInstAlloc(cmDspAudioIn_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspSetDefaultUInt(   ctx, &p->inst, kChAiId,   0,   0);
  cmDspSetDefaultDouble( ctx, &p->inst, kGainAiId, 0, 1.0);

  return &p->inst;
}

cmDspRC_t _cmDspAudioInReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t       rc = kOkDspRC;
  cmDspAudioIn_t* p  = (cmDspAudioIn_t*)inst;
  p->errFl           = false;
  cmDspApplyAllDefaults(ctx,inst);

  return rc;
} 


cmDspRC_t _cmDspAudioInExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  unsigned        chIdx  = cmDspUInt(inst,kChAiId);
  unsigned        iChCnt = ctx->ctx->iChCnt;
  cmDspAudioIn_t* p      = (cmDspAudioIn_t*)inst;
  double          gain   = cmDspDouble(inst,kGainAiId);

  if( chIdx >= iChCnt   )
  {
    if( p->errFl== false )
    {
      cmDspInstErr(ctx,inst,kInvalidArgDspRC,"The input channel index %i is invalid. Channel count:%i.",chIdx,iChCnt);
      p->errFl = true;
    }

    cmDspZeroAudioBuf(ctx,inst,kOutAiId);
    
    return kOkDspRC;
  }

  //unsigned    n  = cmDspSamplesPerCycle(ctx);
  unsigned    n  = cmDspAudioBufSmpCount(ctx,inst,kOutAiId,0);
  cmSample_t* dp = cmDspAudioBuf(ctx,inst,kOutAiId,0);


  assert( n == cmDspAudioBufSmpCount(ctx,inst,kOutAiId,chIdx));
  assert(dp != NULL);

  // if this channel is disabled then iChArray[chIdx] will be NULL
  if( ctx->ctx->iChArray[chIdx]!=NULL )
    cmVOS_MultVVS(dp,n,ctx->ctx->iChArray[chIdx],(cmSample_t)gain);

  return kOkDspRC;
}

cmDspRC_t _cmDspAudioInRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;
  switch( evt->dstVarId )
  {
    case kChAiId:
      if( cmDspSetEvent(ctx,inst,evt) != kOkDspRC )
      {
        // if the exec callback was previously disabled and the new channel value is valid then re-enable the exec callback.
        if( inst->execFunc==NULL && cmDspUInt(inst,kChAiId) < ctx->ctx->iChCnt )
          inst->execFunc = _cmDspAudioInExec;
      }
      break;

    case kGainAiId:
      cmDspSetEvent(ctx,inst,evt);
      break;
  }
  return rc;
}

struct cmDspClass_str* cmAudioInClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmAudioInDC,ctx,"AudioIn",
    NULL,
    _cmDspAudioInAlloc,
    NULL,
    _cmDspAudioInReset,
    _cmDspAudioInExec,
    _cmDspAudioInRecv,
    NULL,
    NULL,
    "Audio output port");

  return &_cmAudioInDC;
}




//==========================================================================================================================================
enum
{
  kChAoId,
  kGainAoId,
  kInAoId
};

cmDspClass_t _cmAudioOutDC;

typedef struct
{
  cmDspInst_t inst;
} cmDspAudioOut_t;

cmDspInst_t*  _cmDspAudioOutAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "ch",  kChAoId,  0,      0, kInDsvFl | kUIntDsvFl   | kReqArgDsvFl, "Audio output channel index"},
    { "gain",kGainAoId,0,      0, kInDsvFl | kDoubleDsvFl | kOptArgDsvFl, "Output gain multiplier"},  
    { "in",  kInAoId,  0,      1, kInDsvFl | kAudioBufDsvFl, "Audio input" },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspAudioOut_t* p = cmDspInstAlloc(cmDspAudioOut_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);


  cmDspSetDefaultUInt(   ctx, &p->inst, kChAoId,   0,   0);
  cmDspSetDefaultDouble( ctx, &p->inst, kGainAoId, 0, 1.0);

  return &p->inst;
}

cmDspRC_t _cmDspAudioOutReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;
  cmDspApplyAllDefaults(ctx,inst);
  return rc;
} 


cmDspRC_t _cmDspAudioOutExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc     = kOkDspRC;
  unsigned          chIdx  = cmDspUInt(inst,kChAoId);
  unsigned          oChCnt = ctx->ctx->oChCnt;
  double            gain   = cmDspDouble(inst,kGainAoId);

  if( chIdx >= oChCnt )
  {
    rc = cmDspInstErr(ctx,inst,kInvalidArgDspRC,"The output channel index %i is invalid. Channel count:%i.",chIdx,oChCnt);
    inst->execFunc = NULL; // disable callbacks - this prevents the error msg from repeating
    return rc;
  }

  const cmSample_t* sp     = cmDspAudioBuf(ctx,inst,kInAoId,0);

  if( sp == NULL )
  {
    inst->execFunc = NULL; // if there is no connected input then disable further callbacks
    return kOkDspRC;
  }

  unsigned n = cmDspSamplesPerCycle(ctx);

  assert( n == cmDspVarRows(inst,kInAoId) );
  
  // if this channel is disabled or set to pass-through then chArray[chIdx] will be NULL
  if( ctx->ctx->oChArray[chIdx] != NULL )
    cmVOS_MultVVS(ctx->ctx->oChArray[chIdx],n,sp,(cmSample_t)gain);

  return kOkDspRC;
}

cmDspRC_t _cmDspAudioOutRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;
  switch( evt->dstVarId )
  {
    case kChAoId:
      if( cmDspSetEvent(ctx,inst,evt) != kOkDspRC )
      {
        // if the exec callback was previously disabled and the new channel value is valid then re-enable the exec callback.
        if( inst->execFunc==NULL && cmDspUInt(inst,kChAoId) < ctx->ctx->oChCnt )
          inst->execFunc = _cmDspAudioOutExec;
      }
      break;

    case kGainAoId:
      cmDspSetEvent(ctx,inst,evt);
      break;
  }
  return rc;
}

struct cmDspClass_str* cmAudioOutClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmAudioOutDC,ctx,"AudioOut",
    NULL,
    _cmDspAudioOutAlloc,
    NULL,
    _cmDspAudioOutReset,
    _cmDspAudioOutExec,
    _cmDspAudioOutRecv,
    NULL,NULL,
    "Audio output port");

  return &_cmAudioOutDC;
}

//==========================================================================================================================================
enum
{
  kFnAofId,
  kChCntAofId,
  kGain0AofId,
  kGain1AofId,
  kIn0AofId,
  kIn1AofId,
  kSelAofId
};

cmDspClass_t _cmAudioFileOutDC;

typedef struct
{
  cmDspInst_t    inst;
  cmSample_t*    smpBuf;
  unsigned       smpCnt;
  unsigned       bits;
  cmAudioFileH_t afH;
  unsigned       openSymId;
  unsigned       closeSymId;
  const cmChar_t* afn;
} cmDspAudioFileOut_t;

cmDspRC_t _cmDspAudioFileOutCreateFile( cmDspCtx_t* ctx, cmDspInst_t* inst, unsigned chCnt )
{
  cmDspRC_t            rc = kOkDspRC;
  cmDspAudioFileOut_t* p  = (cmDspAudioFileOut_t*)inst;
  const cmChar_t*      fn = cmDspStrcz(inst,kFnAofId);

  if(cmAudioFileIsValid(p->afH) )
    cmAudioFileDelete(&p->afH);

  // if the supplied audio file name is actually a directory name then generate a file name
  if( cmFsIsDir(fn) )
  {
    cmMemPtrFree(&p->afn);

    if( cmFsGenFn(fn,"recd","aiff",&p->afn) != kOkFsRC )
      return cmDspInstErr(ctx,&p->inst,kFileSysFailDspRC,"An output audio file name could not be generated.");
    
    fn = p->afn;
  }

  if( cmAudioFileIsValid(p->afH =  cmAudioFileNewCreate(fn, cmDspSampleRate(ctx), p->bits, chCnt, &rc, ctx->rpt )) == false )
    rc = cmDspClassErr(ctx,inst->classPtr,kVarArgParseFailDspRC,"The output audio file '%s' create failed.",fn);

  return rc;
}

cmDspInst_t*  _cmDspAudioFileOutAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "fn",    kFnAofId,    0,   0, kInDsvFl | kStrzDsvFl   | kReqArgDsvFl, "Audio file or directory name"},
    { "chs",   kChCntAofId, 0,   0, kInDsvFl | kUIntDsvFl   | kReqArgDsvFl, "Channel count"}, 
    { "gain0", kGain0AofId, 0,   0, kInDsvFl | kDoubleDsvFl | kOptArgDsvFl, "Output gain 0 multiplier"},  
    { "gain1", kGain1AofId, 0,   0, kInDsvFl | kDoubleDsvFl | kOptArgDsvFl, "Output gain 1 multiplier"},
    { "in0",   kIn0AofId,   0,   1, kInDsvFl | kAudioBufDsvFl,              "Audio input 0"},
    { "in1",   kIn1AofId,   0,   1, kInDsvFl | kAudioBufDsvFl,              "Audio input 1"},
    { "sel",   kSelAofId,   0,   0, kInDsvFl,                               "Open | Close"},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspAudioFileOut_t* p = cmDspInstAlloc(cmDspAudioFileOut_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspValue_t chCntVal;
  cmDsvSetUInt(&chCntVal,cmMin(2,cmDspUInt(&p->inst,kChCntAofId)));
  
  cmDspSetDefault(ctx, &p->inst, kChCntAofId, &chCntVal );

  cmDspSetDefaultDouble( ctx, &p->inst, kGain0AofId, 0, 1.0);
  cmDspSetDefaultDouble( ctx, &p->inst, kGain1AofId, 0, 1.0);

  p->bits       = 16;
  p->afH        = cmNullAudioFileH;
  p->openSymId  = cmSymTblRegisterStaticSymbol(ctx->stH,"open");
  p->closeSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"close");

  return &p->inst;
}

cmDspRC_t _cmDspAudioFileOutReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t            rc = kOkDspRC;
  cmDspAudioFileOut_t* p  = (cmDspAudioFileOut_t*)inst;
  unsigned             chCnt = cmDspUInt(inst,kChCntAofId);

  cmDspApplyAllDefaults(ctx,inst); 

  p->smpCnt = cmDspSamplesPerCycle(ctx) * chCnt;
  p->smpBuf = cmLhResizeN(ctx->lhH, cmSample_t, p->smpBuf, p->smpCnt);
  
  //rc = _cmDspAudioFileOutCreateFile( ctx, inst, chCnt );

  return rc;
} 

cmDspRC_t _cmDspAudioFileOutExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t            rc    = kOkDspRC;
  cmDspAudioFileOut_t* p     = (cmDspAudioFileOut_t*)inst;;
  unsigned             chCnt = cmMin(2,cmDspUInt(inst,kChCntAofId));
  unsigned             smpCnt = 0;
  cmSample_t*          chArray[chCnt];
  unsigned             i,j;

  if(!cmAudioFileIsValid(p->afH) )
    return rc;


  for(i=0,j=0; i<chCnt; ++i)
  {
    unsigned          chVarId = i == 0 ? kIn0AofId : kIn1AofId;     // get audio buf var id for this ch
    unsigned          iSmpCnt = cmDspVarRows(inst,chVarId);

    if( iSmpCnt == 0 )
    {
      chArray[j] = NULL;
    }
    else
    {
      cmSample_t gain = cmDspSample(inst,i==0?kGain0AofId:kGain1AofId); // get ch gain
    
      chArray[j] = cmDspAudioBuf(ctx,inst,chVarId,0);                   // get incoming audio buf ptr

      if( gain != 1.0 )
        cmVOS_MultVVS(chArray[j], iSmpCnt, chArray[j], gain);           // apply gain

      ++j;                                                              // incr chArray[] index
      assert( smpCnt==0 || iSmpCnt==smpCnt);                          
      smpCnt = iSmpCnt;                                                 // set outgoing sample count
    }

  }

  // write the samples
  if( cmAudioFileWriteSample(p->afH, smpCnt, j, chArray ) != kOkAfRC )
    rc = cmDspClassErr(ctx,inst->classPtr,kFileWriteFailDspRC,"An audio output file write failed.");

  return rc;
}

cmDspRC_t _cmDspAudioFileOutRecv( cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;
  switch( evt->dstVarId )
  {
    case kGain1AofId:
    case kGain0AofId:
      cmDspSetEvent(ctx,inst,evt);
      break;

    case kFnAofId:
      cmDspSetEvent(ctx,inst,evt);
      rc = _cmDspAudioFileOutCreateFile(ctx,inst,cmDspUInt(inst,kChCntAofId));
      break;

    case kSelAofId:
      {
          cmDspAudioFileOut_t* p     = (cmDspAudioFileOut_t*)inst;  
          unsigned symId = cmDsvSymbol(evt->valuePtr);
          if( symId == p->openSymId )
            rc = _cmDspAudioFileOutCreateFile(ctx,inst,cmDspUInt(inst,kChCntAofId));
          else
          {
            if( symId == p->closeSymId )
            {
              if(cmAudioFileIsValid(p->afH) )
                cmAudioFileDelete(&p->afH);
            }
            else
            {
              rc = cmErrMsg(&inst->classPtr->err,kInvalidArgDspRC,"Unknown selector symbol (%i) %s.",symId,cmStringNullGuard(cmSymTblLabel(ctx->stH,symId)));
            }
          }
      }
  }
  return rc;
}

cmDspRC_t  _cmDspAudioFileOutFree( cmDspCtx_t* ctx, struct cmDspInst_str*  inst,  const cmDspEvt_t* evtPtr )
{
  cmDspAudioFileOut_t* p     = (cmDspAudioFileOut_t*)inst;  

  if(cmAudioFileIsValid(p->afH) )
    cmAudioFileDelete(&p->afH);

  cmMemPtrFree(&p->afn);

  return kOkDspRC;
}

struct cmDspClass_str* cmAudioFileOutClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmAudioFileOutDC,ctx,"AudioFileOut",
    NULL,
    _cmDspAudioFileOutAlloc,
    _cmDspAudioFileOutFree,
    _cmDspAudioFileOutReset,
    _cmDspAudioFileOutExec,
    _cmDspAudioFileOutRecv,
    NULL,NULL,
    "Audio file output port");

  return &_cmAudioFileOutDC;
}

//==========================================================================================================================================
enum
{
  kTypScId,
  kMinScId,
  kMaxScId,
  kStpScId,
  kValScId,
  kLblScId,
  kSendScId
};

cmDspClass_t _cmScalarDC;

typedef struct
{
  cmDspInst_t inst;
} cmDspScalar_t;

cmDspInst_t*  _cmDspScalarAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "typ",  kTypScId,  0, 0, kInDsvFl  | kUIntDsvFl   | kReqArgDsvFl,  "Type" },
    { "min",  kMinScId,  0, 0, kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl,  "Minimum value"},
    { "max",  kMaxScId,  0, 0, kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl,  "Maximum value"},
    { "step", kStpScId,  0, 0, kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl,  "Step value (set to 0 to ignore)"},
    { "val",  kValScId,  0, 0, kInDsvFl  | kOutDsvFl    | kDoubleDsvFl | kReqArgDsvFl | kSendDfltDsvFl,  "Current value"},
    { "lbl",  kLblScId,  0, 0,                            kStrzDsvFl   | kOptArgDsvFl, "Label"},
    { "send", kSendScId, 0, 0, kInDsvFl  | kTypeDsvMask, "Send value on any msg."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspScalar_t* p = cmDspInstAlloc(cmDspScalar_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspSetDefaultUInt(  ctx, &p->inst, kTypScId,  0,   kNumberDuiId);
  cmDspSetDefaultDouble(ctx, &p->inst, kMinScId,  0.0, 0);
  cmDspSetDefaultDouble(ctx, &p->inst, kMaxScId,  0.0, 1);
  cmDspSetDefaultDouble(ctx, &p->inst, kStpScId,  0.0, 0);

  unsigned typeId = cmDspDefaultUInt(&p->inst,kTypScId);

  // create the UI control
  cmDspUiScalarCreate(ctx,&p->inst,typeId,kMinScId,kMaxScId,kStpScId,kValScId,kLblScId);

  return &p->inst;
}

cmDspRC_t _cmDspScalarReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  return kOkDspRC;
}

cmDspRC_t _cmDspScalarRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{

  if( evt->dstVarId == kSendScId )
  {
    // NOT SURE IF THIS IS CORRECT OR NOT - IT SHOULD FORCE THE CURRENT VALUE TO BE 
    // SENT - THE CURRENT VALUE SHOULD ALREADY BE AT THE UI SO THERE DOESN'T SEEM
    // TO BE ANY REASON TO UPDATE IT THERE.
    cmDspSetDouble( ctx, inst, kValScId, cmDspDouble(inst,kValScId ));
    return kOkDspRC;
  }

  switch( evt->dstVarId )
  {
    case kMinScId:
    case kMaxScId:
    case kStpScId:
    case kValScId:
      cmDspSetEvent(ctx,inst,evt);
      break;

    default:
      {assert(0);}
  }

  return kOkDspRC;
}

cmDspRC_t  _cmDspScalarPresetRdWr( cmDspCtx_t* ctx, cmDspInst_t*  inst,  bool storeFl )
{ 
  return cmDspVarPresetRdWr(ctx,inst,kValScId,storeFl); 
}

struct cmDspClass_str* cmScalarClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmScalarDC,ctx,"Scalar",
    NULL,
    _cmDspScalarAlloc,
    NULL,
    _cmDspScalarReset,
    NULL,
    _cmDspScalarRecv,
    _cmDspScalarPresetRdWr,
    NULL,
    "Scalar value control.");

  return &_cmScalarDC;
}

//==========================================================================================================================================
enum
{
  kValTxId,
  kLblTxId
};

cmDspClass_t _cmTextDC;

typedef struct
{
  cmDspInst_t inst;
} cmDspText_t;

cmDspInst_t*  _cmDspTextAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "val",  kValTxId,  0, 0, kInDsvFl  | kOutDsvFl    | kStrzDsvFl | kReqArgDsvFl | kSendDfltDsvFl,  "Current string"},
    { "lbl",  kLblTxId,  0, 0, kStrzDsvFl | kOptArgDsvFl, "Label"},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspText_t* p = cmDspInstAlloc(cmDspText_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  // create the UI control
  cmDspUiTextCreate(ctx,&p->inst,kValTxId,kLblTxId);

  return &p->inst;
}

cmDspRC_t _cmDspTextReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  return kOkDspRC;
}

cmDspRC_t _cmDspTextRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{

  switch( evt->dstVarId )
  {
    case kValTxId:
      cmDspSetEvent(ctx,inst,evt);
      break;

    default:
      {assert(0);}
  }

  return kOkDspRC;
}

struct cmDspClass_str* cmTextClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmTextDC,ctx,"Text",
    NULL,
    _cmDspTextAlloc,
    NULL,
    _cmDspTextReset,
    NULL,
    _cmDspTextRecv,
    NULL,NULL,
    "Text value control.");

  return &_cmTextDC;
}


//==========================================================================================================================================
enum
{
  kInMtId,
  kMinMtId,
  kMaxMtId,
  kTimeMtId,
  kLblMtId,
};

cmDspClass_t _cmMeterDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned    updSmpCnt;
  unsigned    lastCycleCnt;
  double      value;
  unsigned    cnt;
} cmDspMeter_t;

cmDspInst_t*  _cmDspMeterAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "in",   kInMtId,   0, 0, kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl,  "Current value"},
    { "min",  kMinMtId,  0, 0, kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl,  "Minimum value"},
    { "max",  kMaxMtId,  0, 0, kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl,  "Maximum value"},
    { "time", kTimeMtId, 0, 0, kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,  "UI update time in milliseconds (default:50)"},
    { "label",kLblMtId,  0, 0, kStrzDsvFl, "Label"},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspMeter_t* p = cmDspInstAlloc(cmDspMeter_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspSetDefaultDouble(ctx, &p->inst, kInMtId,   0.0, 0);
  cmDspSetDefaultDouble(ctx, &p->inst, kMinMtId,  0.0, 0);
  cmDspSetDefaultDouble(ctx, &p->inst, kMaxMtId,  0.0, 1);
  cmDspSetDefaultDouble(ctx, &p->inst, kTimeMtId, 0.0, 50.0);

  // create the UI control
  cmDspUiMeterCreate(ctx,&p->inst,kMinMtId,kMaxMtId,kInMtId,kLblMtId);

  return &p->inst;
}

cmDspRC_t _cmDspMeterReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspMeter_t* p = (cmDspMeter_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  double updateMs = cmDspDouble(inst,kTimeMtId);

  p->updSmpCnt    = floor(cmDspSampleRate(ctx) * updateMs / 1000.0);
  p->lastCycleCnt = 0;
  p->cnt          = 0;
  p->value        = 0;
  return kOkDspRC;
}



cmDspRC_t _cmDspMeterExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspMeter_t* p        = (cmDspMeter_t*)inst; 
  double        curVal   = p->value;
  bool          deltaFl  = p->cnt!=0 && curVal != cmDspDouble(inst,kInMtId);
  bool          expireFl = (ctx->cycleCnt - p->lastCycleCnt) * cmDspSamplesPerCycle(ctx) > p->updSmpCnt;
 
  // if the meter value changed and the update time has expired
  if( deltaFl && expireFl )
  {
    cmDspSetDouble(ctx,inst,kInMtId,curVal);
    p->value        = 0;
    p->cnt          = 0;
    p->lastCycleCnt = ctx->cycleCnt;
  }  
      
  return kOkDspRC;
}

cmDspRC_t _cmDspMeterRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{  
  cmDspMeter_t* p = (cmDspMeter_t*)inst;

  switch( evt->dstVarId )
  {
    case kInMtId:
      p->value = cmDsvGetDouble(evt->valuePtr);
      ++p->cnt;      
      return kOkDspRC;

    case kTimeMtId:
      p->updSmpCnt = floor(cmDspSampleRate(ctx) * cmDsvGetDouble(evt->valuePtr) / 1000.0);
      break;
  }

  return cmDspSetEvent(ctx,inst,evt); 

}

struct cmDspClass_str* cmMeterClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmMeterDC,ctx,"Meter",
    NULL,
    _cmDspMeterAlloc,
    NULL,
    _cmDspMeterReset,
    _cmDspMeterExec,
    _cmDspMeterRecv,
    NULL,NULL,
    "Meter display.");

  return &_cmMeterDC;
}


//==========================================================================================================================================
enum
{
  kInLbId,
  kAlignLbId
};

cmDspClass_t _cmLabelDC;

typedef struct
{
  cmDspInst_t inst;
} cmDspLabel_t;

cmDspInst_t*  _cmDspLabelAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "in",   kInLbId,    0, 0,  kInDsvFl | kStrzDsvFl  | kReqArgDsvFl,  "LabelText" },
    { "align",kAlignLbId, 0, 0,  kInDsvFl | kUIntDsvFl  | kOptArgDsvFl,  "Alignment 0=right 1=left 2=center" },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspLabel_t* p = cmDspInstAlloc(cmDspLabel_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspSetDefaultDouble(ctx, &p->inst, kAlignLbId,  0.0, kLeftAlignDuiId);

  // create the UI control
  cmDspUiLabelCreate(ctx,&p->inst,kInLbId,kAlignLbId);

  return &p->inst;
}

cmDspRC_t _cmDspLabelReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  return kOkDspRC;
}

cmDspRC_t _cmDspLabelRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return cmDspSetEvent(ctx,inst,evt);
}

struct cmDspClass_str* cmLabelClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmLabelDC,ctx,"Label",
    NULL,
    _cmDspLabelAlloc,
    NULL,
    _cmDspLabelReset,
    NULL,
    _cmDspLabelRecv,
    NULL,NULL,
    "Label control.");

  return &_cmLabelDC;
}


//==========================================================================================================================================
enum
{
  kTypBtId,
  kOutBtId,
  kSymBtId,
  kLblBtId,
  kInBtId
};

cmDspClass_t _cmButtonDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned    resetSymId;
} cmDspButton_t;

cmDspInst_t*  _cmDspButtonAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  va_list vl1;
  va_copy(vl1,vl);

  assert( va_cnt >= 1 );
  unsigned typeId = va_arg(vl,unsigned);

  // check buttons should transmit their default values - push buttons should not.
  unsigned sendDfltFl = typeId == kCheckDuiId ? kSendDfltDsvFl : 0;
  
  cmDspVarArg_t args[] =
  {
    { "typ",  kTypBtId,  0, 0,             kUIntDsvFl   | kReqArgDsvFl,              "Type" },
    { "out",  kOutBtId,  0, 0, kOutDsvFl | kDoubleDsvFl | kOptArgDsvFl | sendDfltFl, "Value"},
    { "sym",  kSymBtId,  0, 0, kOutDsvFl | kSymDsvFl    | kOptArgDsvFl | sendDfltFl, "Symbol Value"},
    { "label",kLblBtId,  0, 0, kInDsvFl  | kStrzDsvFl   | kOptArgDsvFl,              "Label"},
    { "in",   kInBtId,   0, 0, kInDsvFl  | kTypeDsvMask,                             "Simulate UI"},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspButton_t* p = cmDspInstAlloc(cmDspButton_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl1);

  
  cmDspSetDefaultDouble(ctx, &p->inst, kOutBtId,  0.0, typeId==kCheckDuiId ? 0.0 : 1.0);
  cmDspSetDefaultSymbol(ctx, &p->inst, kSymBtId,        instSymId );
  cmDspSetDefaultStrcz( ctx, &p->inst, kLblBtId, NULL, cmSymTblLabel(ctx->stH,instSymId));

  p->resetSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"_reset");

  // create the UI control
  cmDspUiButtonCreate(ctx,&p->inst,typeId,kOutBtId,kLblBtId);

  return &p->inst;
}

cmDspRC_t _cmDspButtonReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  return kOkDspRC;
}

cmDspRC_t _cmDspButtonRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  // the 'in' port is the only input port 
  // but the UI sends button pushes use kOutBtId - should this be changed?
  assert( evt->dstVarId == kInBtId || evt->dstVarId == kOutBtId );

  // We accept all types at the 'in' port but are only interested
  // in transmitting doubles from the 'out' port.

  if( cmDsvCanConvertFlags( kDoubleDsvFl, evt->valuePtr->flags ) )
  {
    // Redirect events which can be converted to type kDoubleDsvFl 
    // to the output port.
    // 
    // Convert the event dest var id from the 'kInBtId' to 'kOutBtId' 
    // and update the UI  with the incoming value 
    cmDspSetEventUiId(ctx,inst,evt,kOutBtId);

  }

  // no matter what kind of msg enters the 'in' port send a symbol out the 'sym' port
  if( inst->symId != cmInvalidId )
    cmDspSetSymbol( ctx, inst, kSymBtId, inst->symId ); 
  
  return kOkDspRC;
}

cmDspRC_t  _cmDspButtonPresetRdWr( cmDspCtx_t* ctx, cmDspInst_t*  inst,  bool storeFl )
{ 
  cmDspRC_t rc = kOkDspRC;
  if( cmDspUInt(inst,kTypBtId) == kCheckDuiId )
    rc = cmDspVarPresetRdWr(ctx,inst,kOutBtId,storeFl); 
  return rc;
}

cmDspRC_t  _cmDspButtonSysRecvFunc(   cmDspCtx_t* ctx, struct cmDspInst_str* inst,  unsigned attrSymId, const cmDspValue_t* value )
{
  cmDspButton_t* p = (cmDspButton_t*)inst;
  if( attrSymId == p->resetSymId )
  {
    cmDspSetSymbol( ctx, inst, kSymBtId, p->resetSymId );
    cmDspSetDouble(ctx,inst,kOutBtId, cmDspDouble(inst,kOutBtId));
  }

  return kOkDspRC;
}

struct cmDspClass_str* cmButtonClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmButtonDC,ctx,"Button",
    NULL,
    _cmDspButtonAlloc,
    NULL,
    _cmDspButtonReset,
    NULL,
    _cmDspButtonRecv,
    _cmDspButtonPresetRdWr,
    _cmDspButtonSysRecvFunc,
    "Button control.");

  return &_cmButtonDC;
}

//==========================================================================================================================================
enum
{
  kLblCbId,
  kSym1CbId,
  kSym0CbId,
  kVal1CbId,
  kVal0CbId,
  kOutCbId,
  kSymCbId,
  kInCbId
};

cmDspClass_t _cmCheckboxDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned    resetSymId;
  unsigned    onSymId;
  unsigned    offSymId;
} cmDspCheckbox_t;

cmDspInst_t*  _cmDspCheckboxAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{


  // Check buttons should transmit their default values. Set kSendDfltDsvFl on outputs to send default values.
  
  cmDspVarArg_t args[] =
  {
    { "label",kLblCbId,  0, 0, kInDsvFl  | kStrzDsvFl   | kOptArgDsvFl,   "Label"},
    { "sym1", kSym1CbId, 0, 0, kInDsvFl  | kStrzDsvFl   | kOptArgDsvFl,   "'on' symbol value"},
    { "sym0", kSym0CbId, 0, 0, kInDsvFl  | kStrzDsvFl   | kOptArgDsvFl,   "'off' symbol value"},
    { "val1", kVal1CbId, 0, 0, kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "'on' value"},
    { "val0", kVal0CbId, 0, 0, kInDsvFl  | kDoubleDsvFl | kOptArgDsvFl,   "'off' value"},
    { "out",  kOutCbId,  0, 0, kOutDsvFl | kDoubleDsvFl | kOptArgDsvFl | kSendDfltDsvFl, "Value"},
    { "sym",  kSymCbId,  0, 0, kOutDsvFl | kSymDsvFl                   | kSendDfltDsvFl, "Symbol Value"},
    { "in",   kInCbId,   0, 0, kInDsvFl  | kTypeDsvMask,                  "Simulate UI"},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspCheckbox_t* p = cmDspInstAlloc(cmDspCheckbox_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  p->resetSymId = cmSymTblRegisterStaticSymbol(ctx->stH,"_reset");

  cmDspSetDefaultStrcz( ctx, &p->inst, kSym1CbId, NULL, "on");
  cmDspSetDefaultStrcz( ctx, &p->inst, kSym0CbId, NULL, "off");

  p->onSymId    = cmSymTblRegisterSymbol(ctx->stH, cmDspDefaultStrcz(&p->inst,kSym1CbId));
  p->offSymId   = cmSymTblRegisterSymbol(ctx->stH, cmDspDefaultStrcz(&p->inst,kSym0CbId));


  bool fl = cmDspDefaultDouble(&p->inst,kOutCbId)!=0;
  
  cmDspSetDefaultDouble(ctx, &p->inst, kVal1CbId, 0.0, 1.0);
  cmDspSetDefaultDouble(ctx, &p->inst, kVal0CbId, 0.0, 0.0);
  cmDspSetDefaultDouble(ctx, &p->inst, kOutCbId,  0.0, 0.0);
  cmDspSetDefaultSymbol(ctx, &p->inst, kSymCbId,  fl ? p->onSymId : p->offSymId );
  cmDspSetDefaultStrcz( ctx, &p->inst, kLblCbId, NULL, cmSymTblLabel(ctx->stH,instSymId));


  // create the UI control
  cmDspUiButtonCreate(ctx,&p->inst,kCheckDuiId,kOutCbId,kLblCbId);

  return &p->inst;
}

cmDspRC_t _cmDspCheckboxReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  return kOkDspRC;
}

cmDspRC_t _cmDspCheckboxRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;
  cmDspCheckbox_t* p = (cmDspCheckbox_t*)inst;

  switch( evt->dstVarId )
  {
    case kLblCbId:
      // TODO: we have no function for changing a UI control label.
      return rc;

    case kVal1CbId:
    case kVal0CbId:
    case kSym1CbId:
    case kSym0CbId:
      cmDspSetEvent(ctx,inst,evt);

      if( evt->dstVarId == kSym1CbId )
        p->onSymId    = cmSymTblRegisterSymbol(ctx->stH, cmDspStrcz(&p->inst,kSym1CbId));

      if( evt->dstVarId == kSym0CbId )
        p->offSymId   = cmSymTblRegisterSymbol(ctx->stH, cmDspStrcz(&p->inst,kSym0CbId));

      return rc;

  }

  // the 'in' port is the only input port 
  // but the UI button pushes use kOutCbId.
  assert( evt->dstVarId == kInCbId || evt->dstVarId == kOutCbId );

  // We accept all types at the 'in' port but are only interested
  // in transmitting doubles from the 'out' port and symbols from 
  // the 'sym' port.

  if( cmDsvCanConvertFlags( kDoubleDsvFl, evt->valuePtr->flags ) )
  {
    bool checkFl = cmDsvGetDouble(evt->valuePtr)!=0;

    unsigned valId = checkFl ? kVal1CbId  : kVal0CbId;
    unsigned symId = checkFl ? p->onSymId : p->offSymId;


    // Redirect events which can be converted to type kDoubleDsvFl 
    // to the output port.
    // 
    // Convert the event dest var id from the 'kInCbId' to 'kOutCbId' 
    // and update the UI  with the incoming value 
    cmDspEvt_t e;
    cmDspValue_t v;
    
    cmDspEvtCopy(&e,evt);
    e.valuePtr = &v;
    cmDsvSetDouble(&v,cmDspDouble(inst,valId));
    cmDspSetEventUiId(ctx,inst,evt,kOutCbId);

    cmDspSetSymbol( ctx, inst, kSymCbId, symId);

  }

  
  return kOkDspRC;
}

cmDspRC_t  _cmDspCheckboxPresetRdWr( cmDspCtx_t* ctx, cmDspInst_t*  inst,  bool storeFl )
{ 
  return cmDspVarPresetRdWr(ctx,inst,kOutCbId,storeFl); 
}

cmDspRC_t  _cmDspCheckboxSysRecvFunc(   cmDspCtx_t* ctx, struct cmDspInst_str* inst,  unsigned attrSymId, const cmDspValue_t* value )
{
  cmDspCheckbox_t* p = (cmDspCheckbox_t*)inst;
  if( attrSymId == p->resetSymId )
  {
    cmDspSetSymbol( ctx, inst, kSymCbId, p->resetSymId );
    cmDspSetDouble(ctx,inst,kOutCbId, cmDspDouble(inst,kOutCbId));
  }

  return kOkDspRC;
}

struct cmDspClass_str* cmCheckboxClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmCheckboxDC,ctx,"Checkbox",
    NULL,
    _cmDspCheckboxAlloc,
    NULL,
    _cmDspCheckboxReset,
    NULL,
    _cmDspCheckboxRecv,
    _cmDspCheckboxPresetRdWr,
    _cmDspCheckboxSysRecvFunc,
    "Checkbox control.");

  return &_cmCheckboxDC;
}

//==========================================================================================================================================
cmDspClass_t _cmReorderDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned    portCnt;       // count of input ports and count of output ports
  unsigned*   execFlArray;   // execFlArray[portCnt] - true for ports which should cause obj to generate output
  unsigned*   orderArray;    // orderArray[portCnt]  - port output order map

} cmDspReorder_t;

cmDspInst_t*  _cmDspReorderAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"Reorder objects must have arguments.");
    return NULL;
  }

  // the first argument is the count of input ports (which is also the count of output ports)
  int portCnt = va_arg(vl,int);

  if( portCnt < 2 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"Reorder objects must have at least 2 ports.");
    return NULL;
  }

  cmDspVarArg_t args[portCnt*2+1];
  unsigned      i;
  int           maxLabelCharCnt = 15;
  cmChar_t      label[ maxLabelCharCnt+1 ];

  label[maxLabelCharCnt] = 0;

  for(i=0; i<portCnt*2; ++i)
  {
    snprintf(label,maxLabelCharCnt,"%s-%i", (i<portCnt?"in":"out"), i%portCnt);

    unsigned symId  = cmSymTblRegisterSymbol(ctx->stH,label);
    
    args[i].label   = cmSymTblLabel(ctx->stH,symId);  
    args[i].constId = i;
    args[i].rn      = 0;
    args[i].cn      = 0;
    args[i].flags   = (i<portCnt ? kInDsvFl : kOutDsvFl) | kTypeDsvMask;
    args[i].doc     =  i<portCnt ? "Any input" : "Any output";
  }

  memset(args+i,0,sizeof(args[0]));
  
  cmDspReorder_t* p = cmDspInstAlloc(cmDspReorder_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  if( p != NULL )
  {
    p->orderArray = cmLhAllocZ(ctx->lhH,unsigned,portCnt*2);
    p->execFlArray = p->orderArray + portCnt;
    p->portCnt     = portCnt;

    if( va_cnt-1 < portCnt )
      cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The reorder constructor must contain an output order map.");
    else
    {
      for(i=0; i<portCnt; ++i)
      {
        int order = va_arg(vl,int);
    
        if( order >= portCnt )
          cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The port order index %i is outside the valid range of 0-%i.",order,portCnt-1);
        else
          p->orderArray[ order ] = i;
        
      }

      va_cnt -= portCnt+1;

      for(i=0; i<va_cnt; ++i)
      {
        int execPortIdx = va_arg(vl,int);

        if( execPortIdx >= portCnt )
          cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The port exec index %i is outside the valid range of 0-%i.",execPortIdx,portCnt-1);
        else
          p->execFlArray[ execPortIdx ] = true;
      }
      
    }
  }
  
  return &p->inst;
}

cmDspRC_t _cmDspReorderFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspReorder_t* p = (cmDspReorder_t*)inst;
  cmLhFree(ctx->lhH,p->orderArray);
  return kOkDspRC;
}

cmDspRC_t _cmDspReorderRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;
  cmDspReorder_t* p = (cmDspReorder_t*)inst;

  cmDspSetEvent(ctx,inst,evt);

  if( evt->dstVarId < p->portCnt )
    if( p->execFlArray[evt->dstVarId] )
    {
      unsigned i;
      for(i=0; i<p->portCnt; ++i)
      {
        unsigned          inVarId  = p->orderArray[i];
        unsigned          outVarId = inVarId + p->portCnt;
        const cmDspVar_t* varPtr   = cmDspVarIdToCPtr(inst, inVarId );

        assert(varPtr != NULL);

        if((rc = cmDspValueSet(ctx, inst, outVarId, &varPtr->value, 0 )) != kOkDspRC )
          break;        
      }
    }

  return rc;
}

struct cmDspClass_str* cmReorderClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmReorderDC,ctx,"Reorder",
    NULL,
    _cmDspReorderAlloc,
    _cmDspReorderFree,
    NULL,
    NULL,
    _cmDspReorderRecv,
    NULL,NULL,
    "Reorder value control.");

  return &_cmReorderDC;
}


//==========================================================================================================================================
enum
{
  kDirFnId,
  kPatFnId,
  kValFnId,
  kSendFnId
};

cmDspClass_t _cmFnameDC;

typedef struct
{
  cmDspInst_t inst;
} cmDspFname_t;

// Pattern string for HTML and image files:
//   "HTML Files (*.html)\tImage Files (*.{bmp,gif,jpg,png})"

// The va_list must include 3 args: 
// A pointer to a string referning to a default filename or directory or NULL.
// A pointer to a string referring to a pattern string or NULL.
// A bool to set the 'dirFl'.
cmDspInst_t*  _cmDspFnameAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "dir",  kDirFnId,  0, 0, kInDsvFl  | kReqArgDsvFl | kBoolDsvFl, "Dir=true Filename=false" },
    { "pat",  kPatFnId,  0, 0, kInDsvFl  | kOptArgDsvFl | kStrzDsvFl, "File pattern string (e.g. HTML Files (*.html)\tImage Files (*.{bmp,gif,jpg,png}))" },
    { "out",  kValFnId,  0, 0, kOutDsvFl | kOptArgDsvFl | kStrzDsvFl, "Current file or directory name." },
    { "send", kSendFnId, 0, 0, kInDsvFl  | kTypeDsvMask, "Send file name on any msg."},

    { NULL, 0, 0, 0, 0 }
  };

  cmDspFname_t* p = cmDspInstAlloc(cmDspFname_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspSetDefaultBool( ctx, &p->inst, kDirFnId,  false, false);  
  cmDspSetDefaultStrcz(ctx, &p->inst, kPatFnId,  NULL,  "All Files (*.*)");  
  cmDspSetDefaultStrcz(ctx, &p->inst, kValFnId,  NULL,  cmFsUserDir());

  cmDspUiFnameCreate(ctx,&p->inst,kValFnId,kPatFnId,kDirFnId);

  return &p->inst;
}

cmDspRC_t _cmDspFnameReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  return kOkDspRC;
}

cmDspRC_t _cmDspFnameRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspSetEvent(ctx,inst,evt);

  switch( evt->dstVarId )
  {
    case kSendFnId:
      cmDspSetStrcz(ctx, inst, kValFnId, cmDspStrcz(inst,kValFnId) );
      break;

    case kDirFnId:
    case kPatFnId:
    case kValFnId:
      break;

    default:
      {assert(0);}
  }

  return kOkDspRC;
}

struct cmDspClass_str* cmFnameClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmFnameDC,ctx,"Fname",
    NULL,
    _cmDspFnameAlloc,
    NULL,
    _cmDspFnameReset,
    NULL,
    _cmDspFnameRecv,
    NULL,NULL,
    "File or directory chooser control.");

  return &_cmFnameDC;
}

//==========================================================================================================================================

cmDspClass_t _cmMsgListDC;

typedef struct
{
  cmDspInst_t     inst;
  cmJsonH_t       jsH;         // current JSON tree
  cmJsonNode_t*   np;          // ptr to JSON array
  unsigned        colCnt;      // number of elements in the JSON sub-arrays (rows)
  cmJsonH_t       dfltJsH;     // default JSON tree (the default JSON array node ptr (np) is stored in the var array but we must also maintain the assoc'd JSON handle othwerwise the pointer will not be valid)
  unsigned*       typeIdArray; // JSON types for each column  size: typeIdArray[ colCnt ]
  unsigned        symColCnt;   // same as number of elements of typeIdArray[] == kStringTId
  unsigned        symRowCnt;   // same as cmJsonChildCount(p->np)
  unsigned*       symM;        // symM[symRowCnt,symColCnt] symbol matrix
} cmDspMsgList_t;

// create a matrix to hold the symbol id's associated with any string values
cmDspRC_t _cmDspMsgListLoadSymbolMtx( cmDspCtx_t* ctx, cmDspMsgList_t* p )
{
  assert(cmJsonIsArray(p->np));

  unsigned i,j,k;
  
  // remove any existing symbols
  if( p->symM != NULL )
    for(i=0; i<p->symColCnt*p->symRowCnt; ++i)
      if( p->symM[i] != cmInvalidId )
        cmSymTblRemove(ctx->stH,p->symM[i]);

  // reallocate the symbol matrix
  p->symRowCnt = cmJsonChildCount(p->np);
  p->symM      = cmLhResizeN(ctx->lhH,unsigned,p->symM,p->symRowCnt*p->symColCnt);

  // for each row in the JSON array
  for(i=0; i<p->symRowCnt; ++i)
  {
    const cmJsonNode_t* cnp = cmJsonArrayElementC(p->np,i);

    // for each column in row whose data type is a string
    for(j=0,k=0; k<p->symColCnt; ++j)
      if( p->typeIdArray[j] == kStringTId )
      {
        const cmJsonNode_t* vnp = cmJsonArrayElementC(cnp,j);
        unsigned            idx = k*p->symRowCnt + i;
        const cmChar_t*     text;

        // register the string with the symbol table
        if((text = vnp->u.stringVal) != NULL )
          p->symM[ idx ] = cmSymTblRegisterStaticSymbol(ctx->stH,text);
        else
          p->symM[ idx ] = cmInvalidId;

        ++k;        
      }
  }

  return kOkDspRC;
}

// Load a JSON file and set the supplied cmJsonH_t handle.
cmDspRC_t _cmDspMsgListLoadFile( cmDspCtx_t* ctx, cmErr_t* err, const cmChar_t* rsrcLabel, const cmChar_t* fn, cmJsonH_t* hp, unsigned* colCntPtr, cmJsonNode_t** npp )
{
  cmDspRC_t           rc  = kOkDspRC;     //
  cmJsonNode_t*       np  = NULL;         //
  cmJsonH_t           jsH = cmJsonNullHandle;  //

  *hp         = cmJsonNullHandle;
  *colCntPtr  = 0;
  *npp        = NULL;

  // if no file name was given ...
  if( fn==NULL || strlen(fn)==0 )
  {
    jsH = ctx->rsrcJsH; // ... use the rsrc file

    if( cmJsonIsValid(ctx->rsrcJsH) == false )
      return cmErrMsg(err,kJsonFailDspRC,"No JSON cfg resource exists for this DSP program.");

    fn  = NULL;
  }
  else
  {
    if( cmJsonInitializeFromFile(&jsH,fn,ctx->cmCtx) != kOkJsRC )
      return cmErrMsg(err,kJsonFailDspRC,"The msg list JSON file load failed on '%s'.",fn);
  }

  // find the array named by rsrcLabel
  if((np = cmJsonFindValue(jsH,rsrcLabel,NULL,cmInvalidId)) == NULL)
    return cmErrMsg(err,kJsonFailDspRC,"The msg list JSON tree does not have an array named '%s'.",rsrcLabel);

  // be sure the msg list really is an array
  if( cmJsonIsArray(np) == false )
    return cmErrMsg(err,kJsonFailDspRC,"The msg list JSON element named '%s' is not an array.", rsrcLabel);

  if( fn == NULL )
    fn = "<resource file>";

  // count of elements in the array
  unsigned n = cmJsonChildCount(np);
  unsigned m = 0;
  unsigned i,j;

  // for each line in the array
  for(i=0; i<n; ++i)
  {
    const cmJsonNode_t* cnp;

    // get the ith sub-array (row)
    if((cnp = cmJsonArrayElementC(np,i)) != NULL )
    {
      // verify that it is an array
      if( cmJsonIsArray(cnp ) == false )
        return cmErrMsg(err,kJsonFailDspRC,"The msg list JSON element in '%s' at index %i is not an array.",fn,i);
      
      // track the number of elements (columns) per row
      unsigned q = cmJsonChildCount(cnp);

      // if this is the first row then use it to set the valid column count
      if( i==0 )
        m   = q;
      else
      {
        if( m != q )
          return cmErrMsg(err,kJsonFailDspRC,"The msg list sub-array at index %i has a different number of elements than the preceding sub-arrays in '%s'.",i,fn);
      }
    }
  }

  //
  // determine and validate the column types
  //
  
  unsigned typeIdArray[m];

  // for each row
  for(i=0; i<n; ++i)
  {
    const cmJsonNode_t* cnp = cmJsonArrayElementC(np,i);

    // for each column
    for(j=0; j<m; ++j)
    {
      const cmJsonNode_t* sap    = cmJsonArrayElementC(cnp,j);
      unsigned            typeId = sap->typeId & kMaskTId;

      // the first row sets the expected type id for each column
      switch(i)
      {
        case 0:
          if( typeId != kStringTId )
            return cmErrMsg(err,kJsonFailDspRC,"The first row of a msg list (%s) file must contain string elements which set the colum labels.",rsrcLabel);
          break;

        case 1:
          typeIdArray[j] = typeId;
          break;

        default:
          {
            // if the type is a string then it can only 
            // match if the column is a string or null 
            // so we assume an error
            if( typeId == kStringTId )
              rc = kJsonFailDspRC;

            switch(typeIdArray[j])
            {
              case kStringTId:            
                if( typeId != kStringTId && typeId != kNullTId)
                  return cmErrMsg(err,kJsonFailDspRC,"The msg list element at row index %i column index %i cannot be converted to a string in '%s'.",i,j,fn);

                rc = kOkDspRC; // clear the assummed error
                break;

              case kNullTId:   // null can be converted to anything
                typeIdArray[j] = typeId; 
                break;

              case kIntTId:
                if( typeId == kRealTId )    // ints may be promoted to reals
                  typeIdArray[j] = kRealTId;
                break;

              case kRealTId:
                break;

              case kFalseTId:
              case kTrueTId: // bools may be promoted to ints or reals
                if( typeId == kIntTId || typeId == kRealTId )
                  typeIdArray[j] = typeId;
                break;

              default:
                return cmErrMsg(err,kJsonFailDspRC,"The msg list element at row index %i column index %i is not a string,int,real,bool, or null type in '%s'.",i,j,fn);

            }
        
            if( rc != kOkDspRC )
              return cmErrMsg(err,kJsonFailDspRC,"The string msg list element at row index %i column index %i cannot be converted to the column type in '%s'.",i,j,fn);
          } // end dflt
      } // end switch
    } // end row
  } // end list


  // VERY TRICKY - store the column type id's in the label columns type id.
  // This is stupid but safe because the column type id's are known to be set to kStringTId.
  cmJsonNode_t* lnp = np->u.childPtr->u.childPtr;
  for(i=0; i<m; ++i,lnp=lnp->siblingPtr)
    lnp->typeId = typeIdArray[i];

  *hp        = fn==NULL ? cmJsonNullHandle : jsH;
  *npp       = np;
  *colCntPtr = m;

  return rc;
}

// use the JSON list labels to setup the cmDspVarArg_t records associated with each msg output var.
cmDspMsgList_t* _cmDspMsgListCons( cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, const cmDspVarArg_t* args, cmJsonH_t jsH, unsigned colCnt, cmJsonNode_t* np, unsigned typeIdArray[], unsigned va_cnt, va_list vl )
{
  unsigned i,j;
  unsigned fixedArgCnt = 0;
  for(i=0; args[i].label != NULL; ++i)
    ++fixedArgCnt;

  cmDspVarArg_t a[ fixedArgCnt + colCnt + 1 ];

  // copy the fixed arg's into the first fixedArgCnt ele's of a[]
  for(j=0; j<fixedArgCnt; ++j)
    a[j] = args[j];

  // remove the label row from the JSON array
  cmJsonNode_t* lnp = np->u.childPtr;  // store the pointer to the label row
  //np->u.childPtr    = lnp->siblingPtr; // remove the labels from the array 

  // for each column
  for(i=0; i<colCnt; ++i,++j)
  {
    cmJsonNode_t* lp = cmJsonArrayElement(lnp,i);

    // store a pointer to the label
    a[j].label   = lp->u.stringVal;
    a[j].constId = j;
    a[j].rn      = 0;
    a[j].cn      = 0;
    a[j].flags   = kOutDsvFl;
    a[j].doc     = "Msg output";

    typeIdArray[i] = lp->typeId;

    // convert the JSON type to a DSV type
    switch( lp->typeId )
    {
      case kNullTId:   a[j].flags |= kUIntDsvFl;   break;
      case kIntTId:    a[j].flags |= kIntDsvFl;    break;
      case kRealTId:   a[j].flags |= kDoubleDsvFl; break;
      case kTrueTId:   a[j].flags |= kBoolDsvFl;   break;
      case kFalseTId:  a[j].flags |= kBoolDsvFl;   break;
      case kStringTId: a[j].flags |= kSymDsvFl;    break; // strings are treated as symbols (UInt)

      default:
        { assert(0); }
    }

    // undo the tricky bit with the label types
    lp->typeId = kStringTId;
  }

  // set the null sentinel at the end of the arg array
  memset(a + j,0,sizeof(cmDspVarArg_t));

  return (cmDspMsgList_t*)cmDspInstAlloc(cmDspMsgList_t,ctx,classPtr,a,instSymId,id,storeSymId,va_cnt,vl);
}

cmDspRC_t _cmDspMsgListReload( cmDspCtx_t* ctx, cmDspMsgList_t* p, const cmChar_t* rsrcLabel, const cmChar_t* fn )
{
  cmDspRC_t     rc     = kOkDspRC;
  cmJsonH_t     jsH    = cmJsonNullHandle;
  unsigned      colCnt = 0;
  cmJsonNode_t* np     = NULL;

  // load the file
  if((rc = _cmDspMsgListLoadFile(ctx,&p->inst.classPtr->err,rsrcLabel,fn,&jsH,&colCnt,&np)) != kOkDspRC )
    return cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"The msg list file '%s' load failed.",fn);
  
  // verify that the col count is correct
  if( colCnt != p->colCnt )
    return cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"The column count (%i) of msg list file '%s does not match the msg list column count %i.",colCnt,p->colCnt);


  unsigned      fixArgCnt = p->inst.varCnt - p->colCnt;
  cmJsonNode_t* lnp       = np->u.childPtr->u.childPtr;
  unsigned      i,j;

  for(i=0,j=fixArgCnt; i<colCnt; ++i,++j, lnp=lnp->siblingPtr)
  {
    const cmChar_t* labelStr = cmSymTblLabel(ctx->stH,p->inst.varArray[j].symId);

    // the labels of the new file must match the labels of the previous file
    if( strcmp(lnp->u.stringVal,labelStr) )
      return cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"The msg list file '%s' label '%s' does not match the msg list label '%s' in column index %i.",fn,labelStr,lnp->u.stringVal,i);

    // if the msg column is a string ....
    if( (p->inst.varArray[j].flags & kTypeDsvMask) == kStrzDsvFl )
    {
      // ... then the file column must also be a string or null
      if( lnp->typeId != kStringTId && np->typeId != kNullTId )
        return cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"The data type of msg list file '%s' column index %i  must be a string or null.",fn,i);
    }
    else // otherwie if the msg column is a number ...
    {
      // ... then the file type can't be a string
      if( lnp->typeId == kStringTId )
        return cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"The data type of msg list file '%s' column index %i cannot be a string.",fn,i);

      // TODO: maybe there are other type conversions to check for here.
    }

    // reset the typeid of the labels 
    // (this is a cleanup from the tricky bit at the end of _cmDspListLoadFile())
    lnp->typeId = kStringTId;
  }
  
  if( cmJsonIsValid( jsH ) )
  {
    if( cmHandlesAreNotEqual(p->jsH,p->dfltJsH) && cmHandlesAreNotEqual(p->jsH,ctx->rsrcJsH) )
      cmJsonFinalize(&p->jsH);

    p->jsH = jsH;
    p->np  = np;

    _cmDspMsgListLoadSymbolMtx(ctx,p);
  }

  return rc;
}


enum
{
  kRsrcMlId,
  kFnMlId,
  kSelMlId,
  kListMlId,
  kCntMlId,
  kOutBaseMlId  // identify the first output port
};

cmDspInst_t*  _cmDspMsgListAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  va_list       vl2;
  cmDspVarArg_t args[] =
  {
    { "rsrc", kRsrcMlId, 0, 0,   kInDsvFl  | kReqArgDsvFl | kStrzDsvFl,  "Msg list resource label"},
    { "fn",   kFnMlId,   0, 0,   kInDsvFl  | kOptArgDsvFl | kStrzDsvFl,  "Msg list file name"   },
    { "sel",  kSelMlId,  0, 0,   kOutDsvFl | kInDsvFl  | kOptArgDsvFl | kUIntDsvFl,  "Current selection index" },
    { "list", kListMlId, 0, 0,   kInDsvFl  |                kJsonDsvFl,  "Msg list data as a JSON array"},
    { "cnt",  kCntMlId,  0, 0,   kOutDsvFl | kSendDfltDsvFl | kUIntDsvFl ,  "Count of elements."},
    { NULL, 0, 0, 0, 0 }
  };

  va_copy(vl2,vl);

  if( va_cnt < 1 )
  {
    cmErrMsg(&classPtr->err,kVarArgParseFailDspRC,"The message list constructor must contain at least two arguments.");
    goto errLabel;
  }

  const cmChar_t* rsrcLabel = va_arg(vl,const char*);
  const cmChar_t* fn        = va_cnt>1 ? va_arg(vl,const char*) : NULL;
  unsigned        colCnt    = 0;
  cmJsonH_t       jsH       = cmJsonNullHandle;
  cmDspMsgList_t* p;
  cmJsonNode_t*   np;
  cmDspRC_t       rc;

  // be sure the rsrc label contains a valid string
  if( rsrcLabel==NULL || strlen(rsrcLabel)==0 )
  {
    cmErrMsg(&classPtr->err,kVarArgParseFailDspRC,"No msg list resource label was given.");
    goto errLabel;
  }

  // load and validate the JSON file
  if((rc = _cmDspMsgListLoadFile(ctx,&classPtr->err,rsrcLabel,fn,&jsH,&colCnt,&np)) == kOkDspRC )
  {
    unsigned i;
    unsigned typeIdArray[colCnt];

    // allocate the instance
    if((p = _cmDspMsgListCons(ctx,classPtr,storeSymId,instSymId,id,args,jsH,colCnt, np, typeIdArray, va_cnt, vl2 )) != NULL )
    {
      p->jsH         = jsH;
      p->np          = np;
      p->dfltJsH     = jsH;
      p->colCnt      = colCnt;
      p->typeIdArray = cmLhAllocZ( ctx->lhH, unsigned, colCnt );
      memcpy(p->typeIdArray,typeIdArray,sizeof(p->typeIdArray[0])*colCnt);
      p->symColCnt   = 0;

      for(i=0; i<p->colCnt; ++i)
        if( p->typeIdArray[i] == kStringTId )
          ++p->symColCnt;
   
      _cmDspMsgListLoadSymbolMtx(ctx,p);

      cmDspSetDefaultStrcz( ctx,&p->inst, kRsrcMlId,NULL, rsrcLabel); // rsrc label 
      cmDspSetDefaultStrcz( ctx,&p->inst, kFnMlId,  NULL, fn);        // file name var
      cmDspSetDefaultJson( ctx,&p->inst,  kListMlId,NULL, np);        // default tree
      cmDspSetDefaultUInt( ctx,&p->inst,  kSelMlId, 0,     0);        // selection
      cmDspSetDefaultUInt( ctx,&p->inst,  kCntMlId, 0,    p->symRowCnt);

      // if there is only one column then signal the UI to create a menu button rather
      // than a list by setting the height to zero.
      unsigned height = p->colCnt == 1 ? 0 : 5;

      // create the list UI element
      cmDspUiMsgListCreate(ctx, &p->inst, height, kListMlId, kSelMlId );

      return &p->inst;
    }
  }
  
 errLabel:
  return NULL;
}

cmDspRC_t _cmDspMsgListFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspMsgList_t* p = (cmDspMsgList_t*)inst;

  
  // be careful not release the ctx->rsrcJsH handle if is happens to be the p->dflJsH
  if( cmJsonIsValid(p->dfltJsH) && cmHandlesAreNotEqual(p->jsH,p->dfltJsH)  && cmHandlesAreNotEqual(p->dfltJsH,ctx->rsrcJsH))
    if( cmJsonFinalize(&p->dfltJsH) != kOkJsRC )
      cmDspInstErr(ctx,inst,kJsonFailDspRC,"JSON default tree finalize failed.");

  // be careful not to release the ctx->rsrcJsH handle if it happens to be the p->jsH
  if( cmJsonIsValid(p->jsH) && cmHandlesAreNotEqual(p->dfltJsH,ctx->rsrcJsH) )
    if( cmJsonFinalize( &p->jsH ) != kOkJsRC )
      cmDspInstErr(ctx,inst,kJsonFailDspRC,"JSON finalization failed.");

  cmLhFree(ctx->lhH,p->typeIdArray);
  p->typeIdArray = NULL;

  cmLhFree(ctx->lhH,p->symM);
  p->symM = NULL;

  return kOkDspRC;
}

cmDspRC_t _cmDspMsgListReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  return kOkDspRC;
}

cmDspRC_t _cmDspMsgOnSel( cmDspCtx_t* ctx, cmDspMsgList_t* p, unsigned rowIdx )
{
  cmDspRC_t rc = kOkDspRC;
  unsigned i,j;
  unsigned      symId;
  const cmJsonNode_t* cnp = cmJsonArrayElementC(p->np,rowIdx);
          
  assert( cnp != NULL );

  // for each output variable (column)
  for(i=0,j=0; i<p->colCnt; ++i)
  {
    unsigned            varId = kOutBaseMlId + i;            // variable id
    const cmJsonNode_t* np    = cmJsonArrayElementC(cnp,i);  // json row array
    assert( np != NULL );

    switch( p->typeIdArray[i] )
    {
      case kNullTId: 
        break;

      case kIntTId:   
        {
          int v;
          if(  cmJsonIntValue(np,&v) != kOkJsRC )
          { assert(0); }

          rc = cmDspSetInt(ctx,&p->inst,varId,v);
        }
        break;

      case kRealTId:
        {
          double v;
          if(  cmJsonRealValue(np,&v) != kOkJsRC )
          { assert(0); }

          rc = cmDspSetDouble(ctx,&p->inst,varId,v);
        }
        break;

      case kTrueTId:
      case kFalseTId:
        {
          bool v;
          if(  cmJsonBoolValue(np,&v) != kOkJsRC )
          { assert(0); }

          rc = cmDspSetBool(ctx,&p->inst,varId,v);
        }
        break;

      case kStringTId:
        {
          assert( j < p->symColCnt );

          if((symId = p->symM[ (j*p->symRowCnt) + rowIdx ]) != cmInvalidId )
            if((rc = cmDspSetSymbol(ctx,&p->inst,varId,symId )) != kOkDspRC )
              break;

          ++j;
        }
        break;

      default:
        { assert(0); }

    } // end switch
  } // end for

  return rc;
}

cmDspRC_t _cmDspMsgListRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t       rc    = kOkDspRC;
  cmDspMsgList_t* p     = (cmDspMsgList_t*)inst;

  switch( evt->dstVarId )
  {
    
    case kRsrcMlId:
      {
        const cmChar_t* fn        = cmDspStrcz(inst,kFnMlId);
        const cmChar_t* rsrcLabel = cmDsvStrz(evt->valuePtr);

        if( rsrcLabel != NULL )
        {
          if((rc = _cmDspMsgListReload(ctx,p,rsrcLabel,fn)) != kOkDspRC )
            return cmDspInstErr(ctx,&p->inst,kJsonFailDspRC,"The msg list file '%s' load failed.",fn);

          cmDspSetEvent(ctx,inst,evt);
          cmDspSetJson(ctx,inst,kListMlId,p->np);
          cmDspSetUInt(ctx,inst,kSelMlId,0);
          cmDspSetUInt(ctx,inst,kCntMlId,p->symRowCnt);
        }
      }
      break;

    case kFnMlId:
      // Store the new file name.
      // A new file will not be loaded until the next rsrc label is received.
      cmDspSetEvent(ctx,inst,evt); 
      break;

    case kListMlId:
      break;

    case kSelMlId:
      {
        unsigned      rowIdx = cmDsvGetUInt(evt->valuePtr);

        assert( rowIdx < p->symRowCnt);
        
        // set the current selection variable
        if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
          rc = _cmDspMsgOnSel(ctx,p,rowIdx);
      } //end case
      break;
  }

  return rc;
}

cmDspRC_t  _cmDspMsgListPresetRdWr( cmDspCtx_t* ctx, cmDspInst_t*  inst,  bool storeFl )
{ 
  cmDspRC_t rc = kOkDspRC;
  cmDspMsgList_t* p = (cmDspMsgList_t*)inst;
  if((rc = cmDspVarPresetRdWr(ctx,inst,kSelMlId,storeFl)) == kOkDspRC )
  {
    if( !storeFl )
      rc = _cmDspMsgOnSel(ctx,p, cmDspUInt(inst,kSelMlId) );
  }
  return rc;
}


struct cmDspClass_str* cmMsgListClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmMsgListDC,ctx,"MsgList",
    NULL,
    _cmDspMsgListAlloc,
    _cmDspMsgListFree,
    _cmDspMsgListReset,
    NULL,
    _cmDspMsgListRecv,
    _cmDspMsgListPresetRdWr,
    NULL,
    "Message list selection control.");

  return &_cmMsgListDC;
}

//==========================================================================================================================================

enum
{
  kLenWtId,
  kShapeWtId,
  kFnWtId,
  kLoopWtId,
  kBegWtId,
  kEndWtId,
  kCmdWtId,
  kOtWtId,
  kGainWtId,
  kPhsWtId,
  kOutWtId,
  kCntWtId,
  kFIdxWtId,
  kDoneWtId
};

enum
{
  kSilenceWtId, // 0
  kFileWtId,    // 1
  kWhiteWtId,   // 2
  kPinkWtId,    // 3
  kSineWtId,    // 4
  kCosWtId,     // 5
  kSqrWtId,     // 6
  kTriWtId,     // 7
  kSawWtId,     // 8
  kPulseWtId,   // 9
  kImpulseWtId, // 10  
  kPhasorWtId,  // 11
  kShapeWtCnt
};

cmDspClass_t _cmWaveTableDC;

typedef struct
{
  cmDspInst_t    inst;
  cmSample_t*    wt;            // wave table memory
  unsigned       wti;           // next location to write samples into the wavetable
  unsigned       wtn;           // count of empty samples (avail for writing over) in the wavetable.
  unsigned       fi;            // absolute index into the file of the next sample to read
  unsigned       fn;            // length of the file in samples
  unsigned       cfi;           // absolute index into the file of the beginning of the current audio vector
  unsigned       cfn;           // when cfi >= cfn and doneFl is set then the 'done' msg is sent
  unsigned       loopCnt;       // current loop count
  bool           doneFl;        // the wave table source is exhausted 
  cmAudioFileH_t afH;           // current audio file handle
  int            nxtBegSmpIdx;  // the beg/end sample index to use with the next filename to arrive at port 'fn'
  int            nxtEndSmpIdx;  //
  cmThreadH_t    thH;
  bool           loadFileFl;
  cmDspCtx_t*    ctx;
  cmSample_t     phsOffs;
  cmSample_t     phsLast;
  unsigned       onSymId;
  unsigned       offSymId;
  unsigned       doneSymId;
  bool           useThreadFl;
  unsigned       minAfIndexRptCnt; // min count of audio samples between transmitting the current audio file index
  unsigned       afIndexRptCnt;    // current audio file sample index count
 } cmDspWaveTable_t;

bool _cmDspWaveTableThreadFunc( void* param);

cmDspInst_t*  _cmDspWaveTableAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "len",    kLenWtId,    0, 0, kInDsvFl  | kUIntDsvFl | kOptArgDsvFl, "Wave table length in samples" },
    { "shape",  kShapeWtId,  0, 0, kInDsvFl  | kUIntDsvFl | kOptArgDsvFl, "Wave shape 0=silent 1=file 2=sine 3=white"   },
    { "fn",     kFnWtId,     0, 0, kInDsvFl  | kStrzDsvFl | kOptArgDsvFl, "Optional audio file name"     },
    { "loop",   kLoopWtId,   0, 0, kInDsvFl  | kIntDsvFl  | kOptArgDsvFl, "-1=loop forever  >0=loop count (dflt:-1)"},
    { "beg",    kBegWtId,    0, 0, kInDsvFl  | kIntDsvFl  | kOptArgDsvFl, "File begin sample index" },
    { "end",    kEndWtId,    0, 0, kInDsvFl  | kIntDsvFl  | kOptArgDsvFl, "File end sample index (-1=play all)" },
    { "cmd",    kCmdWtId,    0, 0, kInDsvFl  | kSymDsvFl  | kOptArgDsvFl, "Command: on off"},
    { "ot",     kOtWtId,     0, 0, kInDsvFl  | kUIntDsvFl | kOptArgDsvFl, "Overtone count"},
    { "gain",   kGainWtId,   0, 0, kInDsvFl  | kDoubleDsvFl|kOptArgDsvFl, "Gain"},
    { "phs",    kPhsWtId,    0, 0, kInDsvFl  | kAudioBufDsvFl,            "Driving phase" },
    { "out",    kOutWtId,    0, 1, kOutDsvFl | kAudioBufDsvFl,            "Audio output" },
    { "cnt",    kCntWtId,    0, 0, kOutDsvFl | kIntDsvFl,                 "Loop count event."},
    { "fidx",   kFIdxWtId,   0, 0, kOutDsvFl | kUIntDsvFl,                "Current audio file index."},
    { "done",   kDoneWtId,   0, 0, kOutDsvFl | kSymDsvFl,                 "'done' sent after last loop."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspWaveTable_t* p = cmDspInstAlloc(cmDspWaveTable_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  // get the filename given in the va_list (or NULL if no filename was given)
  const cmChar_t* fn = cmDspDefaultStrcz(&p->inst,kFnWtId);

  p->offSymId         = cmSymTblRegisterStaticSymbol(ctx->stH,"off");
  p->onSymId          = cmSymTblRegisterStaticSymbol(ctx->stH,"on");
  p->doneSymId        = cmSymTblRegisterStaticSymbol(ctx->stH,"done");

  double  adCurFileIdxRptPeriodMs = 100.0;
  p->minAfIndexRptCnt = floor(adCurFileIdxRptPeriodMs * cmDspSampleRate(ctx) / 1000.0);

  cmDspSetDefaultUInt(  ctx, &p->inst, kLenWtId,   0,    cmDspSampleRate(ctx));
  cmDspSetDefaultUInt(  ctx, &p->inst, kShapeWtId, 0,    kSilenceWtId  );
  cmDspSetDefaultStrcz( ctx, &p->inst, kFnWtId,    NULL, fn );
  cmDspSetDefaultInt(   ctx, &p->inst, kLoopWtId,  0,    -1 );
  cmDspSetDefaultInt(   ctx, &p->inst, kBegWtId,   0,     0 );
  cmDspSetDefaultInt(   ctx, &p->inst, kEndWtId,   0,    -1 );
  cmDspSetDefaultSymbol(ctx, &p->inst, kCmdWtId,   p->onSymId );
  cmDspSetDefaultUInt(  ctx, &p->inst, kOtWtId,    0,     5 );
  cmDspSetDefaultDouble(ctx, &p->inst, kGainWtId,  0,     1.0 );
  cmDspSetDefaultUInt(  ctx, &p->inst, kFIdxWtId,  0,     0 );

  p->useThreadFl = false;

  return &p->inst;
}

cmDspRC_t _cmDspWaveTableFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspWaveTable_t* p = (cmDspWaveTable_t*)inst;

  if( cmThreadIsValid(p->thH) )
    if( cmThreadDestroy(&p->thH) != kOkThRC )
      cmDspInstErr(ctx,inst,kThreadFailDspRC,"The wavetable file reader thread failed to close.");

  if( cmAudioFileIsValid(p->afH) )
    cmAudioFileDelete(&p->afH);

  cmLhFree(ctx->lhH,p->wt);

  return kOkDspRC;
}

// Read the next block of 'rdSmpCnt' samples starting at the absolute file index 'p->fi'
// into wt[rdSmpCnt].
// If the end of the file segment marked by absolute file indexes 'begSmpIdx' to 'endSmpIdx'
// is encountered in the middle of the requested block and the wave table is in loop
// mode then the the function will automatically begin reading from the begining of the
// file segment.  If the end of the file segment is encountered and the wave table is not
// in loop mode then the empty portion of wt[] will be set to zero.
cmDspRC_t _cmDspWaveTableReadBlock( cmDspCtx_t* ctx, cmDspWaveTable_t* p, cmSample_t* wt, unsigned rdSmpCnt, int begSmpIdx, int endSmpIdx, int maxLoopCnt  )
{
  unsigned    actFrmCnt = 0;
  unsigned    chIdx     = 0;
  unsigned    chCnt     = 1;
  unsigned    fn        = endSmpIdx - p->fi + 1; // count of samples between p->fi and endSmpIdx
  unsigned    n0        = rdSmpCnt;
  unsigned    n1        = 0;

  // if the requested sample count will go past the end of the file segment
  if( rdSmpCnt > fn )
  {
    n1  = rdSmpCnt - fn;
    n0  = rdSmpCnt - n1;
  }

  // if we don't have a valid file yet - then 
  if( cmAudioFileIsValid(p->afH) == false )
  {
    cmVOS_Zero(wt,n0);
    return kOkDspRC;
  }

  // read the first block of samples
  if( cmAudioFileReadSample(p->afH, n0, chIdx, chCnt, &wt, &actFrmCnt ) != kOkAfRC )
    return cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"An error occured while reading the wave table file.");

  // BUG BUG BUG
  // This assertion will fail if the file beg/end markers are not legal given the file length.
  // An error msg would be more appropriate.
  assert( actFrmCnt == n0 );

  // increment the wave table pointer
  wt     += n0;
  p->fi  += n0;

  // if n1 != 0 then we have encountered the end of the file segment
  if( n1 > 0 )
  {
    ++p->loopCnt;

    // send the loop count event
    cmDspSetInt(ctx,&p->inst,kCntWtId,p->loopCnt);

    // if we have played all the requested loops
    if( maxLoopCnt != -1 && p->loopCnt >= maxLoopCnt )
    {
      p->doneFl = true;
      cmVOS_Zero(wt,n1);   // zero to the end of the buffer

      p->cfn = p->cfi + cmDspUInt((cmDspInst_t*)p,kLenWtId) - p->wtn - n0;
      assert( p->cfn >= p->cfi );

    }
    else
    {
      // seek to the first sample indicated by the 'beg' variable
      if( cmAudioFileSeek(p->afH,begSmpIdx) != kOkAfRC )
        return cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"Seeking failed on the wave table file.",fn);

      // read the second block of samples
      if( cmAudioFileReadSample(p->afH, n1, chIdx, chCnt, &wt, &actFrmCnt ) != kOkAfRC )
        return cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"An error occured while reading the wave table file.");

      assert( actFrmCnt == n1 );

      // reset the file index tracker
      p->fi  = begSmpIdx + n1;
      p->cfi = begSmpIdx;
    }
  }

  return kOkDspRC;
}

cmDspRC_t _cmDspWaveTableReadAudioFile( cmDspCtx_t* ctx, cmDspWaveTable_t* p, unsigned wtSmpCnt, unsigned rdSmpCnt )
{
  unsigned    n0        = rdSmpCnt;
  unsigned    n1        = 0;
  int         begSmpIdx = cmDspInt(&p->inst,kBegWtId);
  int         endSmpIdx = cmDspInt(&p->inst,kEndWtId);
  int         maxLoopCnt= cmDspInt(&p->inst,kLoopWtId);

  if( endSmpIdx < begSmpIdx )
    endSmpIdx = p->fn-1;

  // if this read will wrap to the beginning of the wave table
  if( p->wti + rdSmpCnt > wtSmpCnt )
  {
    n0 = wtSmpCnt - p->wti;  // count of samples to read into the end of p->wt[]
    n1 = rdSmpCnt - n0;      // count of samples to read into the beg of p->wt[]
  }

  assert(n1<wtSmpCnt);

  // the first read always starts at p->wt + p->wti
  if( p->doneFl )
    cmVOS_Zero(p->wt + p->wti,n0);
  else
    if( _cmDspWaveTableReadBlock(ctx, p, p->wt+p->wti, n0,begSmpIdx,endSmpIdx,maxLoopCnt  ) != kOkDspRC )
      return cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"An error occured while reading the wave table file.");

  p->wtn -= n0;   // decrease the count of available samples
  p->wti += n0;

  if( n1 > 0 )
  {
    // the second read always starts at the beginning of the wave table
    if( p->doneFl )
      cmVOS_Zero(p->wt,n1);
    else
      if( _cmDspWaveTableReadBlock(ctx, p, p->wt, n1,begSmpIdx,endSmpIdx,maxLoopCnt  ) != kOkDspRC )
        return cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"An error occured while reading the wave table file.");

    p->wtn -= n1;  // decrease the count of available samples
    p->wti = n1;
  }
   

  //p->wtn -= rdSmpCnt;   // decrease the count of available samples

  return kOkDspRC;
}

cmDspRC_t _cmDspWaveTableInitAudioFile( cmDspCtx_t* ctx, cmDspWaveTable_t* p )
{
  cmDspRC_t         rc = kOkDspRC;
  cmAudioFileH_t    afH;
  cmRC_t            afRC;
  cmAudioFileInfo_t afInfo;

  const cmChar_t* fn       = cmDspStrcz(&p->inst,kFnWtId);
  unsigned        wtSmpCnt = cmDspUInt(&p->inst,kLenWtId);
  int             begSmpIdx= cmDspInt(&p->inst,kBegWtId);

  // if the file name is valid
  if( fn == NULL || strlen(fn)==0 )
  {
     rc = cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"Audio file loading was requested for the wave table but no file name was given.");
     goto errLabel;
  }

  // open the audio file
  afH = cmAudioFileNewOpen(fn,&afInfo,&afRC,ctx->rpt);

  // check for file open errors
  if( afRC != kOkAfRC )
  {
    rc =  cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"The audio file '%s' could not be opened. ('%s').",fn,cmAudioFileErrorMsg(afRC));
    goto errLabel;
  }

  // if the file opened but is invalid
  if( cmAudioFileIsValid(p->afH) )
    cmAudioFileDelete(&p->afH);

  // seek to the first sample indicated by the 'beg' variable
  if( cmAudioFileSeek(afH,begSmpIdx) != kOkAfRC )
  {
    rc = cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"Seeking failed on the audio file '%s'.",fn);
    goto errLabel;
  }

  p->afH = afH;
  p->fi  = begSmpIdx;
  p->cfi = begSmpIdx;
  p->fn  = afInfo.frameCnt;
  p->wti = 0;
  p->wtn = wtSmpCnt;

  // read the first block of samples
  if((rc= _cmDspWaveTableReadAudioFile(ctx,p,wtSmpCnt,wtSmpCnt))!= kOkDspRC )
    goto errLabel;

  //printf("Wt:%s %i %i\n",fn,begSmpIdx,cmDspInt(&p->inst,kEndWtId));

  // set the shape param to kFileWtId
  //if((rc= cmDspSetUInt(ctx,&p->inst,kShapeWtId,kFileWtId)) != kOkDspRC )
  //  goto errLabel;

 errLabel:
  
  if( rc != kOkDspRC )
    cmDspSetUInt(ctx,&p->inst,kShapeWtId,kSilenceWtId);

  return rc;
}

bool _cmDspWaveTableThreadFunc( void* param)
{
  cmDspWaveTable_t* p = (cmDspWaveTable_t*)param;

  if( p->loadFileFl )
  {
    p->loadFileFl = false;

    if( _cmDspWaveTableInitAudioFile(p->ctx,p) == kOkDspRC )
    {
      p->phsOffs = p->phsLast;
      cmDspSetUInt(p->ctx,&p->inst,kShapeWtId,kFileWtId);

    }

    cmThreadPause(p->thH,kPauseThFl);
  }

  return true;
}

// Files are loaded via a background thread.
cmDspRC_t _cmDspWaveTableStartFileLoadThread( cmDspCtx_t* ctx, cmDspWaveTable_t* p, const cmChar_t* fn )
{
  cmDspRC_t rc = kOkDspRC;

  if( fn == NULL )
    return rc;

  if( p->loadFileFl )
    return cmDspInstErr(ctx,&p->inst,kInvalidStateDspRC,"The audio file '%s' was not loaded because another file is in the process of being loaded.",cmStringNullGuard(fn));  

  if(p->useThreadFl && cmThreadIsValid(p->thH) == false)
    cmThreadCreate(&p->thH,_cmDspWaveTableThreadFunc,p,ctx->rpt);

  if(p->useThreadFl && cmThreadIsValid(p->thH) == false )
    return cmDspInstErr(ctx,&p->inst,kInvalidStateDspRC,"The audio file '%s' was not loaded because the audio load thread is invalid.",cmStringNullGuard(fn));

  p->loadFileFl = true;
  p->ctx        = ctx;
  cmDspSetUInt(ctx,&p->inst,kShapeWtId,kSilenceWtId);
  cmDspSetStrcz(ctx,&p->inst,kFnWtId,fn);

  if( p->useThreadFl == false )
  {
    // use non-threaded load
    if((rc = _cmDspWaveTableInitAudioFile(p->ctx,p)) != kOkDspRC )
      return cmDspInstErr(ctx,&p->inst,kVarNotValidDspRC,"The audio file '%s' was not loaded.",cmStringNullGuard(fn));
    
    p->phsOffs = p->phsLast;
    cmDspSetUInt(p->ctx,&p->inst,kShapeWtId,kFileWtId);
    p->loadFileFl = false;
  }
  else
  {
    // use threaded load - this works but it isn't really thread safe
    if( cmThreadPause(p->thH,0) != kOkThRC )
      return cmDspInstErr(ctx,&p->inst,kThreadFailDspRC,"The audio file '%s' was not loaded because audio thread enable failed.",cmStringNullGuard(fn));
  }
  return rc;
}

// This function is called whenever the source mode variable changes (or a new file name arrives)
cmDspRC_t _cmDspWaveTableCreateTable( cmDspCtx_t* ctx, cmDspWaveTable_t* p )
{
  cmDspRC_t rc       = kOkDspRC;
  unsigned  shapeId  = cmDspUInt(&p->inst,kShapeWtId);
  unsigned  wtSmpCnt = cmDspUInt(&p->inst,kLenWtId);
  unsigned  otCnt    = cmDspUInt(&p->inst,kOtWtId);
  cmSample_t gain    = 0.9;
  double     hz      = 1.0;
  double     sr      = cmDspSampleRate(ctx);

  assert( wtSmpCnt > 0 );

  if( p->wt == NULL )
    p->wt           = cmLhResizeNZ(ctx->lhH,cmSample_t,p->wt,wtSmpCnt);
  else
    cmVOS_Zero(p->wt,wtSmpCnt);

  p->wtn          = wtSmpCnt;  // all samples in the wt are avail for filling
  p->wti          = 0;         // beginning with the first sample
  p->loopCnt      = 0;         // we are starting from a new source so set the loop cnt to 0
  p->doneFl       = false;     // and the doneFl to false

  assert( p->wt != NULL );

  switch( shapeId )
  {
    case kSilenceWtId:
      break;

    case kFileWtId:
      printf("Loading:%i %i %s\n",p->nxtBegSmpIdx,p->nxtEndSmpIdx,cmDspStrcz(&p->inst,kFnWtId));
      rc = _cmDspWaveTableStartFileLoadThread(ctx,p,cmDspStrcz(&p->inst,kFnWtId));
      break;

    case kWhiteWtId:
      cmVOS_Random(p->wt,wtSmpCnt,-gain,gain);
      break;

    case kPinkWtId:
      cmVOS_SynthPinkNoise(p->wt,wtSmpCnt,0.0);
      cmVOS_MultVS(p->wt,wtSmpCnt,gain);
      break;

    case kSineWtId:
      cmVOS_SynthSine(p->wt,wtSmpCnt,0,sr,hz);
      cmVOS_MultVS(p->wt,wtSmpCnt,gain);
      break;

    case kCosWtId:
      cmVOS_SynthCosine(p->wt,wtSmpCnt,0,sr,hz);
      cmVOS_MultVS(p->wt,wtSmpCnt,gain);
      break;

    case kSawWtId:
      cmVOS_SynthSawtooth(p->wt,wtSmpCnt,0,sr,hz,otCnt);
      cmVOS_MultVS(p->wt,wtSmpCnt,gain);
      break;

    case kSqrWtId:
      cmVOS_SynthSquare(    p->wt,wtSmpCnt,0,sr,hz,otCnt );
      cmVOS_MultVS(p->wt,wtSmpCnt,gain);
      break;

    case kTriWtId:
      cmVOS_SynthTriangle(  p->wt,wtSmpCnt,0,sr,hz,otCnt );
      cmVOS_MultVS(p->wt,wtSmpCnt,gain);
      break;

    case kPulseWtId:
      cmVOS_SynthPulseCos(  p->wt,wtSmpCnt,0,sr,hz,otCnt );
      cmVOS_MultVS(p->wt,wtSmpCnt,gain);
      break;

    case kPhasorWtId:
      cmVOS_SynthPhasor(    p->wt,wtSmpCnt,0,sr,hz ); 
      cmVOS_MultVS(p->wt,wtSmpCnt,gain);
      break;


  }
  return rc;
}


cmDspRC_t _cmDspWaveTableReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspWaveTable_t* p = (cmDspWaveTable_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);
  cmDspZeroAudioBuf(ctx,inst,kOutWtId);

  p->nxtBegSmpIdx = cmDspInt(&p->inst,kBegWtId);
  p->nxtEndSmpIdx = cmDspInt(&p->inst,kEndWtId);

  return _cmDspWaveTableCreateTable(ctx,p);

}


cmDspRC_t _cmDspWaveTableExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t         rc       = kOkDspRC;
  const cmSample_t* phsV     = cmDspAudioBuf(ctx,inst,kPhsWtId,0);

  if( phsV == NULL )
  {
    inst->execFunc = NULL; // disable this instance because it has no input
    return kOkDspRC;
  }

  cmDspWaveTable_t* p        = (cmDspWaveTable_t*)inst;

  unsigned          mode     = cmDspSymbol(inst,kCmdWtId);
  unsigned          srcId    = cmDspUInt(inst,kShapeWtId);

  if( mode == p->offSymId || srcId == kSilenceWtId )
  {
    cmDspZeroAudioBuf(ctx,inst,kOutWtId);
    return kOkDspRC;
  }

  cmSample_t*       outV     = cmDspAudioBuf(ctx,inst,kOutWtId,0);
  unsigned          outCnt   = cmDspVarRows(inst,kOutWtId);
  unsigned          wtSmpCnt = cmDspUInt(inst,kLenWtId);
  double            gain     = cmDspDouble(inst,kGainWtId);
  unsigned          i;

  // for each output sample
  for(i=0; i<outCnt; ++i)
  {
    // get the wave table location
    //unsigned x = fmodf(phsV[i] - p->phsOffs,wtSmpCnt);
    unsigned x = fmodf(phsV[i],wtSmpCnt);

    // if the wt loctn is passed the end of the table
    /*
    if( x >= wtSmpCnt )
    {
      offs += wtSmpCnt;
      x    -= wtSmpCnt;
    }
    */

    outV[i] = gain * p->wt[x];
  }

  p->phsLast = phsV[outCnt-1];

  // if we are reading from a file ...
  if( srcId == kFileWtId )
  {
    unsigned rdSmpCnt = 8192; // file read block sample count

    p->wtn += outCnt;

    // ... and there are rdSmpCnt avail locations in the wave table
    if( p->wtn >= rdSmpCnt )
      rc =  _cmDspWaveTableReadAudioFile(ctx, p, wtSmpCnt, rdSmpCnt );

    // send the current audio file index
    if( p->doneFl && p->cfi < p->cfn && p->cfn <= (p->cfi + outCnt) )
    {
      cmDspSetUInt(ctx,inst,kFIdxWtId,p->cfn);
      cmDspSetSymbol(ctx,inst,kDoneWtId,p->doneSymId);
    }
    else
    {
      if( p->afIndexRptCnt >= p->minAfIndexRptCnt )
      {
        p->afIndexRptCnt -= p->minAfIndexRptCnt;
        cmDspSetUInt(ctx,inst,kFIdxWtId,p->cfi);
      }
    }

    p->afIndexRptCnt += outCnt;
    p->cfi           += outCnt;

  }

  return rc;
}


cmDspRC_t _cmDspWaveTableRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t        rc  = kOkDspRC;
  cmDspWaveTable_t* p  = (cmDspWaveTable_t*)inst;

  switch( evt->dstVarId )
  {
    case kFnWtId: // a new file name arrived - this automatically switches the source mode to kFileWtId
      {
        const cmChar_t* fn = cmDsvStrz(evt->valuePtr); 

        if( cmFsIsFile( fn )==false )
           cmDspInstErr(ctx,&p->inst,kInvalidArgDspRC,"'%s' does not exist.",cmStringNullGuard(fn));
        else
        {
          cmDspSetEvent(ctx,inst,evt);                       // set the file name variable
          cmDspSetInt(ctx,inst,kBegWtId,p->nxtBegSmpIdx);    // set the beg/end smp idx var's from the stored nxtBeg/EndSmpIdx values
          cmDspSetInt(ctx,inst,kEndWtId,p->nxtEndSmpIdx);    // 
          cmDspSetUInt(ctx,inst,kShapeWtId,kFileWtId);       // switch to file mode 
          rc = _cmDspWaveTableCreateTable(ctx,p);            // reload the wavetable
        }
      }
      break;

    case kBegWtId:
      // store for next incoming file name msg
      p->nxtBegSmpIdx = cmDsvGetInt(evt->valuePtr);
      break;

    case kEndWtId:
      // store for next incoming file name msg
      p->nxtEndSmpIdx = cmDsvGetInt(evt->valuePtr);
      break;

    case kShapeWtId:
      if( cmDsvGetUInt(evt->valuePtr) < kShapeWtCnt )
      {
        cmDspSetEvent(ctx,inst,evt);            // switch modes
        rc = _cmDspWaveTableCreateTable(ctx,p); // reload the wavetable
      }
      break;

    case kLenWtId: // we don't support table size changes 
      break;

    case kPhsWtId:    
      break;

    case kCmdWtId:
      if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
      {
        if( cmDspSymbol(inst,kCmdWtId) == p->onSymId )
        {
          //rc = _cmDspWaveTableReset(ctx,inst, evt );
          rc =  _cmDspWaveTableCreateTable(ctx,p);

          cmDspSetSymbol(ctx,inst,kCmdWtId,p->onSymId);
          p->phsOffs = 0;
          p->phsLast = 0;
        }
      }
      break;

    case kOtWtId:
      if((rc = cmDspSetEvent(ctx,inst,evt)) == kOkDspRC )
        rc = _cmDspWaveTableCreateTable(ctx,p); // reload the wavetable
      break;

    case kGainWtId:
      rc = cmDspSetEvent(ctx,inst,evt);
      break;

    default:
      { assert(0); }
  }

  return rc;
}



struct cmDspClass_str* cmWaveTableClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmWaveTableDC,ctx,"WaveTable",
    NULL,
    _cmDspWaveTableAlloc,
    _cmDspWaveTableFree,
    _cmDspWaveTableReset,
    _cmDspWaveTableExec,
    _cmDspWaveTableRecv,
    NULL,
    NULL,
    "Variable frequency and waveshape signal generator." );

  return &_cmWaveTableDC;
}

//==========================================================================================================================================
enum
{
  kFmtSpId,
  kOutSpId,
  kInSpId,

  kSprintfLabelCharCnt = 15,
  kSprintfDocCharCnt   = 31,
  kSprintfBufCharCnt   = 1023
};

cmDspClass_t _cmSprintfDC;

typedef struct
{
  unsigned  flags;  // dsv type id for this fmt conversion or 0 if it is a literal string
  unsigned  fsi; // index into the fmt string where the fmt starts (always points to an '%' char)
  unsigned  fsn; // length of the format string 
  cmChar_t  label[ kSprintfLabelCharCnt+1 ];
  cmChar_t  doc[   kSprintfDocCharCnt+1 ];
} cmDspSprintfFmt_t;

typedef struct
{
  cmDspInst_t        inst;
  cmChar_t           buf[ kSprintfBufCharCnt+1]; // output string buffer
  unsigned           inCnt;       // count conversion spec's in the fmtStr[]
  cmDspSprintfFmt_t* fmtArray;    // fmtArray[inCnt] 
  cmChar_t*          fmtStr;      // C-style printf format string.
} cmDspSprintf_t;

cmDspRC_t _cmSprintfGetInputCnt( cmDspCtx_t* ctx, cmDspClass_t* classPtr, const cmChar_t* fmt, unsigned* inCntPtr )
{
  unsigned i,n;
  unsigned inCnt = 0;

  if( fmt== NULL || (n=strlen(fmt))==0 )
    return cmErrMsg(&classPtr->err,kInvalidArgDspRC,"Empty format string.");

  for(i=0; i<n; ++i)
  {
    // handle the escape character
    if( fmt[i] == '\\' )
      ++i; // for now we will just skip the next character
    else
      if( fmt[i] == '%' )
        ++inCnt;
  }

  *inCntPtr = inCnt;
  return kOkDspRC;
}

cmDspRC_t _cmSprintfGetInputTypes( cmDspCtx_t* ctx, cmDspClass_t* classPtr, const cmChar_t* fmt, cmDspSprintfFmt_t fmtArray[], unsigned inCnt )
{
  unsigned i,j,n;

  if( fmt== NULL || (n=strlen(fmt))==0 )
    return cmErrMsg(&classPtr->err,kInvalidArgDspRC,"Empty format string.");

  n = strlen(fmt);

  for(i=0,j=0; i<n; ++i)
  {
    // handle the escape character
    if( fmt[i] == '\\' )
      ++i; // for now we will just skip the next character
    else
      if( fmt[i] == '%' )
      {
        unsigned fn;
        if((fn = strcspn(fmt+i,"diouxXfeEgGcs")) == 0 )
          return cmErrMsg(&classPtr->err,kInvalidArgDspRC,"Invalid format string on input conversion at index:%i.",j);

        ++fn;

        fmtArray[j].fsi = i;
        fmtArray[j].fsn = fn;
        
        snprintf(fmtArray[j].label,kSprintfLabelCharCnt,"in-%i",j);

        fmtArray[j].label[kSprintfLabelCharCnt]=0;
        fmtArray[j].doc[kSprintfDocCharCnt] = 0;

        switch( fmt[ i + fn - 1 ] )
        {
          case 'd':
          case 'i':
            fmtArray[j].flags = kIntDsvFl;
            snprintf(fmtArray[j].doc,kSprintfDocCharCnt,"Integer input %i.",j);
            break;

          case 'o':
          case 'u':
          case 'x':
          case 'X':
            fmtArray[j].flags = kUIntDsvFl;
            snprintf(fmtArray[j].doc,kSprintfDocCharCnt,"Unsigned input %i.",j);
            break;

          case 'f':
          case 'e':
          case 'E':
          case 'g':
          case 'G':
            fmtArray[j].flags = kDoubleDsvFl;
            snprintf(fmtArray[j].doc,kSprintfDocCharCnt,"Double input %i.",j);
            break;

          case 'c':
            fmtArray[j].flags = kUCharDsvFl;
            snprintf(fmtArray[j].doc,kSprintfDocCharCnt,"Unsigned char input %i.",j);
            break;

          case 's':
            fmtArray[j].flags = kStrzDsvFl | kSymDsvFl;
            snprintf(fmtArray[j].doc,kSprintfDocCharCnt,"String input %i.",j);
            break;

          default:
            { assert(0); }

        }

        i += fn - 1;

        ++j;
      }
  }

  return kOkDspRC;  
}

cmDspRC_t _cmSprintfLoadFormat(cmDspSprintf_t** pp, cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned instSymId, unsigned id, unsigned storeSymId, cmDspVarArg_t args[], unsigned va_cnt, va_list vl )
{
  cmDspRC_t          rc;
  unsigned           i,j;
  va_list            vl2;
  unsigned           inCnt       = 0;
  unsigned           fixedArgCnt = 0;
  cmDspSprintf_t*    p           = NULL;
  const cmChar_t*    fmt         = NULL; 
  cmDspSprintfFmt_t* fmtArray    = NULL;

  va_copy(vl2,vl);

  *pp = NULL;

  if( va_cnt > 0 )
    fmt = va_arg(vl,const char*);

  if( va_cnt < 1 || fmt == NULL )
    return cmErrMsg(&classPtr->err,kVarArgParseFailDspRC,"Expected the format string as the first argument.");


  // calc the number of fixed args
  for(i=0; args[i].label != NULL; ++i)
    ++fixedArgCnt;

  // calc the count of input args
  if((rc = _cmSprintfGetInputCnt(ctx, classPtr, fmt, &inCnt)) != kOkDspRC )
    return rc;

  if( inCnt > 0 )
  {
    fmtArray = cmLhAllocZ( ctx->lhH, cmDspSprintfFmt_t, inCnt );

    rc = _cmSprintfGetInputTypes(ctx, classPtr, fmt, fmtArray, inCnt );
  }

  if( rc == kOkDspRC )
  {
    unsigned      argCnt = fixedArgCnt + inCnt;
    cmDspVarArg_t argArray[ argCnt+1 ];

    // copy in fixed args
    for(i=0; i<fixedArgCnt; ++i)
      argArray[i] = args[i];

    // calc input args
    for(j=0; i<argCnt; ++i,++j)
    {
      argArray[i].label   = fmtArray[j].label;
      argArray[i].constId = i;
      argArray[i].rn      = 0;
      argArray[i].cn      = 0;
      argArray[i].flags   = kInDsvFl | fmtArray[j].flags;
      argArray[i].doc     = fmtArray[j].doc;
    }

    // set the sentinel arg to all zeros
    memset(argArray + argCnt,0,sizeof(cmDspVarArg_t));

    if((p = cmDspInstAlloc(cmDspSprintf_t,ctx,classPtr,argArray,instSymId,id,storeSymId,va_cnt,vl2)) != NULL )
    {
      // make a copy of the format string - we need to be sure that it is in
      // r/w memory in order that _cmDspSprintfGenString() can write to it
      p->fmtStr = cmLhResizeN(ctx->lhH,cmChar_t,p->fmtStr,strlen(fmt)+1);
      strcpy(p->fmtStr,fmt);

    
      cmLhFree(ctx->lhH,p->fmtArray);
      p->fmtArray = fmtArray;

      p->inCnt = inCnt;
      memset(p->buf,0,kSprintfBufCharCnt+1);
    }

    *pp = p;

  }

  if( cmErrLastRC(&classPtr->err) !=  kOkDspRC )
    cmLhFree(ctx->lhH,fmtArray);
    
  return rc;
}

cmDspRC_t _cmDspSprintfGenString(cmDspCtx_t* ctx, cmDspSprintf_t* p )
{
  cmDspRC_t   rc  = kOkDspRC;
  unsigned    fsi = 0;                   // format string index
  unsigned    i   = 0;                   // fmtArray[] index
  unsigned    bi  = 0;                   // string buffer index
  unsigned    bn  = kSprintfBufCharCnt;  // available char's in the string buffer
  cmChar_t*   fmt = p->fmtStr;
  unsigned    fn  = strlen(fmt)+1;

  // for each 
  for(i=0; i<p->inCnt && bn>0; ++i)
  {
    const cmDspSprintfFmt_t* f      = p->fmtArray + i;
    unsigned                 varId  = kInSpId + i;
    const cmDspVar_t*        varPtr = cmDspVarIdToCPtr(&p->inst, varId);
    
    assert(varPtr != NULL);

    // if there are literal char's to copy prior to the format
    if( fsi < f->fsi )
    {
      unsigned cn = cmMin(f->fsi-fsi,bn);
      strncpy(p->buf+bi,fmt+fsi,cn);
      bn  -= cn;
      fsi += cn;
      bi  += cn;
    } 
    
    if( bn == 0 )
    {
      rc = cmDspInstErr(ctx,&p->inst,kInvalidArgDspRC,"The internal string buffer is too small.");
      break;
    }

    unsigned pn = 0;
    char     c  = fmt[ f->fsi + f->fsn ];

    // zero terminate the format string for this input
    fmt[ f->fsi + f->fsn] = 0;

    // if the conversion fmt is for a string then the kSymDsvFl will be set
    // which will prevent the switch() from working - so clear the sym flag here.
    unsigned flags = cmClrFlag(f->flags,kSymDsvFl);

    switch(flags)
    {
      case kUCharDsvFl:
        // not implemented - need to implment a uchar variable type or
        // assume a one character strz.
        assert(0);
        break;
          
      case kIntDsvFl:        
        pn = snprintf(p->buf+bi,bn,fmt + fsi, cmDspInt(&p->inst,varId));
        break;

      case kUIntDsvFl:
        pn = snprintf(p->buf+bi,bn,fmt + fsi, cmDspUInt(&p->inst,varId));
        break;

      case kDoubleDsvFl:
        pn = snprintf(p->buf+bi,bn,fmt + fsi, cmDspDouble(&p->inst,varId));
        break;

      case kStrzDsvFl:
        if( cmDspIsSymbol(&p->inst,varId) )
          pn = snprintf(p->buf+bi,bn,fmt + fsi, cmStringNullGuard(cmSymTblLabel(ctx->stH,cmDspSymbol(&p->inst,varId))));
        else
          pn = snprintf(p->buf+bi,bn,fmt + fsi, cmDspStrcz(&p->inst,varId));
        break;

      default:
        { assert(0); }
    }

    // restore the char written over by the termination zero
    fmt[ f->fsi + f->fsn] = c;

    assert(pn<=bn);
    bn  -= pn;
    bi  += pn;
    fsi += f->fsn;

  }  

  // if there is literal text in the format string after the last conversion spec.
  if( fsi < fn )
  {
    unsigned cn = cmMin(fn-fsi,bn);
    strncpy(p->buf+bi,fmt+fsi,cn);
  }

  return rc;
}

cmDspInst_t*  _cmDspSprintfAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "fmt",    kFmtSpId,    0, 0, kInDsvFl  | kStrzDsvFl | kReqArgDsvFl, "Format string" },
    { "out",    kOutSpId,    0, 0, kOutDsvFl | kStrzDsvFl,                "Output string" },               
    { NULL, 0, 0, 0, 0 }
  };

  cmDspSprintf_t* p = NULL;
  if( _cmSprintfLoadFormat(&p, ctx, classPtr, instSymId, id, storeSymId, args, va_cnt, vl ) == kOkDspRC )
    return &p->inst;
  return NULL;
}

cmDspRC_t _cmDspSprintfFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspSprintf_t* p = (cmDspSprintf_t*)inst;
  cmLhFree(ctx->lhH,p->fmtArray);
  cmLhFree(ctx->lhH,p->fmtStr);
  p->fmtArray=NULL;
  return kOkDspRC;
}

cmDspRC_t _cmDspSprintfReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return kOkDspRC;
}

cmDspRC_t _cmDspSprintfRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t       rc= kOkDspRC;
  cmDspSprintf_t* p = (cmDspSprintf_t*)inst;

  if( kInSpId <= evt->dstVarId && evt->dstVarId < kInSpId + p->inCnt )
  {
    cmDspSetEvent(ctx,inst,evt);
    //if( evt->dstVarId == kInSpId )
      if((rc = _cmDspSprintfGenString(ctx,p)) == kOkDspRC )
        cmDspSetStrcz(ctx,inst,kOutSpId,p->buf);
    
  }

  return rc;
}

struct cmDspClass_str* cmSprintfClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmSprintfDC,ctx,"Sprintf",
    NULL,
    _cmDspSprintfAlloc,
    _cmDspSprintfFree,
    _cmDspSprintfReset,
    NULL,
    _cmDspSprintfRecv,
    NULL,NULL,
    "String formatter." );

  return &_cmSprintfDC;
}

//==========================================================================================================================================
enum
{
  kOutAmId,
  kBaseInAmId
};

cmDspClass_t _cmAMixDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned    inPortCnt;
  unsigned    baseGainId;
  unsigned    baseMuteId;
} cmDspAMix_t;

cmDspInst_t*  _cmDspAMixAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'AMix' constructor must have a count of input ports.");
    return NULL;
  }

  // args:
  // <inPortCnt>  <gain0>, <gain1>  (the default gains are optional)

  unsigned      i;
  int           inPortCnt    = va_arg(vl,int);
  unsigned      baseGainAmId = kBaseInAmId + inPortCnt;
  unsigned      baseMuteAmId = baseGainAmId + inPortCnt;
  double        dfltGain[ inPortCnt ];

  if( va_cnt == 1 )
    cmVOD_Fill(dfltGain,inPortCnt,1.0);
  else
    if( va_cnt == 2 )
    {
      dfltGain[0] = va_arg(vl,double);
      cmVOD_Fill(dfltGain+1,inPortCnt-1,dfltGain[0]);
    }
    else
      if( va_cnt == inPortCnt + 1 )
      {
        for(i=0; i<inPortCnt; ++i)
          dfltGain[i] = va_arg(vl,double);
      }
      else
      {
        cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The mix argument list must contain no default gain values, one default gain value, or all default gain values.");
        return NULL;
      }


  cmDspAMix_t* p = cmDspInstAllocV(cmDspAMix_t,ctx,classPtr,instSymId,id,storeSymId,0,vl,
    1,         "out",  kOutAmId,     0, 1,  kOutDsvFl | kAudioBufDsvFl,           "Audio output",
    inPortCnt, "in",   kBaseInAmId,  0, 0,  kInDsvFl  | kAudioBufDsvFl,           "Audio input",
    inPortCnt, "gain", baseGainAmId, 0, 0,  kInDsvFl  | kDoubleDsvFl,             "Gain input",
    inPortCnt, "mute", baseMuteAmId, 0, 0,  kInDsvFl  | kBoolDsvFl,               "Mute input",
    0 );


  p->inPortCnt  = inPortCnt;
  p->baseGainId = baseGainAmId;
  p->baseMuteId = baseMuteAmId;


  for(i=0; i<inPortCnt; ++i)
  {
    cmDspSetDefaultDouble( ctx, &p->inst, p->baseGainId + i,   0.0,   dfltGain[i]);    
    cmDspSetDefaultBool(   ctx, &p->inst, p->baseMuteId + i,   false, false );
  }
  /*
  // read any default gain settings 
  --va_cnt;
  for(i=0; i<inPortCnt; ++i)
  {
    // if excplicit gains are not given then default to 1.0.
    double dflt = 1.0; 
    if( i < va_cnt )
      dflt = va_arg(vl,double);

    cmDspSetDefaultDouble(   ctx, &p->inst, p->baseGainId + i,   0.0,   dflt);
  }

  */

  return &p->inst;


}


cmDspRC_t _cmDspAMixReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t       rc = kOkDspRC;
  cmDspApplyAllDefaults(ctx,inst);
  cmDspZeroAudioBuf(ctx,inst,kOutAmId);
  return rc;
} 


cmDspRC_t _cmDspAMixExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspAMix_t* p  = (cmDspAMix_t*)inst;
  unsigned     i;

  cmDspZeroAudioBuf(ctx,inst,kOutAmId);
  
  unsigned    n  = cmDspAudioBufSmpCount(ctx,inst,kOutAmId,0);
  cmSample_t* dp = cmDspAudioBuf(ctx,inst,kOutAmId,0);
  
  for(i=0; i<p->inPortCnt; ++i)
  {
    const cmSample_t* sp = cmDspAudioBuf(ctx,inst,kBaseInAmId+i,0);
    if( sp != NULL )
    {
      double            gain = cmDspDouble(inst,p->baseGainId+i);
      cmVOS_MultSumVVS(dp,n,sp,(cmSample_t)gain);
    }
  }

  return kOkDspRC;
}

cmDspRC_t _cmDspAMixRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t    rc = kOkDspRC;
  cmDspAMix_t* p  = (cmDspAMix_t*)inst;
  
  if( p->baseGainId <= evt->dstVarId && evt->dstVarId < p->baseGainId + p->inPortCnt )
  {
    cmDspSetEvent(ctx,inst,evt);
    //printf("rcv:%i %f\n",evt->dstVarId,cmDspDouble(inst,evt->dstVarId));
  }
  return rc;
}

struct cmDspClass_str* cmAMixClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmAMixDC,ctx,"AMix",
    NULL,
    _cmDspAMixAlloc,
    NULL,
    _cmDspAMixReset,
    _cmDspAMixExec,
    _cmDspAMixRecv,
    NULL,NULL,
    "Audio mixer");

  return &_cmAMixDC;
} 

//==========================================================================================================================================
enum
{
  kInAsId,
  kBaseOutAsId
};

cmDspClass_t _cmASplitDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned    outPortCnt; 
  unsigned    baseGainId;
} cmDspASplit_t;

// A splitter has one audio input port and multiple audio output ports.
// A gain input is automatically provided for each output port.
cmDspInst_t*  _cmDspASplitAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "in",  kInAsId,  0,      0,  kInDsvFl | kAudioBufDsvFl,              "Audio input" },
    { NULL, 0, 0, 0, 0 }
  };

  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'ASplit' constructor must have a count of input ports.");
    return NULL;
  }

  // args:
  // <outPortCnt>  <gain0>, <gain1>  (the default gains are optional)

  unsigned      i,j,k;
  int           outPortCnt    = va_arg(vl,int);
  unsigned      fixArgCnt    = sizeof(args)/sizeof(args[0]) - 1;
  unsigned      argCnt       = fixArgCnt + 2*outPortCnt + 1;
  cmDspVarArg_t argArray[ argCnt ];
  int           labelCharCnt = 15;
  cmChar_t      label[ labelCharCnt + 1 ];
  label[labelCharCnt]        = 0;

  // 
  for(i=0; i<fixArgCnt; ++i)
    argArray[i] = args[i];

  // define the audio output port specifications
  for(j=0,k=0; j<outPortCnt; ++i,++j,++k)
  {
    snprintf(label,labelCharCnt,"out-%i",j);
    unsigned symId      = cmSymTblRegisterSymbol(ctx->stH,label);
    argArray[i].label   = cmSymTblLabel(ctx->stH,symId);
    argArray[i].constId = kBaseOutAsId + k;
    argArray[i].rn      = 0;
    argArray[i].cn      = 1;
    argArray[i].flags   = kOutDsvFl | kAudioBufDsvFl;
    argArray[i].doc     = "Audio Input";
  }

  // define the gain input specifications
  for(j=0; j<outPortCnt; ++i,++j,++k)
  {
    snprintf(label,labelCharCnt,"gain-%i",j);
    unsigned symId      = cmSymTblRegisterSymbol(ctx->stH,label);
    argArray[i].label   = cmSymTblLabel(ctx->stH,symId);
    argArray[i].constId = kBaseOutAsId + k;
    argArray[i].rn      = 0;
    argArray[i].cn      = 0;
    argArray[i].flags   = kInDsvFl | kDoubleDsvFl;
    argArray[i].doc     = "Gain input";
  }

  // set the NULL end-of-arg-array sentinel
  memset(argArray + i, 0, sizeof(argArray[0]));

  cmDspASplit_t* p = cmDspInstAlloc(cmDspASplit_t,ctx,classPtr,argArray,instSymId,id,storeSymId,0,vl);

  p->outPortCnt  = outPortCnt;
  p->baseGainId = kBaseOutAsId + outPortCnt;

  // read any default gain settings 
  --va_cnt;
  for(i=0; i<outPortCnt; ++i)
  {
    double dflt = 1.0;
    if( i < va_cnt )
      dflt = va_arg(vl,double);

    cmDspSetDefaultDouble(   ctx, &p->inst, p->baseGainId + i,   0.0,   dflt);
  }
  return &p->inst;
}

cmDspRC_t _cmDspASplitReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  cmDspASplit_t* p  = (cmDspASplit_t*)inst;
  int            i;

  cmDspApplyAllDefaults(ctx,inst);

  for(i=0; i<p->outPortCnt; ++i)
    cmDspZeroAudioBuf(ctx,inst,kBaseOutAsId+i);

  return rc;
} 


cmDspRC_t _cmDspASplitExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspASplit_t* p  = (cmDspASplit_t*)inst;
  unsigned     i;

  unsigned          n  = cmDspAudioBufSmpCount(ctx,inst,kBaseOutAsId,0);
  const cmSample_t* sp = cmDspAudioBuf(ctx,inst,kInAsId,0);
  
  for(i=0; i<p->outPortCnt; ++i)
  {
    cmSample_t*       dp   = cmDspAudioBuf(ctx,inst,kBaseOutAsId+i,0);
    double            gain = cmDspDouble(inst,p->baseGainId+i);
    cmVOS_MultVVS(dp,n,sp,(cmSample_t)gain);
  }

  return kOkDspRC;
}

cmDspRC_t _cmDspASplitRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t    rc = kOkDspRC;
  cmDspASplit_t* p  = (cmDspASplit_t*)inst;
  
  if( p->baseGainId <= evt->dstVarId && evt->dstVarId < p->baseGainId + p->outPortCnt )
  {
    cmDspSetEvent(ctx,inst,evt);
  }
  return rc;
}

struct cmDspClass_str* cmASplitClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmASplitDC,ctx,"ASplit",
    NULL,
    _cmDspASplitAlloc,
    NULL,
    _cmDspASplitReset,
    _cmDspASplitExec,
    _cmDspASplitRecv,
    NULL,NULL,
    "Audio splitter");

  return &_cmASplitDC;
} 

//==========================================================================================================================================
enum
{
  kInAmId,
  kMinAmId,
  kMaxAmId,
  kValAmId, // meter value
  kLblAmId,
};

#define cmDspMeter_MIN (-100)
#define cmDspMeter_MAX (0)

cmDspClass_t _cmAMeterDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned    bufN;
  unsigned    idx;
  cmReal_t    sum;
  cmReal_t    val;
} cmDspAMeter_t;

cmDspInst_t*  _cmDspAMeterAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "in",   kInAmId,   0, 0, kInDsvFl     | kAudioBufDsvFl, "Audio input"},
    { "min",  kMinAmId,  0, 0, kDoubleDsvFl,  "Minimum value"},
    { "max",  kMaxAmId,  0, 0, kDoubleDsvFl,  "Maximum value"},
    { "val",  kValAmId,  0, 0, kDoubleDsvFl,  "Meter value"},
    { "label",kLblAmId,  0, 0, kStrzDsvFl,    "Label."},
    { NULL, 0, 0, 0, 0 }
  };

  cmDspAMeter_t* p = cmDspInstAlloc(cmDspAMeter_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  double   updateMs = 100;
  double   sr       = cmDspSampleRate(ctx);
  unsigned spc      = cmDspSamplesPerCycle(ctx);
  p->bufN           = cmMax(1,floor(updateMs * sr/ (1000.0 * spc)));
  

  cmDspSetDefaultDouble(ctx, &p->inst, kValAmId,  0.0,  cmDspMeter_MIN);
  cmDspSetDefaultDouble(ctx, &p->inst, kMinAmId,  0.0,  cmDspMeter_MIN);
  cmDspSetDefaultDouble(ctx, &p->inst, kMaxAmId,  0.0,  cmDspMeter_MAX);

  // create the UI control
  cmDspUiMeterCreate(ctx,&p->inst,kMinAmId,kMaxAmId,kValAmId,kLblAmId);

  return &p->inst;
}

cmDspRC_t _cmDspAMeterReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspAMeter_t* p = (cmDspAMeter_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  //cmDspZeroAudioBuf(ctx,inst,kInAmId);

  p->idx = 0;
  p->sum = 0;
  p->val = 0;
  return kOkDspRC;
}

cmDspRC_t _cmDspAMeterExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspAMeter_t*    p     = (cmDspAMeter_t*)inst;
  unsigned          n     = cmDspAudioBufSmpCount(ctx,inst,kInAmId,0);
  const cmSample_t* sp    = cmDspAudioBuf(ctx,inst,kInAmId,0);

  if( sp == NULL )
  {
    inst->execFunc = NULL; // if there is no connected input then disable further callbacks
    return kOkDspRC;
  }

  p->sum += cmVOS_SquaredSum(sp,n);
  ++p->idx;

  if( p->idx == p->bufN )
  {
    cmReal_t     coeff = 0.7;
    cmReal_t     rms   = sqrt(p->sum/(n*p->bufN));

    p->idx = 0;
    p->sum = 0;

    p->val = rms > p->val ? rms : (rms*(1.0-coeff)) + (p->val*coeff);

    double db = cmMax(cmDspMeter_MIN,cmMin(cmDspMeter_MAX,20.0 * log10(p->val)));

    cmDspSetDouble(ctx, inst, kValAmId, db);
  }

  
  return kOkDspRC;
}


struct cmDspClass_str* cmAMeterClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmAMeterDC,ctx,"AMeter",
    NULL,
    _cmDspAMeterAlloc,
    NULL,
    _cmDspAMeterReset,
    _cmDspAMeterExec,
    NULL,
    NULL,NULL,
    "Audio meter display.");

  return &_cmAMeterDC;
}

//==========================================================================================================================================

//
//
//  Read files created by this object with the Octave function cmTextFile().
//
//

enum
{
  kCntTfId,
  kFnTfId,
  kBaseTfId
};

cmDspClass_t _cmTextFileDC;

typedef struct
{
  cmDspInst_t inst;
  int         inPortCnt;
  cmFileH_t   fH;
} cmDspTextFile_t;

cmDspInst_t*  _cmDspTextFileAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "cnt",  kCntTfId,  0, 0, kIntDsvFl | kReqArgDsvFl, "Input port count"},
    { "fn",   kFnTfId,   0, 0, kInDsvFl | kStrzDsvFl   | kReqArgDsvFl, "File name"},
    { NULL, 0, 0, 0, 0 }
  };

  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The text file object must be given a input port count argument.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  int i,j;
  int             inPortCnt    = va_arg(vl1,int);
  unsigned        fixArgCnt    = sizeof(args)/sizeof(args[0]) - 1;
  unsigned        argCnt       = fixArgCnt + inPortCnt + 1;
  cmDspVarArg_t   argArray[ argCnt ];
  

  if( inPortCnt <= 0 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The text file input port count  must be a positive integer.");
    return NULL;
  }

  // 
  for(i=0; i<fixArgCnt; ++i)
    argArray[i] = args[i];

  // define the input port specifications
  for(j=0; j<inPortCnt; ++j,++i)
  {
    int             labelCharCnt = 15;
    cmChar_t        label[ labelCharCnt + 1 ];
    label[labelCharCnt]        = 0;

    snprintf(label,labelCharCnt,"in-%i",j);
    unsigned symId      = cmSymTblRegisterSymbol(ctx->stH,label);
    argArray[i].label   = cmSymTblLabel(ctx->stH,symId);
    argArray[i].constId = kBaseTfId + j;
    argArray[i].rn      = 0;
    argArray[i].cn      = 0;
    argArray[i].flags   = kInDsvFl | kDoubleDsvFl | kSymDsvFl;
    argArray[i].doc     = "Data input";
  }

  // set the NULL end-of-arg-array sentinel
  memset(argArray + i, 0, sizeof(argArray[0]));


  cmDspTextFile_t* p = cmDspInstAlloc(cmDspTextFile_t,ctx,classPtr,argArray,instSymId,id,storeSymId,va_cnt,vl);

  p->inPortCnt  = inPortCnt;
  p->fH         = cmFileNullHandle;
  return &p->inst;
}

cmDspRC_t _cmDspTextFileFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspTextFile_t* p = (cmDspTextFile_t*)inst;
  if( cmFileClose(&p->fH) )
    return cmErrMsg(&inst->classPtr->err, kInstFinalFailDspRC, "Text file close failed.");
  return kOkDspRC;
}

cmDspRC_t _cmDspTextFileReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc  = kOkDspRC;

  cmDspTextFile_t* p = (cmDspTextFile_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);
  
  const cmChar_t* fn = cmDspStrcz(inst,kFnTfId);

  if( cmFileOpen( &p->fH, fn, kWriteFileFl, ctx->cmCtx->err.rpt ) != kOkFileRC )
    rc = cmErrMsg(&inst->classPtr->err, kInstResetFailDspRC, "Text file open failed.");

  return rc;
}

cmDspRC_t _cmDspTextFileRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspTextFile_t*    p     = (cmDspTextFile_t*)inst;
  cmDspSetEvent(ctx,inst,evt);
  
  if( cmFileIsValid(p->fH) && evt->dstVarId >= kBaseTfId )
  {
    double secs = ctx->ctx->begSmpIdx / cmDspSampleRate(ctx);
    cmFilePrintf(p->fH,"%f %f %i ", secs, secs, evt->dstVarId-kBaseTfId);

    if( cmIsFlag(evt->valuePtr->flags,kSymDsvFl) )
      cmFilePrintf(p->fH,"%s\n", cmStringNullGuard(cmSymTblLabel(ctx->stH,cmDsvSymbol(evt->valuePtr))));
    else
      cmFilePrintf(p->fH,"%f\n", cmDspDouble(inst,evt->dstVarId));
  }

  return kOkDspRC;
}

struct cmDspClass_str* cmTextFileClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmTextFileDC,ctx,"TextFile",
    NULL,
    _cmDspTextFileAlloc,
    _cmDspTextFileFree,
    _cmDspTextFileReset,
    NULL,
    _cmDspTextFileRecv,
    NULL,NULL,
    "Time tagged text file.");

  return &_cmTextFileDC;
}

//==========================================================================================================================================
enum
{
  kRsrcArId,
  kCmdArId,
  kIdxArId,
  kValArId,
  kCntArId,
  kDoneArId,
  kBaseOutArId
};

cmDspClass_t _cmArrayDC;

typedef struct
{
  cmDspInst_t inst;
  unsigned    cnt;
  cmReal_t*   array;
  unsigned    printSymId;
  unsigned    sendSymId;
  unsigned    cntSymId;
  unsigned    doneSymId;
} cmDspArray_t;

cmDspInst_t*  _cmDspArrayAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  va_list vl1;
  va_copy(vl1,vl);


  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The array object must be given a 'rsrc' parameter.");
    return NULL;
  }

  const cmChar_t* rsrcStr    = va_arg(vl,cmChar_t*);
  unsigned        outPortCnt = 0;
  cmReal_t*       array      = NULL;
  unsigned        doneSymId  = cmDspSysRegisterStaticSymbol(ctx->dspH,"done");
  unsigned        i;

  if( rsrcStr == NULL )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The array object 'rsrc' parameter appears to be missing.");
    return NULL;
  }

  
  if( cmDspRsrcRealArray( ctx->dspH, &outPortCnt, &array, rsrcStr, NULL ) != kOkDspRC )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The array resource '%s' could not be read.",cmStringNullGuard(rsrcStr));
    return NULL;
  }

  cmDspArray_t* p = cmDspInstAllocV(cmDspArray_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl1,
    1,          "rsrc",  kRsrcArId,    0, 0, kStrzDsvFl   | kInDsvFl | kReqArgDsvFl,    "Array data resource label.",
    1,          "cmd",   kCmdArId,     0, 0, kSymDsvFl    | kInDsvFl,                   "Command: send | print | count.",
    1,          "idx",   kIdxArId,     0, 0, kUIntDsvFl   | kInDsvFl,                   "Send value at index out 'val' port.",
    1,          "val",   kValArId,     0, 0, kDoubleDsvFl | kOutDsvFl,                  "Index output value.",
    1,          "cnt",   kCntArId,     0, 0, kUIntDsvFl   | kOutDsvFl,                  "Count output value.",
    1,          "done",  kDoneArId,    0, 0, kSymDsvFl    | kOutDsvFl,                  "'done' after last send.",
    outPortCnt, "out",   kBaseOutArId, 0, 0, kDoubleDsvFl | kOutDsvFl | kSendDfltDsvFl, "Individual real value outputs.",
    0 );

  cmDspSetDefaultDouble( ctx, &p->inst, kValArId, 0, 0 );
  cmDspSetDefaultUInt(   ctx, &p->inst, kCntArId, 0, outPortCnt );
  cmDspSetDefaultSymbol( ctx, &p->inst, kDoneArId, doneSymId);

  for(i=0; i<outPortCnt; ++i)
    cmDspSetDefaultDouble( ctx, &p->inst, kBaseOutArId+i, 0, array[i] );

  p->array      = array;
  p->cnt        = outPortCnt;
  p->sendSymId  = cmDspSysRegisterStaticSymbol(ctx->dspH,"send");
  p->printSymId = cmDspSysRegisterStaticSymbol(ctx->dspH,"print");
  p->cntSymId   = cmDspSysRegisterStaticSymbol(ctx->dspH,"count");
  p->doneSymId  = doneSymId;
  return &p->inst;
}


cmDspRC_t _cmDspArrayReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspArray_t* p = (cmDspArray_t*)inst;
  cmDspRC_t     rc;

  // send 'out' values and then 'done' value
  if((rc = cmDspApplyAllDefaults(ctx,inst)) == kOkDspRC )
    cmDspSetSymbol(ctx,inst,kDoneArId,p->doneSymId);

  return rc;
}

cmDspRC_t _cmDspArrayRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t     rc;
  cmDspArray_t* p = (cmDspArray_t*)inst;

  if((rc = cmDspSetEvent(ctx,inst,evt)) != kOkDspRC )
    return rc;

  switch( evt->dstVarId )
  {
    case kCmdArId:
      {
        unsigned i;
        unsigned cmdSymId = cmDsvSymbol(evt->valuePtr);

        if( cmdSymId == p->printSymId )
        {
          for(i=0; i<p->cnt; ++i)
            cmRptPrintf(ctx->rpt,"%f ",p->array[i]);
          cmRptPrintf(ctx->rpt,"\n");
        }
        else
          if( cmdSymId == p->sendSymId )
          {
            for(i=0; i<p->cnt; ++i)
              cmDspSetDouble(ctx,inst,kBaseOutArId+i,p->array[i]);
            cmDspSetSymbol(ctx,inst,kDoneArId,p->doneSymId);
          }
          else
            if( cmdSymId == p->cntSymId )
            {
              cmDspSetUInt(ctx,inst,kCntArId,p->cnt);
            }
      }
      break;

    case kIdxArId:
      {
        unsigned idx = cmDsvUInt(evt->valuePtr);
        if( idx < p->cnt )
          cmDspSetDouble(ctx,inst,kValArId,p->array[idx]);
      }
      break;
  }
  

  return kOkDspRC;
}

struct cmDspClass_str* cmArrayClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmArrayDC,ctx,"Array",
    NULL,
    _cmDspArrayAlloc,
    NULL,
    _cmDspArrayReset,
    NULL,
    _cmDspArrayRecv,
    NULL,NULL,
    "Time tagged text file.");

  return &_cmArrayDC;
}


//==========================================================================================================================================

enum
{
  kMidiPcId,
  kHzPcId,
  kOffsPcId,
  kStrPcId,
  kRatioPcId
};

cmDspClass_t _cmPitchCvtDC;

typedef struct
{
  cmDspInst_t inst;
  int         midi;
  double      hz;
} cmDspPitchCvt_t;

cmDspInst_t*  _cmDspPitchCvtAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  va_cnt = 0; // ignore an errant arguments

  cmDspPitchCvt_t* p = cmDspInstAllocV(cmDspPitchCvt_t,ctx,classPtr,instSymId,id,storeSymId,va_cnt,vl,
    1,          "midi",  kMidiPcId,    0, 0, kUIntDsvFl   | kInDsvFl | kOutDsvFl | kOptArgDsvFl, "MIDI pitch value input.",
    1,          "hz",    kHzPcId,      0, 0, kDoubleDsvFl | kInDsvFl | kOutDsvFl | kOptArgDsvFl, "Hz pitch value.",
    1,          "offs",  kOffsPcId,    0, 0, kDoubleDsvFl | kInDsvFl | kInDsvFl  | kOptArgDsvFl, "Semitone offset.",
    1,          "str",   kStrPcId,     0, 0, kStrzDsvFl   | kOutDsvFl,                           "Pitch string output.",
    1,          "ratio", kRatioPcId,   0, 0, kDoubleDsvFl | kOutDsvFl,                           "Offset as a ratio",
    0 );

  cmDspSetDefaultUInt(   ctx, &p->inst, kMidiPcId, 0, 0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kHzPcId,   0, 0.0 );
  cmDspSetDefaultDouble( ctx, &p->inst, kOffsPcId, 0, 0 );
  cmDspSetDefaultStrcz(  ctx, &p->inst, kStrPcId,  NULL, "" );
  cmDspSetDefaultDouble( ctx, &p->inst, kRatioPcId, 0, 0 );

  return &p->inst;
}

cmDspRC_t _cmDspPitchCvtReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = cmDspApplyAllDefaults(ctx,inst);
  return rc;
}

cmDspRC_t _cmDspPitchCvtOutput( cmDspCtx_t* ctx, cmDspPitchCvt_t* p )
{
  cmDspInst_t* inst = &p->inst;
  double       offs = cmDspDouble( inst, kOffsPcId );
  unsigned     midi = cmMax(0, p->midi + rint(offs) );
  double       ratio= pow(2.0,offs/12.0);
  double       hz   = p->hz * ratio;

  //cmRptPrintf(ctx->rpt,"%i %i %f %f\n",offs,midi,hz,p->hz);

  cmDspSetStrcz(  ctx, inst, kStrPcId,  cmMidiToSciPitch(midi,NULL,0));
  cmDspSetUInt(   ctx, inst, kMidiPcId, midi);
  cmDspSetDouble( ctx, inst, kHzPcId,   hz);
  cmDspSetDouble( ctx, inst, kRatioPcId, ratio );

  return kOkDspRC;
}

cmDspRC_t _cmDspPitchCvtRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t        rc = kOkDspRC;
  cmDspPitchCvt_t* p  = (cmDspPitchCvt_t*)inst;

  switch( evt->dstVarId )
  {
    case kMidiPcId:
      p->midi = cmMax(0,cmDsvGetInt(evt->valuePtr));
      p->hz   = cmMidiToHz(p->midi);
      rc      = _cmDspPitchCvtOutput(ctx,p);
      break;

    case kHzPcId:
      p->hz   = cmDsvGetDouble(evt->valuePtr);
      p->midi = cmHzToMidi(p->hz);
      rc      = _cmDspPitchCvtOutput(ctx,p);
      break;

    case kOffsPcId:
      if((rc = cmDspSetEvent(ctx, inst, evt )) == kOkDspRC )
        rc = _cmDspPitchCvtOutput(ctx,p);
      break;    
  }

  return rc;
}

struct cmDspClass_str* cmPitchCvtClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmPitchCvtDC,ctx,"PitchCvt",
    NULL,
    _cmDspPitchCvtAlloc,
    NULL,
    _cmDspPitchCvtReset,
    NULL,
    _cmDspPitchCvtRecv,
    NULL,NULL,
    "Time tagged text file.");

  return &_cmPitchCvtDC;
}

//==========================================================================================================================================

//
//
//  Create a file which can be read by readBinFile.m
//
//

enum
{
  kCntBmId,
  kFnBmId,
  kBaseBmId
};

cmDspClass_t _cmBinMtxFileDC;

typedef struct
{
  cmDspInst_t     inst;
  int             inPortCnt;
  cmBinMtxFile_t* bmfp;
  cmReal_t *      valArray;     // valArray[ inPortCnt ]
} cmDspBinMtxFile_t;

cmDspInst_t*  _cmDspBinMtxFileAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] = 
  {
    { "cnt",  kCntBmId,  0, 0, kIntDsvFl | kReqArgDsvFl, "Input port count"},
    { "fn",   kFnBmId,   0, 0, kInDsvFl  | kStrzDsvFl   | kReqArgDsvFl, "File name"},
  };


  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The binary matrix file object must be given a input port count argument.");
    return NULL;
  }

  va_list vl1;
  va_copy(vl1,vl);

  int                inPortCnt = va_arg(vl,int);
  unsigned           fixArgCnt = sizeof(args)/sizeof(args[0]);
  unsigned           argCnt    = fixArgCnt + inPortCnt;
  cmDspVarArg_t      a[ argCnt+1 ];
  cmDspBinMtxFile_t* p         = NULL;

  if( inPortCnt <= 0 )
  {
    cmDspClassErr(ctx,classPtr,kInvalidArgDspRC,"The binary matrix file must be a positive integer.");
    return NULL;
  }

  cmDspArgCopy(  a, argCnt, 0, args, fixArgCnt );
  cmDspArgSetupN(ctx, a, argCnt, kBaseBmId, inPortCnt, "in", kBaseBmId, 0, 0, kInDsvFl  | kDoubleDsvFl, "input ports");
  cmDspArgSetupNull( a+argCnt );

  if((p = cmDspInstAlloc(cmDspBinMtxFile_t,ctx,classPtr,a,instSymId,id,storeSymId,va_cnt,vl1)) == NULL )
    return NULL;

  p->bmfp      = cmBinMtxFileAlloc(ctx->cmProcCtx, NULL, NULL );
  p->inPortCnt = inPortCnt;
  p->valArray  = cmMemAllocZ(cmReal_t,inPortCnt);

  return &p->inst;
}

cmDspRC_t _cmDspBinMtxFileOpen( cmDspCtx_t* ctx, cmDspInst_t* inst )
{
  cmDspRC_t          rc = kOkDspRC;
  cmDspBinMtxFile_t* p  = (cmDspBinMtxFile_t*)inst;

  if( p->bmfp != NULL )
  {
    // finalize the current file
    if( cmBinMtxFileFinal(p->bmfp) != cmOkRC )
      cmDspInstErr(ctx,inst,kFileCloseFailDspRC,"File close failed.");

    // open a new one
    if( cmBinMtxFileInit( p->bmfp, cmDspDefaultStrcz(&p->inst,kFnBmId) ) != cmOkRC)
      rc                                                                  = cmDspInstErr(ctx,inst,kFileOpenFailDspRC,"File open failed.");
  }

  return rc;
}

cmDspRC_t _cmDspBinMtxFileFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspBinMtxFile_t* p = (cmDspBinMtxFile_t*)inst;

  cmBinMtxFileFree(&p->bmfp);
  cmMemFree(p->valArray);
  return kOkDspRC;
}

cmDspRC_t _cmDspBinMtxFileReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  return _cmDspBinMtxFileOpen(ctx,inst);
}
cmDspRC_t _cmDspBinMtxFileExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t          rc = kOkDspRC;
  cmDspBinMtxFile_t* p  = (cmDspBinMtxFile_t*)inst;

  // write the file
  if( cmBinMtxFileIsValid( p->bmfp ) )
    if( cmBinMtxFileExecR(p->bmfp, p->valArray, p->inPortCnt ) != cmOkRC )
      return cmDspInstErr(ctx,inst,kFileWriteFailDspRC,"File write failure.");

  return rc;
}
cmDspRC_t _cmDspBinMtxFileRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspRC_t          rc = kOkDspRC;
  cmDspBinMtxFile_t* p  = (cmDspBinMtxFile_t*)inst;
  cmDspSetEvent(ctx,inst,evt);
  
  // new file name - create new output file
  if( evt->dstVarId == kFnBmId )
  {
    rc = _cmDspBinMtxFileOpen(ctx,inst);
  }
  else
    // new value - store in p->valArray[]
  if( kBaseBmId <= evt->dstVarId && evt->dstVarId < kBaseBmId + p->inPortCnt )
  {
    p->valArray[ evt->dstVarId - kBaseBmId ] = cmDspDouble(inst,evt->dstVarId );
  }
  else
  { assert(0); }

  return rc;
}

struct cmDspClass_str* cmBinMtxFileClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmBinMtxFileDC,ctx,"BinMtxFile",
    NULL,
    _cmDspBinMtxFileAlloc,
    _cmDspBinMtxFileFree,
    _cmDspBinMtxFileReset,
    _cmDspBinMtxFileExec,
    _cmDspBinMtxFileRecv,
    NULL,NULL,
    "Time tagged text file.");

  return &_cmBinMtxFileDC;
}


//==========================================================================================================================================
enum
{
  kHopMsSbId,
  kWndFactSbId,
  kInSbId,
  kOutSbId
};

cmDspClass_t _cmShiftBufDC;

typedef struct
{
  cmDspInst_t inst;
  cmShiftBuf* sbp;
} cmDspShiftBuf_t;

void _cmDspShiftBufSetup( cmDspCtx_t* ctx, cmDspShiftBuf_t* p )
{
  double   hopMs     = cmDspDouble(&p->inst,kHopMsSbId);
  unsigned hopSmpCnt = lround(cmDspSampleRate(ctx) * hopMs / 1000.0 );
  unsigned wndSmpCnt = cmDspUInt(&p->inst,kWndFactSbId) * hopSmpCnt;
  
  if( p->sbp == NULL || hopSmpCnt != p->sbp->hopSmpCnt || wndSmpCnt != p->sbp->wndSmpCnt )
  {
    cmShiftBufFree(&p->sbp);
    p->sbp = cmShiftBufAlloc(ctx->cmProcCtx, NULL, cmDspSamplesPerCycle(ctx), wndSmpCnt, hopSmpCnt );
    
  }
}

cmDspInst_t*  _cmDspShiftBufAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] = 
  {
    { "hopMs",     kHopMsSbId,   0, 0, kInDsvFl  | kDoubleDsvFl | kReqArgDsvFl, "Hop size on milliseconds"},
    { "wndFact",   kWndFactSbId, 0, 0, kInDsvFl  | kUIntDsvFl   | kReqArgDsvFl, "Count of hops contained in each output buffer."},
    { "in",        kInSbId,      0, 0, kInDsvFl  | kAudioBufDsvFl, "Audio input"},
    { "out",       kOutSbId,     0, 0, kOutDsvFl | kAudioBufDsvFl, "Audio output"},
    { NULL, 0, 0, 0, 0 }
  };

  // Note: by setting the column count of the output audio variable to zero
  // we prevent it from being automatically assigned vector memory.

  cmDspShiftBuf_t* p = cmDspInstAlloc(cmDspShiftBuf_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);


  return &p->inst;
}

cmDspRC_t _cmDspShiftBufFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspShiftBuf_t* p = (cmDspShiftBuf_t*)inst;

  cmShiftBufFree(&p->sbp);

  return kOkDspRC;
}

cmDspRC_t _cmDspShiftBufReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t rc = kOkDspRC;

  cmDspShiftBuf_t* p = (cmDspShiftBuf_t*)inst;

  cmDspApplyAllDefaults(ctx,inst);

  _cmDspShiftBufSetup(ctx,p);
  
  return rc;
}

cmDspRC_t _cmDspShiftBufRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{ 
  cmDspShiftBuf_t* p = (cmDspShiftBuf_t*)inst;
  cmDspSetEvent(ctx,inst,evt);

  switch(evt->dstVarId)
  {
    case kHopMsSbId:
    case kWndFactSbId:
      _cmDspShiftBufSetup( ctx, p );
      break;
  };

  return kOkDspRC;
}

cmDspRC_t _cmDspShiftBufExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspShiftBuf_t*  p      = (cmDspShiftBuf_t*)inst;
  unsigned          sn     = cmDspAudioBufSmpCount(ctx,inst,kInSbId,0);
  const cmSample_t* sp     = cmDspAudioBuf(ctx,inst,kInSbId,0);
  cmDspVar_t*       varPtr = cmDspVarIdToPtr(inst,kOutSbId);

  if( cmShiftBufExec(p->sbp, sp, sn ) )
    cmDsvSetSampleMtx(  &varPtr->value, p->sbp->outV, p->sbp->outN, 1);
  else
    cmDsvSetSampleMtx(  &varPtr->value, NULL, 0, 0);

  return kOkDspRC;
}

struct cmDspClass_str* cmShiftBufClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmShiftBufDC,ctx,"ShiftBuf",
    NULL,
    _cmDspShiftBufAlloc,
    _cmDspShiftBufFree,
    _cmDspShiftBufReset,
    _cmDspShiftBufExec,
    _cmDspShiftBufRecv,
    NULL,NULL,
    "Time tagged text file.");

  return &_cmShiftBufDC;
}

//==========================================================================================================================================
enum
{
  kInNsId

};

cmDspClass_t _cmNetSendDC;

typedef struct
{
  cmDspInst_t inst;
  _cmDspSrcConn_t* srcConnPtr;
} cmDspNetSend_t;

cmDspInst_t*  _cmDspNetSendAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] = 
  {
    { "in",   kInNsId,   0, 0,   kInDsvFl  | kTypeDsvMask, "Input port"   },
    { NULL, 0, 0, 0, 0 }
  };
  
  assert( va_cnt == 1 );
  _cmDspSrcConn_t* srcConnPtr = va_arg(vl,_cmDspSrcConn_t*);
  assert( srcConnPtr != NULL );

  cmDspNetSend_t* p = cmDspInstAlloc(cmDspNetSend_t,ctx,classPtr,args,instSymId,id,storeSymId,0,vl);
  p->srcConnPtr = srcConnPtr;
  return &p->inst;
}

cmDspRC_t _cmDspNetSendReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return kOkDspRC;
}

cmDspRC_t _cmDspNetSendRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspNetSend_t* p = (cmDspNetSend_t*)inst;
  return _cmDspSysNetSendEvent(ctx->dspH, p->srcConnPtr->dstNetNodeId, p->srcConnPtr->dstId, evt );
}

struct cmDspClass_str* cmNetSendClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmNetSendDC,ctx,"NetSend",
    NULL,
    _cmDspNetSendAlloc,
    NULL,
    _cmDspNetSendReset,
    NULL,
    _cmDspNetSendRecv,
    NULL,NULL,
    "Print the value of any event arriving at 'in'.");

  return &_cmNetSendDC;
}

//==========================================================================================================================================
enum
{
  kBaseInPtsId,
};

cmDspClass_t _cmRsrcWrDC;

typedef struct
{
  cmDspInst_t inst;
  char**      pathArray;
  unsigned    pathCnt;
} cmDspRsrcWr_t;

cmDspInst_t*  _cmDspRsrcWrAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  va_list       vl1;
  va_copy(vl1,vl);

  if( va_cnt < 1 )
  {
    cmDspClassErr(ctx,classPtr,kVarArgParseFailDspRC,"The 'RsrcWr' constructor argument list must contain at least one resource path specificiation.");
    return NULL;
  }

  unsigned      pathCnt    = va_cnt;
  unsigned      argCnt    = pathCnt;
  cmDspVarArg_t args[argCnt+1];

  char**       pathArray   = cmMemAllocZ(char*,pathCnt);

  unsigned i;

  for(i=0; i<pathCnt; ++i)
  {
    // get the path
    const cmChar_t* pathLabel = va_arg(vl,const char*);
    assert( pathLabel != NULL );

    // store the path name
    pathArray[i] = cmMemAllocStr(pathLabel);

    cmDspArgSetup(ctx, args+kBaseInPtsId+i, pathLabel, cmInvalidId, kBaseInPtsId+i, 0, 0, kInDsvFl  | kTypeDsvMask, cmTsPrintfH(ctx->lhH,"%s Input.",pathLabel) );

  }

  cmDspArgSetupNull(args + argCnt);

  cmDspRsrcWr_t* p = cmDspInstAlloc(cmDspRsrcWr_t,ctx,classPtr,args,instSymId,id,storeSymId,0,vl1);

  p->pathCnt     = pathCnt;
  p->pathArray   = pathArray;


  return &p->inst;
}

cmDspRC_t _cmDspRsrcWrFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRsrcWr_t* p = (cmDspRsrcWr_t*)inst;
  int i;
  for(i=0; i<p->pathCnt; ++i)
    cmMemFree(p->pathArray[i]);
  cmMemFree(p->pathArray);

  return kOkDspRC;
}

cmDspRC_t _cmDspRsrcWrReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  return kOkDspRC;
}


cmDspRC_t _cmDspRsrcWrRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRsrcWr_t* p = (cmDspRsrcWr_t*)inst;
  cmDspRC_t rc = kOkDspRC;

    
  // if a msg of any type is recieved on an input port - send out the associated symbol
  if( kBaseInPtsId <= evt->dstVarId && evt->dstVarId < kBaseInPtsId + p->pathCnt )
  {
    unsigned idx = evt->dstVarId - kBaseInPtsId;
    assert( idx < p->pathCnt );

    if( cmDsvIsStrz(evt->valuePtr) )
    {
      rc = cmDspRsrcWriteString( ctx->dspH, cmDsvStrz(evt->valuePtr), p->pathArray[idx], NULL );
    }

  }

  return rc;
}

struct cmDspClass_str* cmRsrcWrClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmRsrcWrDC,ctx,"RsrcWr",
    NULL,
    _cmDspRsrcWrAlloc,
    _cmDspRsrcWrFree,
    _cmDspRsrcWrReset,
    NULL,
    _cmDspRsrcWrRecv,
    NULL,
    NULL,
    "Set the value of a resource variable.");

  return &_cmRsrcWrDC;
}

//==========================================================================================================================================
enum
{
  kModeBeId,
  kAzimBeId,
  kElevBeId,
  kDistBeId,
  kAudioInBeId,
  kAudioOut0BeId,
  kAudioOut1BeId
};

typedef struct
{
  cmDspInst_t inst;
  cmBinEnc*   bep;
} cmDspBinEnc_t;
 
cmDspClass_t _cmBeDC;

cmDspInst_t*  _cmDspBinEncAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "mode",    kModeBeId,        0, 0,   kInDsvFl  | kUIntDsvFl | kReqArgDsvFl, "Mode" },
    { "azim",    kAzimBeId,        0, 0,   kInDsvFl  | kDoubleDsvFl,              "Azimuth" },
    { "elev",    kElevBeId,        0, 0,   kInDsvFl  | kDoubleDsvFl,              "Elevation" },
    { "dist",    kDistBeId,        0, 0,   kInDsvFl  | kDoubleDsvFl,              "Distance" },
    { "in",      kAudioInBeId,     0, 0,   kInDsvFl  | kAudioBufDsvFl,            "Audio Input" },
    { "out0",    kAudioOut0BeId,   0, 1,   kOutDsvFl | kAudioBufDsvFl,            "Audio Output 0" },
    { "out1",    kAudioOut1BeId,   0, 1,   kOutDsvFl | kAudioBufDsvFl,            "Audio Output 1" },
    { NULL, 0, 0, 0, 0 }
  };

  cmDspBinEnc_t* p = cmDspInstAlloc(cmDspBinEnc_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);
  
  cmDspSetDefaultUInt(   ctx,&p->inst, kModeBeId, 0, 0.0 );
  cmDspSetDefaultDouble( ctx,&p->inst, kAzimBeId, 0, 0.0 );
  cmDspSetDefaultDouble( ctx,&p->inst, kElevBeId, 0, 0.0 );
  cmDspSetDefaultDouble( ctx,&p->inst, kDistBeId, 0, 0.0 );

  return &p->inst;
}

cmDspRC_t _cmDspBinEncFree(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspRC_t      rc = kOkDspRC;
  cmDspBinEnc_t* p  = (cmDspBinEnc_t*)inst;

  cmBinEncFree(&p->bep);

  return rc;
}

cmDspRC_t _cmDspBinEncSetup(cmDspCtx_t* ctx, cmDspBinEnc_t* p )
{
  cmDspRC_t rc           = kOkDspRC;

  cmBinEncFree(&p->bep);

  p->bep = cmBinEncAlloc(ctx->cmProcCtx,NULL,cmDspSampleRate(ctx), cmDspSamplesPerCycle(ctx));

  return rc;
}

cmDspRC_t _cmDspBinEncReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspBinEnc_t*   p  = (cmDspBinEnc_t*)inst;
  cmDspRC_t    rc;

  if((rc = cmDspApplyAllDefaults(ctx,inst)) != kOkDspRC )
    return rc;

  return _cmDspBinEncSetup(ctx,p);
}

cmDspRC_t _cmDspBinEncExec(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspBinEnc_t* p  = (cmDspBinEnc_t*)inst;
  cmDspRC_t      rc = kOkDspRC;

  unsigned          iChIdx  = 0;
  const cmSample_t* ip      = cmDspAudioBuf(ctx,inst,kAudioInBeId,iChIdx);
  unsigned          iSmpCnt = cmDspVarRows(inst,kAudioInBeId);

  // if no connected
  if( iSmpCnt == 0 )
    return rc;
  
  unsigned          oChIdx   = 0;
  cmSample_t*       o0p      = cmDspAudioBuf(ctx,inst,kAudioOut0BeId,oChIdx);
  unsigned          oSmp0Cnt = cmDspVarRows(inst,kAudioOut0BeId);
  cmSample_t*       o1p      = cmDspAudioBuf(ctx,inst,kAudioOut1BeId,oChIdx);
  unsigned          oSmp1Cnt = cmDspVarRows(inst,kAudioOut0BeId);

  assert( iSmpCnt==oSmp0Cnt && iSmpCnt==oSmp1Cnt );

  cmBinEncExec( p->bep, ip, o0p, o1p, iSmpCnt );

  return rc;
}

cmDspRC_t _cmDspBinEncRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspBinEnc_t* p  = (cmDspBinEnc_t*)inst;
  cmDspRC_t      rc = kOkDspRC;

  cmDspSetEvent(ctx,inst,evt);

  switch( evt->dstVarId )
  {
    case kModeBeId:
      cmBinEncSetMode(p->bep, cmDspUInt(inst,kModeBeId));
      break;

    case kAzimBeId:
    case kElevBeId:
    case kDistBeId:
      {
        float azim = cmDspDouble(inst,kAzimBeId);
        float elev = cmDspDouble(inst,kElevBeId);
        float dist = cmDspDouble(inst,kDistBeId);
        cmBinEncSetLoc(p->bep, azim, elev, dist );
      }
      break;

    default:
      { assert(0); }
  }

  return rc;
}

struct cmDspClass_str* cmBinEncClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cmBeDC,ctx,"BinauralEnc",
    NULL,
    _cmDspBinEncAlloc,
    _cmDspBinEncFree,
    _cmDspBinEncReset,
    _cmDspBinEncExec,
    _cmDspBinEncRecv,
    NULL,NULL,
    "Binaural filter.");

  return &_cmBeDC;
}

//==========================================================================================================================================
enum
{
  kX2dId,
  kY2dId,
  kRadius2dId,
  kAngle2dId
};

cmDspClass_t _cm2dDC;

typedef struct
{
  cmDspInst_t inst;
} cmDsp2d_t;

cmDspInst_t*  _cmDsp2dAlloc(cmDspCtx_t* ctx, cmDspClass_t* classPtr, unsigned storeSymId, unsigned instSymId, unsigned id, unsigned va_cnt, va_list vl )
{
  cmDspVarArg_t args[] =
  {
    { "x",     kX2dId,      0, 0, kOutDsvFl  | kDoubleDsvFl,  "X coordinate" },
    { "y",     kY2dId,      0, 0, kOutDsvFl  | kDoubleDsvFl,  "Y coordinate"},
    { "radius",kRadius2dId, 0, 0, kOutDsvFl  | kDoubleDsvFl,  "Radius"},
    { "angle", kAngle2dId,  0, 0, kOutDsvFl  | kDoubleDsvFl,  "Angle"},
    { NULL, 0, 0, 0, 0 }
  };

  cmDsp2d_t* p = cmDspInstAlloc(cmDsp2d_t,ctx,classPtr,args,instSymId,id,storeSymId,va_cnt,vl);

  cmDspSetDefaultDouble(ctx, &p->inst, kX2dId,       0.0, 0.0);
  cmDspSetDefaultDouble(ctx, &p->inst, kY2dId,       0.0, 0.0);
  cmDspSetDefaultDouble(ctx, &p->inst, kRadius2dId,  0.0, 0.0);
  cmDspSetDefaultDouble(ctx, &p->inst, kAngle2dId,   0.0, 0.0);

  // create the UI control
  cmDspUi2dCreate(ctx,&p->inst,kX2dId,kY2dId,kRadius2dId,kAngle2dId);

  return &p->inst;
}

cmDspRC_t _cmDsp2dReset(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspApplyAllDefaults(ctx,inst);
  return kOkDspRC;
}

cmDspRC_t _cmDsp2dRecv(cmDspCtx_t* ctx, cmDspInst_t* inst, const cmDspEvt_t* evt )
{
  cmDspSetEvent(ctx,inst,evt);

  return kOkDspRC;
}


struct cmDspClass_str* cm2dClassCons( cmDspCtx_t* ctx )
{
  cmDspClassSetup(&_cm2dDC,ctx,"twod",
    NULL,
    _cmDsp2dAlloc,
    NULL,
    _cmDsp2dReset,
    NULL,
    _cmDsp2dRecv,
    NULL,
    NULL,
    "2d value control.");

  return &_cm2dDC;
}


//==========================================================================================================================================

//==========================================================================================================================================


cmDspClassConsFunc_t _cmDspClassBuiltInArray[] = 
{
  cmPrinterClassCons,
  cmCounterClassCons,

  cmPhasorClassCons,
  cmMidiOutClassCons,
  cmMidiInClassCons,
  cmAudioInClassCons,
  cmAudioOutClassCons,
  cmAudioFileOutClassCons,
  cmSigGenClassCons,

  cmScalarClassCons,
  cmTextClassCons,
  cmMeterClassCons,
  cmLabelClassCons,
  cmButtonClassCons,
  cmCheckboxClassCons,

  cmReorderClassCons,
  cmFnameClassCons,
  cmMsgListClassCons,
  cmWaveTableClassCons,

  cmSprintfClassCons,
  cmAMixClassCons,
  cmASplitClassCons,
  cmAMeterClassCons,
  cmTextFileClassCons,
  cmBinMtxFileClassCons,
  cmArrayClassCons,
  cmPitchCvtClassCons,

  cmShiftBufClassCons,
  cmNetSendClassCons,
  cmRsrcWrClassCons,
  cmBinEncClassCons,
  cm2dClassCons,

  cmDelayClassCons,
  cmPShiftClassCons,
  cmLoopRecdClassCons,
  cmRectifyClassCons,
  cmGateDetectClassCons,
  cmAutoGainClassCons,
  cmEnvFollowClassCons,
  cmXfaderClassCons,
  cmChCfgClassCons,
  cmChordDetectClassCons,
  cmFaderClassCons,
  cmNoteSelectClassCons,
  cmNetNoteSelectClassCons,
  cmCombFiltClassCons,
  cmScalarOpClassCons,

  cmGroupSelClassCons,
  cmAudioNofMClassCons,
  cmRingModClassCons,
  cmMsgDelayClassCons,

  cmLineClassCons,
  cmAdsrClassCons,
  cmCompressorClassCons,
  cmBiQuadEqClassCons,
  cmDistDsClassCons,
  cmDbToLinClassCons,
  cmMtDelayClassCons,

  cmNofMClassCons,
  cm1ofNClassCons,
  cm1UpClassCons,
  cmGateToSymClassCons,
  cmPortToSymClassCons,
  cmRouterClassCons,
  cmAvailChClassCons,

  cmPresetClassCons,
  cmBcastSymClassCons,
  cmSegLineClassCons,

  cmKrClassCons,  
  cmTimeLineClassCons,
  cmScoreClassCons,
  cmMidiFilePlayClassCons,
  cmScFolClassCons,
  cmScModClassCons,
  cmGSwitchClassCons,
  cmScaleRangeClassCons,
  cmActiveMeasClassCons,
  cmAmSyncClassCons,
  cmNanoMapClassCons,
  cmRecdPlayClassCons,
  cmGoertzelClassCons,
  cmSyncRecdClassCons,
  cmTakeSeqBldrClassCons,
  cmTakeSeqRendClassCons,
  cmReflectCalcClassCons,
  cmEchoCancelClassCons,
  NULL,
};

cmDspClassConsFunc_t cmDspClassGetBuiltIn( unsigned index )
{
  unsigned n = sizeof(_cmDspClassBuiltInArray)/sizeof(cmDspClass_t*);

  if( index >= n )
    return NULL;

  return _cmDspClassBuiltInArray[index];
}
