/*
    conn.c -- Connection module to handle individual HTTP connections.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static int connectionDestructor(HttpConn *conn);
static inline HttpPacket *getPacket(HttpConn *conn, int *bytesToRead);
static void readEvent(HttpConn *conn);
static void writeEvent(HttpConn *conn);

/*********************************** Code *************************************/
/*
    Create a new connection object.
 */
HttpConn *httpCreateConn(Http *http, HttpServer *server)
{
    HttpConn        *conn;

    conn = mprAllocObjWithDestructorZeroed(http, HttpConn, connectionDestructor);
    if (conn == 0) {
        return 0;
    }
    conn->http = http;
    conn->canProceed = 1;
    conn->limits = (server) ? server->limits : http->clientLimits;
    conn->keepAliveCount = (conn->limits->keepAliveCount) ? conn->limits->keepAliveCount : -1;
    conn->waitHandler.fd = -1;

    //  MOB -- should this come from http?
    conn->protocol = "HTTP/1.1";
    conn->port = -1;
    conn->retries = HTTP_RETRIES;
    conn->server = server;
    conn->time = mprGetTime(conn);
    conn->lastActivity = conn->time;
    conn->requestTimeout = conn->limits->requestTimeout;
    conn->inactivityTimeout = conn->limits->inactivityTimeout;
    conn->callback = (HttpCallback) httpEvent;
    conn->callbackArg = conn;

    conn->traceMask = HTTP_TRACE_TRANSMIT | HTTP_TRACE_RECEIVE | HTTP_TRACE_HEADERS;
    conn->traceLevel = HTTP_TRACE_LEVEL;
    conn->traceMaxLength = INT_MAX;

    httpInitSchedulerQueue(&conn->serviceq);
    if (server) {
        conn->dispatcher = server->dispatcher;
        conn->notifier = server->notifier;
    }
    httpSetState(conn, HTTP_STATE_BEGIN);
    httpAddConn(http, conn);
    return conn;
}


/*  
    Cleanup a connection. Invoked automatically whenever the connection is freed.
 */
static int connectionDestructor(HttpConn *conn)
{
    mprAssert(conn);

    if (conn->server) {
        httpValidateLimits(conn->server, HTTP_EVENT_CLOSE_REQUEST, conn);
    }
    HTTP_NOTIFY(conn, HTTP_STATE_FREE_CONN, 0);
    httpRemoveConn(conn->http, conn);
    if (conn->sock) {
        httpCloseConn(conn);
    }
    return 0;
}


void httpCloseClientConn(HttpConn *conn)
{
    httpCloseConn(conn);
}


/*  
    Close the connection (but don't free). 
    WARNING: Once this is called, you can't get wait handler events. So handlers must not call this. 
    Rather, handlers should call mprDisconnectSocket that will cause a readable event to come and readEvent can
    then do an orderly close and free the connection structure.
 */
void httpCloseConn(HttpConn *conn)
{
    mprAssert(conn);

#if UNUSED
    conn->keepAliveCount = -1;
#endif
#if UNUSED
    if (HTTP_STATE_PARSED <= conn->state && conn->state < HTTP_STATE_COMPLETE) {
        if (conn->transmitter) {
            httpDiscardTransmitData(conn);
            httpCompleteRequest(conn);
        }
    }
#endif
    if (conn->sock) {
        mprLog(conn, 4, "Closing connection");
        if (conn->waitHandler.fd >= 0) {
            mprRemoveWaitHandler(&conn->waitHandler);
        }
        mprCloseSocket(conn->sock, 0);
        mprFree(conn->sock);
        conn->sock = 0;
    }
}


/*  
    Prepare a connection for a new request after completing a prior request.
 */
void httpPrepServerConn(HttpConn *conn)
{
    mprAssert(conn);

    if (conn->state != HTTP_STATE_BEGIN) {
        conn->canProceed = 1;
        conn->receiver = 0;
        conn->transmitter = 0;
        conn->error = 0;
        conn->flags = 0;
        conn->errorMsg = 0;
        conn->state = 0;
        conn->traceMask = 0;
        httpSetState(conn, HTTP_STATE_BEGIN);
        httpInitSchedulerQueue(&conn->serviceq);
    }
}


