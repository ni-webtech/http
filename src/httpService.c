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
    { 408, "408", "Request Timeout" },
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
    { 504, "504", "Gateway Timeout" },
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
static void terminateHttp(int how, int status);
static void updateCurrentDate(Http *http);

/*********************************** Code *************************************/

Http *httpCreate()
{
    Http            *http;
    HttpStatusCode  *code;

    if (MPR->httpService) {
        return MPR->httpService;
    }
    if ((http = mprAllocObj(Http, manageHttp)) == 0) {
        return 0;
    }
    MPR->httpService = http;
    http->software = sclone(HTTP_NAME);
    http->protocol = sclone("HTTP/1.1");
    http->mutex = mprCreateLock(http);
    http->stages = mprCreateHash(-1, 0);
    http->routeTargets = mprCreateHash(-1, MPR_HASH_STATIC_VALUES);
    http->routeConditions = mprCreateHash(-1, MPR_HASH_STATIC_VALUES);
    http->routeUpdates = mprCreateHash(-1, MPR_HASH_STATIC_VALUES);
    http->hosts = mprCreateList(-1, MPR_LIST_STATIC_VALUES);
    http->endpoints = mprCreateList(-1, MPR_LIST_STATIC_VALUES);
    http->connections = mprCreateList(-1, MPR_LIST_STATIC_VALUES);
    http->defaultClientHost = sclone("127.0.0.1");
    http->defaultClientPort = 80;
    http->booted = mprGetTime();

    updateCurrentDate(http);
    http->statusCodes = mprCreateHash(41, MPR_HASH_STATIC_VALUES | MPR_HASH_STATIC_KEYS);
    for (code = HttpStatusCodes; code->code; code++) {
        mprAddKey(http->statusCodes, code->codeString, code);
    }
    httpCreateSecret(http);
    httpInitAuth(http);
    httpOpenNetConnector(http);
    httpOpenSendConnector(http);
    httpOpenRangeFilter(http);
    httpOpenChunkFilter(http);
    httpOpenUploadFilter(http);
    httpOpenCacheHandler(http);
    httpOpenPassHandler(http);

    http->clientLimits = httpCreateLimits(0);
    http->serverLimits = httpCreateLimits(1);
    http->clientRoute = httpCreateConfiguredRoute(0, 0);

    mprSetIdleCallback(isIdle);
    mprAddTerminator(terminateHttp);
    httpDefineRouteBuiltins();
    return http;
}


static void manageHttp(Http *http, int flags)
{
    HttpConn    *conn;
    int         next;

    if (flags & MPR_MANAGE_MARK) {
        mprMark(http->endpoints);
        mprMark(http->hosts);
        mprMark(http->connections);
        mprMark(http->stages);
        mprMark(http->statusCodes);
        mprMark(http->routeTargets);
        mprMark(http->routeConditions);
        mprMark(http->routeUpdates);
        /* Don't mark convenience stage references as they will be in http->stages */
        
        mprMark(http->clientLimits);
        mprMark(http->serverLimits);
        mprMark(http->clientRoute);
        mprMark(http->timer);
        mprMark(http->timestamp);
        mprMark(http->mutex);
        mprMark(http->software);
        mprMark(http->forkData);
        mprMark(http->context);
        mprMark(http->currentDate);
        mprMark(http->expiresDate);
        mprMark(http->secret);
        mprMark(http->defaultClientHost);
        mprMark(http->protocol);
        mprMark(http->proxyHost);

        /*
            Endpoints keep connections alive until a timeout. Keep marking even if no other references.
         */
        lock(http);
        for (next = 0; (conn = mprGetNextItem(http->connections, &next)) != 0; ) {
            if (conn->endpoint) {
                mprMark(conn);
            }
        }
        unlock(http);
    }
}


void httpDestroy(Http *http)
{
    if (http->timer) {
        mprRemoveEvent(http->timer);
    }
    if (http->timestamp) {
        mprRemoveEvent(http->timestamp);
    }
    MPR->httpService = NULL;
}


void httpAddEndpoint(Http *http, HttpEndpoint *endpoint)
{
    mprAddItem(http->endpoints, endpoint);
}


void httpRemoveEndpoint(Http *http, HttpEndpoint *endpoint)
{
    mprRemoveItem(http->endpoints, endpoint);
}


/*  
    Lookup a host address. If ipAddr is null or port is -1, then those elements are wild.
 */
HttpEndpoint *httpLookupEndpoint(Http *http, cchar *ip, int port)
{
    HttpEndpoint    *endpoint;
    int             next;

    if (ip == 0) {
        ip = "";
    }
    for (next = 0; (endpoint = mprGetNextItem(http->endpoints, &next)) != 0; ) {
        if (endpoint->port <= 0 || port <= 0 || endpoint->port == port) {
            mprAssert(endpoint->ip);
            if (*endpoint->ip == '\0' || *ip == '\0' || scmp(endpoint->ip, ip) == 0) {
                return endpoint;
            }
        }
    }
    return 0;
}


