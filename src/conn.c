/*
    conn.c -- Connection module to handle individual HTTP connections.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static void manageConn(HttpConn *conn, int flags);
static HttpPacket *getPacket(HttpConn *conn, ssize *bytesToRead);
static void readEvent(HttpConn *conn);
static void writeEvent(HttpConn *conn);

/*********************************** Code *************************************/
/*
    Create a new connection object.
 */
HttpConn *httpCreateConn(Http *http, HttpServer *server, MprDispatcher *dispatcher)
{
    HttpConn    *conn;
    HttpHost    *host;

    if ((conn = mprAllocObj(HttpConn, manageConn)) == 0) {
        return 0;
    }
    conn->http = http;
    conn->canProceed = 1;

    conn->protocol = http->protocol;
    conn->port = -1;
    conn->retries = HTTP_RETRIES;
    conn->server = server;
    conn->lastActivity = http->now;
    conn->ioCallback = httpEvent;

    if (server) {
        conn->notifier = server->notifier;
        host = mprGetFirstItem(server->hosts);
        conn->limits = (host) ? host->limits : http->serverLimits;
    } else {
        conn->limits = http->clientLimits;
    }
    conn->keepAliveCount = conn->limits->keepAliveCount;
    conn->serviceq = httpCreateQueueHead(conn, "serviceq");
    httpInitTrace(conn->trace);

    if (dispatcher) {
        conn->dispatcher = dispatcher;
    } else if (server) {
        conn->dispatcher = server->dispatcher;
    } else {
        conn->dispatcher = mprGetDispatcher();
    }
    httpSetState(conn, HTTP_STATE_BEGIN);
    httpAddConn(http, conn);
    return conn;
}


/*
    Destroy a connection. This removes the connection from the list of connections. Should GC after that.
 */
void httpDestroyConn(HttpConn *conn)
{
    if (conn->http) {
        if (conn->server) {
            httpValidateLimits(conn->server, HTTP_VALIDATE_CLOSE_CONN, conn);
        }
        if (HTTP_STATE_PARSED <= conn->state && conn->state < HTTP_STATE_COMPLETE) {
            HTTP_NOTIFY(conn, HTTP_STATE_COMPLETE, 0);
        }
        HTTP_NOTIFY(conn, -1, 0);
        httpRemoveConn(conn->http, conn);
        httpCloseConn(conn);
        conn->input = 0;
        if (conn->rx) {
            conn->rx->conn = 0;
            conn->rx = 0;
        }
        if (conn->tx) {
            conn->tx->conn = 0;
            conn->tx = 0;
        }
        conn->http = 0;
    }
}


static void manageConn(HttpConn *conn, int flags)
{
    mprAssert(conn);

    if (flags & MPR_MANAGE_MARK) {
        mprMark(conn->rx);
        mprMark(conn->tx);
        mprMark(conn->limits);
        mprMark(conn->http);
        mprMark(conn->stages);
        mprMark(conn->dispatcher);
        mprMark(conn->newDispatcher);
        mprMark(conn->oldDispatcher);
        mprMark(conn->waitHandler);
        mprMark(conn->server);
        mprMark(conn->host);
        mprMark(conn->sock);
        mprMark(conn->serviceq);
        mprMark(conn->currentq);
        mprMark(conn->input);
        mprMark(conn->readq);
        mprMark(conn->writeq);
        mprMark(conn->context);
        mprMark(conn->boundary);
        mprMark(conn->errorMsg);
        mprMark(conn->host);
        mprMark(conn->ip);
        mprMark(conn->protocol);
        mprMark(conn->headersCallbackArg);
        mprMark(conn->timeoutEvent);
        mprMark(conn->workerEvent);
        mprMark(conn->mark);

        httpManageTrace(&conn->trace[0], flags);
        httpManageTrace(&conn->trace[1], flags);

        mprMark(conn->authCnonce);
        mprMark(conn->authDomain);
        mprMark(conn->authNonce);
        mprMark(conn->authOpaque);
        mprMark(conn->authRealm);
        mprMark(conn->authQop);
        mprMark(conn->authType);
        mprMark(conn->authGroup);
        mprMark(conn->authUser);
        mprMark(conn->authPassword);

    } else if (flags & MPR_MANAGE_FREE) {
        httpDestroyConn(conn);
    }
}


