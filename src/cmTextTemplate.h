//| Copyright: (C) 2009-2020 Kevin Larke <contact AT larke DOT org> 
//| License: GNU GPL version 3.0 or above. See the accompanying LICENSE file.
#ifndef cmTextTemplate_h
#define cmTextTemplate_h

#ifdef __cplusplus
extern "C" {
#endif

  //( { file_desc:"Generate text using templates with replaceable variables." kw:[text] }
  enum
  {
    kOkTtRC = cmOkRC,
    kFileFailTtRC,
    kLHeapFailTtRC,
    kSyntaxErrTtRC,
    kFindFailTtRC,
    kInvalidTypeTtRC,
    kJsonFailTtRC
  };

  typedef cmHandle_t cmTtH_t;
  typedef unsigned   cmTtRC_t;
  extern  cmTtH_t    cmTtNullHandle;

  // Initialize a template file.
  cmTtRC_t cmTextTemplateInitialize( cmCtx_t* ctx, cmTtH_t* hp, const cmChar_t* fn );

  // Finalize a template file
  cmTtRC_t cmTextTemplateFinalize( cmTtH_t* hp );

  // Return true if the template file is intialized.
  bool     cmTextTemplateIsValid( cmTtH_t h );

  // Set the value of a template variable.
  // The node identified by { label,index, label, index ... } must
  // be a variable node.  The function will fail if a 'set' or 'text' node
  // is identified.
  // Set 'value' to NULL to erase a previously set value.
  cmTtRC_t cmTextTemplateSetValue( cmTtH_t h, const cmChar_t* value, const cmChar_t* label, unsigned index, ... );

  // Create a copy of the sub-tree identified by the variable path
  // and insert it as the left sibling of the sub-tree's root.
  cmTtRC_t cmTextTemplateRepeat( cmTtH_t h, const cmChar_t* label, unsigned index, ... );

  // Write the template file.
  cmTtRC_t cmTextTemplateWrite( cmTtH_t h, const cmChar_t* fn );

  // Apply a template value JSON file to this template
  cmTtRC_t cmTextTemplateApply( cmTtH_t h, const cmChar_t* fn );

  // Print an annotated template tree.
  void cmTtPrintTree( cmTtH_t h, cmRpt_t* rpt );

  cmTtRC_t cmTextTemplateTest( cmCtx_t* ctx, const cmChar_t* fn );
  
  //)
  
#ifdef __cplusplus
}
#endif

#endif
