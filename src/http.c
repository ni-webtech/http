/*
    http.c -- Http service. Includes timer for expired requests.
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

static int httpTimer(Http *http, MprEvent *event);
static void initLimits(Http *http);
static void updateCurrentDate(Http *http);

/*********************************** Code *************************************/

Http *httpCreate(MprCtx ctx)
{
    Http            *http;
    HttpStatusCode  *code;
    HttpLocation    *location;

    http = mprAllocObjZeroed(ctx, Http);
    if (http == 0) {
        return 0;
    }
    http->connections = mprCreateList(http);
    http->stages = mprCreateHash(http, 31);
    http->keepAliveTimeout = HTTP_KEEP_TIMEOUT;
    http->maxKeepAlive = HTTP_MAX_KEEP_ALIVE;
    http->timeout = HTTP_SERVER_TIMEOUT;

    initLimits(http);
    httpCreateSecret(http);
    httpInitAuth(http);

    http->statusCodes = mprCreateHash(http, 41);
    for (code = HttpStatusCodes; code->code; code++) {
        mprAddHash(http->statusCodes, code->codeString, code);
    }
    http->mutex = mprCreateLock(http);
    updateCurrentDate(http);
    httpOpenNetConnector(http);
    httpOpenAuthFilter(http);
    httpOpenRangeFilter(http);
    httpOpenChunkFilter(http);
    httpOpenUploadFilter(http);
    httpOpenPassHandler(http);

    //  MOB -- this needs to be controllable via the HttpServer API in ejs
    //  MOB -- test that memory allocation errors are correctly handled
    http->location = location = httpCreateLocation(http);
    httpAddFilter(location, http->authFilter->name, NULL, HTTP_STAGE_OUTGOING);
    httpAddFilter(location, http->rangeFilter->name, NULL, HTTP_STAGE_OUTGOING);
    httpAddFilter(location, http->chunkFilter->name, NULL, HTTP_STAGE_OUTGOING);

    httpAddFilter(location, http->uploadFilter->name, NULL, HTTP_STAGE_INCOMING);
    httpAddFilter(location, http->chunkFilter->name, NULL, HTTP_STAGE_INCOMING);

    location->connector = http->netConnector;
    return http;
}


static void initLimits(Http *http)
{
    HttpLimits  *limits;

    limits = http->limits = mprAllocObj(http, HttpLimits);;
    limits->maxReceiveBody = HTTP_MAX_RECEIVE_BODY;
    limits->maxChunkSize = HTTP_MAX_CHUNK;
    limits->maxRequests = HTTP_MAX_REQUESTS;
    limits->maxTransmissionBody = HTTP_MAX_TRANSMISSION_BODY;
    limits->maxStageBuffer = HTTP_MAX_STAGE_BUFFER;
    limits->maxNumHeaders = HTTP_MAX_NUM_HEADERS;
    limits->maxHeader = HTTP_MAX_HEADERS;
    limits->maxUri = MPR_MAX_URL;
    limits->maxUploadSize = HTTP_MAX_UPLOAD;
    limits->maxThreads = HTTP_DEFAULT_MAX_THREADS;
    limits->minThreads = 0;

    /* 
       Zero means use O/S defaults. TODO OPT - we should modify this to be smaller!
     */
    limits->threadStackSize = 0;
}


void httpRegisterStage(Http *http, HttpStage *stage)
{
    mprAddHash(http->stages, stage->name, stage);
}


HttpStage *httpLookupStage(Http *http, cchar *name)
{
    return (HttpStage*) mprLookupHash(http->stages, name);
}


cchar *httpLookupStatus(Http *http, int status)
{
    HttpStatusCode  *ep;
    char            key[8];
    
    mprItoa(key, sizeof(key), status, 10);
    ep = (HttpStatusCode*) mprLookupHash(http->statusCodes, key);
    if (ep == 0) {
        return "Custom error";
    }
    return ep->msg;
}


/*  
    Start the http timer. This may create multiple timers -- no worry. httpAddConn does its best to only schedule one.
 */
static void startTimer(Http *http)
{
    updateCurrentDate(http);
    http->timer = mprCreateTimerEvent(mprGetDispatcher(http), "httpTimer", HTTP_TIMER_PERIOD, (MprEventProc) httpTimer, 
        http, MPR_EVENT_CONTINUOUS);
}


