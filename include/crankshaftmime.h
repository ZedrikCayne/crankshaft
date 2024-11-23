#ifndef __crankshaftmimedoth__
#define __crankshaftmimedoth__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum CS_MIMETypes {
    CS_MIME_DO_NOT_SET = -1,
    CS_MIME_AAC,
    CS_MIME_APNG,
    CS_MIME_AVI,
    CS_MIME_AZW,
    CS_MIME_BIN,
    CS_MIME_BMP,
    CS_MIME_BZ,
    CS_MIME_BZ2,
    CS_MIME_CSS,
    CS_MIME_GIF,
    CS_MIME_HTM,
    CS_MIME_HTML,
    CS_MIME_ICO,
    CS_MIME_JPG,
    CS_MIME_JPEG,
    CS_MIME_JS,
    CS_MIME_MP3,
    CS_MIME_MP4,
    CS_MIME_OGA,
    CS_MIME_OGV,
    CS_MIME_OGX,
    CS_MIME_OTF,
    CS_MIME_PNG,
    CS_MIME_PDF,
    CS_MIME_RAR,
    CS_MIME_RTF,
    CS_MIME_SVG,
    CS_MIME_TAR,
    CS_MIME_TTF,
    CS_MIME_TXT,
    CS_MIME_WAV,
    CS_MIME_WEBA,
    CS_MIME_WEBM,
    CS_MIME_WEBP,
    CS_MIME_WOFF,
    CS_MIME_WOFF2,
    CS_MIME_XML,
    CS_MIME_FORM_URLENCODED,
    CS_MIME_FORM_MULTIPART,
    MAX_CS_MIME_TYPES
};

int CS_mimeFileExtensionToEnum(const char *extension );
const char *CS_mimeFileExtensionToString(const char *extension);
const char *CS_mimeEnumToString(int mimeEnum);
 
#ifdef __cplusplus
}
#endif
#endif