/*  
    Close the connection but don't destroy the conn object.
 */
void httpCloseConn(HttpConn *conn)
{
    mprAssert(conn);

    if (conn->sock) {
        mprLog(6, "Closing connection");
        if (conn->waitHandler) {
            mprRemoveWaitHandler(conn->waitHandler);
            conn->waitHandler = 0;
        }
        mprCloseSocket(conn->sock, 0);
        conn->sock = 0;
    }
}


void httpConnTimeout(HttpConn *conn)
{
    HttpRx      *rx;
    HttpLimits  *limits;
    MprTime     now;

    rx = conn->rx;
    now = conn->http->now;
    mprAssert(rx);
    limits = conn->limits;
    mprAssert(limits);

    if (conn->state < HTTP_STATE_PARSED) {
        mprLog(6, "Inactive connection timed out");
        mprDisconnectSocket(conn->sock);
    } else {
        if ((conn->lastActivity + limits->inactivityTimeout) < now) {
            httpError(conn, HTTP_CODE_REQUEST_TIMEOUT,
                "Exceeded inactivity timeout of %d sec", limits->inactivityTimeout / 1000);

        } else if ((conn->started + limits->requestTimeout) < now) {
            httpError(conn, HTTP_CODE_REQUEST_TIMEOUT, "Exceeded timeout %d sec", limits->requestTimeout / 1000);
        }
        httpFinalize(conn);
#if UNUSED
        httpSetState(conn, HTTP_STATE_COMPLETE);
        httpProcess(conn, NULL);
#endif
        httpDisconnect(conn);
    }
}


static void commonPrep(HttpConn *conn)
{
    Http    *http;

    http = conn->http;
    lock(http);

    if (conn->timeoutEvent) {
        mprRemoveEvent(conn->timeoutEvent);
    }
    conn->canProceed = 1;
    conn->error = 0;
    conn->errorMsg = 0;
    conn->flags = 0;
    conn->state = 0;
    conn->writeComplete = 0;
    conn->lastActivity = conn->http->now;
    httpSetState(conn, HTTP_STATE_BEGIN);
    httpInitSchedulerQueue(conn->serviceq);
    unlock(http);
}


/*  
    Prepare a connection for a new request after completing a prior request.
 */
void httpPrepServerConn(HttpConn *conn)
{
    mprAssert(conn);
    mprAssert(conn->rx == 0);
    mprAssert(conn->tx == 0);
    mprAssert(conn->server);

    conn->readq = 0;
    conn->writeq = 0;
    commonPrep(conn);
}


void httpPrepClientConn(HttpConn *conn, int keepHeaders)
{
    MprHashTable    *headers;

    mprAssert(conn);

    if (conn->keepAliveCount >= 0 && conn->sock) {
        /* Eat remaining input incase last request did not consume all data */
        httpConsumeLastRequest(conn);
    } else {
        conn->input = 0;
    }
    if (conn->tx) {
        conn->tx->conn = 0;
    }
    headers = (keepHeaders && conn->tx) ? conn->tx->headers: NULL;
    conn->tx = httpCreateTx(conn, headers);
    if (conn->rx) {
        conn->rx->conn = 0;
    }
    conn->rx = httpCreateRx(conn);
#if UNUSED
    httpCreatePipeline(conn, NULL, NULL);
#endif
    commonPrep(conn);
}