/*  
    The http timer does maintenance activities and will fire per second while there is active requests.
    When multi-threaded, the http timer runs as an event off the service thread. Because we lock the http here,
    connections cannot be deleted while we are modifying the list.
 */
static int httpTimer(Http *http, MprEvent *event)
{
    HttpConn      *conn;
    int         next, connCount;

    mprAssert(event);
    
    updateCurrentDate(http);

    /* 
       Check for any expired connections. Locking ensures connections won't be deleted.
     */
    lock(http);
    for (connCount = 0, next = 0; (conn = mprGetNextItem(http->connections, &next)) != 0; connCount++) {
        /* 
            Workaround for a GCC bug when comparing two 64bit numerics directly. Need a temporary.
         */
        int64 diff = conn->expire - http->now;
        if (diff < 0 && !mprGetDebugMode(http)) {
            conn->keepAliveCount = 0;
            if (conn->receiver) {
                LOG(http, 4, "Request timed out %s", conn->receiver->uri);
            } else {
                LOG(http, 4, "Idle connection timed out");
            }
            httpRemoveConn(http, conn);
            if (conn->sock) {
                mprDisconnectSocket(conn->sock);
            }
        }
    }
    if (connCount == 0) {
        mprFree(event);
        http->timer = 0;
    }
    unlock(http);
    return 0;
}


void httpAddConn(Http *http, HttpConn *conn)
{
    lock(http);
    mprAddItem(http->connections, conn);
    conn->started = mprGetTime(conn);
    if ((http->now + MPR_TICKS_PER_SEC) < conn->started) {
        updateCurrentDate(http);
    }
    if (http->timer == 0) {
        startTimer(http);
    }
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

    if (mprGetRandomBytes(http, bytes, sizeof(bytes), 0) < 0) {
        mprError(http, "Can't get sufficient random data for secure SSL operation. If SSL is used, it may not be secure.");
        now = mprGetTime(http); 
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
    mprFree(http->secret);
    http->secret = mprStrdup(http, ascii);
    return 0;
}


void httpEnableTraceMethod(Http *http, bool on)
{
    http->traceEnabled = on;
}


char *httpGetDateString(MprCtx ctx, MprPath *sbuf)
{
    MprTime     when;
    struct tm   tm;

    if (sbuf == 0) {
        when = mprGetTime(ctx);
    } else {
        when = (MprTime) sbuf->mtime * MPR_TICKS_PER_SEC;
    }
    mprDecodeUniversalTime(ctx, &tm, when);
    return mprFormatTime(ctx, HTTP_DATE_FORMAT, &tm);
}


void *httpGetContext(Http *http)
{
    return http->context;
}


void httpSetContext(Http *http, void *context)
{
    http->context = context;
}


int httpGetDefaultPort(Http *http)
{
    return http->defaultPort;
}


cchar *httpGetDefaultHost(Http *http)
{
    return http->defaultHost;
}


void httpRemoveConn(Http *http, HttpConn *conn)
{
    lock(http);
    mprRemoveItem(http->connections, conn);
    unlock(http);
}


void httpSetDefaultPort(Http *http, int port)
{
    http->defaultPort = port;
}


void httpSetDefaultHost(Http *http, cchar *host)
{
    mprFree(http->defaultHost);
    http->defaultHost = mprStrdup(http, host);
}


void httpSetProxy(Http *http, cchar *host, int port)
{
    mprFree(http->proxyHost);
    http->proxyHost = mprStrdup(http, host);
    http->proxyPort = port;
}


static void updateCurrentDate(Http *http)
{
    static char date[2][64];
    static char expires[2][64];
    static int  dateSelect, expiresSelect;
    struct tm   tm;
    char        *ds;

    lock(http);
    http->now = mprGetTime(http);
    ds = httpGetDateString(http, NULL);
    mprStrcpy(date[dateSelect], sizeof(date[0]) - 1, ds);
    http->currentDate = date[dateSelect];
    dateSelect = !dateSelect;
    mprFree(ds);

    //  MOB - check. Could do this once per minute
    mprDecodeUniversalTime(http, &tm, http->now + (86400 * 1000));
    ds = mprFormatTime(http, HTTP_DATE_FORMAT, &tm);
    mprStrcpy(expires[expiresSelect], sizeof(expires[0]) - 1, ds);
    http->expiresDate = expires[expiresSelect];
    expiresSelect = !expiresSelect;
    unlock(http);
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.

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

    @end
 */
