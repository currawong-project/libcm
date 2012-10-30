#ifndef cmKeyboard_h
#define cmKeyboard_h


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

#endif
