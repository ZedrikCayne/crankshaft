#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/util.h>

#include <crankshaft/mime.h>

struct ExtensionToMIME {
    char *extension;
    char *filetype;
};

#include <crankshaft/mimevalues.h>
#include <stdint.h>

int32_t CS_mimeFileExtensionToEnum(const char *extension ) {
    for( int32_t i = 0; i < CS_ARRAY_SIZE(extensions); ++i ) {
        if( strcmp( extension, extensions[ i ].extension ) == 0 ) {
            return i;
        }
    }
    return CS_MIME_BIN;
}

const char *CS_mimeFileExtensionToString(const char *extension) {
    int32_t mimeEnum = CS_mimeFileExtensionToEnum( extension );
    return extensions[ mimeEnum ].filetype;
}

const char *CS_mimeEnumToString(int32_t mimeEnum) {
    if( mimeEnum < CS_MIME_AAC || mimeEnum >= MAX_CS_MIME_TYPES )
        return extensions[ CS_MIME_BIN ].filetype;
    return extensions[ mimeEnum ].filetype;
}

