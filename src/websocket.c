#include <stdlib.h>
#include <stdio.h>

#include <openssl/sha.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/websocket.h>

#include <crankshaft/tempbuff.h>
#include <crankshaft/tempbuff.h>
#include <crankshaft/server.h>
#include <crankshaft/mime.h>
#include <crankshaft/base64.h>
#include <crankshaft/random.h>

static const char *opcodeToString[] = {
    "CS_WS_OPCODE_CONTINUE",
    "CS_WS_OPCODE_TEXT",
    "CS_WS_OPCODE_BINARY",
    "CS_WS_OPCODE_RESERVED0",
    "CS_WS_OPCODE_RESERVED1",
    "CS_WS_OPCODE_RESERVED2",
    "CS_WS_OPCODE_RESERVED3",
    "CS_WS_OPCODE_RESERVED4",
    "CS_WS_OPCODE_CLOSE",
    "CS_WS_OPCODE_PING",
    "CS_WS_OPCODE_PONG",
    "CS_WS_OPCODE_RESERVED5",
    "CS_WS_OPCODE_RESERVED6",
    "CS_WS_OPCODE_RESERVED7",
    "CS_WS_OPCODE_RESERVED8",
    "CS_WS_OPCODE_RESERVED9"
};

struct CS_WebSocket {
    struct CS_ClientInfo *clientInfo;
    struct CS_SlabAllocator *frameAllocator;
    struct CS_WebSocketFrame *currentIncomingFrame;
    struct CS_WebSocketFrame *currentOutgoingFrame;
    void *applicationData;
};

void *CS_WS_getApplicationData( struct CS_WebSocket *ws ) {
    return ws->applicationData;
}

static bool privateIsWSUpgradeRequest( struct CS_ClientInfo *clientInfo ) {
    const char *connection = CS_serverGetRequestHeader( clientInfo, "Connection" );
    const char *upgrade = CS_serverGetRequestHeader( clientInfo, "Upgrade" );
    //The source buffers on this are at least a few hundred bytes long..
    return ( connection && strncmp( "Upgrade", connection, 32 ) == 0 &&
             upgrade && strncmp( "websocket", upgrade, 32 ) == 0 );
}
//Peek into the request info
bool CS_WS_requestWantsWebsocket( struct CS_ClientInfo *clientInfo ) {
    return privateIsWSUpgradeRequest( clientInfo );
}

static const char *wsAcceptConcat = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
struct CS_WebSocket *CS_WS_create( struct CS_ClientInfo *clientInfo, void *applicationData ) {
    if( !privateIsWSUpgradeRequest( clientInfo ) ) {
        return NULL;
    }
    struct CS_WebSocket *returnValue = CS_allocZero( sizeof( struct CS_WebSocket ) );
    if( !returnValue ) {
        CS_LOG_ERROR("OOM craeting a CS_WebSocket");
        goto ERR_CREATE;
    }
    returnValue->frameAllocator = CS_slabInit( "WS", sizeof( struct CS_WebSocketFrame ), 64, sizeof(void*) );
    if( returnValue->frameAllocator == NULL ) {
        CS_LOG_ERROR("OOM creating frame stack.");
        goto ERR_CREATE;
    }
    returnValue->clientInfo = clientInfo;
    returnValue->applicationData = applicationData;

    struct CS_Reply *reply = CS_serverCreateReply( clientInfo, CS_RESPONSE_101, CS_MIME_DO_NOT_SET, NULL, 0 );
    if( reply ) {
        const char *incomingKey = CS_serverGetRequestHeader( clientInfo, "Sec-WebSocket-Key" );
        CS_serverSetReplyHeader( reply, "Upgrade", "websocket" );
        CS_serverSetReplyHeader( reply, "Connection", "Upgrade" );
        if( incomingKey ) {
            unsigned char outgoingHash[ SHA_DIGEST_LENGTH ];
            const char *combined = CS_tempBuffSnprintf( 128, "%s%s", incomingKey, wsAcceptConcat );
            int length = strlen( combined );
            SHA1( (const unsigned char*)combined, length, outgoingHash );
            const char *encoded = CS_base64EncodeTemp( outgoingHash, SHA_DIGEST_LENGTH, NULL );
            CS_serverSetReplyHeader( reply, "Sec-WebSocket-Accept",  encoded );
        }
        CS_serverDoReply( clientInfo, reply );
    } else {
        goto ERR_CREATE;
    }

    return returnValue;
ERR_CREATE:
    if( returnValue ) {
        if( returnValue->frameAllocator ) CS_slabFree( returnValue->frameAllocator );
        CS_free( returnValue );
    }
    return NULL;
}

struct CS_ClientInfo *CS_WS_destroy( struct CS_WebSocket *ws ) {
    struct CS_ClientInfo *clientInfo = ws->clientInfo;
    CS_slabFree( ws->frameAllocator );
    CS_free( ws );
    return clientInfo;
}

