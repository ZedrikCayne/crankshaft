#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshaftslaballoc.h"
#include "crankshafttempbuff.h"

#include "crankshaftuuid.h"
#include "crankshaftrandom.h"

static pthread_mutex_t uuidMutex = PTHREAD_MUTEX_INITIALIZER;
struct CS_LCG_rand_state uuidRand = {0};
static void *voidSlabAllocator = NULL;

static char *hexDigit = "0123456789abcdef";
static void sixteenBytesToText( const struct CS_UUID *uuid, char *output ) {
    const unsigned char *in = uuid->uuid;
    char *out = output;
    for( int i = 0; i < 16; ++i ) {
        switch(i) {
            case 4:
            case 6:
            case 8:
            case 10:
                *out++ = '-';
            default:
                *out++ = hexDigit[ (*in & 0xF0) >> 4 ];
                *out++ = hexDigit[ (*in++ & 0x0F) ];
                break;
        }
    }
    *out = 0;
}

static int hexDigitToInt( const char *u ) {
    if( *u < '0' ) return -1;
    if( *u > 'f' ) return -1;
    if( *u <= '9' ) return (int)( *u - '0' );
    if( *u >= 'a' ) return (int)( *u - 'a' ) + 10;
    if( *u > 'F' ) return -1;
    if( *u >= 'A' ) return (int)( *u - 'A' ) + 10;
    return -1;
}

static bool textToSixteenBytes( const char *inString, struct CS_UUID *uuid ) {
    const char *in = inString;
    unsigned char *out = (unsigned char *)uuid->uuid;
    int nextNibble;
    int whichByte = 0;
    int byteAccumulator = 0;
    for( int i = 0; i < UUID_CHAR_SIZE_BYTES - 1; ++i ) {
        switch(i) {
            case 8:
            case 13:
            case 18:
            case 23:
                if( *in != '-' ) {
                    CS_LOG_TRACE("UUID at position %d not a -: %s", in - inString, inString);
                    return true;
                }
                ++in;
                break;
            default:
                nextNibble = hexDigitToInt( in );
                if( nextNibble < 0 ) {
                    CS_LOG_ERROR("UUID at position %d not a hex digit: %c %s", in - inString, *in, inString);
                    return true;
                }
                if( whichByte ) {
                    *out++ = byteAccumulator + nextNibble;
                } else {
                    byteAccumulator = nextNibble << 4;
                }
                whichByte = 1-whichByte;
                ++in;
                break;
        }
    }
    return false;
}

#define GRAB_MUTEX() pthread_mutex_lock(&uuidMutex);
#define RELEASE_MUTEX() pthread_mutex_unlock(&uuidMutex)

void setSixteenBytesUuid4( struct CS_UUID *uuid ) {
    int *sixteenBytes = (int*)uuid->uuid;
    GRAB_MUTEX();
    sixteenBytes[0] = CS_LCG_rand( &uuidRand );
    sixteenBytes[1] = CS_LCG_rand( &uuidRand );
    sixteenBytes[2] = CS_LCG_rand( &uuidRand );
    sixteenBytes[3] = CS_LCG_rand( &uuidRand );
    RELEASE_MUTEX();
    unsigned char *in = (unsigned char *)sixteenBytes;
    in[ 6 ] = (in[ 6 ] & 0x0F) | 0x40;
    in[ 8 ] = (in[ 8 ] & 0xBF) | 0x80;
}

void CS_uuidInit() {
    voidSlabAllocator = CS_slabInit( "UUID", sizeof(struct CS_UUID), 200, 4 ); 
    if( uuidRand.seed == 0 ) CS_uuidSetSeed(0);
}

void CS_uuidSetSeed( int seed ) {
    GRAB_MUTEX();
    CS_LCG_rand_init(&uuidRand,seed?seed:time(NULL));
    RELEASE_MUTEX();
}

void CS_uuidKill() {
    if( voidSlabAllocator ) CS_slabFree( voidSlabAllocator );
    voidSlabAllocator = NULL;
}

const struct CS_UUID *CS_uuid4() {
    struct CS_UUID *returnUuid = CS_slabTake( voidSlabAllocator );
    if( returnUuid ) {
        setSixteenBytesUuid4( returnUuid );
    }
    return returnUuid;
}

void CS_uuidFree(const struct CS_UUID *uuid) {
    CS_slabReturn( voidSlabAllocator, (void*)uuid );
}

const struct CS_UUID *CS_uuid4Temp() {
    struct CS_UUID *returnUuid = CS_tempBuff( sizeof(struct CS_UUID) );
    if( returnUuid ) {
        setSixteenBytesUuid4( returnUuid );
    }
    return returnUuid;
}

const char *CS_uuid4String() {
    struct CS_UUID uuid;
    char *returnValue = CS_alloc(UUID_CHAR_SIZE_BYTES);
    if( returnValue ) {
        setSixteenBytesUuid4( &uuid );
        sixteenBytesToText( &uuid, returnValue );
    }
    return returnValue;
}

const char *CS_uuid4StringTemp(void) {
    struct CS_UUID uuid;
    char *returnValue = CS_tempBuff(UUID_CHAR_SIZE_BYTES);
    if( returnValue ) {
        setSixteenBytesUuid4( &uuid );
        sixteenBytesToText( &uuid, returnValue );
    }
    return returnValue;
}

const char *CS_uuid4StringOut(char *out, int length) {
    struct CS_UUID uuid;
    if( length < UUID_CHAR_SIZE_BYTES ) {
        return NULL;
    }
    setSixteenBytesUuid4( &uuid );
    sixteenBytesToText( &uuid, out );
    return out;
}

const char *CS_uuidToString(const struct CS_UUID *uuid) {
    char *returnValue = CS_alloc( UUID_CHAR_SIZE_BYTES );
    if( returnValue ) sixteenBytesToText( uuid, returnValue );
    return returnValue;
}

const char *CS_uuidToStringTemp(const struct CS_UUID *uuid) {
    char *returnValue = CS_tempBuff( UUID_CHAR_SIZE_BYTES );
    if( returnValue ) sixteenBytesToText( uuid, returnValue );
    return returnValue;
}

const struct CS_UUID *CS_uuidFromString(char *in, int length) {
    if( length < UUID_CHAR_SIZE_BYTES ) return NULL;
    struct CS_UUID *returnValue = CS_slabTake(voidSlabAllocator);
    if( returnValue ) {
        if( textToSixteenBytes( in, returnValue ) ) {
            CS_slabReturn(voidSlabAllocator, returnValue);
            return NULL;
        }
    }
    return returnValue;
}

const struct CS_UUID *CS_uuidFromStringTemp(char *in, int length) {
    if( length < UUID_CHAR_SIZE_BYTES ) return NULL;
    struct CS_UUID *returnValue = CS_tempBuff( sizeof(struct CS_UUID) );
    if( returnValue && textToSixteenBytes( in, returnValue ) ) return NULL;
    return returnValue;
}

const char *CS_uuidToStringOut(const struct CS_UUID *uuid, char *out, int outLength) {
    if( outLength < UUID_CHAR_SIZE_BYTES ) return NULL;
    sixteenBytesToText( uuid, out );
    return out;
}

void CS_uuidCopy(struct CS_UUID *dest, const struct CS_UUID *src) {
    memcpy(dest, src, sizeof(struct CS_UUID) );
}
