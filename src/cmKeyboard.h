//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmKeyboard_h
#define cmKeyboard_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Query and get keypresses directly from the console." kw:[system devices] }
  
  enum
  {
    kInvalidKId,
    kAsciiKId,
    kLeftArrowKId,
    kRightArrowKId,
    kUpArrowKId,
    kDownArrowKId,
    kHomeKId,
    kEndKId,
    kPgUpKId,
    kPgDownKId,
    kInsertKId,
    kDeleteKId,
  
  };

  typedef struct
  {
    unsigned code;
    char     ch;
    bool     ctlFl;
    bool     altFl;
  } cmKbRecd;

  // Set 'p' to NULL if the value of the key is not required.
  void cmKeyPress( cmKbRecd* p );


  // Return non-zero if a key is waiting to be read otherwise return 0.
  // Use getchar() to pick up the key.
  // 
  // Example:
  // while( 1 )
  // {
  //    if( cmIsKeyWaiting() == 0 )
  //       usleep(20000);
  //    else
  //    {
  //      char c = getchar();
  //      switch(c)
  //      {
  //        ....
  //      } 
  //    }
  //
  // }
  //
  // TODO: Note that this function turns off line-buffering on stdin.
  // It should be changed to a three function sequence.
  // bool org_state =  cmSetStdinLineBuffering(false);
  // ....
  // cmIsKeyWaiting()
  // ....
  // cmSetStdinLineBuffering(org_state)
  int cmIsKeyWaiting();

  //)
  
#ifdef __cplusplus
  extern "C" {
#endif
  
#endif
