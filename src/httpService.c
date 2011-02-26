/*
    httpService.c -- Http service. Includes timer for expired requests.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Locals ************************************/
/**
    Standard HTTP error code table
 */
typedef struct HttpStatusCode {
    int     code;                           /**< Http error code */
    char    *codeString;                    /**< Code as a string (for hashing) */
    char    *msg;                           /**< Error message */
} HttpStatusCode;


HttpStatusCode HttpStatusCodes[] = {
    { 100, "100", "Continue" },
    { 200, "200", "OK" },
    { 201, "201", "Created" },
    { 202, "202", "Accepted" },
    { 204, "204", "No Content" },
    { 205, "205", "Reset Content" },
    { 206, "206", "Partial Content" },
    { 301, "301", "Moved Permanently" },
    { 302, "302", "Moved Temporarily" },
    { 304, "304", "Not Modified" },
    { 305, "305", "Use Proxy" },
    { 307, "307", "Temporary Redirect" },
    { 400, "400", "Bad Request" },
    { 401, "401", "Unauthorized" },
    { 402, "402", "Payment Required" },
    { 403, "403", "Forbidden" },
    { 404, "404", "Not Found" },
    { 405, "405", "Method Not Allowed" },
    { 406, "406", "Not Acceptable" },
    { 408, "408", "Request Time-out" },
    { 409, "409", "Conflict" },
    { 410, "410", "Length Required" },
    { 411, "411", "Length Required" },
    { 412, "412", "Precondition Failed" },
    { 413, "413", "Request Entity Too Large" },
    { 414, "414", "Request-URI Too Large" },
    { 415, "415", "Unsupported Media Type" },
    { 416, "416", "Requested Range Not Satisfiable" },
    { 417, "417", "Expectation Failed" },
    { 500, "500", "Internal Server Error" },
    { 501, "501", "Not Implemented" },
    { 502, "502", "Bad Gateway" },
    { 503, "503", "Service Unavailable" },
    { 504, "504", "Gateway Time-out" },
    { 505, "505", "Http Version Not Supported" },
    { 507, "507", "Insufficient Storage" },

    /*
        Proprietary codes (used internally) when connection to client is severed
     */
    { 550, "550", "Comms Error" },
    { 551, "551", "General Client Error" },
    { 0,   0 }
};

/****************************** Forward Declarations **************************/

static void httpTimer(Http *http, MprEvent *event);
static bool isIdle();
static void manageHttp(Http *http, int flags);
static void updateCurrentDate(Http *http);

/*********************************** Code *************************************/

Http *httpCreate()
{
    Http            *http;
    HttpStatusCode  *code;

    if ((http = mprAllocObj(Http, manageHttp)) == 0) {
        return 0;
    }
    mprGetMpr()->httpService = http;
    http->software = sclone(HTTP_NAME);
    http->protocol = sclone("HTTP/1.1");
    http->mutex = mprCreateLock(http);
    http->stages = mprCreateHash(-1, 0);
    http->hosts = mprCreateList(-1, MPR_LIST_STATIC_VALUES);
    http->servers = mprCreateList(-1, MPR_LIST_STATIC_VALUES);
    http->connections = mprCreateList(-1, MPR_LIST_STATIC_VALUES);
    http->defaultClientHost = sclone("127.0.0.1");
    http->defaultClientPort = 80;

    updateCurrentDate(http);
    http->statusCodes = mprCreateHash(41, MPR_HASH_STATIC_VALUES | MPR_HASH_STATIC_KEYS);
    for (code = HttpStatusCodes; code->code; code++) {
        mprAddKey(http->statusCodes, code->codeString, code);
    }
    httpCreateSecret(http);
    httpInitAuth(http);
    httpOpenNetConnector(http);
    httpOpenSendConnector(http);
    httpOpenAuthFilter(http);
    httpOpenRangeFilter(http);
    httpOpenChunkFilter(http);
    httpOpenUploadFilter(http);
    httpOpenPassHandler(http);

    http->clientLimits = httpCreateLimits(0);
    http->serverLimits = httpCreateLimits(1);
    http->clientLocation = httpInitLocation(http, 0);

    mprSetIdleCallback(isIdle);
    return http;
}


