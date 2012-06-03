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
HttpConn *httpCreateConn(Http *http, HttpEndpoint *endpoint, MprDispatcher *dispatcher)
{
    HttpConn    *conn;
    HttpHost    *host;
    HttpRoute   *route;

    if ((conn = mprAllocObj(HttpConn, manageConn)) == 0) {
        return 0;
    }
    conn->http = http;
    conn->canProceed = 1;

    conn->protocol = http->protocol;
    conn->port = -1;
    conn->retries = HTTP_RETRIES;
    conn->endpoint = endpoint;
    conn->lastActivity = http->now;
    conn->ioCallback = httpEvent;

    if (endpoint) {
        conn->notifier = endpoint->notifier;
        host = mprGetFirstItem(endpoint->hosts);
        if (host && (route = host->defaultRoute) != 0) {
            conn->limits = route->limits;
            conn->trace[0] = route->trace[0];
            conn->trace[1] = route->trace[1];
        } else {
            conn->limits = http->serverLimits;
            httpInitTrace(conn->trace);
        }
    } else {
        conn->limits = http->clientLimits;
        httpInitTrace(conn->trace);
    }
    conn->keepAliveCount = conn->limits->keepAliveMax;
    conn->serviceq = httpCreateQueueHead(conn, "serviceq");

    if (dispatcher) {
        conn->dispatcher = dispatcher;
    } else if (endpoint) {
        conn->dispatcher = endpoint->dispatcher;
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
        mprAssert(conn->http);
        httpRemoveConn(conn->http, conn);
        if (conn->endpoint) {
            if (conn->rx) {
                httpValidateLimits(conn->endpoint, HTTP_VALIDATE_CLOSE_REQUEST, conn);
            }
            httpValidateLimits(conn->endpoint, HTTP_VALIDATE_CLOSE_CONN, conn);
        }
        if (HTTP_STATE_PARSED <= conn->state && conn->state < HTTP_STATE_COMPLETE) {
            HTTP_NOTIFY(conn, HTTP_STATE_COMPLETE, 0);
        }
        HTTP_NOTIFY(conn, HTTP_EVENT_CLOSE, 0);
        conn->input = 0;
        if (conn->tx) {
            httpDestroyPipeline(conn);
            conn->tx->conn = 0;
            conn->tx = 0;
        }
        if (conn->rx) {
            conn->rx->conn = 0;
            conn->rx = 0;
        }
        httpCloseConn(conn);
        conn->http = 0;
    }
}