void httpConsumeLastRequest(HttpConn *conn)
{
    MprTime     mark;
    char        junk[4096];

    if (!conn->sock || conn->state < HTTP_STATE_FIRST) {
        return;
    }
    mark = mprGetTime();
    while (!httpIsEof(conn) && mprGetRemainingTime(mark, conn->limits->requestTimeout) > 0) {
        if (httpRead(conn, junk, sizeof(junk)) <= 0) {
            break;
        }
    }
    if (HTTP_STATE_CONNECTED <= conn->state && conn->state < HTTP_STATE_COMPLETE) {
        conn->keepAliveCount = -1;
    }
}


void httpCallEvent(HttpConn *conn, int mask)
{
    MprEvent    e;

    e.mask = mask;
    e.timestamp = mprGetTime();
    httpEvent(conn, &e);
}


/*  
    IO event handler. This is invoked by the wait subsystem in response to I/O events. It is also invoked via relay when 
    an accept event is received by the server. Initially the conn->dispatcher will be set to the server->dispatcher and 
    the first I/O event will be handled on the server thread (or main thread). A request handler may create a new 
    conn->dispatcher and transfer execution to a worker thread if required.
 */
void httpEvent(HttpConn *conn, MprEvent *event)
{
    LOG(7, "httpEvent for fd %d, mask %d\n", conn->sock->fd, event->mask);
    conn->lastActivity = conn->http->now;

    if (event->mask & MPR_WRITABLE) {
        writeEvent(conn);
    }
    if (event->mask & MPR_READABLE) {
        readEvent(conn);
    }
    if (conn->server && conn->keepAliveCount < 0) {
        /*  
            NOTE: compare keepAliveCount with "< 0" so that the client can have one more keep alive request. 
            It should respond to the "Connection: close" and thus initiate a client-led close. This reduces 
            TIME_WAIT states on the server. NOTE: after httpDestroyConn, conn structure and memory is still 
            intact but conn->sock is zero.
         */
        httpDestroyConn(conn);
    } else {
        httpEnableConnEvents(conn);
    }
}


/*
    Process a socket readable event
 */
static void readEvent(HttpConn *conn)
{
    HttpPacket  *packet;
    ssize       nbytes, len;

    while (!conn->connError && (packet = getPacket(conn, &len)) != 0) {
        nbytes = mprReadSocket(conn->sock, mprGetBufEnd(packet->content), len);
        LOG(8, "http: read event. Got %d", nbytes);
       
        if (nbytes > 0) {
            mprAdjustBufEnd(packet->content, nbytes);
            httpProcess(conn, packet);

        } else if (nbytes < 0) {
            if (conn->state <= HTTP_STATE_CONNECTED) {
                if (mprIsSocketEof(conn->sock)) {
                    conn->keepAliveCount = -1;
                }
                break;
            } else if (conn->state < HTTP_STATE_COMPLETE) {
                httpProcess(conn, packet);
                if (!conn->error && conn->state < HTTP_STATE_COMPLETE && mprIsSocketEof(conn->sock)) {
                    httpError(conn, HTTP_ABORT | HTTP_CODE_COMMS_ERROR, "Connection lost");
                    break;
                }
            }
            break;
        }
        if (nbytes == 0 || conn->state >= HTTP_STATE_RUNNING || conn->workerEvent) {
            break;
        }
        if (conn->readq && conn->readq->count > conn->readq->max) {
            break;
        }
    }
}


static void writeEvent(HttpConn *conn)
{
    //  MOB 6
    LOG(3, "httpProcessWriteEvent, state %d", conn->state);

    conn->writeBlocked = 0;
    if (conn->tx) {
        httpEnableQueue(conn->tx->queue[HTTP_QUEUE_TRANS]->prevQ);
        httpServiceQueues(conn);
        httpProcess(conn, NULL);
    }
}


/*
    This can be called by any thread
 */
