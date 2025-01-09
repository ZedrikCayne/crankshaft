#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <signal.h>

#include "crankshaft/logger.h"
#include "crankshaft/commandline.h"
#include "crankshaft/server.h"
#include "crankshaft/tempbuff.h"
#include "crankshaft/test.h"

int acceptSocket = 0;

static bool doTest = false;
static int testSeed = 0;
static bool onlyFails = false;
static char defaultServerName[] = "CRANKSHAFT";
static bool suppressErrors = false;
static bool wantHelp = false;
static bool noWarn = false;
static bool quiet = false;
static bool info = false;
static bool verbose = false;
static bool trace = false;
static bool runAsDaemon = false;
static int portNum = 8080;
static char *serverName = defaultServerName;
static char *logFile = NULL;
static char defaultFileServingDir[] = "./root";
static char *fileServingDir = defaultFileServingDir;
static char defaultFileServingFile[] = "index.html";
static char *fileServingFile = defaultFileServingFile;
static char *certFile = NULL;
static char *keyFile = NULL;
static char *selfSignHostname = NULL;
static int cacheTimeInSeconds = 0;

CS_ARG_DEF(wantHelp,CS_ARG_CMP("-?","-help","--help"),"Prints this help");
CS_ARG_DEF(doTest,CS_ARG_CMP("--test"), "Unit testing.");
CS_ARG_DEF(testSeed,CS_ARG_CMP("--seed"), "Seed for test RNG.");
CS_ARG_DEF(onlyFails,CS_ARG_CMP("--only-fails"), "Only print fails during unit testing.");
CS_ARG_DEF(noWarn,CS_ARG_CMP("-w","--no-warn"),"No warning logs.");
CS_ARG_DEF(quiet,CS_ARG_CMP("-q","--quiet"),"Quiet logs (Warnings, errors and 'loud logs' only.)");
CS_ARG_DEF(info,CS_ARG_CMP("-i","--info"),"Info logs.");
CS_ARG_DEF(verbose,CS_ARG_CMP("-v", "--verbose"),"Verbose logs");
CS_ARG_DEF(trace,CS_ARG_CMP("-t", "--trace"),"Trace logs");
CS_ARG_DEF(suppressErrors,CS_ARG_CMP("--suppress-errors"),"Supress error logs (useful during --test)");
CS_ARG_DEF(portNum,CS_ARG_CMP("-p", "--port"),"Port Number");
CS_ARG_DEF(serverName,CS_ARG_CMP("-n","--name"),"Name of server");
CS_ARG_DEF(cacheTimeInSeconds, CS_ARG_CMP("--cache"),"Cache time of default file server (0 is no cache)");
CS_ARG_DEF(runAsDaemon,CS_ARG_CMP("-d","--daemon"),"Run as daemon");
CS_ARG_DEF(logFile,CS_ARG_CMP("-l","--log"),"Log to file");
CS_ARG_DEF(fileServingFile,CS_ARG_CMP("-f","--default-file"), "Default file when using GET on a directory.");
CS_ARG_DEF(fileServingDir,CS_ARG_CMP("--dir","--default-directory"), "Default directory for default file server.");
CS_ARG_DEF(keyFile,CS_ARG_CMP("-k","--key"), "pemfile for ssl private key.");
CS_ARG_DEF(certFile,CS_ARG_CMP("-c","--certificate"), "pemfile for certificate");
CS_ARG_DEF(selfSignHostname,CS_ARG_CMP("--self-sign"), "create a self signed certificate for provided host");

