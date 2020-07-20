/*! \mainpage cm Manual
 
  To modify this page edit cmDocMain.h

  \section building Building
  \subsection debug_mode Debug/Release Compile Mode 
  By default the project builds in debug mode.  To build in release mode define NDEBUG 
  on the compiler command line.  The existence of NDEBUG is tested in cmGlobal.h  and 
  the value of the preprocessor variable #cmDEBUG_FL is set to 0 if NDEBUG was defined
  and 1 otherwise.  Code which depends on the debug/release mode then tests the value of
  #cmDEBUG_FL. 
  
 
  The cm library is a set of C routines for working audio signals.
 
  \section foundation Foundation
  \subsection mem Memory Management
  \subsection output Output and Error Reporting
  \subsection files File Management
  \subsection cfg Program Configuration and Data

  
 
  \subsection step1 Step 1: Opening the box
   
 */


/*!


\defgroup base Base
@{

@}

\defgroup rt Real-time
@{
@}

\defgroup audio Audio
@{

@}

\defgroup dsp Signal Processing
@{
@}

\defgroup gr Graphics
@{
@}

 */
