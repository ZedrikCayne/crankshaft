#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/pushpull.h>
#include <crankshaft/compress.h>

#ifdef __has_include
#if __has_include(<bzlib.h>)
#define HAS_BZIP2 1
#endif
#endif

extern bool test_compress(void);

static int testCount = 0;
static int testSucceeded = 0;

typedef long (*CompressFunc)(CS_Compress*, struct CS_PushPullBuffer*, struct CS_PushPullBuffer*);

static bool check_roundtrip(const char *algoName, 
                            CompressFunc compressFunc,
                            CompressFunc decompressFunc,
                            CS_CompressType type) {
    int inputLen = 1024;
    unsigned char *inputData = CS_alloc(inputLen);
    for( int i = 0; i < inputLen; ++i ) {
        inputData[i] = (unsigned char)CS_testRandMax(255);
    }

    struct CS_PushPullBuffer *in = CS_PP_defaultAlloc(1024);
    struct CS_PushPullBuffer *mid = CS_PP_defaultAlloc(2048);
    struct CS_PushPullBuffer *out = CS_PP_defaultAlloc(1024);

    // Put data into 'in' buffer (Fill it)
    CS_PP_readFromBuffer(in, inputData, inputLen);
    
    // Compress
    CS_Compress ctx;
    CS_compressInit(&ctx);
    
    long ret;
    long totalCompressed = 0;
    
    // Loop until input is consumed
    while(CS_PP_dataSize(in) > 0) {
        ret = compressFunc(&ctx, in, mid);
        if (ret < 0) {
            CS_FAIL_ON_TRUE(true, algoName, "Compression failed");
            goto cleanup;
        }
        totalCompressed += ret;
    }
    
    // Flush
    ret = compressFunc(&ctx, in, mid); 
    if (ret < 0) {
         CS_FAIL_ON_TRUE(true, algoName, "Compression flush failed");
         goto cleanup;
    }
    totalCompressed += ret;

    CS_compressDestroy(&ctx);

    if (CS_PP_dataSize(mid) == 0) {
         CS_FAIL_ON_TRUE(true, algoName, "Compressed data size is 0");
         goto cleanup;
    }

    // Decompress
    CS_compressInit(&ctx);
    
    long totalDecompressed = 0;
    while(CS_PP_dataSize(mid) > 0) {
        ret = decompressFunc(&ctx, mid, out);
        if (ret < 0) {
             CS_FAIL_ON_TRUE(true, algoName, "Decompression failed");
             goto cleanup;
        }
        totalDecompressed += ret;
    }

    // Flush decompress?
    ret = decompressFunc(&ctx, mid, out);
    if (ret < 0) {
         CS_FAIL_ON_TRUE(true, algoName, "Decompression flush failed");
         goto cleanup;
    }
    totalDecompressed += ret;
    
    CS_compressDestroy(&ctx);

    // Verify
    if (CS_PP_dataSize(out) != inputLen) {
        CS_FAIL_ON_TRUE(true, algoName, "Decompressed size mismatch");
        goto cleanup;
    }

    char *outputData = CS_alloc(inputLen);
    // Read from out buffer (Consume it)
    CS_PP_writeToBuffer(out, outputData, inputLen);

    if (memcmp(inputData, outputData, inputLen) != 0) {
        CS_FAIL_ON_TRUE(true, algoName, "Decompressed data mismatch");
    } else {
        CS_LOG_OK(algoName);
    }
    CS_free(outputData);

cleanup:
    CS_PP_defaultFree(in);
    CS_PP_defaultFree(mid);
    CS_PP_defaultFree(out);
    return true;
}

bool test_compress(void) {
    check_roundtrip("GZIP", CS_compressGzip, CS_compressGunzip, CS_COMPRESS_TYPE_GZIP);
    check_roundtrip("ZLIB", CS_compressCompress, CS_compressInflate, CS_COMPRESS_TYPE_ZLIB);
    
    return testCount != testSucceeded;
}