const struct CS_ArgElement myArgs[] = 
    { CS_ARG_ELEMENT(wantHelp,CS_BOOL_ARG),
      CS_ARG_ELEMENT(doTest,CS_BOOL_ARG),
      CS_ARG_ELEMENT(testSeed,CS_INT_ARG),
      CS_ARG_ELEMENT(onlyFails,CS_BOOL_ARG),
      CS_ARG_ELEMENT(noWarn,CS_BOOL_ARG),
      CS_ARG_ELEMENT(quiet,CS_BOOL_ARG),
      CS_ARG_ELEMENT(info,CS_BOOL_ARG),
      CS_ARG_ELEMENT(verbose,CS_BOOL_ARG),
      CS_ARG_ELEMENT(trace,CS_BOOL_ARG),
      CS_ARG_ELEMENT(suppressErrors,CS_BOOL_ARG),
      CS_ARG_ELEMENT(portNum,CS_INT_ARG),
      CS_ARG_ELEMENT(serverName,CS_STRING_ARG),
      CS_ARG_ELEMENT(runAsDaemon,CS_BOOL_ARG),
      CS_ARG_ELEMENT(logFile,CS_STRING_ARG),
      CS_ARG_ELEMENT(fileServingFile,CS_STRING_ARG),
      CS_ARG_ELEMENT(fileServingDir,CS_STRING_ARG),
      CS_ARG_ELEMENT(cacheTimeInSeconds,CS_INT_ARG),
      CS_ARG_ELEMENT(keyFile,CS_STRING_ARG),
      CS_ARG_ELEMENT(certFile,CS_STRING_ARG),
      CS_ARG_ELEMENT(selfSignHostname,CS_STRING_ARG)
    };

struct CS_ArgTable myCS_ArgTable = { sizeof(myArgs)/sizeof(CS_ArgElement), 0, NULL, myArgs };

void PrintHelp() {
    CS_argsPrint(&myCS_ArgTable);
}

void PrintHeader() {
    //      12345678901234567890123456789012345678901234567890123456789012345678901234567890
    printf("crankshaft, around which the world turns.        \\\n");
    printf("                              ==     ==    ==     \\\n");
    printf("                             /  \\   /  \\  /  \\     \\\n");
    printf(" =========================   |  |   |  |  |   ======\\===\n");
    printf("                          \\  /  \\  /   \\  /          \\\n");
    printf("                           ==    ==     ==            \\\n");
    printf("                                                       \\\n");
    printf("\n");
}

static bool GotInterrupt = false;
static bool GotHup = false;

static void terminateHandler(int sig) {
    signal(sig, SIG_IGN);
    GotInterrupt = true;
    signal(SIGTERM, terminateHandler);
}

static void interruptHandler(int sig) {
    signal(sig, SIG_IGN);
    GotInterrupt = true;
    signal(SIGINT, interruptHandler);
}

static void hupHandler(int sig) {
    signal(sig, SIG_IGN);
    GotHup = true;
    signal(SIGHUP, hupHandler);
}

void hupOnMainThread() {
    CS_LOG_TRACE("HUP");
    GotHup = false;
}

void dcCallback( struct CS_ClientInfo *info ) {
    CS_LOG_TRACE("Disconnecting.");
}

bool googleLogin( struct CS_ClientInfo *info ) {
    FILE * fout = fopen("/home/zed/from.js", "w");
    fwrite( CS_PP_startOfData( info->buffer ), 1, CS_PP_dataSize( info->buffer ), fout );
    fclose(fout);
    return CS_serverDiagnostic200(info);
}

bool fudge( struct CS_ClientInfo *info ) {
    if( info->disconnectCallback == NULL ) {
        info->disconnectCallback = dcCallback;
    }
    return CS_serverDiagnostic200(info);
}

bool doQuit( struct CS_ClientInfo *info ) {
    GotInterrupt = true;
    return CS_serverDiagnostic200(info);
}

struct CS_Route serverRoutes[] = {
    { CS_HTTP_METHOD_GET,  CS_ROUTE_TYPE_EXACT, 0, "/googlelogin", googleLogin},
    { CS_HTTP_METHOD_POST, CS_ROUTE_TYPE_EXACT, 0, "/googlelogin", googleLogin},
    { CS_HTTP_METHOD_GET,  CS_ROUTE_TYPE_PREFIX, 0, "/api", fudge },
    { CS_HTTP_METHOD_POST, CS_ROUTE_TYPE_PREFIX, 0, "/api", fudge },
    { CS_HTTP_METHOD_HEAD, CS_ROUTE_TYPE_WILDCARD, 0, "", CS_serverFileServer },
    { CS_HTTP_METHOD_GET,  CS_ROUTE_TYPE_WILDCARD, 0, "", CS_serverFileServer },
};

