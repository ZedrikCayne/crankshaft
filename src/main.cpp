#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <signal.h>

#include "crankshaftlogger.h"
#include "crankshaftcommandline.h"
#include "crankshaftserver.h"
#include "crankshafttempbuff.h"
#include "crankshafttest.h"

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
CS_ARG_DEF(runAsDaemon,CS_ARG_CMP("-d","--daemon"),"Run as daemon");
CS_ARG_DEF(logFile,CS_ARG_CMP("-l","--log"),"Log to file");
CS_ARG_DEF(fileServingFile,CS_ARG_CMP("-f","--default-file"), "Default file when using GET on a directory.");
CS_ARG_DEF(fileServingDir,CS_ARG_CMP("--dir","--default-directory"), "Default directory for default file server.");
CS_ARG_DEF(keyFile,CS_ARG_CMP("-k","--key"), "pemfile for ssl private key.");
CS_ARG_DEF(certFile,CS_ARG_CMP("-c","--certificate"), "pemfile for certificate");

const struct ArgElement myArgs[] = 
    { CS_ARG_ELEMENT(wantHelp,BOOL_ARG),
      CS_ARG_ELEMENT(doTest,BOOL_ARG),
      CS_ARG_ELEMENT(testSeed,INT_ARG),
      CS_ARG_ELEMENT(onlyFails,BOOL_ARG),
      CS_ARG_ELEMENT(noWarn,BOOL_ARG),
      CS_ARG_ELEMENT(quiet,BOOL_ARG),
      CS_ARG_ELEMENT(info,BOOL_ARG),
      CS_ARG_ELEMENT(verbose,BOOL_ARG),
      CS_ARG_ELEMENT(trace,BOOL_ARG),
      CS_ARG_ELEMENT(suppressErrors,BOOL_ARG),
      CS_ARG_ELEMENT(portNum,INT_ARG),
      CS_ARG_ELEMENT(serverName,STRING_ARG),
      CS_ARG_ELEMENT(runAsDaemon,BOOL_ARG),
      CS_ARG_ELEMENT(logFile,STRING_ARG),
      CS_ARG_ELEMENT(fileServingFile,STRING_ARG),
      CS_ARG_ELEMENT(fileServingDir,STRING_ARG),
      CS_ARG_ELEMENT(keyFile,STRING_ARG),
      CS_ARG_ELEMENT(certFile,STRING_ARG)
    };

struct ArgTable myArgTable = { sizeof(myArgs)/sizeof(ArgElement), 0, NULL, myArgs };

void PrintHelp() {
    CS_printArgs(&myArgTable);
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

void dcCallback( struct CrankshaftClientInfo *info ) {
    CS_LOG_TRACE("Disconnecting.");
}

bool fudge( struct CrankshaftClientInfo *info ) {
    if( info->disconnectCallback == NULL ) {
        info->disconnectCallback = dcCallback;
    }
    return CS_Diagnostic200(info);
}

bool doQuit( struct CrankshaftClientInfo *info ) {
    GotInterrupt = true;
    return CS_Diagnostic200(info);
}

struct CrankshaftRoute serverRoutes[] = {
    { METHOD_GET,  ROUTE_TYPE_PREFIX, 0, "/Prefix/Match/Me", CS_Diagnostic200 },
    { METHOD_GET,  ROUTE_TYPE_EXACT, 0, "/Very/Match/Me", CS_Diagnostic200 },
    { METHOD_GET,  ROUTE_TYPE_PREFIX, 0, "/api", fudge },
    { METHOD_GET,  ROUTE_TYPE_EXACT, 0, "/quit", doQuit },
    { METHOD_GET,  ROUTE_TYPE_WILDCARD, 0, "", CS_FileServer },
    { METHOD_HEAD, ROUTE_TYPE_WILDCARD, 0, "", CS_FileServer }
};

int main(int argc, char *argv[] ) {
    const char * error = CS_parseArgs(argc, argv, &myArgTable);
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

    CS_LOG_VERBOSE("Server Name: %s", serverName);
    CS_LOG_VERBOSE("Port Number is %d", portNum);

    if( doTest ) {
        CS_TEST_PRINT_ONLY_ERRORS = onlyFails;
        if( testSeed != 0 ) {
            CS_testSetRandomSeed(testSeed);
        }
        exit(CS_testMain()?255:0);
    }

    signal(SIGINT, interruptHandler);
    signal(SIGHUP, hupHandler);
    signal(SIGTERM, terminateHandler);

    CS_allocateTempBuffs();

    CS_LOG_TRACE("Starting web server.");

    struct CrankshaftWebServer *server = CS_StartWebServer( portNum, certFile, keyFile, fileServingDir, fileServingFile, serverRoutes, sizeof(serverRoutes)/sizeof(serverRoutes[0]) );
    if( server == NULL )
        return -1;
    while(!GotInterrupt) {
        if( GotHup ) hupOnMainThread();
        sleep(1);
    }

    CS_KillWebServer(server);
    CS_freeAllTempBuffs();
    return 0;
}