void httpConsumeLastRequest(HttpConn *conn)
{
    MprTime     mark;
    char        junk[4096];
    int         requestTimeout, rc;

    if (!conn->sock || conn->state < HTTP_STATE_WAIT) {
        return;
    }
    mark = mprGetTime(conn);
    requestTimeout = conn->requestTimeout ? conn->requestTimeout : INT_MAX;
    while (!httpIsEof(conn) && mprGetRemainingTime(conn, mark, requestTimeout) > 0) {
        if ((rc = httpRead(conn, junk, sizeof(junk))) <= 0) {
            break;
        }
    }
    if (HTTP_STATE_STARTED <= conn->state && conn->state < HTTP_STATE_COMPLETE) {
        conn->keepAliveCount = -1;
    }
}


void httpPrepClientConn(HttpConn *conn, int retry)
{
    MprHashTable    *headers;
    HttpTransmitter *trans;

    mprAssert(conn);

    headers = 0;
    if (conn->state != HTTP_STATE_BEGIN) {
        if (conn->keepAliveCount >= 0) {
            httpConsumeLastRequest(conn);
        }
        if (retry && (trans = conn->transmitter) != 0) {
            headers = trans->headers;
            mprStealBlock(conn, headers);
        }
        conn->canProceed = 1;
        conn->error = 0;
        conn->flags = 0;
        conn->errorMsg = 0;
        conn->state = 0;
        httpSetState(conn, HTTP_STATE_BEGIN);
        httpInitSchedulerQueue(&conn->serviceq);
        mprFree(conn->transmitter);
        mprFree(conn->receiver);
        conn->receiver = httpCreateReceiver(conn);
        conn->transmitter = httpCreateTransmitter(conn, headers);
        httpCreatePipeline(conn, NULL, NULL);
    }
}


void httpCallEvent(HttpConn *conn, int mask)
{
    MprEvent    e;

    e.mask = mask;
    e.timestamp = mprGetTime(conn);
    httpEvent(conn, &e);
}


/*  
    IO event handler. This is invoked by the wait subsystem in response to I/O events. It is also invoked via relay
    when an accept event is received by the server.

    Initially the conn->dispatcher will be set to the server->dispatcher and the first I/O event
    will be handled on the server thread (or main thread). A request handler may create a new conn->dispatcher and
    transfer execution to a worker thread if required.
 */
void httpEvent(HttpConn *conn, MprEvent *event)
{
    LOG(conn, 7, "httpEvent for fd %d, mask %d\n", conn->sock->fd, event->mask);

    conn->lastActivity = conn->time = event->timestamp;
    mprAssert(conn->time);

    if (event->mask & MPR_WRITABLE) {
        writeEvent(conn);
    }
    if (event->mask & MPR_READABLE) {
        readEvent(conn);
    }
    if (conn->server) {
        if (mprIsSocketEof(conn->sock) || (!conn->receiver && conn->keepAliveCount < 0)) {
            /*  
                NOTE: compare keepAliveCount with "< 0" so that the client can have one more keep alive request. 
                It should respond to the "Connection: close" and thus initiate a client-led close. 
                This reduces TIME_WAIT states on the server. 
             */
            mprFree(conn);
            return;
        }
    }
    httpEnableConnEvents(conn);
}


/*
    Process a socket readable event
 */
static void readEvent(HttpConn *conn)
{
    HttpPacket  *packet;
    MprBuf      *content;
    int         nbytes, len;

    while ((packet = getPacket(conn, &len)) != 0) {
        mprAssert(len > 0);
        content = packet->content;
        nbytes = mprReadSocket(conn->sock, mprGetBufEnd(content), len);
        LOG(conn, 8, "http: read event. Got %d", nbytes);
       
        if (nbytes > 0) {
            mprAdjustBufEnd(content, nbytes);
            httpAdvanceReceiver(conn, packet);
        } else {
            if (nbytes < 0) {
                if (mprIsSocketEof(conn->sock) && conn->receiver && conn->state > HTTP_STATE_WAIT) {
                   httpAdvanceReceiver(conn, packet);
                } 
                if (conn->state > HTTP_STATE_STARTED && conn->state < HTTP_STATE_PROCESS) {
                    httpConnError(conn, HTTP_CODE_COMMS_ERROR, "Communications read error. State %d", conn->state);
                }
            }
            break;
        }
        if (conn->connError) {
            break;
        }
        //  MOB -- why break if state < begin?
        if (conn->state < HTTP_STATE_BEGIN || conn->state >= HTTP_STATE_PROCESS || conn->startingThread) {
            break;
        }
    }
}


