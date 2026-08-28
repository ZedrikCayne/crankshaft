#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <crankshaft/commandline.h>
#include <stdint.h>

const char *CS_argsParse(int32_t argc, char **argv, struct CS_ArgTable *argTable) {
    static const char *argErr1 = "Invalid # of arguments";
    static const char *argErr2 = "Invalid argument.";
    argTable->outNRemainders = 0;
    argTable->remainders = NULL;
    for( int32_t i = 1; i < argc; ++i ) {
        char *currentArg = argv[ i ];
        if( *currentArg == '-' ) {
            bool cmp = false;
            for( int32_t j = 0; j < argTable->nArgs && !cmp; ++j ) {
                const struct CS_ArgElement *currentElement = argTable->elements + j;
                for( int32_t k = 0; k < currentElement->nCmp; ++k ) {
                    const char *currentCmp = currentElement->cmp[k];
                    if( strcmp(currentCmp,currentArg) == 0 ) {
                        cmp = true;
                        switch( currentElement->what ) {
                            case CS_INT_ARG:
                                ++i;
                                if( i >= argc ) return argErr1;
                                *((int32_t*)currentElement->out) = atoi(argv[i]);
                                break;
                            case CS_STRING_ARG:
                                ++i;
                                if( i >= argc ) return argErr1;
                                *((char**)currentElement->out) = argv[i];
                                break;
                            case CS_BOOL_ARG:
                                *((bool*)currentElement->out) = true;
                                break;
                        }
                    }
                }
            }
            if( !cmp ) return argErr2;
        } else {
            argTable->outNRemainders = argc - i;
            argTable->remainders = argv + i;
        }
    }
    return NULL;
}


void CS_argsPrint(struct CS_ArgTable *argTable) {
    static const char firstBetween[] = " ";
    static const char restBetween[] = " | ";
    for( int32_t i = 0; i < argTable->nArgs; ++i ) {
        const char * currentBetween = firstBetween;
        const struct CS_ArgElement *currentArg = argTable->elements + i;
        //       12345678901234567890123456789012345678901234567890123456789012345678901234567890
        printf( "  *" );
        for( int32_t j = 0; j < currentArg->nCmp; ++j ) {
            printf( "%s%s", currentBetween, currentArg->cmp[ j ]);
            switch(currentArg->what) {
                case CS_INT_ARG:
                    printf( " <integer>" );
                    break;
                case CS_STRING_ARG:
                    printf( " <string>" );
                    break;
                default:
                    break;
            }
            currentBetween = restBetween;
        }
        printf("\n");
        printf("    %s\n", currentArg->desc);
    }
}
