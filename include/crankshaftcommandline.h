#ifndef __crankshaftcommandlinedoth__
#define __crankshaftcommandlinedoth__

#ifdef __cplusplus
extern "C" {
#endif
enum ARG_WHAT {
    INT_ARG,
    STRING_ARG,
    BOOL_ARG
};

struct ArgElement {
    int nCmp;
    enum ARG_WHAT what;
    void *out;
    const char **cmp;
    const char *desc;
};

struct ArgTable {
    int nArgs;
    int outNRemainders;
    char **remainders;
    const struct ArgElement *elements;
};

void CS_printArgs(struct ArgTable *argTable);

const char *CS_parseArgs(int argc, char **argv, struct ArgTable *argTable);

#define CS_ARG_DEF(varName,cmpString,helpString) static const char *CMP_##varName[] = cmpString; static const char HELP_##varName[] = helpString;
#define CS_ARG_ELEMENT(varName,varType) { sizeof(CMP_##varName)/sizeof(CMP_##varName[0]), varType, &varName, CMP_##varName, HELP_##varName }
#define CS_ARG_CMP(...) {__VA_ARGS__}
#ifdef __cplusplus
}
#endif

#endif