void httpEnableConnEvents(HttpConn *conn)
{
    HttpTx      *tx;
    HttpQueue   *q;
    MprEvent    *event;
    int         eventMask;

    mprLog(7, "EnableConnEvents");

    if (!conn->async) {
        return;
    }
    tx = conn->tx;
    eventMask = 0;
    conn->lastActivity = conn->http->now;

    if (conn->state < HTTP_STATE_COMPLETE && conn->sock && !mprIsSocketEof(conn->sock)) {
        lock(conn->http);
        if (conn->workerEvent) {
            event = conn->workerEvent;
            conn->workerEvent = 0;
            conn->dispatcher = conn->newDispatcher;
            mprQueueEvent(conn->dispatcher, event);
            unlock(conn->http);
            return;
        }
        if (tx) {
            //  MOB OPT - can we just test writeBlocked?
            if (conn->writeBlocked || tx->queue[HTTP_QUEUE_TRANS]->prevQ->count > 0) {
                eventMask |= MPR_WRITABLE;
            }
            /*
                Allow read events even if the current request is not complete. The pipelined request will be buffered 
                and will be ready when the current request completes.
             */
            q = tx->queue[HTTP_QUEUE_RECEIVE]->nextQ;
            if (q->count < q->max || conn->recall) {
                eventMask |= MPR_READABLE;
            }
        } else {
            eventMask |= MPR_READABLE;
        }
        if (eventMask) {
            if (conn->waitHandler == 0) {
                conn->waitHandler = mprCreateWaitHandler(conn->sock->fd, eventMask, conn->dispatcher, conn->ioCallback, 
                    conn, 0);
            } else if (eventMask != conn->waitHandler->desiredMask) {
                mprEnableWaitEvents(conn->waitHandler, eventMask);
            }
        } else {
            if (conn->waitHandler && eventMask != conn->waitHandler->desiredMask) {
                mprEnableWaitEvents(conn->waitHandler, eventMask);
            }
        }
#if UNUSED
        if (conn->recall && conn->waitHandler) {
            mprRecallWaitHandler(conn->waitHandler);
        }
#endif
        conn->recall = 0;
        mprAssert(conn->dispatcher->enabled);
        unlock(conn->http);
    }
}


void httpFollowRedirects(HttpConn *conn, bool follow)
{
    conn->followRedirects = follow;
}


/*  
    Get the packet into which to read data. This may be owned by the connection or if mid-request, may be owned by the
    request. Also return in *bytesToRead the length of data to attempt to read.
 */
static HttpPacket *getPacket(HttpConn *conn, ssize *bytesToRead)
{
    HttpPacket  *packet;
    MprBuf      *content;
    HttpRx      *rx;
    MprOff      remaining;
    ssize       len;

    rx = conn->rx;
    len = HTTP_BUFSIZE;

    //  MOB -- simplify. Okay to lose some optimization for chunked data?
    /*  
        The input packet may have pipelined headers and data left from the prior request. It may also have incomplete
        chunk boundary data.
     */
    if ((packet = conn->input) == NULL) {
        conn->input = packet = httpCreatePacket(len);
    } else {
        content = packet->content;
        mprResetBufIfEmpty(content);
        mprAddNullToBuf(content);
        if (rx) {
            /*  
                Don't read more than the remainingContent unless chunked. We do this to minimize requests split 
                accross packets.
             */
            if (rx->remainingContent) {
                remaining = rx->remainingContent;
                if (rx->flags & HTTP_CHUNKED) {
                    remaining = max(remaining, HTTP_BUFSIZE);
                }
                len = (ssize) min(remaining, MAXSSIZE);
            }
            len = min(len, HTTP_BUFSIZE);
            mprAssert(len > 0);
            if (mprGetBufSpace(content) < len) {
                mprGrowBuf(content, (ssize) len);
            }
        } else {
            if (mprGetBufSpace(content) < HTTP_BUFSIZE) {
                mprGrowBuf(content, HTTP_BUFSIZE);
            }
            len = mprGetBufSpace(content);
        }
    }
    mprAssert(packet == conn->input);
    mprAssert(len > 0);
    *bytesToRead = (ssize) len;
    return packet;
}