static void manageConn(HttpConn *conn, int flags)
{
    mprAssert(conn);

    if (flags & MPR_MANAGE_MARK) {
        mprMark(conn->rx);
        mprMark(conn->tx);
        mprMark(conn->endpoint);
        mprMark(conn->host);
        mprMark(conn->limits);
        mprMark(conn->http);
        mprMark(conn->stages);
        mprMark(conn->dispatcher);
        mprMark(conn->newDispatcher);
        mprMark(conn->oldDispatcher);
        mprMark(conn->waitHandler);
        mprMark(conn->sock);
        mprMark(conn->serviceq);
        mprMark(conn->currentq);
        mprMark(conn->input);
        mprMark(conn->readq);
        mprMark(conn->writeq);
        mprMark(conn->connectorq);
        mprMark(conn->timeoutEvent);
        mprMark(conn->workerEvent);
        mprMark(conn->context);
        mprMark(conn->ejs);
        mprMark(conn->pool);
        mprMark(conn->mark);
        mprMark(conn->data);
        mprMark(conn->grid);
        mprMark(conn->record);
        mprMark(conn->boundary);
        mprMark(conn->errorMsg);
        mprMark(conn->ip);
        mprMark(conn->protocol);
        httpManageTrace(&conn->trace[0], flags);
        httpManageTrace(&conn->trace[1], flags);

        mprMark(conn->authCnonce);
        mprMark(conn->authDomain);
        mprMark(conn->authNonce);
        mprMark(conn->authOpaque);
        mprMark(conn->authRealm);
        mprMark(conn->authQop);
        mprMark(conn->authType);
        mprMark(conn->authUser);
        mprMark(conn->authPassword);
        mprMark(conn->headersCallbackArg);

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
    HttpLimits  *limits;
    MprTime     now;

    if (!conn->http) {
        return;
    }
    now = conn->http->now;
    limits = conn->limits;
    mprAssert(limits);

    mprLog(6, "Inactive connection timed out");
    if (conn->state >= HTTP_STATE_PARSED) {
        if ((conn->lastActivity + limits->inactivityTimeout) < now) {
            httpError(conn, HTTP_CODE_REQUEST_TIMEOUT,
                "Exceeded inactivity timeout of %Ld sec", limits->inactivityTimeout / 1000);

        } else if ((conn->started + limits->requestTimeout) < now) {
            httpError(conn, HTTP_CODE_REQUEST_TIMEOUT, "Exceeded timeout %d sec", limits->requestTimeout / 1000);
        }
    }
    httpDisconnect(conn);
    httpDiscardQueueData(conn->writeq, 1);
    httpEnableConnEvents(conn);
    conn->timeoutEvent = 0;
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
    conn->connError = 0;
    conn->errorMsg = 0;
    conn->state = 0;
    conn->responded = 0;
    conn->finalized = 0;
    conn->refinalize = 0;
    conn->connectorComplete = 0;
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
    mprAssert(conn->endpoint);

    conn->readq = 0;
    conn->writeq = 0;
    commonPrep(conn);
}


void httpPrepClientConn(HttpConn *conn, bool keepHeaders)
{
    MprHash     *headers;

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
    commonPrep(conn);
}


void httpConsumeLastRequest(HttpConn *conn)
{
    MprTime     mark;
    char        junk[4096];

    if (!conn->sock) {
        return;
    }
    if (conn->state >= HTTP_STATE_FIRST) {
        mark = conn->http->now;
        while (!httpIsEof(conn) && mprGetRemainingTime(mark, conn->limits->requestTimeout) > 0) {
            if (httpRead(conn, junk, sizeof(junk)) <= 0) {
                break;
            }
        }
    }
    if (HTTP_STATE_CONNECTED <= conn->state && conn->state < HTTP_STATE_COMPLETE) {
        conn->keepAliveCount = -1;
    }
}


void httpCallEvent(HttpConn *conn, int mask)
{
    MprEvent    e;

    if (conn->http) {
        e.mask = mask;
        e.timestamp = conn->http->now;
        httpEvent(conn, &e);
    }
}


/*  
    IO event handler. This is invoked by the wait subsystem in response to I/O events. It is also invoked via 
    relay when an accept event is received by the server. Initially the conn->dispatcher will be set to the
    server->dispatcher and the first I/O event will be handled on the server thread (or main thread). A request handler
    may create a new conn->dispatcher and transfer execution to a worker thread if required.
 */
void httpEvent(HttpConn *conn, MprEvent *event)
{
    LOG(5, "httpEvent for fd %d, mask %d\n", conn->sock->fd, event->mask);
    conn->lastActivity = conn->http->now;

    if (event->mask & MPR_WRITABLE) {
        writeEvent(conn);
    }
    if (event->mask & MPR_READABLE) {
        readEvent(conn);
    }
    if (conn->endpoint) {
        if (conn->error || (conn->keepAliveCount < 0 && conn->state <= HTTP_STATE_CONNECTED)) {
            /*  
                Either an unhandled error or an Idle connection.
                NOTE: compare keepAliveCount with "< 0" so that the client can have one more keep alive request. 
                It should respond to the "Connection: close" and thus initiate a client-led close. This reduces 
                TIME_WAIT states on the server. NOTE: after httpDestroyConn, conn structure and memory is still 
                intact but conn->sock is zero.
             */
            httpDestroyConn(conn);

        } else if (conn->sock) {
            if (mprIsSocketEof(conn->sock)) {
                httpDestroyConn(conn);
            } else {
                mprAssert(conn->state < HTTP_STATE_COMPLETE);
                httpEnableConnEvents(conn);
            }
        }
    } else if (conn->state < HTTP_STATE_COMPLETE) {
        httpEnableConnEvents(conn);
    }
    mprYield(0);
}


/*
    Process a socket readable event
 */
static void readEvent(HttpConn *conn)
{
    HttpPacket  *packet;
    ssize       nbytes, size;

    while (!conn->connError && (packet = getPacket(conn, &size)) != 0) {
        nbytes = mprReadSocket(conn->sock, mprGetBufEnd(packet->content), size);
        LOG(8, "http: read event. Got %d", nbytes);
       
        if (nbytes > 0) {
            mprAdjustBufEnd(packet->content, nbytes);
            httpPump(conn, packet);

        } else if (nbytes < 0) {
            if (conn->state <= HTTP_STATE_CONNECTED) {
                if (mprIsSocketEof(conn->sock)) {
                    conn->keepAliveCount = -1;
                }
                break;
            } else if (conn->state < HTTP_STATE_COMPLETE) {
                httpPump(conn, packet);
                if (!conn->error && conn->state < HTTP_STATE_COMPLETE && mprIsSocketEof(conn->sock)) {
                    httpError(conn, HTTP_ABORT | HTTP_CODE_COMMS_ERROR, "Connection lost");
                    break;
                }
            }
            break;
        }
        //  MOB - refactor these tests
        if (nbytes == 0 || conn->state >= HTTP_STATE_READY || !conn->canProceed) {
            break;
        }
        if (conn->readq && conn->readq->count > conn->readq->max) {
            break;
        }
        if (mprDispatcherHasEvents(conn->dispatcher)) {
            break;
        }
    }
}


static void writeEvent(HttpConn *conn)
{
    LOG(6, "httpProcessWriteEvent, state %d", conn->state);

    conn->writeBlocked = 0;
    if (conn->tx) {
        httpResumeQueue(conn->connectorq);
        httpServiceQueues(conn);
        httpPump(conn, NULL);
    }
}


void httpUseWorker(HttpConn *conn, MprDispatcher *dispatcher, MprEvent *event)
{
    lock(conn->http);
    conn->oldDispatcher = conn->dispatcher;
    conn->dispatcher = dispatcher;
    conn->worker = 1;
    mprAssert(!conn->workerEvent);
    conn->workerEvent = event;
    unlock(conn->http);
}


void httpUsePrimary(HttpConn *conn)
{
    lock(conn->http);
    mprAssert(conn->worker);
    mprAssert(conn->state == HTTP_STATE_BEGIN);
    mprAssert(conn->oldDispatcher && conn->dispatcher != conn->oldDispatcher);
    conn->dispatcher = conn->oldDispatcher;
    conn->oldDispatcher = 0;
    conn->worker = 0;
    unlock(conn->http);
}


/*
    Steal a connection with open socket from Http and disconnect it from management by Http.
    It is the callers responsibility to call mprCloseSocket when required.
 */
MprSocket *httpStealConn(HttpConn *conn)
{
    MprSocket   *sock;

    sock = conn->sock;
    conn->sock = 0;

    if (conn->waitHandler) {
        mprRemoveWaitHandler(conn->waitHandler);
        conn->waitHandler = 0;
    }
    if (conn->http) {
        lock(conn->http);
        httpRemoveConn(conn->http, conn);
        httpDiscardData(conn, HTTP_QUEUE_TX);
        httpDiscardData(conn, HTTP_QUEUE_RX);
        httpSetState(conn, HTTP_STATE_COMPLETE);
        unlock(conn->http);
    }
    return sock;
}


void httpEnableConnEvents(HttpConn *conn)
{
    HttpTx      *tx;
    HttpRx      *rx;
    HttpQueue   *q;
    MprEvent    *event;
    int         eventMask;

    mprLog(7, "EnableConnEvents");

    if (!conn->async || !conn->sock || mprIsSocketEof(conn->sock)) {
        return;
    }
    tx = conn->tx;
    rx = conn->rx;
    eventMask = 0;
    conn->lastActivity = conn->http->now;

    if (conn->workerEvent) {
        event = conn->workerEvent;
        conn->workerEvent = 0;
        mprQueueEvent(conn->dispatcher, event);

    } else {
        lock(conn->http);
        if (tx) {
            /*
                Can be writeBlocked with data in the iovec and none in the queue
             */
            if (conn->writeBlocked || (conn->connectorq && conn->connectorq->count > 0)) {
                eventMask |= MPR_WRITABLE;
            }
            /*
                Enable read events if the read queue is not full. 
             */
            q = tx->queue[HTTP_QUEUE_RX]->nextQ;
            if (q->count < q->max || rx->form) {
                eventMask |= MPR_READABLE;
            }
        } else {
            eventMask |= MPR_READABLE;
        }
        if (eventMask) {
            if (conn->waitHandler == 0) {
                conn->waitHandler = mprCreateWaitHandler(conn->sock->fd, eventMask, conn->dispatcher, conn->ioCallback, 
                    conn, 0);
            } else {
                conn->waitHandler->dispatcher = conn->dispatcher;
                mprWaitOn(conn->waitHandler, eventMask);
            }
        } else if (conn->waitHandler) {
            mprWaitOn(conn->waitHandler, eventMask);
        }
        mprAssert(conn->dispatcher->enabled);
        unlock(conn->http);
    }
    if (tx && tx->handler && tx->handler->module) {
        tx->handler->module->lastActivity = conn->lastActivity;
    }
}


void httpFollowRedirects(HttpConn *conn, bool follow)
{
    conn->followRedirects = follow;
}


/*  
    Get the packet into which to read data. Return in *size the length of data to attempt to read.
 */
static HttpPacket *getPacket(HttpConn *conn, ssize *size)
{
    HttpPacket  *packet;
    MprBuf      *content;

    if ((packet = conn->input) == NULL) {
        conn->input = packet = httpCreatePacket(HTTP_BUFSIZE);
    } else {
        content = packet->content;
        mprResetBufIfEmpty(content);
        mprAddNullToBuf(content);
        if (mprGetBufSpace(content) < HTTP_BUFSIZE) {
            mprGrowBuf(content, HTTP_BUFSIZE);
        }
    }
    *size = mprGetBufSpace(packet->content);
    mprAssert(*size > 0);
    return packet;
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
    "IO_EVENT", "BEGIN", "STARTED", "FIRST", "PARSED", "CONTENT", "READY", "RUNNING", "COMPLETE", 
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

    if ((limits = mprAllocStruct(HttpLimits)) != 0) {
        *limits = *conn->limits;
        conn->limits = limits;
    }
    return limits;
}


void httpNotifyWritable(HttpConn *conn)
{
    HTTP_NOTIFY(conn, HTTP_EVENT_IO, HTTP_NOTIFY_WRITABLE);
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
