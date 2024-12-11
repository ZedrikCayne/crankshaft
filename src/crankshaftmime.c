#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"

#include "crankshaftmime.h"

struct ExtensionToMIME {
    char *extension;
    char *filetype;
};

#include "crankshaftmimevalues.h"

int CS_mimeFileExtensionToEnum(const char *extension ) {
    for( int i = 0; i < sizeof(extensions)/sizeof(extensions[0]); ++i ) {
        if( strcmp( extension, extensions[ i ].extension ) == 0 ) {
            return i;
        }
    }
    return CS_MIME_BIN;
}

const char *CS_mimeFileExtensionToString(const char *extension) {
    int mimeEnum = CS_mimeFileExtensionToEnum( extension );
    return extensions[ mimeEnum ].filetype;
}

const char *CS_mimeEnumToString(int mimeEnum) {
    if( mimeEnum < CS_MIME_AAC || mimeEnum >= MAX_CS_MIME_TYPES )
        return extensions[ CS_MIME_BIN ].filetype;
    return extensions[ mimeEnum ].filetype;
}

