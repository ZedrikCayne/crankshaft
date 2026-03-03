#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/http.h>
#include <crankshaft/server.h>
#include <crankshaft/mime.h>
#include <crankshaft/compress.h>
#include <crankshaft/pushpull.h>

extern bool test_compression_server(void);

static int testCount = 0;
static int testSucceeded = 0;

static bool large_response_handler(struct CS_ClientInfo *info) {
    // Create a large text response that is highly compressible
    int size = 10000;
    char *data = CS_alloc(size);
    memset(data, 'A', size);
    struct CS_Reply *reply = CS_serverCreateReply(info, CS_RESPONSE_200, CS_MIME_TXT, data, size);
    bool result = CS_serverDoReply(info, reply);
    CS_free(data);
    return result;
}

static struct CS_Route compressionRoutes[] = {
    { CS_HTTP_METHOD_GET, CS_ROUTE_TYPE_EXACT, 0, "/large", large_response_handler }
};

bool test_compression_server(void) {
    struct CS_WebServer *server = CS_serverStart(0, NULL, NULL, NULL, NULL, NULL, 0, compressionRoutes, 1);
    CS_FAIL_ON_NULL(server, "Start compression test server", "Failed");

    char url[128];
    snprintf(url, sizeof(url), "http://localhost:%d/large", server->serverPort);

    // 1. Request without Accept-Encoding
    struct CS_RequestReply *reply = CS_httpMakeRequest(CS_HTTP_METHOD_GET, url, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL);
    CS_FAIL_ON_NULL(reply, "Request without compression", "Failed");
    if (reply) {
        const char *ce = CS_httpReplyHeader(reply, "Content-Encoding");
        CS_FAIL_ON_NOT_NULL(ce, "Content-Encoding should be NULL", "Got %s", ce);
        CS_FAIL_ON_FALSE(CS_PP_dataSize(reply->buffer) == 10000, "Should be original size", "Got %d", CS_PP_dataSize(reply->buffer));
        CS_httpCloseRequest(reply);
    }

    // 2. Request with Accept-Encoding: gzip
    struct CS_RequestHeader headers[] = {
        {"Accept-Encoding", "gzip"}
    };
    reply = CS_httpMakeRequest(CS_HTTP_METHOD_GET, url, headers, 1, NULL, 0, NULL, 0, NULL, 0, NULL);
    CS_FAIL_ON_NULL(reply, "Request with compression", "Failed");
    if (reply) {
        const char *ce = CS_httpReplyHeader(reply, "Content-Encoding");
        CS_FAIL_ON_NULL(ce, "Content-Encoding should be gzip", "Got NULL");
        if (ce) {
            CS_FAIL_ON_FALSE(strcmp(ce, "gzip") == 0, "Content-Encoding should be gzip", "Got %s", ce);
        }
        
        int compressedSize = CS_PP_dataSize(reply->buffer);
        CS_LOG_INFO("Original size: 10000, Compressed size: %d", compressedSize);
        CS_FAIL_ON_FALSE(compressedSize < 10000, "Compressed size should be smaller", "Got %d", compressedSize);

        // Decompress to verify
        CS_Compress ctx;
        CS_compressInit(&ctx);
        struct CS_PushPullBuffer *out = CS_PP_defaultAlloc(10000);
        long ret = CS_compressGunzip(&ctx, reply->buffer, out);
        if (ret >= 0) {
            ret = CS_compressGunzip(&ctx, reply->buffer, out); // Flush
        }
        
        CS_FAIL_ON_TRUE(ret < 0, "Decompression should succeed", "Failed");
        CS_FAIL_ON_FALSE(CS_PP_dataSize(out) == 10000, "Decompressed size mismatch", "Got %d", CS_PP_dataSize(out));
        
        char *outData = CS_PP_startOfData(out);
        bool match = true;
        for(int i = 0; i < 10000; ++i) {
            if (outData[i] != 'A') {
                match = false;
                break;
            }
        }
        CS_FAIL_ON_FALSE(match, "Decompressed data mismatch", "Data corrupted");

        CS_PP_defaultFree(out);
        CS_compressDestroy(&ctx);
        CS_httpCloseRequest(reply);
    }

    CS_serverKill(server);
    CS_httpCleanupReplies();

    return testCount != testSucceeded;
}
