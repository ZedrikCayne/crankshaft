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

struct ExtensionToMIME extensions[] = {
    {"aac", "audio/aac"},
    {"apng", "image/apng"},
    {"avi", "video/x-msvideo"},
    {"azw", "applicatoin/vnd.amazon.ebook"},
    {"bin", "application/octet-stream"},
    {"bmp", "image/bmp"},
    {"bz", "application/x-bzip"},
    {"bz2", "application/x-bzip2"},
    {"css", "text/css"},
    {"gif", "image/gif"},
    {"htm", "text/html"},
    {"html", "text/html"},
    {"ico", "image/vnd.microsoft.icon"},
    {"jpg", "image/jpg"},
    {"jpeg", "image/jpeg"},
    {"js", "text/javascript"},
    {"mp3", "audio/mpeg"},
    {"mp4", "video/mp4"},
    {"oga", "audio/ogg"},
    {"ogv", "video/ogg"},
    {"ogx", "application/ogg"},
    {"otf", "font/otf"},
    {"png", "image/png"},
    {"pdf", "application/pdf"},
    {"rar", "application/vnd.rar"},
    {"rtf", "application/rtf"},
    {"svg", "image/svg+xml"},
    {"tar", "application/x-tar"},
    {"ttf", "font/ttf"},
    {"txt", "text/plain"},
    {"wav", "audio/wav"},
    {"weba", "audio/webm"},
    {"webm", "video/webm"},
    {"webp", "image/webp"},
    {"woff", "font/woff"},
    {"woff2", "font/woff2"},
    {"xml", "applicatoin/xml"},
    {"xxx", "application/x-www-form-urlencoded" },
    {"xxy", "multipart/form-data" }
};

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