struct CS_WebSocketFrame *CS_WS_createFrame( struct CS_WebSocket *ws, int opcode, bool masked, const void *payload, int payloadSize ) {
    if( payload && payloadSize <= 0 ) {
        CS_LOG_ERROR( "Trying to put a payload in with no payload supplied." );
        return NULL;
    }
    struct CS_WebSocketFrame *frame = CS_WS_getEmptyFrame(ws);
    if( frame ) {
        frame->fin = true;
        frame->opcode = opcode;
        frame->payloadLength = payloadSize;
        if( payloadSize > 0 ) {
            frame->payload = CS_alloc( payloadSize );
            if( frame->payload == NULL ) {
                CS_LOG_ERROR( "OOM trying to allocate a payload for a new frame." );
                CS_WS_returnFrame( ws, frame );
                return NULL;
            }
            if( masked ) {
                frame->mask = true;
                for( int i = 0; i < CS_WS_NUM_MASK_BYTES; ++i ) {
                    frame->maskBytes[i] = CS_rand();
                }
                for( int i = 0; i < payloadSize; ++i ) {
                    ((char*)frame->payload)[ i ] = ((char*)payload)[ i ] ^ frame->maskBytes[ i % CS_WS_NUM_MASK_BYTES ];
                }
            } else {
                memcpy( frame->payload, payload, payloadSize );
            }
        }
    }
    return frame;
}

//Frame management
struct CS_WebSocketFrame *CS_WS_getEmptyFrame( struct CS_WebSocket *ws ){
    if( ws == NULL || ws->frameAllocator == NULL ) return NULL;
    return CS_slabTakeZero( ws->frameAllocator );
}

bool CS_WS_returnFrame( struct CS_WebSocket *ws, struct CS_WebSocketFrame *frame ) {
    if( ws == NULL || ws->frameAllocator || frame == NULL )
        return true;
    if( frame->payload ) CS_free( frame->payload );
    return CS_slabReturn( ws->frameAllocator, frame );
}

/********************************************************************
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-------+-+-------------+-------------------------------+
 * |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 * |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 * |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 * | |1|2|3|       |K|             |                               |
 * +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 * |     Extended payload length continued, if payload len == 127  |
 * + - - - - - - - - - - - - - - - +-------------------------------+
 * |                               |Masking-key, if MASK set to 1  |
 * +-------------------------------+-------------------------------+
 * | Masking-key (continued)       |          Payload Data         |
 * +-------------------------------- - - - - - - - - - - - - - - - +
 * :                     Payload Data continued ...                :
 * + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 * |                     Payload Data continued ...                |
 * +---------------------------------------------------------------+
 *********************************************************************/
//Parse incoming frame off of the open websocket.
#define READ_WS(__WS) ((__WS)->clientInfo->ssl?CS_PP_readFromSSL((__WS)
struct CS_WebSocketFrame *CS_WS_nextIncomingFrame( struct CS_WebSocket *ws ) {
    int bytesRead = CS_serverFillIncomingBuffer( ws->clientInfo );

    if( bytesRead < 0 ) {
        CS_LOG_ERROR( "Error with socket.");
        return NULL;
    }
    if( bytesRead < 4 ) {
        CS_LOG_ERROR( "Not enough bytes read to make up a frame..");
        return NULL;
    }

    struct CS_WebSocketFrame *frame = CS_WS_getEmptyFrame( ws );
    if( !frame ) {
        CS_LOG_ERROR( "OOM creating a frame." );
        return NULL;
    }

    unsigned char *top = (unsigned char*)CS_PP_startOfData( ws->clientInfo->buffer );
    
    unsigned int current = (unsigned int)*top;

    frame->completed = 0;
    frame->fin  = (current & 0x80) != 0;
    frame->rsv1 = (current & 0x40) != 0;
    frame->rsv2 = (current & 0x20) != 0;
    frame->rsv3 = (current & 0x10) != 0;
    frame->opcode = (current & 0x0F);

    current = (unsigned long)*++top;

    frame->mask = (current & 0x80) != 0;
    frame->payloadLength = (current & 0x7F);

    CS_PP_write( ws->clientInfo->buffer, 2 );

    if( frame->payloadLength == 127 ) {
        CS_LOG_ERROR("Incoming frame too big.");
        goto ERROR_READING;
    }

    if( frame->payloadLength == 126 ) {
        current = (unsigned long)*++top;
        frame->payloadLength = (current) << 8;
        current = (unsigned long)*++top;
        frame->payloadLength += current;
        CS_PP_write( ws->clientInfo->buffer, 2 );
    }

    if( frame->mask ) {
        if( CS_PP_writeToBuffer( ws->clientInfo->buffer, frame->maskBytes, CS_WS_NUM_MASK_BYTES ) < CS_WS_NUM_MASK_BYTES ) {
            goto ERROR_READING;
        }
    }

    frame->payload = CS_alloc( frame->payloadLength );
    if( frame->payload == NULL ) {
        CS_LOG_ERROR("OOM for taking in the payload.");
        goto ERROR_READING;
    }

    int numBytesTransferred = 0;

