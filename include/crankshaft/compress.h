#ifndef __crankshaftcompressdoth__
#define __crankshaftcompressdoth__
#include <stdbool.h>
#include <crankshaft/pushpull.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CS_COMPRESS_TYPE_NONE = 0,
    CS_COMPRESS_TYPE_ZLIB,
    CS_COMPRESS_TYPE_GZIP,
    CS_COMPRESS_TYPE_BZIP2
} CS_CompressType;

typedef struct CS_Compress {
    CS_CompressType type;
    union {
        void *zlib;  // z_stream*
        void *bzip2; // bz_stream*
    } ctx;
    unsigned long bytesProcessed;
    unsigned long bytesOutput;
    bool initialized;
} CS_Compress;

void CS_compressInit(CS_Compress *ctx);
void CS_compressDestroy(CS_Compress *ctx);

long CS_compressBzip(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out);
long CS_compressBunzip(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out);

long CS_compressGzip(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out);
long CS_compressGunzip(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out);

long CS_compressCompress(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out);
long CS_compressInflate(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out);

#ifdef __cplusplus
}
#endif
#endif