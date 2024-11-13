#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"
#include "crankshaftjson.h"
#include "crankshaftpushpull.h"

extern bool test_JSONTestSuite();

#pragma GCC diagnostic ignored "-Wformat-truncation="

static int testCount = 0;
static int testSucceeded = 0;

static char *testSuiteFiles = "../JSONTestSuite/test_parsing";

static char *nullBuffer = "";


//#define CS_PP_BUFFER_SIZE (512 * 1024)
#define CS_PP_BUFFER_SIZE 300000


bool test_JSONTestSuite() {
    //Tests go here:

    //../JSONTestSuite/test_parsing

    //From our directory, this is where the tests are if you have sunk them at the same level as
    //crankshaft https://github.com/nst/JSONTestSuite
    DIR *dp;
    struct dirent *ep;

    char pathToFile[ PATH_MAX ] = {0};

    char *dupeBuffer = CS_alloc( CS_PP_BUFFER_SIZE );

    dp = opendir( testSuiteFiles );

    struct CS_PushPullBuffer *pp = CS_PP_defaultAlloc( CS_PP_BUFFER_SIZE );

    if( !pp ) return true;

    if( dp != NULL ) {
        while( (ep = readdir( dp )) != NULL ) {
            if( ep->d_name[0] == '.' ) continue;
            snprintf( pathToFile, PATH_MAX, "%s/%s", testSuiteFiles, ep->d_name );
            int f = open( pathToFile, 0 );
            if( f != -1 ) {
                int bytesRead = CS_PP_readFromFile( pp, f );
                CS_FAIL_ON_TRUE( bytesRead < 0 || bytesRead >= CS_PP_BUFFER_SIZE, pathToFile, "Failed to read or too big." );
                CS_PP_readFromBuffer( pp, nullBuffer, 1 );
                memcpy(dupeBuffer, pp->buff, bytesRead);
                if( bytesRead > 0 && bytesRead < CS_PP_BUFFER_SIZE ) {
                    pp->buff[ bytesRead + 1 ] = 0;
                    CS_LOG_TRACE("Parse \"%s\":%s", pp->buff, pathToFile);
                    struct CS_JsonNode *out = CS_parseJsonCopy( CS_PP_startOfData(pp), bytesRead, CS_PP_BUFFER_SIZE );
                    if( ep->d_name[0] == 'n' ) {
                        CS_FAIL_ON_NOT_NULL( out, pp->buff, "%s", pathToFile );
                    } else if ( ep->d_name[0] == 'y' ) {
                        CS_FAIL_ON_NULL( out, pp->buff, "%s", pathToFile );
                    } else {
                        //CS_FAIL_ON_NULL( out, pp->buff, "%s", CS_jsonNodePrintable( out )->buffer );
                    }
                    struct CS_JsonNode *outNoCopy = CS_parseJson( dupeBuffer, bytesRead, CS_PP_BUFFER_SIZE );
                    if( ep->d_name[0] == 'n' ) {
                        CS_FAIL_ON_NOT_NULL( outNoCopy, pp->buff, "%s", pathToFile );
                    } else if ( ep->d_name[0] == 'y' ) {
                        CS_FAIL_ON_NULL( outNoCopy, pp->buff, "%s", pathToFile );
                    } else {
                        //CS_FAIL_ON_NULL( outNoCopy, pp->buff, "%s", CS_jsonNodePrintable( out )->buffer );
                    }
                    //CS_FAIL_ON_FALSE( CS_jsonNodesEquivalent( out, outNoCopy ), "Nodes not equivalent.", "%s", pathToFile );
                    if( out ) CS_freeJson( out );
                    if( outNoCopy ) CS_freeJson( outNoCopy );
                }
                CS_PP_reset(pp);
            }
        }
        closedir( dp );
    }

    CS_free(dupeBuffer);
    CS_PP_defaultFree(pp);
    

    return testCount !=
           testSucceeded;
}