    while( numBytesTransferred < frame->payloadLength ) {
        int numBytesAvailable = CS_PP_dataSize( ws->clientInfo->buffer );
        if( numBytesAvailable > frame->payloadLength ) numBytesAvailable = frame->payloadLength;
        if( frame->mask ) {
            char *current = CS_PP_startOfData( ws->clientInfo->buffer );
            for( int currentXfer = 0; currentXfer < numBytesAvailable; ++currentXfer ) {
                ((char*)frame->payload)[ numBytesTransferred ] = current[ numBytesTransferred ] ^ frame->maskBytes[ numBytesTransferred % CS_WS_NUM_MASK_BYTES ];
                ++numBytesTransferred;
            }
            CS_PP_write( ws->clientInfo->buffer, numBytesAvailable );
        } else {
            numBytesTransferred += CS_PP_writeToBuffer( ws->clientInfo->buffer, (char*)frame->payload + numBytesTransferred, numBytesAvailable );
        }
        //Do we have enough, suck in moreif we don't.
        if( numBytesTransferred < frame->payloadLength ) {
            int numBytesRead = CS_serverFillIncomingBuffer( ws->clientInfo );
            if( numBytesRead < 0 ) {
                CS_LOG_ERROR("Error reading from client socket.");
                goto ERROR_READING;
            }
        }
    }

    return frame;
ERROR_READING:
    if( frame != NULL ) {
        if( frame->payload != NULL ) CS_free( frame->payload );
        CS_WS_returnFrame( ws, frame );
    }
    return NULL;
}

//Push websocket back to client.
bool CS_WS_pushFrame( struct CS_WebSocket *ws, struct CS_WebSocketFrame *frame ) {
    if( ws == NULL || frame == NULL ) {
        return true;
    }
    struct CS_PushPullBuffer *pp = ws->clientInfo->output;

    unsigned char temp = 0;

    if( CS_PP_bufferRemaining( pp ) < 32 ) {
        if( CS_serverWriteOutputBuffer( ws->clientInfo ) < 0 ) {
            CS_LOG_ERROR("Websocket write error.");
            return true; 
        }
        if( CS_PP_bufferRemaining( pp ) < 32 ) {
            CS_LOG_ERROR("Websocket needs more space to write, far side not reading fast enough.");
            return true;
        }
    }

    if( frame->fin ) temp = 0x80;
    temp |= (frame->opcode & 0x0F);

    CS_PP_readFromBuffer( pp, &temp, 1 );

    if( frame->mask ) temp = 0x80; else temp = 0;
    if( frame->payloadLength < 125 ) {
        temp |= frame->payloadLength & 0x7F;
    } else {
        if( frame->payloadLength <= 0x0000FFFF ) {
            temp |= 126;
        } else {
            temp |= 127;
        }
    }
    CS_PP_readFromBuffer( pp, &temp, 1 );

    if( frame->payloadLength > 0x0000FFFF ) {
        unsigned char fourBytes[] = {0,0,0,0};
        //We're assuming we'll never be bigger than 2 gigs here...
        CS_PP_readFromBuffer( pp, fourBytes, 4 );
        temp = ( frame->payloadLength & 0xFF000000 ) >> 24;
        CS_PP_readFromBuffer( pp, &temp, 1 );
        temp = ( frame->payloadLength & 0x00FF0000 ) >> 16;
        CS_PP_readFromBuffer( pp, &temp, 1 );
    }

    if( frame->payloadLength > 126 ) {
        temp = ( frame->payloadLength & 0x0000FF00 ) >> 8;
        CS_PP_readFromBuffer( pp, &temp, 1 );
        temp = ( frame->payloadLength & 0x000000FF );
        CS_PP_readFromBuffer( pp, &temp, 1 );
    }

    if( frame->mask ) {
        CS_PP_readFromBuffer( pp, frame->maskBytes, 4 );
    }
    
    int bytesTotallyTransferred = 0;

    while( bytesTotallyTransferred < frame->payloadLength ) {
        int lastTransfer = 
            CS_PP_readFromBuffer( pp,
                         ((char*)frame->payload) + bytesTotallyTransferred,
                         frame->payloadLength - bytesTotallyTransferred );
        if( lastTransfer < 0 ) {
            CS_LOG_ERROR("Websocket failed to push data to the output buffer.");
            return true;
        }
        int lastWriteToSocket = CS_serverWriteOutputBuffer( ws->clientInfo );
        if( lastWriteToSocket < 0 ) {
            CS_LOG_ERROR("Websocket write failed.");
            return true;
        }
        bytesTotallyTransferred += lastTransfer;
    }

    CS_WS_returnFrame( ws, frame );

    return false;
}

const char *nullFrameError = "NULL FRAME";

const char *CS_WS_describeFrame( struct CS_WebSocketFrame *frame ) {
    if( frame == NULL ) {
        return nullFrameError;
    }
    return CS_tempBuffSnprintf( 128, "Opcode: %s Fin: %d DataSize: %d", opcodeToString[ frame->opcode ], frame->fin?1:0, frame->payloadLength );
}