/*
    Called by connectors when writing the transmission is complete
 */
void httpCompleteWriting(HttpConn *conn)
{
    conn->writeComplete = 1;
    if (conn->tx) {
        conn->tx->finalized = 1;
    }
}


int httpGetAsync(HttpConn *conn)
{
    return conn->async;
}


ssize httpGetChunkSize(HttpConn *conn)
{
    if (conn->tx) {
        return conn->tx->chunkSize;
    }
    return 0;
}


void *httpGetConnContext(HttpConn *conn)
{
    return conn->context;
}


void *httpGetConnHost(HttpConn *conn)
{
    return conn->host;
}


cchar *httpGetError(HttpConn *conn)
{
    if (conn->errorMsg) {
        return conn->errorMsg;
    } else if (conn->state >= HTTP_STATE_FIRST) {
        return httpLookupStatus(conn->http, conn->rx->status);
    } else {
        return "";
    }
}


void httpResetCredentials(HttpConn *conn)
{
    conn->authType = 0;
    conn->authDomain = 0;
    conn->authCnonce = 0;
    conn->authNonce = 0;
    conn->authOpaque = 0;
    conn->authPassword = 0;
    conn->authRealm = 0;
    conn->authQop = 0;
    conn->authType = 0;
    conn->authUser = 0;
    
    httpRemoveHeader(conn, "Authorization");
}


void httpSetAsync(HttpConn *conn, int enable)
{
    conn->async = (enable) ? 1 : 0;
}


void httpSetConnNotifier(HttpConn *conn, HttpNotifier notifier)
{
    conn->notifier = notifier;
}


#if UNUSED
void httpSetRequestNotifier(HttpConn *conn, HttpNotifier notifier)
{
    conn->requestNotifier = notifier;
}
#endif

void httpSetCredentials(HttpConn *conn, cchar *user, cchar *password)
{
    httpResetCredentials(conn);
    conn->authUser = sclone(user);
    if (password == NULL && strchr(user, ':') != 0) {
        conn->authUser = stok(conn->authUser, ":", &conn->authPassword);
        conn->authPassword = sclone(conn->authPassword);
    } else {
        conn->authPassword = sclone(password);
    }
}


void httpSetKeepAliveCount(HttpConn *conn, int count)
{
    conn->keepAliveCount = count;
}


void httpSetChunkSize(HttpConn *conn, ssize size)
{
    if (conn->tx) {
        conn->tx->chunkSize = size;
    }
}


void httpSetHeadersCallback(HttpConn *conn, HttpHeadersCallback fn, void *arg)
{
    conn->headersCallback = fn;
    conn->headersCallbackArg = arg;
}


void httpSetIOCallback(HttpConn *conn, HttpIOCallback fn)
{
    conn->ioCallback = fn;
}


void httpSetConnContext(HttpConn *conn, void *context)
{
    conn->context = context;
}


void httpSetConnHost(HttpConn *conn, void *host)
{
    conn->host = host;
}


/*  
    Set the protocol to use for outbound requests
 */
void httpSetProtocol(HttpConn *conn, cchar *protocol)
{
    if (conn->state < HTTP_STATE_CONNECTED) {
        conn->protocol = sclone(protocol);
        if (strcmp(protocol, "HTTP/1.0") == 0) {
            conn->keepAliveCount = -1;
        }
    }
}


void httpSetRetries(HttpConn *conn, int count)
{
    conn->retries = count;
}


static char *notifyState[] = {
    "IO_EVENT", "BEGIN", "STARTED", "FIRST", "PARSED", "CONTENT", "RUNNING", "COMPLETE",
};


void httpSetState(HttpConn *conn, int state)
{
    if (state == conn->state) {
        return;
    }
    if (state < conn->state) {
        /* Prevent regressions */
        return;
    }
    conn->state = state;
    LOG(6, "Connection state change to %s", notifyState[state]);
    HTTP_NOTIFY(conn, state, 0);
}