static void manageHttp(Http *http, int flags)
{
    HttpConn    *conn;
    int         next;

    if (flags & MPR_MANAGE_MARK) {
        /* Note servers and hosts are static values - contents are not marked so they can be collected */
        mprMark(http->servers);
        mprMark(http->hosts);
        mprMark(http->connections);
        mprMark(http->stages);
        mprMark(http->statusCodes);
        mprMark(http->clientLimits);
        mprMark(http->serverLimits);
        mprMark(http->clientLocation);
        mprMark(http->timer);
        mprMark(http->mutex);
        mprMark(http->software);
        mprMark(http->forkData);
        mprMark(http->context);
        mprMark(http->currentDate);
        mprMark(http->expiresDate);
        mprMark(http->secret);
        mprMark(http->protocol);
        mprMark(http->proxyHost);
        mprMark(http->servers);
        mprMark(http->defaultClientHost);

        /*
            Servers keep connections alive until a timeout. Keep marking even if no other references.
         */
        lock(http);
        for (next = 0; (conn = mprGetNextItem(http->connections, &next)) != 0; ) {
            if (conn->server) {
                mprMark(conn);
            }
        }
        unlock(http);
    }
}


void httpDestroy(Http *http)
{
    MPR->httpService = NULL;
}


void httpAddServer(Http *http, HttpServer *server)
{
    mprAddItem(http->servers, server);
}


void httpRemoveServer(Http *http, HttpServer *server)
{
    mprRemoveItem(http->servers, server);
}


/*  
    Lookup a host address. If ipAddr is null or port is -1, then those elements are wild.
    MOB - wild cards are not being used - see asserts
 */
HttpServer *httpLookupServer(Http *http, cchar *ip, int port)
{
    HttpServer  *server;
    int         next;

    if (ip == 0) {
        ip = "";
    }
    for (next = 0; (server = mprGetNextItem(http->servers, &next)) != 0; ) {
        if (server->port <= 0 || port <= 0 || server->port == port) {
            mprAssert(server->ip);
            if (*server->ip == '\0' || *ip == '\0' || scmp(server->ip, ip) == 0) {
                return server;
            }
        }
    }
    return 0;
}


int httpAddHostToServers(Http *http, struct HttpHost *host)
{
    HttpServer  *server;
    cchar       *ip;
    int         count, next;

    ip = host->ip ? host->ip : "";
    mprAssert(ip);

    for (count = next = 0; (server = mprGetNextItem(http->servers, &next)) != 0; ) {
        if (server->port <= 0 || host->port <= 0 || server->port == host->port) {
            mprAssert(server->ip);
            if (*server->ip == '\0' || *ip == '\0' || scmp(server->ip, ip) == 0) {
                httpAddHostToServer(server, host);
                count++;
            }
        }
    }
    return (count == 0) ? MPR_ERR_CANT_FIND : 0;
}


int httpSetNamedVirtualServers(Http *http, cchar *ip, int port)
{
    HttpServer  *server;
    int         count, next;

    if (ip == 0) {
        ip = "";
    }
    for (count = next = 0; (server = mprGetNextItem(http->servers, &next)) != 0; ) {
        if (server->port <= 0 || port <= 0 || server->port == port) {
            mprAssert(server->ip);
            if (*server->ip == '\0' || *ip == '\0' || scmp(server->ip, ip) == 0) {
                httpSetNamedVirtualServer(server);
                count++;
            }
        }
    }
    return (count == 0) ? MPR_ERR_CANT_FIND : 0;
}


void httpAddHost(Http *http, HttpHost *host)
{
    mprAddItem(http->hosts, host);
}


void httpRemoveHost(Http *http, HttpHost *host)
{
    mprRemoveItem(http->hosts, host);
}


#if UNUSED
//  MOB is this used?
HttpHost *httpLookupHost(Http *http, cchar *name)
{
    HttpHost  *host;
    int         next;

    for (next = 0; (host = mprGetNextItem(http->hosts, &next)) != 0; ) {
        if (strcmp(host->name, name) == 0) {
            return host;
        }
    }
    return 0;
}
#endif


