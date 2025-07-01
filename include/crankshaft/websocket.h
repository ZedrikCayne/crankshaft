#ifndef __crankshaftwebsocketdoth__
#define __crankshaftwebsocketdoth__
#include <stdbool.h>

#include <crankshaft/server.h>

/********************************************************************
 *
 * Handling of websockets, based on the machinery of CS_WebServer
 *
 * 
 *
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

enum CS_WebSocketFrameOpcode {
    CS_WS_OPCODE_CONTINUE  = 0x00,
    CS_WS_OPCODE_TEXT      = 0x01,
    CS_WS_OPCODE_BINARY    = 0x02,
    CS_WS_OPCODE_RESERVED0 = 0x03,
    CS_WS_OPCODE_RESERVED1 = 0x04,
    CS_WS_OPCODE_RESERVED2 = 0x05,
    CS_WS_OPCODE_RESERVED3 = 0x06,
    CS_WS_OPCODE_RESERVED4 = 0x07,
    CS_WS_OPCODE_CLOSE     = 0x08,
    CS_WS_OPCODE_PING      = 0x09,
    CS_WS_OPCODE_PONG      = 0x0A,
    CS_WS_OPCODE_RESERVED5 = 0x0B,
    CS_WS_OPCODE_RESERVED6 = 0x0C,
    CS_WS_OPCODE_RESERVED7 = 0x0D,
    CS_WS_OPCODE_RESERVED8 = 0x0E,
    CS_WS_OPCODE_RESERVED9 = 0x0F
};

enum CS_WebSocketClose {
    CS_WS_CLOSE_NORMAL           = 1000,
    CS_WS_CLOSE_GOING_AWAY       = 1001,
    CS_WS_CLOSE_PROTOCOL_ERROR   = 1002,
    CS_WS_CLOSE_BAD_DATA         = 1003,
    CS_WS_CLOSE_RESERVED         = 1004,
    CS_WS_CLOSE_RESERVED_NO_USE  = 1005,
    CS_WS_CLOSE_RESERVED_NO_USE2 = 1006,
    CS_WS_CLOSE_INCONSISTED_DATA = 1007,
    CS_WS_CLOSE_POLICY_VIOLATION = 1008,
    CS_WS_CLOSE_TOO_BIG          = 1009,
    CS_WS_CLOSE_NEED_EXTENSION   = 1010,
    CS_WS_CLOSE_SERVER_ERROR     = 1011,
    CS_WS_CLOSE_RESERVED_NO_USE3 = 1015
};

#define CS_WS_NUM_MASK_BYTES 4

struct CS_WebSocketFrame {
    bool completed;
    bool fin;
    bool rsv1;
    bool rsv2;
    bool rsv3;
    int  opcode;
    bool mask;
    int payloadLength;
    void *payload; 
    void *extensionData;
    void *applicationData;
    unsigned char maskBytes[CS_WS_NUM_MASK_BYTES];
    struct CS_WebSocketFrame *next;
};

struct CS_WebSocket;
//Peek into the request info
bool CS_WS_requestWantsWebsocket( struct CS_ClientInfo *clientInfo );
struct CS_WebSocket *CS_WS_create( struct CS_ClientInfo *clientInfo, void *applicationData );
struct CS_ClientInfo *CS_WS_destroy( struct CS_WebSocket *ws );
void *CS_WS_getApplicationData( struct CS_WebSocket *ws );
void CS_WS_close( struct CS_WebSocket *ws, int closeCode );

//Frame management
struct CS_WebSocketFrame *CS_WS_createFrame( struct CS_WebSocket *ws, int opcode, bool masked, const void *payload, int payloadSize );
struct CS_WebSocketFrame *CS_WS_getEmptyFrame( struct CS_WebSocket *ws );
bool CS_WS_returnFrame( struct CS_WebSocket *ws, struct CS_WebSocketFrame *frame );

//Parse incoming frame off of the open websocket.
struct CS_WebSocketFrame *CS_WS_nextIncomingFrame( struct CS_WebSocket *ws );
//Push websocket back to client.
bool CS_WS_pushFrame( struct CS_WebSocket *ws, struct CS_WebSocketFrame *frame );

const char *CS_WS_describeFrame( struct CS_WebSocketFrame *frame );

#ifdef __cplusplus
}
#endif
#endif