/*
    Set each timeout arg to -1 to skip. Set to zero for no timeout. Otherwise set to number of msecs
 */
void httpSetTimeout(HttpConn *conn, int requestTimeout, int inactivityTimeout)
{
    if (requestTimeout >= 0) {
        if (requestTimeout == 0) {
            conn->limits->requestTimeout = MAXINT;
        } else {
            conn->limits->requestTimeout = requestTimeout;
        }
    }
    if (inactivityTimeout >= 0) {
        if (inactivityTimeout == 0) {
            conn->limits->inactivityTimeout = MAXINT;
        } else {
            conn->limits->inactivityTimeout = inactivityTimeout;
        }
    }
}


HttpLimits *httpSetUniqueConnLimits(HttpConn *conn)
{
    HttpLimits      *limits;

    limits = mprAllocObj(HttpLimits, NULL);
    *limits = *conn->limits;
    conn->limits = limits;
    return limits;
}


void httpWritable(HttpConn *conn)
{
    HTTP_NOTIFY(conn, 0, HTTP_NOTIFY_WRITABLE);
}


void httpFormatErrorV(HttpConn *conn, int status, cchar *fmt, va_list args)
{
    if (conn->errorMsg == 0) {
        conn->errorMsg = mprAsprintfv(fmt, args);
        if (status) {
            if (conn->server && conn->tx) {
                conn->tx->status = status;
            } else if (conn->rx) {
                conn->rx->status = status;
            }
        }
        if (conn->rx == 0 || conn->rx->uri == 0) {
            mprLog(2, "\"%s\", status %d: %s.", httpLookupStatus(conn->http, status), status, conn->errorMsg);
        } else {
            mprLog(2, "Error: \"%s\", status %d for URI \"%s\": %s.",
                httpLookupStatus(conn->http, status), status, conn->rx->uri ? conn->rx->uri : "", conn->errorMsg);
        }
    }
}


/*
    Just format conn->errorMsg and set status - nothing more
 */
void httpFormatError(HttpConn *conn, int status, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt); 
    httpFormatErrorV(conn, status, fmt, args);
    va_end(args); 
}


/*
    The current request has an error and cannot complete as normal. This call sets the Http response status and 
    overrides the normal output with an alternate error message. If the output has alread started (headers sent), then
    the connection MUST be closed so the client can get some indication the request failed.
 */
static void httpErrorV(HttpConn *conn, int flags, cchar *fmt, va_list args)
{
    HttpRx      *rx;
    HttpTx      *tx;
    int         status;

    mprAssert(fmt);
    rx = conn->rx;
    tx = conn->tx;

    if (flags & HTTP_ABORT) {
        conn->connError = 1;
    }
    conn->error = 1;
    status = flags & HTTP_CODE_MASK;
    httpFormatErrorV(conn, status, fmt, args);

    if (flags & (HTTP_ABORT | HTTP_CLOSE)) {
        conn->keepAliveCount = -1;
    }
    if (conn->server) {
        /*
            Server side must not call httpCloseConn() as it will remove wait handlers.
         */
        if (flags & HTTP_ABORT || (tx && tx->flags & HTTP_TX_HEADERS_CREATED)) {
            /* 
                If headers have been sent, must let the client know the request has failed - abort is the only way.
                Disconnect will cause a readable (EOF) event
             */
            httpDisconnect(conn);
        } else {
            httpSetResponseBody(conn, status, conn->errorMsg);
        }
    } else {
        if (flags & HTTP_ABORT || (tx && tx->flags & HTTP_TX_HEADERS_CREATED)) {
            httpCloseConn(conn);
        }
    }
}


void httpError(HttpConn *conn, int flags, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    httpErrorV(conn, flags, fmt, args);
    va_end(args);
}


void httpDisconnect(HttpConn *conn)
{
    if (conn->sock) {
        mprDisconnectSocket(conn->sock);
    }
    conn->connError = 1;
    conn->keepAliveCount = -1;
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