static void writeEvent(HttpConn *conn)
{
    LOG(conn, 6, "httpProcessWriteEvent, state %d", conn->state);
    conn->writeBlocked = 0;
    if (conn->transmitter) {
        httpEnableQueue(conn->transmitter->queue[HTTP_QUEUE_TRANS].prevQ);
        httpServiceQueues(conn);
    }
}


void httpEnableConnEvents(HttpConn *conn)
{
    HttpTransmitter     *trans;
    HttpQueue           *q;
    int                 eventMask;

    if (!conn->async) {
        return;
    }
    trans = conn->transmitter;
    eventMask = 0;
    conn->lastActivity = conn->time;

    if (conn->state < HTTP_STATE_COMPLETE && conn->sock && !mprIsSocketEof(conn->sock)) {
        if (trans) {
            if (trans->queue[HTTP_QUEUE_TRANS].prevQ->count > 0) {
                eventMask |= MPR_WRITABLE;
            } else {
                mprAssert(!conn->writeBlocked);
            } 
            /*
                Allow read events even if the current request is not complete. The pipelined request will be buffered 
                and will be ready when the current request completes.
             */
            q = trans->queue[HTTP_QUEUE_RECEIVE].nextQ;
            if (q->count < q->max) {
                eventMask |= MPR_READABLE;
            }
        } else {
            eventMask |= MPR_READABLE;
        }
        if (conn->startingThread) {
            conn->startingThread = 0;
            if (conn->waitHandler.fd >= 0) {
                mprRemoveWaitHandler(&conn->waitHandler);
                conn->waitHandler.fd = -1;
            }
        }
        mprLog(conn, 7, "EnableConnEvents mask %x", eventMask);
        if (eventMask) {
            if (conn->waitHandler.fd < 0) {
                mprInitWaitHandler(conn, &conn->waitHandler, conn->sock->fd, eventMask, conn->dispatcher, 
                    (MprEventProc) conn->callback, conn->callbackArg);
            } else if (eventMask != conn->waitHandler.desiredMask) {
                mprEnableWaitEvents(&conn->waitHandler, eventMask);
            }
        } else {
            if (conn->waitHandler.fd >= 0 && eventMask != conn->waitHandler.desiredMask) {
                mprEnableWaitEvents(&conn->waitHandler, eventMask);
            }
        }
        mprEnableDispatcher(conn->dispatcher);
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
static inline HttpPacket *getPacket(HttpConn *conn, int *bytesToRead)
{
    HttpPacket    *packet;
    MprBuf      *content;
    HttpReceiver   *req;
    int         len;

    req = conn->receiver;
    len = HTTP_BUFSIZE;

    //  MOB -- simplify. Okay to lose some optimization for chunked data?
    /*  
        The input packet may have pipelined headers and data left from the prior request. It may also have incomplete
        chunk boundary data.
     */
    if ((packet = conn->input) == NULL) {
        conn->input = packet = httpCreateConnPacket(conn, len);
    } else {
        content = packet->content;
        mprResetBufIfEmpty(content);
        if (req) {
            /*  
                Don't read more than the remainingContent unless chunked. We do this to minimize requests split 
                accross packets.
             */
            if (req->remainingContent) {
                len = req->remainingContent;
                if (req->flags & HTTP_REC_CHUNKED) {
                    len = max(len, HTTP_BUFSIZE);
                }
            }
            len = min(len, HTTP_BUFSIZE);
            mprAssert(len > 0);
            if (mprGetBufSpace(content) < len) {
                mprGrowBuf(content, len);
            }

        } else {
            //  MOB -- but this logic is not in the oter "then" case above.
            /* Still reading the headers */
            if (mprGetBufLength(content) >= conn->limits->headerSize) {
                httpLimitError(conn, HTTP_CODE_REQUEST_TOO_LARGE, "Header too big");
                return 0;
            }
            if (mprGetBufSpace(content) < HTTP_BUFSIZE) {
                mprGrowBuf(content, HTTP_BUFSIZE);
            }
            len = mprGetBufSpace(content);
        }
    }
    mprAssert(packet == conn->input);
    mprAssert(len > 0);
    *bytesToRead = len;
    return packet;
}


void httpCompleteRequest(HttpConn *conn)
{
    httpFinalize(conn);
    httpSetState(conn, HTTP_STATE_COMPLETE);
}


/*
    Called by connectors when writing the transmission is complete
 */
void httpCompleteWriting(HttpConn *conn)
{
    conn->transmitter->writeComplete = 1;
    if (conn->server) {
        if (HTTP_STATE_PROCESS <= conn->state && conn->state < HTTP_STATE_COMPLETE) {
            httpSetState(conn, HTTP_STATE_COMPLETE);
        }
    } else {
        httpSetState(conn, HTTP_STATE_WAIT);
    }
}


int httpGetAsync(HttpConn *conn)
{
    return conn->async;
}


int httpGetChunkSize(HttpConn *conn)
{
    if (conn->transmitter) {
        return conn->transmitter->chunkSize;
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
    } else if (conn->state > HTTP_STATE_WAIT) {
        return httpLookupStatus(conn->http, conn->receiver->status);
    } else {
        return "";
    }
}


void httpResetCredentials(HttpConn *conn)
{
    mprFree(conn->authDomain);
    mprFree(conn->authCnonce);
    mprFree(conn->authNonce);
    mprFree(conn->authOpaque);
    mprFree(conn->authPassword);
    mprFree(conn->authRealm);
    mprFree(conn->authQop);
    mprFree(conn->authType);
    mprFree(conn->authUser);

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


void httpSetRequestNotifier(HttpConn *conn, HttpNotifier notifier)
{
    conn->requestNotifier = notifier;
}


void httpSetCredentials(HttpConn *conn, cchar *user, cchar *password)
{
    httpResetCredentials(conn);
    conn->authUser = mprStrdup(conn, user);
    if (password == NULL && strchr(user, ':') != 0) {
        conn->authUser = mprStrTok(conn->authUser, ":", &conn->authPassword);
    } else {
        conn->authPassword = mprStrdup(conn, password);
    }
}


void httpSetKeepAliveCount(HttpConn *conn, int count)
{
    conn->keepAliveCount = count;
}


void httpSetCallback(HttpConn *conn, HttpCallback callback, void *arg)
{
    conn->callback = callback;
    conn->callbackArg = arg;
}


void httpSetChunkSize(HttpConn *conn, int size)
{
    if (conn->transmitter) {
        conn->transmitter->chunkSize = size;
    }
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
    Protocol must be persistent 
 */
void httpSetProtocol(HttpConn *conn, cchar *protocol)
{
    if (conn->state < HTTP_STATE_WAIT) {
        conn->protocol = protocol;
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
    "IO_EVENT", "BEGIN", "STARTED", "WAIT", "FIRST", "PARSED", "CONTENT", "PROCESS", "RUNNING", "ERROR", "COMPLETE",
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
    //  MOB -- generalize
    if (conn->state < HTTP_STATE_PARSED && (HTTP_STATE_PARSED < state && state <= HTTP_STATE_COMPLETE)) {
        if (!conn->connError && conn->receiver) {
            /* Go through the parsed state so a response can be returned to the user */
            conn->state = HTTP_STATE_PARSED;
            LOG(conn, 6, "Connection state change to %s", notifyState[conn->state]);
            HTTP_NOTIFY(conn, conn->state, 0);
        }
    }
    conn->state = state;
    LOG(conn, 6, "Connection state change to %s", notifyState[state]);
    HTTP_NOTIFY(conn, state, 0);

    //  MOB -- see if this can be removed
    if (state == HTTP_STATE_PROCESS && conn->error) {
        httpFinalize(conn);
    }
}


void httpSetTimeout(HttpConn *conn, int requestTimeout, int inactivityTimeout)
{
    if (requestTimeout >= 0) {
        conn->requestTimeout = requestTimeout * MPR_TICKS_PER_SEC;
    }
    if (inactivityTimeout >= 0) {
        conn->inactivityTimeout = inactivityTimeout * MPR_TICKS_PER_SEC;
    }
}


HttpLimits *httpSetUniqueConnLimits(HttpConn *conn)
{
    HttpLimits      *limits;

    limits = mprAllocObj(conn, HttpLimits);
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
    mprFree(conn->errorMsg);
    conn->errorMsg = mprVasprintf(conn, HTTP_BUFSIZE, fmt, args);
    conn->status = status;
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
    Cases:

    Always:
        - Abort normal output and send a formatted error message. If output has already begun, this will be ignored.

    1. Can't write to client - connection lost
        Close connection immediately. 
    2. Can write to client, but must not continue with request. 
        If headers have gone, MUST close connection. If headers have not gone, could keep connection alive.

    - Client messed up
        - Bad protocol
        - Too big, exceed limits
    - Connection lots
    - Server messed up

    How to know if started writing: (trans->flags & HTTP_TRANS_HEADERS_CREATED)

    httpError
        - Send error to client
        - If headers gone, and not already error - must close connection (relay to httpCloseError)
        - Discard read data
    httpConnError
        - Connection error
        - Force connection close
        - Try to first send response 

    How can request
        - Force a connection closure?
 */
/*
    The current request has an error and cannot complete as normal. This call sets the Http response status and 
    overrides the normal output with an alternate error message. If the output has alread started (headers sent), then
    the connection MUST be closed so the client can get some indication the request failed.
 */
static void httpErrorV(HttpConn *conn, int status, cchar *fmt, va_list args)
{
    HttpReceiver    *rec;
    HttpTransmitter *trans;

    mprAssert(fmt);

    rec = conn->receiver;
    trans = conn->transmitter;

    if (!conn->error) {
        conn->error = 1;
        httpFormatErrorV(conn, status, fmt, args);
        if (rec == 0) {
            mprLog(conn, 2, "\"%s\", status %d: %s.", httpLookupStatus(conn->http, status), status, conn->errorMsg);
        } else {
            mprLog(conn, 2, "Error: \"%s\", status %d for URI \"%s\": %s.",
                httpLookupStatus(conn->http, status), status, rec->uri ? rec->uri : "", conn->errorMsg);
        }
        if (trans->flags & HTTP_TRANS_HEADERS_CREATED) {
            if (conn->server) {
                /* Headers and status have been sent, so must let the client know the request has failed */
                mprDisconnectSocket(conn->sock);
            } else {
                httpCloseConn(conn);
            }
            httpSetState(conn, HTTP_STATE_ERROR);
            httpSetState(conn, HTTP_STATE_COMPLETE);
        } else {
            if (conn->server && conn->transmitter) {
                httpSetResponseBody(conn, status, conn->errorMsg);
            }
            /* 
               If remaining content, dont' change state so any receive body data can be consumed and the 
               connection be kept alive 
             */
            if (rec->remainingContent <= 0) {
                httpSetState(conn, HTTP_STATE_ERROR);
            }
        }
    }
}


void httpError(HttpConn *conn, int status, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    httpErrorV(conn, status, fmt, args);
    va_end(args);
}


void httpLimitError(HttpConn *conn, int status, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    httpErrorV(conn, status, fmt, args);
    va_end(args);
}


/*
    A HTTP protocol error has occurred. For servers: best to stop future requests on the current connection, but 
    try to send a meaningful response back to the client. For clients, just close the connection.
 */
void httpProtocolError(HttpConn *conn, int status, cchar *fmt, ...)
{
    va_list     args;

    if (!conn->connError) { 
        conn->connError = 1;
        va_start(args, fmt);
        httpErrorV(conn, status, fmt, args);
        va_end(args);
        if (conn->server) {
            httpCompleteRequest(conn);
            conn->keepAliveCount = -1;
        } else {
            /* Handlers must not call CloseConn as it disables wait events */
            httpCloseConn(conn);
        }
    }
}


/*
    The connection has been broken. Fail the current request and the processing pipeline. Close the connection.
 */
void httpConnError(HttpConn *conn, int status, cchar *fmt, ...)
{
    va_list     args;

    if (!conn->connError) { 
        conn->connError = 1;
        conn->keepAliveCount = -1;
        va_start(args, fmt);
        httpErrorV(conn, status, fmt, args);
        va_end(args);
        if (conn->server) {
            mprDisconnectSocket(conn->sock);
#if UNUSED
            httpCompleteRequest(conn);
#endif
        } else {
            /* Handlers must not call CloseConn as it disables wait events */
            httpCloseConn(conn);
        }
    }
}


int httpSetupTrace(HttpConn *conn, cchar *ext)
{
    if (conn->traceLevel > mprGetLogLevel(conn)) {
        return 0;
    }
    if (ext) {
        if (conn->traceInclude && !mprLookupHash(conn->traceInclude, ext)) {
            return 0;
        }
        if (conn->traceExclude && mprLookupHash(conn->traceExclude, ext)) {
            return 0;
        }
    }
    return conn->traceMask;
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