HttpEndpoint *httpGetFirstEndpoint(Http *http)
{
    return mprGetFirstItem(http->endpoints);
}


/*
    WARNING: this should not be called by users as httpCreateHost will automatically call this.
 */
void httpAddHost(Http *http, HttpHost *host)
{
    mprAddItem(http->hosts, host);
}


void httpRemoveHost(Http *http, HttpHost *host)
{
    mprRemoveItem(http->hosts, host);
}


HttpHost *httpLookupHost(Http *http, cchar *name)
{
    HttpHost    *host;
    int         next;

    for (next = 0; (host = mprGetNextItem(http->hosts, &next)) != 0; ) {
        if (smatch(name, host->name)) {
            return host;
        }
    }
    return 0;
}


void httpInitLimits(HttpLimits *limits, bool serverSide)
{
    memset(limits, 0, sizeof(HttpLimits));
    limits->cacheItemSize = HTTP_MAX_CACHE_ITEM;
    limits->chunkSize = HTTP_MAX_CHUNK;
    limits->clientMax = HTTP_MAX_CLIENTS;
    limits->headerMax = HTTP_MAX_NUM_HEADERS;
    limits->headerSize = HTTP_MAX_HEADERS;
    limits->keepAliveMax = HTTP_MAX_KEEP_ALIVE;
    limits->receiveFormSize = HTTP_MAX_RECEIVE_FORM;
    limits->receiveBodySize = HTTP_MAX_RECEIVE_BODY;
    limits->processMax = HTTP_MAX_REQUESTS;
    limits->requestMax = HTTP_MAX_REQUESTS;
    limits->stageBufferSize = HTTP_MAX_STAGE_BUFFER;
    limits->transmissionBodySize = HTTP_MAX_TX_BODY;
    limits->uploadSize = HTTP_MAX_UPLOAD;
    limits->uriSize = MPR_MAX_URL;

    limits->inactivityTimeout = HTTP_INACTIVITY_TIMEOUT;
    limits->requestTimeout = MAXINT;
    limits->sessionTimeout = HTTP_SESSION_TIMEOUT;

#if FUTURE
    mprSetMaxSocketClients(endpoint, atoi(value));

    if (scasecmp(key, "LimitClients") == 0) {
        mprSetMaxSocketClients(endpoint, atoi(value));
        return 1;
    }
    if (scasecmp(key, "LimitMemoryMax") == 0) {
        mprSetAllocLimits(endpoint, -1, atoi(value));
        return 1;
    }
    if (scasecmp(key, "LimitMemoryRedline") == 0) {
        mprSetAllocLimits(endpoint, atoi(value), -1);
        return 1;
    }
#endif
}


HttpLimits *httpCreateLimits(int serverSide)
{
    HttpLimits  *limits;

    if ((limits = mprAllocStruct(HttpLimits)) != 0) {
        httpInitLimits(limits, serverSide);
    }
    return limits;
}


void httpEaseLimits(HttpLimits *limits)
{
    limits->receiveFormSize = MAXOFF;
    limits->receiveBodySize = MAXOFF;
    limits->transmissionBodySize = MAXOFF;
    limits->uploadSize = MAXOFF;
}


void httpAddStage(Http *http, HttpStage *stage)
{
    mprAddKey(http->stages, stage->name, stage);
}


HttpStage *httpLookupStage(Http *http, cchar *name)
{
    return mprLookupKey(http->stages, name);
}


void *httpLookupStageData(Http *http, cchar *name)
{
    HttpStage   *stage;
    if ((stage = mprLookupKey(http->stages, name)) != 0) {
        return stage->stageData;
    }
    return 0;
}