//  MOB - rename
HttpLoc *httpInitLocation(Http *http, int serverSide)
{
    HttpLoc     *loc;

    /*
        Create default incoming and outgoing pipelines. Order matters.
     */
    loc = httpCreateLocation(http);
    httpAddFilter(loc, http->authFilter->name, NULL, HTTP_STAGE_OUTGOING);
    httpAddFilter(loc, http->rangeFilter->name, NULL, HTTP_STAGE_OUTGOING);
    httpAddFilter(loc, http->chunkFilter->name, NULL, HTTP_STAGE_OUTGOING);

    httpAddFilter(loc, http->chunkFilter->name, NULL, HTTP_STAGE_INCOMING);
    httpAddFilter(loc, http->uploadFilter->name, NULL, HTTP_STAGE_INCOMING);
    loc->connector = http->netConnector;
    return loc;
}


void httpInitLimits(HttpLimits *limits, int serverSide)
{
    memset(limits, 0, sizeof(HttpLimits));
    limits->chunkSize = HTTP_MAX_CHUNK;
    limits->headerCount = HTTP_MAX_NUM_HEADERS;
    limits->headerSize = HTTP_MAX_HEADERS;
    limits->receiveBodySize = HTTP_MAX_RECEIVE_BODY;
    limits->requestCount = HTTP_MAX_REQUESTS;
    limits->stageBufferSize = HTTP_MAX_STAGE_BUFFER;
    limits->transmissionBodySize = HTTP_MAX_TRANSMISSION_BODY;
    limits->uploadSize = HTTP_MAX_UPLOAD;
    limits->uriSize = MPR_MAX_URL;

    limits->inactivityTimeout = HTTP_INACTIVITY_TIMEOUT;
    limits->requestTimeout = INT_MAX;
    limits->sessionTimeout = HTTP_SESSION_TIMEOUT;

    limits->clientCount = HTTP_MAX_CLIENTS;
    limits->keepAliveCount = HTTP_MAX_KEEP_ALIVE;
    limits->requestCount = HTTP_MAX_REQUESTS;
    limits->sessionCount = HTTP_MAX_SESSIONS;

#if FUTURE
    mprSetMaxSocketClients(server, atoi(value));

    if (scasecmp(key, "LimitClients") == 0) {
        mprSetMaxSocketClients(server, atoi(value));
        return 1;
    }
    if (scasecmp(key, "LimitMemoryMax") == 0) {
        mprSetAllocLimits(server, -1, atoi(value));
        return 1;
    }
    if (scasecmp(key, "LimitMemoryRedline") == 0) {
        mprSetAllocLimits(server, atoi(value), -1);
        return 1;
    }
#endif
}


HttpLimits *httpCreateLimits(int serverSide)
{
    HttpLimits  *limits;

    if ((limits = mprAllocObj(HttpLimits, NULL)) != 0) {
        httpInitLimits(limits, serverSide);
    }
    return limits;
}


void httpAddStage(Http *http, HttpStage *stage)
{
    mprAddKey(http->stages, stage->name, stage);
}


HttpStage *httpLookupStage(Http *http, cchar *name)
{
    return (HttpStage*) mprLookupHash(http->stages, name);
}


cchar *httpLookupStatus(Http *http, int status)
{
    HttpStatusCode  *ep;
    char            key[8];
    
    itos(key, sizeof(key), status, 10);
    ep = (HttpStatusCode*) mprLookupHash(http->statusCodes, key);
    if (ep == 0) {
        return "Custom error";
    }
    return ep->msg;
}


void httpSetForkCallback(Http *http, MprForkCallback callback, void *data)
{
    http->forkCallback = callback;
    http->forkData = data;
}


void httpSetListenCallback(Http *http, HttpListenCallback fn)
{
    http->listenCallback = fn;
}


void httpSetMatchCallback(Http *http, HttpMatchCallback fn)
{
    http->matchCallback = fn;
}


/*  
    Start the http timer. This may create multiple timers -- no worry. httpAddConn does its best to only schedule one.
 */
