#ifndef __crankshaftcommandlinedoth__
#define __crankshaftcommandlinedoth__

#include <stdint.h>
/********************************************************************
 *
 * Command line handling. Check main.cpp for an example.
 *
 * In your main.cpp:
 *
 * static bool wantHelp;
 *
 * CS_ARG_DEF(wantHelp,CS_ARG_CMP("-?","-help","--help"),"Prints this help");
 *
 * const struct CS_ArgElement myArgs[] =
 *  { CS_ARG_ELEMENT(wantHelp,CS_BOOL_ARG) };
 *
 * struct CS_ArgTable myCS_ArgTable = { sizeof(myArgs)/sizeof(CS_ArgElement), 0, NULL, myArgs };
 *
 * int32_t main(int32_t argc, char *argv[] ) {
 *    const char * error = CS_argsParse(argc, argv, &myCS_ArgTable);
 *    if( error != NULL || wantHelp ) {
 *        if( error ) printf( error );
 *        CS_argsPrint( myCS_ArgTable );
 *    }
 *    <PROGRAM HERE>
 * }
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif
enum CS_ARG_WHAT {
    CS_INT_ARG,
    CS_STRING_ARG,
    CS_BOOL_ARG
};

struct CS_ArgElement {
    int32_t nCmp;
    enum CS_ARG_WHAT what;
    void *out;
    const char **cmp;
    const char *desc;
};

struct CS_ArgTable {
    int32_t nArgs;
    int32_t outNRemainders;
    char **remainders;
    const struct CS_ArgElement *elements;
};

void CS_argsPrint(struct CS_ArgTable *argTable);

const char *CS_argsParse(int32_t argc, char **argv, struct CS_ArgTable *argTable);

#define CS_ARG_DEF(varName,cmpString,helpString) static const char *CMP_##varName[] = cmpString; static const char HELP_##varName[] = helpString;
#define CS_ARG_ELEMENT(varName,varType) { sizeof(CMP_##varName)/sizeof(CMP_##varName[0]), varType, &varName, CMP_##varName, HELP_##varName }
#define CS_ARG_CMP(...) {__VA_ARGS__}
#ifdef __cplusplus
}
#endif

#endif
