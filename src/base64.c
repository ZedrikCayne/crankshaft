#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/tempbuff.h>
#include <crankshaft/stringbuilder.h>
#include <crankshaft/base64.h>
#include <crankshaft/linearalloc.h>

static char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static unsigned char decoding_table[] = {
   //NUL   SOH   STX   ETX   EOT   ENQ   ACK   BEL
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   // BS   TAB    LF    VT    FF    CR    SO    SI
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //DLE   DC1   DC2   DC3   DC4   NAK   SYN   ETB
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //CAN    EM   SUB   ESC    FS    GS    RS    US
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //' '     !     "     #     $     %     &     '
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //  (     )     *     +     ,     -     .     /
    0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x3f,
   //  0     1     2     3     4     5     6     7
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
   //  8     9     :     ;     <     =     >     ?
    0x3c, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //  @     A     B     C     D     E     F     G
    0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
   //  H     I     J     K     L     M     N     O
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
   //  P     Q     R     S     T     U     V     W
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
   //  X     Y     Z     [     \     /     ^     _
    0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
   //  `     a     b     c     d     e     f     g
    0x00, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
   //  h     i     j     k     l     m     n     o
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
   //  p     q     r     s     t     u     v     w
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
   //  x     y     z     {     |     }     ~   DEL
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
    int endingEquals = 0;
    if( outputLength > encodeBufferLength ) return -1;
    switch( encodeLength % 3 ) {
        case 0:
            break;
        case 1:
            endingEquals = 2;
            break;
        case 2:
            endingEquals = 1;
            break;
    }

    char *output = encodeBuffer;
    const unsigned char *input = (const unsigned char *)toEncode;
    const unsigned char *end = input + encodeLength;

    unsigned int threeBytes = 0;
    while( input < end - 3 ) {
        threeBytes = *input++ << 16;
        threeBytes += *input++ << 8;
        threeBytes += (*input++);
        *output++ = encoding_table[ (threeBytes >> 18) ];
        *output++ = encoding_table[ (threeBytes >> 12) & 0x3F ];
        *output++ = encoding_table[ (threeBytes >>  6) & 0x3F ];
        *output++ = encoding_table[ (threeBytes      ) & 0x3F ];
    }
    threeBytes = 0;
    if( input < end ) threeBytes = *input++;
    threeBytes <<= 8;
    if( input < end ) threeBytes += *input++;
    threeBytes <<= 8;
    if( input < end ) threeBytes += *input++;
    *output++ = encoding_table[ (threeBytes >> 18) ];
    *output++ = encoding_table[ (threeBytes >> 12) & 0x3F ];
    *output++ = endingEquals == 2?'=':encoding_table[ (threeBytes >>  6) & 0x3F ];
    *output++ = endingEquals >= 1?'=':encoding_table[ (threeBytes      ) & 0x3F ];
/*    for( int i = 0; i < endingEquals; ++i ) {
        *--output = '=';
    } */

    return outputLength;
}

static int privateDecode( void *toDecode, int decodeLength, void *decodedBuffer, int decodeBufferLength ) {
    char *output = decodedBuffer;
    unsigned char *input = (unsigned char *)toDecode;
    unsigned char *end = input + decodeLength;
    int outputLength = (decodeLength / 4) * 3;

    if( decodeLength % 4 ) {
        CS_LOG_ERROR("Tring to decode base64 on a buffer that isn't a multiple of 4 bytes long. (was %d)", decodeLength);
        return -1;
    }

    if (input[decodeLength - 1] == '=') (outputLength)--;
    if (input[decodeLength - 2] == '=') (outputLength)--;

    if( decodeBufferLength < outputLength ) {
        return -1;
    }
    
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
    char *output = CS_alloc( requiredSpace + 2 );
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
    char *output = CS_tempBuff(requiredSpace + 2 );
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
    void *output = CS_alloc(required);
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
static void privateSwapUrl( char *in, int length ) {
    for( int i = 0; i < length; ++i ) {
        switch(in[i]) {
            case '-':
                in[i]='+';
                break;
            case '_':
                in[i]='/';
                break;
        }
    }
}
void *CS_base64DecodeUrl( const char *toDecode, int length, int *outputLength ) {
    int newLength;
    char *copy = CS_tempStringCopyWithPad( toDecode, length, '=', &newLength, 4 );
    if( copy == NULL ) return NULL;
    privateSwapUrl(copy, newLength);
    return CS_base64Decode( copy, newLength, outputLength );
}
void *CS_base64DecodeUrlTemp( const char *toDecode, int length, int *outputLength ) {
    int newLength;
    char *copy = CS_tempStringCopyWithPad( toDecode, length, '=', &newLength, 4 );
    if( copy == NULL ) return NULL;
    privateSwapUrl(copy, newLength);
    return CS_base64DecodeTemp(copy, newLength, outputLength);
}
void *CS_base64DecodeUrlInPlace( char *toDecode, int length, int *outputLength ) {
    int newLength;
    char *copy = CS_tempStringCopyWithPad( toDecode, length, '=', &newLength, 4 );
    if( copy == NULL ) return NULL;
    privateSwapUrl(copy,newLength);
    int outLength = privateDecode(copy, newLength, toDecode, length );
    if( outLength < 0 ) return NULL;
    return toDecode;
}
void *CS_base64DecodeUrlLinearAlloc( const char *toDecode, int length, int *outputLength, void *linearAllocator ) {
    int newLength;
    char *copy = CS_tempStringCopyWithPad( toDecode, length, '=', &newLength, 4 );
    if( copy == NULL ) return NULL;
    privateSwapUrl(copy, newLength);
    return CS_base64DecodeLinearAlloc(copy, newLength, outputLength, linearAllocator);
}