static void startTimer(Http *http)
{
    updateCurrentDate(http);
    http->timer = mprCreateTimerEvent(NULL, "httpTimer", HTTP_TIMER_PERIOD, httpTimer, http, 
        MPR_EVENT_CONTINUOUS | MPR_EVENT_QUICK);
}


/*  
    The http timer does maintenance activities and will fire per second while there is active requests.
    When multi-threaded, the http timer runs as an event off the service thread. Because we lock the http here,
    connections cannot be deleted while we are modifying the list.
 */
static void httpTimer(Http *http, MprEvent *event)
{
    HttpConn    *conn;
    HttpStage   *stage;
    HttpRx      *rx;
    HttpLimits  *limits;
    MprModule   *module;
    int         next, count;

    mprAssert(event);
    
    updateCurrentDate(http);
    if (mprGetDebugMode(http)) {
        return;
    }

    /* 
       Check for any inactive connections or expired requests (inactivityTimeout and requestTimeout)
     */
    lock(http);
    mprLog(8, "httpTimer: %d active connections", mprGetListLength(http->connections));
    for (count = 0, next = 0; (conn = mprGetNextItem(http->connections, &next)) != 0; count++) {
        rx = conn->rx;
        limits = conn->limits;
        if ((conn->lastActivity + limits->requestTimeout) < http->now || 
            (conn->started + limits->requestTimeout) < http->now) {
            if (rx) {
                /*
                    Don't call APIs on the onn directly (thread-race). Schedule a timer on the connection's dispatcher
                 */
                if (!conn->timeout) {
                    conn->timeout = mprCreateEvent(conn->dispatcher, "connTimeout", 0, httpConnTimeout, conn, 0);
                }
            } else {
                mprLog(6, "Idle connection timed out");
                conn->complete = 1;
                mprDisconnectSocket(conn->sock);
            }
        }
    }

    /*
        Check for unloadable modules
     */
    if (mprGetListLength(http->connections) == 0) {
        for (next = 0; (module = mprGetNextItem(MPR->moduleService->modules, &next)) != 0; ) {
            if (module->timeout) {
                if (module->lastActivity + module->timeout < http->now) {
                    mprLog(2, "Unloading inactive module %s", module->name);
                    if ((stage = httpLookupStage(http, module->name)) != 0) {
                        if (stage->match) {
                            mprError("Can't unload modules with match routines");
                            module->timeout = 0;
                        } else {
                            mprUnloadModule(module);
                            stage->flags |= HTTP_STAGE_UNLOADED;
                        }
                    } else {
                        mprUnloadModule(module);
                    }
                } else {
                    count++;
                }
            }
        }
    }
    if (count == 0) {
        mprRemoveEvent(event);
        http->timer = 0;
    }
    unlock(http);
}


static bool isIdle()
{
    HttpConn        *conn;
    Http            *http;
    MprTime         now;
    int             next;
    static MprTime  lastTrace = 0;

    http = (Http*) mprGetMpr()->httpService;
    now = mprGetTime();

    lock(http);
    for (next = 0; (conn = mprGetNextItem(http->connections, &next)) != 0; ) {
        if (conn->state != HTTP_STATE_BEGIN) {
            if (lastTrace < now) {
                mprLog(0, "Waiting for request %s to complete", conn->rx->uri ? conn->rx->uri : conn->rx->pathInfo);
                lastTrace = now;
            }
            unlock(http);
            return 0;
        }
    }
    unlock(http);
    if (!mprServicesAreIdle()) {
        if (lastTrace < now) {
            mprLog(4, "Waiting for MPR services complete");
            lastTrace = now;
        }
        return 0;
    }
    return 1;
}


void httpAddConn(Http *http, HttpConn *conn)
{
    lock(http);
    //  MOB
    mprAssert(http->connections->length < 25);
    mprAddItem(http->connections, conn);
    conn->started = mprGetTime();
    conn->seqno = http->connCount++;
    if ((http->now + MPR_TICKS_PER_SEC) < conn->started) {
        updateCurrentDate(http);
    }
    if (http->timer == 0) {
        startTimer(http);
    }
    unlock(http);
}


