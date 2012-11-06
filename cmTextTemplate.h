#ifndef cmTextTemplate_h
#define cmTextTemplate_h


enum
{
  kOkTtRC = cmOkRC,
  kFileFailTtRC,
  kLHeapFailTtRC,
  kSyntaxErrTtRC
};

typedef cmHandle_t cmTtH_t;
typedef unsigned   cmTtRC_t;
extern cmTtH_t cmTtNullHandle;


cmTtRC_t cmTextTemplateInitialize( cmCtx_t* ctx, cmTtH_t* hp, const cmChar_t* fn );
cmTtRC_t cmTextTemplateFinalize( cmTtH_t* hp );
bool     cmTextTemplateIsValid( cmTtH_t h );
cmTtRC_t cmTextTemplateTest( cmCtx_t* ctx, const cmChar_t* fn );



#endif