int main(int argc, char *argv[] ) {
    const char * error = CS_argsParse(argc, argv, &myCS_ArgTable);
    if( error != NULL || wantHelp ) {
        PrintHeader();
        if( error != NULL ) CS_LOG_ERROR("%s", error);
        PrintHelp();
        return -1;
    }
    int countLogMods = 0;
    if( noWarn ) ++countLogMods;
    if( quiet ) ++countLogMods;
    if( verbose ) ++countLogMods;
    if( trace ) ++countLogMods;

    if( countLogMods > 1 ) CS_LOG_WARN("More than one of trace, quiet, noWarn or verbose specified. The loudest one will rule");

    if( noWarn )  { CS_LOG_VERBOSE_BOOL=false; CS_LOG_INFO_BOOL=false; CS_LOG_QUIET_BOOL=true ; CS_LOG_TRACE_BOOL=false; CS_LOG_WARN_BOOL=false; }
    if( quiet )   { CS_LOG_VERBOSE_BOOL=false; CS_LOG_INFO_BOOL=false; CS_LOG_QUIET_BOOL=true ; CS_LOG_TRACE_BOOL=false; CS_LOG_WARN_BOOL=true;}
    if( verbose ) { CS_LOG_VERBOSE_BOOL=true ; CS_LOG_INFO_BOOL=false; CS_LOG_QUIET_BOOL=false; CS_LOG_TRACE_BOOL=false; CS_LOG_WARN_BOOL=true; }
    if( info )    { CS_LOG_VERBOSE_BOOL=true ; CS_LOG_INFO_BOOL=true ; CS_LOG_QUIET_BOOL=false; CS_LOG_TRACE_BOOL=false; CS_LOG_WARN_BOOL=true; }
    if( trace )   { CS_LOG_VERBOSE_BOOL=true ; CS_LOG_INFO_BOOL=true ; CS_LOG_QUIET_BOOL=false; CS_LOG_TRACE_BOOL=true ; CS_LOG_WARN_BOOL=true; }
    if( suppressErrors ) { CS_LOG_ERROR_BOOL = false; }

    if( logFile != NULL ) {
        if( CS_logInit( logFile ) ) {
            CS_LOG_ERROR("Logging subsystem failed to init. Bailing!");
            return -1;
        }
    }

    if( doTest ) {
        CS_TEST_PRINT_ONLY_ERRORS = onlyFails;
        if( testSeed != 0 ) {
            CS_testSetRandomSeed(testSeed);
        }
        bool testMain = CS_testMain();
        if( logFile != NULL ) CS_logKill();
        exit(testMain?255:0);
    }

    CS_LOG_INFO("Server Name: %s", serverName);
    CS_LOG_INFO("Port Number is %d", portNum);

    signal(SIGINT, interruptHandler);
    signal(SIGHUP, hupHandler);
    signal(SIGTERM, terminateHandler);

    CS_tempAllocateGlobal(10*1024*1024);

    CS_LOG_INFO("Starting web server.");

    struct CS_WebServer *server = CS_serverStart( portNum, certFile, keyFile, selfSignHostname, fileServingDir, fileServingFile, cacheTimeInSeconds, serverRoutes, sizeof(serverRoutes)/sizeof(serverRoutes[0]) );
    if( server != NULL ) {
        CS_LOG_INFO("Server started at port %d", server->serverPort);
        while(!GotInterrupt) {
            if( GotHup ) hupOnMainThread();
            sleep(1);
        }
        CS_serverKill(server);
    } else {
        CS_LOG_ERROR("Server failed to start...");
    }

    if( logFile != NULL ) CS_logKill();
    CS_tempFreeGlobal();
    return 0;
}