cchar *httpLookupStatus(Http *http, int status)
{
    HttpStatusCode  *ep;
    char            *key;
    
    key = itos(status);
    ep = (HttpStatusCode*) mprLookupKey(http->statusCodes, key);
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
    The http timer does maintenance activities and will fire per second while there are active requests.
    NOTE: Because we lock the http here, connections cannot be deleted while we are modifying the list.
 */
static void httpTimer(Http *http, MprEvent *event)
{
    HttpConn    *conn;
    HttpStage   *stage;
    HttpRx      *rx;
    HttpLimits  *limits;
    MprModule   *module;
    int         next, active;

    mprAssert(event);
    
    updateCurrentDate(http);
    if (mprGetDebugMode()) {
        return;
    }
    /* 
       Check for any inactive connections or expired requests (inactivityTimeout and requestTimeout)
     */
    lock(http);
    mprLog(6, "httpTimer: %d active connections", mprGetListLength(http->connections));
    for (active = 0, next = 0; (conn = mprGetNextItem(http->connections, &next)) != 0; active++) {
        rx = conn->rx;
        limits = conn->limits;
        if (!conn->timeoutEvent && (
            (conn->lastActivity + limits->inactivityTimeout) < http->now || 
            (conn->started + limits->requestTimeout) < http->now)) {
            if (rx) {
                /*
                    Don't call APIs on the conn directly (thread-race). Schedule a timer on the connection's dispatcher
                 */
                if (!conn->timeoutEvent) {
                    conn->timeoutEvent = mprCreateEvent(conn->dispatcher, "connTimeout", 0, httpConnTimeout, conn, 0);
                }
            } else {
                mprLog(6, "Idle connection timed out");
                httpDisconnect(conn);
                httpDiscardQueueData(conn->writeq, 1);
                httpEnableConnEvents(conn);
                conn->lastActivity = conn->started = http->now;
            }
        }
    }

    /*
        Check for unloadable modules
        MOB - move down into MPR and set stage->flags in an unload callback
     */
    if (mprGetListLength(http->connections) == 0) {
        for (next = 0; (module = mprGetNextItem(MPR->moduleService->modules, &next)) != 0; ) {
            if (module->timeout) {
                if (module->lastActivity + module->timeout < http->now) {
                    mprLog(2, "Unloading inactive module %s", module->name);
                    if ((stage = httpLookupStage(http, module->name)) != 0) {
                        if (mprUnloadModule(module) < 0)  {
                            active++;
                        } else {
                            stage->flags |= HTTP_STAGE_UNLOADED;
                        }
                    } else {
                        mprUnloadModule(module);
                    }
                } else {
                    active++;
                }
            }
        }
    }
    if (active == 0) {
        mprRemoveEvent(event);
        http->timer = 0;
    }
    unlock(http);
}


static void timestamp()
{
    mprLog(0, "Time: %s", mprGetDate(NULL));
}


void httpSetTimestamp(MprTime period)
{
    Http    *http;

    http = MPR->httpService;
    if (http->timestamp) {
        mprRemoveEvent(http->timestamp);
    }
    if (period < (10 * MPR_TICKS_PER_SEC)) {
        period = (10 * MPR_TICKS_PER_SEC);
    }
    if (period > 0) {
        http->timer = mprCreateTimerEvent(NULL, "httpTimestamp", period, timestamp, NULL, 
            MPR_EVENT_CONTINUOUS | MPR_EVENT_QUICK);
    }
}


static void terminateHttp(int how, int status)
{
    Http            *http;
    HttpEndpoint    *endpoint;
    int             next;

    /*
        Stop listening for new requests
     */
    http = (Http*) mprGetMpr()->httpService;
    if (http) {
        for (ITERATE_ITEMS(http->endpoints, endpoint, next)) {
            httpStopEndpoint(endpoint);
        }
    }
}


static bool isIdle()
{
    HttpConn        *conn;
    Http            *http;
    MprTime         now;
    int             next;
    static MprTime  lastTrace = 0;

    http = (Http*) mprGetMpr()->httpService;
    now = http->now;

    lock(http);
    for (next = 0; (conn = mprGetNextItem(http->connections, &next)) != 0; ) {
        if (conn->state != HTTP_STATE_BEGIN) {
            if (lastTrace < now) {
                mprLog(1, "Waiting for request %s to complete", conn->rx->uri ? conn->rx->uri : conn->rx->pathInfo);
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
    mprAddItem(http->connections, conn);
    conn->started = http->now;
    conn->seqno = http->connCount++;
    updateCurrentDate(http);
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
        now = http->now;
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

    if (sbuf == 0) {
        when = ((Http*) MPR->httpService)->now;
    } else {
        when = (MprTime) sbuf->mtime * MPR_TICKS_PER_SEC;
    }
    return mprFormatUniversalTime(HTTP_DATE_FORMAT, when);
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
#if BIT_FEATURE_SSL
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
    static MprTime  recalcExpires = 0;

    http->now = mprGetTime();
    if (http->now > (http->currentTime + MPR_TICKS_PER_SEC - 1)) {
        http->currentTime = http->now;
        http->currentDate = httpGetDateString(NULL);
        if (http->expiresDate == 0 || recalcExpires < (http->now / (60 * 1000))) {
            http->expiresDate = mprFormatUniversalTime(HTTP_DATE_FORMAT, http->now + (86400 * 1000));
            recalcExpires = http->now / (60 * 1000);
        }
    }
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.TXT distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version. See the GNU General Public License for more
    details at: http://embedthis.com/downloads/gplLicense.html

    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
