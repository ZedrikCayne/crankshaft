#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"

#include "crankshafttempbuff.h"
#include "crankshaftstringbuilder.h"
#include "crankshaftbase64.h"
#include "crankshaftlinearalloc.h"

static char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static unsigned char decoding_table[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int requiredSpaceToEncode( int inputBufferSize ) {
    return 4 * ((inputBufferSize + 2)/3);
}

static int requiredSpaceToDecode( int inputBufferSize ) {
    return inputBufferSize / 4 * 3;
}

static int privateEncode( const void *toEncode, int encodeLength, void *encodeBuffer, int encodeBufferLength ) {
    int outputLength = requiredSpaceToEncode( encodeLength );
    if( outputLength > encodeBufferLength ) return -1;

    char *output = encodeBuffer;
    const unsigned char *input = (const unsigned char *)toEncode;
    const unsigned char *end = input + encodeLength;
    char *outputEnd = output + outputLength;

    unsigned int threeBytes;
    while( input < end - 3 ) {
        threeBytes = *input++ << 16;
        threeBytes += *input++ << 8;
        threeBytes += (*input++);
        *output++ = encoding_table[ (threeBytes >> 18) ];
        *output++ = encoding_table[ (threeBytes >> 12) & 0x3F ];
        *output++ = encoding_table[ (threeBytes >>  6) & 0x3F ];
        *output++ = encoding_table[ (threeBytes      ) & 0x3F ];
    }
    if( input < end ) threeBytes = *input++;
    threeBytes <<= 8;
    if( input < end ) threeBytes += *input++;
    threeBytes <<= 8;
    if( input < end ) threeBytes += *input++;
    *output++ = encoding_table[ (threeBytes >> 18) ];
    *output++ = encoding_table[ (threeBytes >> 12) & 0x3F ];
    *output++ = encoding_table[ (threeBytes >>  6) & 0x3F ];
    *output++ = encoding_table[ (threeBytes      ) & 0x3F ];
    while( output < outputEnd ) *output++ = '=';

    return outputLength;
}

static int privateDecode( void *toDecode, int decodeLength, void *decodedBuffer, int decodeBufferLength ) {
    char *output = decodedBuffer;
    unsigned char *input = (unsigned char *)toDecode;
    unsigned char *end = input + decodeLength;
    int outputLength = (decodeLength / 4) * 3;

    //if (input[decodeLength - 1] == '=') (outputLength)--;
    //if (input[decodeLength - 2] == '=') (outputLength)--;

    if( decodeBufferLength < outputLength ) {
        return -1;
    }
    
    //if( decodeLength % 4 ) return -1;
    unsigned int threeBytes;
    while( input < end - 4 ) {
        threeBytes  = decoding_table[ *input++ ] << 18;
        threeBytes += decoding_table[ *input++ ] << 12;
        threeBytes += decoding_table[ *input++ ] <<  6;
        threeBytes += decoding_table[ *input++ ];
        *output++ = ( threeBytes >> 16 );
        *output++ = ( threeBytes >> 8  ) & 0xFF;
        *output++ = ( threeBytes       ) & 0xFF;
    }
    threeBytes = 0;
    threeBytes = decoding_table[ *input++ ] << 18;
    threeBytes += decoding_table[ *input++ ] << 12;
    *output++ = ( threeBytes >> 16 );
    if( *input != '=' && input < end ) {
        threeBytes += decoding_table[ *input++ ] << 6;
        *output++ = ( threeBytes >> 8  ) & 0xFF;
        if( *input != '=' && input < end ) {
            threeBytes += decoding_table[ *input++ ];
            *output++ = ( threeBytes       ) & 0xFF;
        }
    }
    return output - (char*)decodedBuffer;
}

char *CS_base64Encode( void *toEncode, int length, int *outputLength ) {
    int requiredSpace = requiredSpaceToEncode( length );
    char *output = CS_alloc( requiredSpace + 1 );
    if( output == NULL ) return NULL;
    int outLength = privateEncode( toEncode, length, output, requiredSpace );
    if( outLength < 0 ) {
        CS_free(output);
        return NULL;
    }
    if( outputLength ) *outputLength = outLength;
    return output;
}
char *CS_base64EncodeTemp( void *toEncode, int length, int *outputLength ) {
    int requiredSpace = requiredSpaceToEncode(length);
    char *output = CS_tempBuff(requiredSpace + 1);
    if( output == NULL ) return NULL;
    int outLength = privateEncode( toEncode, length, output, requiredSpace );
    if( outLength < 0 ) return NULL;
    output[ requiredSpace ] = 0;
    if( outputLength ) *outputLength = outLength;
    return output;
}
struct CS_StringBuilder *CS_base64EncodeAppend( const void *toEncode, int length,  struct CS_StringBuilder *appendTo ) {
    int requiredSpace = requiredSpaceToEncode(length);
    if( CS_SB_expandBy( appendTo, requiredSpace ) ) return NULL;
    char *output = CS_SB_writePosition( appendTo );
    int outLength = privateEncode( (void*)toEncode, length, output, requiredSpace );
    if( outLength < 0 ) { *output = 0; return NULL; }
    CS_SB_fakeAppend( appendTo, outLength );
    return appendTo;
}

void *CS_base64Decode( const char *toDecode, int length, int *outputLength ) {
    int required = requiredSpaceToDecode(length);
    void *output = CS_tempBuff(required);
    if( output == NULL ) return NULL;
    int outLength = privateDecode( (void*)toDecode, length, output, required);
    if( outLength < 0 ) return NULL;
    if( outputLength ) *outputLength = outLength;
    return output;
}
void *CS_base64DecodeTemp( const char *toDecode, int length, int *outputLength ) {
    int required = requiredSpaceToDecode(length);
    void *output = CS_tempBuff(required);
    if( output == NULL ) return NULL;
    int outLength = privateDecode( (void*)toDecode, length, output, required);
    if( outLength < 0 ) return NULL;
    if( outputLength ) *outputLength = outLength;
    return output;
}
void *CS_base64DecodeInPlace( char *toDecode, int length, int *outputLength ) {
    int required = requiredSpaceToDecode( length );
    int outLength = privateDecode( toDecode, length, toDecode, required );
    if( outLength < 0 ) return NULL;
    if( outputLength ) *outputLength = outLength;
    return toDecode;
}
void *CS_base64DecodeLinearAlloc( const char *toDecode, int length, int *outputLength, void *linearAllocator ) {
    int required = requiredSpaceToDecode( length );
    void *output = CS_linearTake( linearAllocator, length, sizeof(void*) );
    if( output == NULL ) return NULL;
    int outLength = privateDecode( (void*)toDecode, length, output, required );
    if( outLength < 0 ) return NULL;
    if( outputLength ) *outputLength = outLength;
    return output;
}