void httpRemoveConn(Http *http, HttpConn *conn)
{
    lock(http);
    mprRemoveItem(http->connections, conn);
    unlock(http);
}


/*  
    Create a random secret for use in authentication. Create once for the entire http service. Created on demand.
    Users can recall as required to update.
 */
int httpCreateSecret(Http *http)
{
    MprTime     now;
    char        *hex = "0123456789abcdef";
    char        bytes[HTTP_MAX_SECRET], ascii[HTTP_MAX_SECRET * 2 + 1], *ap, *cp, *bp;
    int         i, pid;

    if (mprGetRandomBytes(bytes, sizeof(bytes), 0) < 0) {
        mprError("Can't get sufficient random data for secure SSL operation. If SSL is used, it may not be secure.");
        now = mprGetTime(); 
        pid = (int) getpid();
        cp = (char*) &now;
        bp = bytes;
        for (i = 0; i < sizeof(now) && bp < &bytes[HTTP_MAX_SECRET]; i++) {
            *bp++= *cp++;
        }
        cp = (char*) &now;
        for (i = 0; i < sizeof(pid) && bp < &bytes[HTTP_MAX_SECRET]; i++) {
            *bp++ = *cp++;
        }
        mprAssert(0);
        return MPR_ERR_CANT_INITIALIZE;
    }

    ap = ascii;
    for (i = 0; i < (int) sizeof(bytes); i++) {
        *ap++ = hex[((uchar) bytes[i]) >> 4];
        *ap++ = hex[((uchar) bytes[i]) & 0xf];
    }
    *ap = '\0';
    http->secret = sclone(ascii);
    return 0;
}


void httpEnableTraceMethod(HttpLimits *limits, bool on)
{
    limits->enableTraceMethod = on;
}


char *httpGetDateString(MprPath *sbuf)
{
    MprTime     when;
    struct tm   tm;

    if (sbuf == 0) {
        when = mprGetTime();
    } else {
        when = (MprTime) sbuf->mtime * MPR_TICKS_PER_SEC;
    }
    mprDecodeUniversalTime(&tm, when);
    return mprFormatTime(HTTP_DATE_FORMAT, &tm);
}


void *httpGetContext(Http *http)
{
    return http->context;
}


void httpSetContext(Http *http, void *context)
{
    http->context = context;
}


int httpGetDefaultClientPort(Http *http)
{
    return http->defaultClientPort;
}


cchar *httpGetDefaultClientHost(Http *http)
{
    return http->defaultClientHost;
}


int httpLoadSsl(Http *http)
{
#if BLD_FEATURE_SSL
    if (!http->sslLoaded) {
        if (!mprLoadSsl(0)) {
            mprError("Can't load SSL provider");
            return MPR_ERR_CANT_LOAD;
        }
        http->sslLoaded = 1;
    }
#else
    mprError("SSL communications support not included in build");
#endif
    return 0;
}


void httpSetDefaultClientPort(Http *http, int port)
{
    http->defaultClientPort = port;
}


void httpSetDefaultClientHost(Http *http, cchar *host)
{
    http->defaultClientHost = sclone(host);
}


void httpSetSoftware(Http *http, cchar *software)
{
    http->software = sclone(software);
}


void httpSetProxy(Http *http, cchar *host, int port)
{
    http->proxyHost = sclone(host);
    http->proxyPort = port;
}


static void updateCurrentDate(Http *http)
{
    struct tm       tm;
    static MprTime  recalcExpires = 0;

    lock(http);
    http->now = mprGetTime();
    http->currentDate = httpGetDateString(NULL);

    if (http->expiresDate == 0 || recalcExpires < (http->now / (60 * 1000))) {
        mprDecodeUniversalTime(&tm, http->now + (86400 * 1000));
        http->expiresDate = mprFormatTime(HTTP_DATE_FORMAT, &tm);
        recalcExpires = http->now / (60 * 1000);
    }
    unlock(http);
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.TXT distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version. See the GNU General Public License for more
    details at: http://www.embedthis.com/downloads/gplLicense.html

    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://www.embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
