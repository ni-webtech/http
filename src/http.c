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

static int httpTimer(Http *http, MprEvent *event);
static void updateCurrentDate(Http *http);

/*********************************** Code *************************************/

Http *httpCreate(MprCtx ctx)
{
    Http            *http;
    HttpStatusCode  *code;

    if ((http = mprAllocObj(ctx, Http, NULL)) == 0) {
        return 0;
    }
    http->protocol = "HTTP/1.1";
    http->mutex = mprCreateLock(http);
    http->connections = mprCreateList(http);
    http->stages = mprCreateHash(http, 31, 0);

    updateCurrentDate(http);
    http->statusCodes = mprCreateHash(http, 41, 0);
    for (code = HttpStatusCodes; code->code; code++) {
        mprAddHash(http->statusCodes, code->codeString, code);
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

    http->clientLimits = httpCreateLimits(http, 0);
    http->serverLimits = httpCreateLimits(http, 1);
    http->clientLocation = httpInitLocation(http, http, 0);
    return http;
}


HttpLoc *httpInitLocation(Http *http, MprCtx ctx, int serverSide)
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
    limits->requestTimeout = 0;
    limits->sessionTimeout = HTTP_SESSION_TIMEOUT;

    limits->clientCount = HTTP_MAX_CLIENTS;
    limits->keepAliveCount = HTTP_MAX_KEEP_ALIVE;
    limits->requestCount = HTTP_MAX_REQUESTS;
    limits->sessionCount = HTTP_MAX_SESSIONS;

#if FUTURE
    mprSetMaxSocketClients(server, atoi(value));

    if (mprStrcmpAnyCase(key, "LimitClients") == 0) {
        mprSetMaxSocketClients(server, atoi(value));
        return 1;
    }
    if (mprStrcmpAnyCase(key, "LimitMemoryMax") == 0) {
        mprSetAllocLimits(server, -1, atoi(value));
        return 1;
    }
    if (mprStrcmpAnyCase(key, "LimitMemoryRedline") == 0) {
        mprSetAllocLimits(server, atoi(value), -1);
        return 1;
    }
#endif
}


HttpLimits *httpCreateLimits(MprCtx ctx, int serverSide)
{
    HttpLimits  *limits;

    if ((limits = mprAllocObj(ctx, HttpLimits, NULL)) != 0) {
        httpInitLimits(limits, serverSide);
    }
    return limits;
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
    HttpConn    *conn;
    int64       diff;
    int         next, connCount, inactivity, requestTimeout, inactivityTimeout;

    mprAssert(event);
    
    updateCurrentDate(http);
    if (mprGetDebugMode(http)) {
        return 0;
    }

    /* 
       Check for any inactive or expired connections (inactivityTimeout and requestTimeout)
     */
    lock(http);
    for (connCount = 0, next = 0; (conn = mprGetNextItem(http->connections, &next)) != 0; connCount++) {
        requestTimeout = conn->limits->requestTimeout ? conn->limits->requestTimeout : INT_MAX;
        inactivityTimeout = conn->limits->inactivityTimeout ? conn->limits->inactivityTimeout : INT_MAX;
        /* 
            Workaround for a GCC bug when comparing two 64bit numerics directly. Need a temporary.
         */
        diff = (conn->lastActivity + inactivityTimeout) - http->now;
        inactivity = 1;
        if (diff > 0 && conn->rx) {
            diff = (conn->lastActivity + requestTimeout) - http->now;
            inactivity = 0;
        }

        if (diff < 0 && !conn->complete) {
            if (conn->rx) {
                if (inactivity) {
                    httpConnError(conn, HTTP_CODE_REQUEST_TIMEOUT,
                        "Inactive request timed out, exceeded inactivity timeout %d sec", inactivityTimeout / 1000);
                } else {
                    httpConnError(conn, HTTP_CODE_REQUEST_TIMEOUT, 
                        "Request timed out, exceeded timeout %d sec", requestTimeout / 1000);
                }
            } else {
                mprLog(http, 4, "Idle connection timed out");
                conn->complete = 1;
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
    conn->seqno = http->connCount++;
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


void httpEnableTraceMethod(HttpLimits *limits, bool on)
{
    limits->enableTraceMethod = on;
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


int httpLoadSsl(Http *http)
{
#if BLD_FEATURE_SSL
    if (!http->sslLoaded) {
        if (!mprLoadSsl(http, 0)) {
            mprError(http, "Can't load SSL provider");
            return MPR_ERR_CANT_LOAD;
        }
        http->sslLoaded = 1;
    }
#else
    mprError(http, "SSL communications support not included in build");
#endif
    return 0;
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
