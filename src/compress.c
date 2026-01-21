#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <zlib.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/compress.h>
#include <crankshaft/pushpull.h>

// Custom allocators for zlib
static voidpf cs_zalloc(voidpf opaque, uInt items, uInt size) {
    return (voidpf)CS_alloc(items * size);
}

static void cs_zfree(voidpf opaque, voidpf address) {
    CS_free(address);
}

void CS_compressInit(CS_Compress *ctx) {
    if (ctx) {
        memset(ctx, 0, sizeof(CS_Compress));
        ctx->type = CS_COMPRESS_TYPE_NONE;
    }
}

void CS_compressDestroy(CS_Compress *ctx) {
    if (!ctx || !ctx->initialized) return;

    if (ctx->type == CS_COMPRESS_TYPE_ZLIB || ctx->type == CS_COMPRESS_TYPE_GZIP) {
        if (ctx->ctx.zlib) {
            z_stream *strm = (z_stream *)ctx->ctx.zlib;
            if (deflateEnd(strm) == Z_STREAM_ERROR) {
                inflateEnd(strm);
            }
            CS_free(strm);
        }
    }
    ctx->initialized = false;
    ctx->ctx.zlib = NULL;
}

// Helper to setup Zlib stream
static int ensure_zlib_init(CS_Compress *ctx, CS_CompressType type, bool compress) {
    if (ctx->initialized) {
        if (ctx->type != type) {
             CS_LOG_ERROR("Context initialized with type %d but requested %d", ctx->type, type);
             return -1;
        }
        return 0;
    }

    ctx->ctx.zlib = CS_alloc(sizeof(z_stream));
    if (!ctx->ctx.zlib) return -1;
    
    z_stream *strm = (z_stream *)ctx->ctx.zlib;
    memset(strm, 0, sizeof(z_stream));
    strm->zalloc = cs_zalloc;
    strm->zfree = cs_zfree;
    strm->opaque = NULL;

    int ret;
    if (compress) {
        if (type == CS_COMPRESS_TYPE_GZIP) {
            // windowBits + 16 for gzip wrapper
            ret = deflateInit2(strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
        } else {
            ret = deflateInit(strm, Z_DEFAULT_COMPRESSION);
        }
    } else {
        if (type == CS_COMPRESS_TYPE_GZIP) {
             // windowBits + 16 or just 47 (auto detect)
             ret = inflateInit2(strm, 47);
        } else {
            ret = inflateInit(strm);
        }
    }

    if (ret != Z_OK) {
        CS_LOG_ERROR("Zlib init failed: %d", ret);
        CS_free(strm);
        ctx->ctx.zlib = NULL;
        return -1;
    }

    ctx->type = type;
    ctx->initialized = true;
    return 0;
}

static long zlib_process(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out, bool compress, int flush) {
    z_stream *strm = (z_stream *)ctx->ctx.zlib;
    
    strm->next_in = (Bytef *)CS_PP_startOfData(in);
    strm->avail_in = CS_PP_dataSize(in);
    
    strm->next_out = (Bytef *)CS_PP_endOfData(out);
    strm->avail_out = CS_PP_bufferRemaining(out);

    long out_before = strm->total_out;
    long in_before = strm->total_in;

    int ret;
    if (compress) {
        ret = deflate(strm, flush);
    } else {
        ret = inflate(strm, flush);
    }

    if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) { // Z_BUF_ERROR is not always fatal, just means no progress
         CS_LOG_ERROR("Zlib process failed: %d", ret);
         return -1;
    }

    // Update PushPull buffers
    long bytes_read = strm->total_in - in_before;
    long bytes_written = strm->total_out - out_before;

    in->currentWriteOffset += bytes_read;
    if (in->currentWriteOffset == in->currentReadOffset) {
        in->currentWriteOffset = in->currentReadOffset = 0;
    }

    out->currentReadOffset += bytes_written;

    ctx->bytesProcessed += bytes_read;
    ctx->bytesOutput += bytes_written;

    return bytes_written;
}

long CS_compressGzip(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out) {
    if (ensure_zlib_init(ctx, CS_COMPRESS_TYPE_GZIP, true) != 0) return -1;
    int flush = (CS_PP_dataSize(in) == 0) ? Z_FINISH : Z_NO_FLUSH;
    return zlib_process(ctx, in, out, true, flush);
}

long CS_compressGunzip(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out) {
    if (ensure_zlib_init(ctx, CS_COMPRESS_TYPE_GZIP, false) != 0) return -1;
    return zlib_process(ctx, in, out, false, Z_NO_FLUSH);
}

long CS_compressCompress(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out) {
    if (ensure_zlib_init(ctx, CS_COMPRESS_TYPE_ZLIB, true) != 0) return -1;
    int flush = (CS_PP_dataSize(in) == 0) ? Z_FINISH : Z_NO_FLUSH;
    return zlib_process(ctx, in, out, true, flush);
}

long CS_compressInflate(CS_Compress *ctx, struct CS_PushPullBuffer *in, struct CS_PushPullBuffer *out) {
    if (ensure_zlib_init(ctx, CS_COMPRESS_TYPE_ZLIB, false) != 0) return -1;
    return zlib_process(ctx, in, out, false, Z_NO_FLUSH);
}
